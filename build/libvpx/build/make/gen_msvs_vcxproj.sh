#!/bin/bash
##
##  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##


self=$0
self_basename=${self##*/}
self_dirname=$(dirname "$0")
EOL=$'\n'

show_help() {
    cat <<EOF
Usage: ${self_basename} --name=projname [options] file1 [file2 ...]

This script generates a Visual Studio project file from a list of source
code files.

Options:
    --help                      Print this message
    --exe                       Generate a project for building an Application
    --lib                       Generate a project for creating a static library
    --dll                       Generate a project for creating a dll
    --static-crt                Use the static C runtime (/MT)
    --enable-werror             Treat warnings as errors (/WX)
    --target=isa-os-cc          Target specifier (required)
    --out=filename              Write output to a file [stdout]
    --name=project_name         Name of the project (required)
    --proj-guid=GUID            GUID to use for the project
    --module-def=filename       File containing export definitions (for DLLs)
    --ver=version               Version (10,11,12) of visual studio to generate for
    --src-path-bare=dir         Path to root of source tree
    -Ipath/to/include           Additional include directories
    -DFLAG[=value]              Preprocessor macros to define
    -Lpath/to/lib               Additional library search paths
    -llibname                   Library to link against
EOF
    exit 1
}

die() {
    echo "${self_basename}: $@" >&2
    exit 1
}

die_unknown(){
    echo "Unknown option \"$1\"." >&2
    echo "See ${self_basename} --help for available options." >&2
    exit 1
}

generate_uuid() {
    local hex="0123456789ABCDEF"
    local i
    local uuid=""
    local j
    #93995380-89BD-4b04-88EB-625FBE52EBFB
    for ((i=0; i<32; i++)); do
        (( j = $RANDOM % 16 ))
        uuid="${uuid}${hex:$j:1}"
    done
    echo "${uuid:0:8}-${uuid:8:4}-${uuid:12:4}-${uuid:16:4}-${uuid:20:12}"
}

indent1="    "
indent=""
indent_push() {
    indent="${indent}${indent1}"
}
indent_pop() {
    indent="${indent%${indent1}}"
}

tag_attributes() {
    for opt in "$@"; do
        optval="${opt#*=}"
        [ -n "${optval}" ] ||
            die "Missing attribute value in '$opt' while generating $tag tag"
        echo "${indent}${opt%%=*}=\"${optval}\""
    done
}

open_tag() {
    local tag=$1
    shift
    if [ $# -ne 0 ]; then
        echo "${indent}<${tag}"
        indent_push
        tag_attributes "$@"
        echo "${indent}>"
    else
        echo "${indent}<${tag}>"
        indent_push
    fi
}

close_tag() {
    local tag=$1
    indent_pop
    echo "${indent}</${tag}>"
}

tag() {
    local tag=$1
    shift
    if [ $# -ne 0 ]; then
        echo "${indent}<${tag}"
        indent_push
        tag_attributes "$@"
        indent_pop
        echo "${indent}/>"
    else
        echo "${indent}<${tag}/>"
    fi
}

tag_content() {
    local tag=$1
    local content=$2
    shift
    shift
    if [ $# -ne 0 ]; then
        echo "${indent}<${tag}"
        indent_push
        tag_attributes "$@"
        echo "${indent}>${content}</${tag}>"
        indent_pop
    else
        echo "${indent}<${tag}>${content}</${tag}>"
    fi
}

generate_filter() {
    local name=$1
    local pats=$2
    local file_list_sz
    local i
    local f
    local saveIFS="$IFS"
    local pack
    echo "generating filter '$name' from ${#file_list[@]} files" >&2
    IFS=*

    file_list_sz=${#file_list[@]}
    for i in ${!file_list[@]}; do
        f=${file_list[i]}
        for pat in ${pats//;/$IFS}; do
            if [ "${f##*.}" == "$pat" ]; then
                unset file_list[i]

                objf=$(echo ${f%.*}.obj | sed -e 's/^[\./]\+//g' -e 's,/,_,g')

                if ([ "$pat" == "asm" ] || [ "$pat" == "s" ]) && $asm_use_custom_step; then
                    # Avoid object file name collisions, i.e. vpx_config.c and
                    # vpx_config.asm produce the same object file without
                    # this additional suffix.
                    objf=${objf%.obj}_asm.obj
                    open_tag CustomBuild \
                        Include=".\\$f"
                    for plat in "${platforms[@]}"; do
                        for cfg in Debug Release; do
                            tag_content Message "Assembling %(Filename)%(Extension)" \
                                Condition="'\$(Configuration)|\$(Platform)'=='$cfg|$plat'"
                            tag_content Command "$(eval echo \$asm_${cfg}_cmdline) -o \$(IntDir)$objf" \
                                Condition="'\$(Configuration)|\$(Platform)'=='$cfg|$plat'"
                            tag_content Outputs "\$(IntDir)$objf" \
                                Condition="'\$(Configuration)|\$(Platform)'=='$cfg|$plat'"
                        done
                    done
                    close_tag CustomBuild
                elif [ "$pat" == "c" ] || \
                     [ "$pat" == "cc" ] || [ "$pat" == "cpp" ]; then
                    open_tag ClCompile \
                        Include=".\\$f"
                    # Separate file names with Condition?
                    tag_content ObjectFileName "\$(IntDir)$objf"
                    # Check for AVX and turn it on to avoid warnings.
                    if [[ $f =~ avx.?\.c$ ]]; then
                        tag_content AdditionalOptions "/arch:AVX"
                    fi
                    close_tag ClCompile
                elif [ "$pat" == "h" ] ; then
                    tag ClInclude \
                        Include=".\\$f"
                elif [ "$pat" == "vcxproj" ] ; then
                    open_tag ProjectReference \
                        Include="$f"
                    depguid=`grep ProjectGuid "$f" | sed 's,.*<.*>\(.*\)</.*>.*,\1,'`
                    tag_content Project "$depguid"
                    tag_content ReferenceOutputAssembly false
                    close_tag ProjectReference
                else
                    tag None \
                        Include=".\\$f"
                fi

                break
            fi
        done
    done

    IFS="$saveIFS"
}

# Process command line
unset target
for opt in "$@"; do
    optval="${opt#*=}"
    case "$opt" in
        --help|-h) show_help
        ;;
        --target=*) target="${optval}"
        ;;
        --out=*) outfile="$optval"
        ;;
        --name=*) name="${optval}"
        ;;
        --proj-guid=*) guid="${optval}"
        ;;
        --module-def=*) module_def="${optval}"
        ;;
        --exe) proj_kind="exe"
        ;;
        --dll) proj_kind="dll"
        ;;
        --lib) proj_kind="lib"
        ;;
        --src-path-bare=*) src_path_bare="$optval"
        ;;
        --static-crt) use_static_runtime=true
        ;;
        --enable-werror) werror=true
        ;;
        --ver=*)
            vs_ver="$optval"
            case "$optval" in
                10|11|12)
                ;;
                *) die Unrecognized Visual Studio Version in $opt
                ;;
            esac
        ;;
        -I*)
            opt="${opt%/}"
            incs="${incs}${incs:+;}${opt##-I}"
            yasmincs="${yasmincs} ${opt}"
        ;;
        -D*) defines="${defines}${defines:+;}${opt##-D}"
        ;;
        -L*) # fudge . to $(OutDir)
            if [ "${opt##-L}" == "." ]; then
                libdirs="${libdirs}${libdirs:+;}\$(OutDir)"
            else
                 # Also try directories for this platform/configuration
                 libdirs="${libdirs}${libdirs:+;}${opt##-L}"
                 libdirs="${libdirs}${libdirs:+;}${opt##-L}/\$(PlatformName)/\$(Configuration)"
                 libdirs="${libdirs}${libdirs:+;}${opt##-L}/\$(PlatformName)"
            fi
        ;;
        -l*) libs="${libs}${libs:+ }${opt##-l}.lib"
        ;;
        -*) die_unknown $opt
        ;;
        *)
            file_list[${#file_list[@]}]="$opt"
            case "$opt" in
                 *.asm|*.s) uses_asm=true
                 ;;
            esac
        ;;
    esac
done
outfile=${outfile:-/dev/stdout}
guid=${guid:-`generate_uuid`}
asm_use_custom_step=false
uses_asm=${uses_asm:-false}
case "${vs_ver:-11}" in
    10|11|12)
       asm_use_custom_step=$uses_asm
    ;;
esac

[ -n "$name" ] || die "Project name (--name) must be specified!"
[ -n "$target" ] || die "Target (--target) must be specified!"

if ${use_static_runtime:-false}; then
    release_runtime=MultiThreaded
    debug_runtime=MultiThreadedDebug
    lib_sfx=mt
else
    release_runtime=MultiThreadedDLL
    debug_runtime=MultiThreadedDebugDLL
    lib_sfx=md
fi

# Calculate debug lib names: If a lib ends in ${lib_sfx}.lib, then rename
# it to ${lib_sfx}d.lib. This precludes linking to release libs from a
# debug exe, so this may need to be refactored later.
for lib in ${libs}; do
    if [ "$lib" != "${lib%${lib_sfx}.lib}" ]; then
        lib=${lib%.lib}d.lib
    fi
    debug_libs="${debug_libs}${debug_libs:+ }${lib}"
done
debug_libs=${debug_libs// /;}
libs=${libs// /;}


# List of all platforms supported for this target
case "$target" in
    x86_64*)
        platforms[0]="x64"
        asm_Debug_cmdline="yasm -Xvc -g cv8 -f \$(PlatformName) ${yasmincs} &quot;%(FullPath)&quot;"
        asm_Release_cmdline="yasm -Xvc -f \$(PlatformName) ${yasmincs} &quot;%(FullPath)&quot;"
    ;;
    x86*)
        platforms[0]="Win32"
        asm_Debug_cmdline="yasm -Xvc -g cv8 -f \$(PlatformName) ${yasmincs} &quot;%(FullPath)&quot;"
        asm_Release_cmdline="yasm -Xvc -f \$(PlatformName) ${yasmincs} &quot;%(FullPath)&quot;"
    ;;
    arm*)
        asm_Debug_cmdline="armasm -nologo &quot;%(FullPath)&quot;"
        asm_Release_cmdline="armasm -nologo &quot;%(FullPath)&quot;"
        if [ "$name" = "obj_int_extract" ]; then
            # We don't want to build this tool for the target architecture,
            # but for an architecture we can run locally during the build.
            platforms[0]="Win32"
        else
            platforms[0]="ARM"
        fi
    ;;
    *) die "Unsupported target $target!"
    ;;
esac

generate_vcxproj() {
    echo "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    open_tag Project \
        DefaultTargets="Build" \
        ToolsVersion="4.0" \
        xmlns="http://schemas.microsoft.com/developer/msbuild/2003" \

    open_tag ItemGroup \
        Label="ProjectConfigurations"
    for plat in "${platforms[@]}"; do
        for config in Debug Release; do
            open_tag ProjectConfiguration \
                Include="$config|$plat"
            tag_content Configuration $config
            tag_content Platform $plat
            close_tag ProjectConfiguration
        done
    done
    close_tag ItemGroup

    open_tag PropertyGroup \
        Label="Globals"
        tag_content ProjectGuid "{${guid}}"
        tag_content RootNamespace ${name}
        tag_content Keyword ManagedCProj
    close_tag PropertyGroup

    tag Import \
        Project="\$(VCTargetsPath)\\Microsoft.Cpp.Default.props"

    for plat in "${platforms[@]}"; do
        for config in Release Debug; do
            open_tag PropertyGroup \
                Condition="'\$(Configuration)|\$(Platform)'=='$config|$plat'" \
                Label="Configuration"
            if [ "$proj_kind" = "exe" ]; then
                tag_content ConfigurationType Application
            elif [ "$proj_kind" = "dll" ]; then
                tag_content ConfigurationType DynamicLibrary
            else
                tag_content ConfigurationType StaticLibrary
            fi
            if [ "$vs_ver" = "11" ]; then
                if [ "$plat" = "ARM" ]; then
                    # Setting the wp80 toolchain automatically sets the
                    # WINAPI_FAMILY define, which is required for building
                    # code for arm with the windows headers. Alternatively,
                    # one could add AppContainerApplication=true in the Globals
                    # section and add PrecompiledHeader=NotUsing and
                    # CompileAsWinRT=false in ClCompile and SubSystem=Console
                    # in Link.
                    tag_content PlatformToolset v110_wp80
                else
                    tag_content PlatformToolset v110
                fi
            fi
            if [ "$vs_ver" = "12" ]; then
                if [ "$plat" = "ARM" ]; then
                    # Setting the wp80 toolchain automatically sets the
                    # WINAPI_FAMILY define, which is required for building
                    # code for arm with the windows headers. Alternatively,
                    # one could add AppContainerApplication=true in the Globals
                    # section and add PrecompiledHeader=NotUsing and
                    # CompileAsWinRT=false in ClCompile and SubSystem=Console
                    # in Link.
                    tag_content PlatformToolset v120_wp80
                else
                    tag_content PlatformToolset v120
                fi
            fi
            tag_content CharacterSet Unicode
            if [ "$config" = "Release" ]; then
                tag_content WholeProgramOptimization true
            fi
            close_tag PropertyGroup
        done
    done

    tag Import \
        Project="\$(VCTargetsPath)\\Microsoft.Cpp.props"

    open_tag ImportGroup \
        Label="PropertySheets"
        tag Import \
            Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" \
            Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" \
            Label="LocalAppDataPlatform"
    close_tag ImportGroup

    tag PropertyGroup \
        Label="UserMacros"

    for plat in "${platforms[@]}"; do
        plat_no_ws=`echo $plat | sed 's/[^A-Za-z0-9_]/_/g'`
        for config in Debug Release; do
            open_tag PropertyGroup \
                Condition="'\$(Configuration)|\$(Platform)'=='$config|$plat'"
            tag_content OutDir "\$(SolutionDir)$plat_no_ws\\\$(Configuration)\\"
            tag_content IntDir "$plat_no_ws\\\$(Configuration)\\${name}\\"
            if [ "$proj_kind" == "lib" ]; then
              if [ "$config" == "Debug" ]; then
                config_suffix=d
              else
                config_suffix=""
              fi
              tag_content TargetName "${name}${lib_sfx}${config_suffix}"
            fi
            close_tag PropertyGroup
        done
    done

    for plat in "${platforms[@]}"; do
        for config in Debug Release; do
            open_tag ItemDefinitionGroup \
                Condition="'\$(Configuration)|\$(Platform)'=='$config|$plat'"
            if [ "$name" == "vpx" ]; then
                hostplat=$plat
                if [ "$hostplat" == "ARM" ]; then
                    hostplat=Win32
                fi
                open_tag PreBuildEvent
                tag_content Command "call obj_int_extract.bat $src_path_bare $hostplat\\\$(Configuration)"
                close_tag PreBuildEvent
            fi
            open_tag ClCompile
            if [ "$config" = "Debug" ]; then
                opt=Disabled
                runtime=$debug_runtime
                curlibs=$debug_libs
                case "$name" in
                obj_int_extract)
                    debug=DEBUG
                    ;;
                *)
                    debug=_DEBUG
                    ;;
                esac
            else
                opt=MaxSpeed
                runtime=$release_runtime
                curlibs=$libs
                tag_content FavorSizeOrSpeed Speed
                debug=NDEBUG
            fi
            case "$name" in
            obj_int_extract)
                extradefines=";_CONSOLE"
                ;;
            *)
                extradefines=";$defines"
                ;;
            esac
            tag_content Optimization $opt
            tag_content AdditionalIncludeDirectories "$incs;%(AdditionalIncludeDirectories)"
            tag_content PreprocessorDefinitions "WIN32;$debug;_CRT_SECURE_NO_WARNINGS;_CRT_SECURE_NO_DEPRECATE$extradefines;%(PreprocessorDefinitions)"
            tag_content RuntimeLibrary $runtime
            tag_content WarningLevel Level3
            if ${werror:-false}; then
                tag_content TreatWarningAsError true
            fi
            close_tag ClCompile
            case "$proj_kind" in
            exe)
                open_tag Link
                if [ "$name" != "obj_int_extract" ]; then
                    tag_content AdditionalDependencies "$curlibs"
                    tag_content AdditionalLibraryDirectories "$libdirs;%(AdditionalLibraryDirectories)"
                fi
                tag_content GenerateDebugInformation true
                close_tag Link
                ;;
            dll)
                open_tag Link
                tag_content GenerateDebugInformation true
                tag_content ModuleDefinitionFile $module_def
                close_tag Link
                ;;
            lib)
                ;;
            esac
            close_tag ItemDefinitionGroup
        done

    done

    open_tag ItemGroup
    generate_filter "Source Files"   "c;cc;cpp;def;odl;idl;hpj;bat;asm;asmx;s"
    close_tag ItemGroup
    open_tag ItemGroup
    generate_filter "Header Files"   "h;hm;inl;inc;xsd"
    close_tag ItemGroup
    open_tag ItemGroup
    generate_filter "Build Files"    "mk"
    close_tag ItemGroup
    open_tag ItemGroup
    generate_filter "References"     "vcxproj"
    close_tag ItemGroup

    tag Import \
        Project="\$(VCTargetsPath)\\Microsoft.Cpp.targets"

    open_tag ImportGroup \
        Label="ExtensionTargets"
    close_tag ImportGroup

    close_tag Project

    # This must be done from within the {} subshell
    echo "Ignored files list (${#file_list[@]} items) is:" >&2
    for f in "${file_list[@]}"; do
        echo "    $f" >&2
    done
}

# This regexp doesn't catch most of the strings in the vcxproj format,
# since they're like <tag>path</tag> instead of <tag attr="path" />
# as previously. It still seems to work ok despite this.
generate_vcxproj |
    sed  -e '/"/s;\([^ "]\)/;\1\\;g' |
    sed  -e '/xmlns/s;\\;/;g' > ${outfile}

exit
