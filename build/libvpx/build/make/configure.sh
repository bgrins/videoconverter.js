#!/bin/sh
##
##  configure.sh
##
##  This script is sourced by the main configure script and contains
##  utility functions and other common bits that aren't strictly libvpx
##  related.
##
##  This build system is based in part on the FFmpeg configure script.
##


#
# Logging / Output Functions
#
die_unknown(){
    echo "Unknown option \"$1\"."
    echo "See $0 --help for available options."
    clean_temp_files
    exit 1
}


die() {
    echo "$@"
    echo
    echo "Configuration failed. This could reflect a misconfiguration of your"
    echo "toolchains, improper options selected, or another problem. If you"
    echo "don't see any useful error messages above, the next step is to look"
    echo "at the configure error log file ($logfile) to determine what"
    echo "configure was trying to do when it died."
    clean_temp_files
    exit 1
}


log(){
    echo "$@" >>$logfile
}


log_file(){
    log BEGIN $1
    cat -n $1 >>$logfile
    log END $1
}


log_echo() {
    echo "$@"
    log "$@"
}


fwrite () {
    outfile=$1
    shift
    echo "$@" >> ${outfile}
}


show_help_pre(){
    for opt in ${CMDLINE_SELECT}; do
        opt2=`echo $opt | sed -e 's;_;-;g'`
        if enabled $opt; then
            eval "toggle_${opt}=\"--disable-${opt2}\""
        else
            eval "toggle_${opt}=\"--enable-${opt2} \""
        fi
    done

    cat <<EOF
Usage: configure [options]
Options:

Build options:
  --help                      print this message
  --log=yes|no|FILE           file configure log is written to [config.log]
  --target=TARGET             target platform tuple [generic-gnu]
  --cpu=CPU                   optimize for a specific cpu rather than a family
  --extra-cflags=ECFLAGS      add ECFLAGS to CFLAGS [$CFLAGS]
  ${toggle_extra_warnings}    emit harmless warnings (always non-fatal)
  ${toggle_werror}            treat warnings as errors, if possible
                              (not available with all compilers)
  ${toggle_optimizations}     turn on/off compiler optimization flags
  ${toggle_pic}               turn on/off Position Independent Code
  ${toggle_ccache}            turn on/off compiler cache
  ${toggle_debug}             enable/disable debug mode
  ${toggle_gprof}             enable/disable gprof profiling instrumentation
  ${toggle_gcov}              enable/disable gcov coverage instrumentation
  ${toggle_thumb}             enable/disable building arm assembly in thumb mode

Install options:
  ${toggle_install_docs}      control whether docs are installed
  ${toggle_install_bins}      control whether binaries are installed
  ${toggle_install_libs}      control whether libraries are installed
  ${toggle_install_srcs}      control whether sources are installed


EOF
}


show_help_post(){
    cat <<EOF


NOTES:
    Object files are built at the place where configure is launched.

    All boolean options can be negated. The default value is the opposite
    of that shown above. If the option --disable-foo is listed, then
    the default value for foo is enabled.

Supported targets:
EOF
  show_targets ${all_platforms}
  echo
  exit 1
}


show_targets() {
    while [ -n "$*" ]; do
        if [ "${1%%-*}" = "${2%%-*}" ]; then
            if [ "${2%%-*}" = "${3%%-*}" ]; then
                printf "    %-24s %-24s %-24s\n" "$1" "$2" "$3"
                shift; shift; shift
            else
                printf "    %-24s %-24s\n" "$1" "$2"
                shift; shift
            fi
        else
            printf "    %-24s\n" "$1"
            shift
        fi
    done
}


show_help() {
    show_help_pre
    show_help_post
}

#
# List Processing Functions
#
set_all(){
    value=$1
    shift
    for var in $*; do
        eval $var=$value
    done
}


is_in(){
    value=$1
    shift
    for var in $*; do
        [ $var = $value ] && return 0
    done
    return 1
}


add_cflags() {
    CFLAGS="${CFLAGS} $@"
    CXXFLAGS="${CXXFLAGS} $@"
}


add_cflags_only() {
    CFLAGS="${CFLAGS} $@"
}


add_cxxflags_only() {
    CXXFLAGS="${CXXFLAGS} $@"
}


add_ldflags() {
    LDFLAGS="${LDFLAGS} $@"
}


add_asflags() {
    ASFLAGS="${ASFLAGS} $@"
}


add_extralibs() {
    extralibs="${extralibs} $@"
}

#
# Boolean Manipulation Functions
#
enable_feature(){
    set_all yes $*
}

disable_feature(){
    set_all no $*
}

enabled(){
    eval test "x\$$1" = "xyes"
}

disabled(){
    eval test "x\$$1" = "xno"
}


soft_enable() {
    for var in $*; do
        if ! disabled $var; then
            log_echo "  enabling $var"
            enable_feature $var
        fi
    done
}

soft_disable() {
    for var in $*; do
        if ! enabled $var; then
            log_echo "  disabling $var"
            disable_feature $var
        fi
    done
}


#
# Text Processing Functions
#
toupper(){
    echo "$@" | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ
}


tolower(){
    echo "$@" | tr ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz
}


#
# Temporary File Functions
#
source_path=${0%/*}
enable_feature source_path_used
if test -z "$source_path" -o "$source_path" = "." ; then
    source_path="`pwd`"
    disable_feature source_path_used
fi

if test ! -z "$TMPDIR" ; then
    TMPDIRx="${TMPDIR}"
elif test ! -z "$TEMPDIR" ; then
    TMPDIRx="${TEMPDIR}"
else
    TMPDIRx="/tmp"
fi
RAND=$(awk 'BEGIN { srand(); printf "%d\n",(rand() * 32768)}')
TMP_H="${TMPDIRx}/vpx-conf-$$-${RAND}.h"
TMP_C="${TMPDIRx}/vpx-conf-$$-${RAND}.c"
TMP_CC="${TMPDIRx}/vpx-conf-$$-${RAND}.cc"
TMP_O="${TMPDIRx}/vpx-conf-$$-${RAND}.o"
TMP_X="${TMPDIRx}/vpx-conf-$$-${RAND}.x"
TMP_ASM="${TMPDIRx}/vpx-conf-$$-${RAND}.asm"

clean_temp_files() {
    rm -f ${TMP_C} ${TMP_CC} ${TMP_H} ${TMP_O} ${TMP_X} ${TMP_ASM}
    enabled gcov && rm -f ${TMP_C%.c}.gcno ${TMP_CC%.cc}.gcno
}

#
# Toolchain Check Functions
#
check_cmd() {
    enabled external_build && return
    log "$@"
    "$@" >>${logfile} 2>&1
}

check_cc() {
    log check_cc "$@"
    cat >${TMP_C}
    log_file ${TMP_C}
    check_cmd ${CC} ${CFLAGS} "$@" -c -o ${TMP_O} ${TMP_C}
}

check_cxx() {
    log check_cxx "$@"
    cat >${TMP_CC}
    log_file ${TMP_CC}
    check_cmd ${CXX} ${CXXFLAGS} "$@" -c -o ${TMP_O} ${TMP_CC}
}

check_cpp() {
    log check_cpp "$@"
    cat > ${TMP_C}
    log_file ${TMP_C}
    check_cmd ${CC} ${CFLAGS} "$@" -E -o ${TMP_O} ${TMP_C}
}

check_ld() {
    log check_ld "$@"
    check_cc $@ \
        && check_cmd ${LD} ${LDFLAGS} "$@" -o ${TMP_X} ${TMP_O} ${extralibs}
}

check_header(){
    log check_header "$@"
    header=$1
    shift
    var=`echo $header | sed 's/[^A-Za-z0-9_]/_/g'`
    disable_feature $var
    check_cpp "$@" <<EOF && enable_feature $var
#include "$header"
int x;
EOF
}


check_cflags() {
    log check_cflags "$@"
    check_cc -Werror "$@" <<EOF
int x;
EOF
}

check_cxxflags() {
    log check_cxxflags "$@"

    # Catch CFLAGS that trigger CXX warnings
    case "$CXX" in
      *c++-analyzer|*clang++|*g++*) check_cxx -Werror "$@" <<EOF
int x;
EOF
      ;;
      *) check_cxx -Werror "$@" <<EOF
int x;
EOF
      ;;
    esac
}

check_add_cflags() {
    check_cxxflags "$@" && add_cxxflags_only "$@"
    check_cflags "$@" && add_cflags_only "$@"
}

check_add_asflags() {
    log add_asflags "$@"
    add_asflags "$@"
}

check_add_ldflags() {
    log add_ldflags "$@"
    add_ldflags "$@"
}

check_asm_align() {
    log check_asm_align "$@"
    cat >${TMP_ASM} <<EOF
section .rodata
align 16
EOF
    log_file ${TMP_ASM}
    check_cmd ${AS} ${ASFLAGS} -o ${TMP_O} ${TMP_ASM}
    readelf -WS ${TMP_O} >${TMP_X}
    log_file ${TMP_X}
    if ! grep -q '\.rodata .* 16$' ${TMP_X}; then
        die "${AS} ${ASFLAGS} does not support section alignment (nasm <=2.08?)"
    fi
}

# tests for -m$1 toggling the feature given in $2. If $2 is empty $1 is used.
check_gcc_machine_option() {
    local opt="$1"
    local feature="$2"
    [ -n "$feature" ] || feature="$opt"

    if enabled gcc && ! disabled "$feature" && ! check_cflags "-m$opt"; then
        RTCD_OPTIONS="${RTCD_OPTIONS}--disable-$feature "
    else
        soft_enable "$feature"
    fi
}

write_common_config_banner() {
    print_webm_license config.mk "##" ""
    echo '# This file automatically generated by configure. Do not edit!' >> config.mk
    echo "TOOLCHAIN := ${toolchain}" >> config.mk

    case ${toolchain} in
        *-linux-rvct)
            echo "ALT_LIBC := ${alt_libc}" >> config.mk
            ;;
    esac
}

write_common_config_targets() {
    for t in ${all_targets}; do
        if enabled ${t}; then
            if enabled universal || enabled child; then
                fwrite config.mk "ALL_TARGETS += ${t}-${toolchain}"
            else
                fwrite config.mk "ALL_TARGETS += ${t}"
            fi
        fi
    true;
    done
true
}

write_common_target_config_mk() {
    local CC="${CC}"
    local CXX="${CXX}"
    enabled ccache && CC="ccache ${CC}"
    enabled ccache && CXX="ccache ${CXX}"
    print_webm_license $1 "##" ""

    cat >> $1 << EOF
# This file automatically generated by configure. Do not edit!
SRC_PATH="$source_path"
SRC_PATH_BARE=$source_path
BUILD_PFX=${BUILD_PFX}
TOOLCHAIN=${toolchain}
ASM_CONVERSION=${asm_conversion_cmd:-${source_path}/build/make/ads2gas.pl}
GEN_VCPROJ=${gen_vcproj_cmd}
MSVS_ARCH_DIR=${msvs_arch_dir}

CC=${CC}
CXX=${CXX}
AR=${AR}
LD=${LD}
AS=${AS}
STRIP=${STRIP}
NM=${NM}

CFLAGS  = ${CFLAGS}
CXXFLAGS  = ${CXXFLAGS}
ARFLAGS = rus\$(if \$(quiet),c,v)
LDFLAGS = ${LDFLAGS}
ASFLAGS = ${ASFLAGS}
extralibs = ${extralibs}
AS_SFX    = ${AS_SFX:-.asm}
EXE_SFX   = ${EXE_SFX}
VCPROJ_SFX = ${VCPROJ_SFX}
RTCD_OPTIONS = ${RTCD_OPTIONS}
EOF

    if enabled rvct; then cat >> $1 << EOF
fmt_deps = sed -e 's;^__image.axf;\${@:.d=.o} \$@;' #hide
EOF
    else cat >> $1 << EOF
fmt_deps = sed -e 's;^\([a-zA-Z0-9_]*\)\.o;\${@:.d=.o} \$@;'
EOF
    fi

    print_config_mk ARCH   "${1}" ${ARCH_LIST}
    print_config_mk HAVE   "${1}" ${HAVE_LIST}
    print_config_mk CONFIG "${1}" ${CONFIG_LIST}
    print_config_mk HAVE   "${1}" gnu_strip

    enabled msvs && echo "CONFIG_VS_VERSION=${vs_version}" >> "${1}"

}


write_common_target_config_h() {
    print_webm_license ${TMP_H} "/*" " */"
    cat >> ${TMP_H} << EOF
/* This file automatically generated by configure. Do not edit! */
#ifndef VPX_CONFIG_H
#define VPX_CONFIG_H
#define RESTRICT    ${RESTRICT}
#define INLINE      ${INLINE}
EOF
    print_config_h ARCH   "${TMP_H}" ${ARCH_LIST}
    print_config_h HAVE   "${TMP_H}" ${HAVE_LIST}
    print_config_h CONFIG "${TMP_H}" ${CONFIG_LIST}
    echo "#endif /* VPX_CONFIG_H */" >> ${TMP_H}
    mkdir -p `dirname "$1"`
    cmp "$1" ${TMP_H} >/dev/null 2>&1 || mv ${TMP_H} "$1"
}

process_common_cmdline() {
    for opt in "$@"; do
        optval="${opt#*=}"
        case "$opt" in
        --child) enable_feature child
        ;;
        --log*)
        logging="$optval"
        if ! disabled logging ; then
            enabled logging || logfile="$logging"
        else
            logfile=/dev/null
        fi
        ;;
        --target=*) toolchain="${toolchain:-${optval}}"
        ;;
        --force-target=*) toolchain="${toolchain:-${optval}}"; enable_feature force_toolchain
        ;;
        --cpu)
        ;;
        --cpu=*) tune_cpu="$optval"
        ;;
        --extra-cflags=*)
        extra_cflags="${optval}"
        ;;
        --enable-?*|--disable-?*)
        eval `echo "$opt" | sed 's/--/action=/;s/-/ option=/;s/-/_/g'`
        if echo "${ARCH_EXT_LIST}" | grep "^ *$option\$" >/dev/null; then
            [ $action = "disable" ] && RTCD_OPTIONS="${RTCD_OPTIONS}${opt} "
        elif [ $action = "disable" ] && ! disabled $option ; then
          echo "${CMDLINE_SELECT}" | grep "^ *$option\$" >/dev/null ||
            die_unknown $opt
        elif [ $action = "enable" ] && ! enabled $option ; then
          echo "${CMDLINE_SELECT}" | grep "^ *$option\$" >/dev/null ||
            die_unknown $opt
        fi
        ${action}_feature $option
        ;;
        --require-?*)
        eval `echo "$opt" | sed 's/--/action=/;s/-/ option=/;s/-/_/g'`
        if echo "${ARCH_EXT_LIST}" none | grep "^ *$option\$" >/dev/null; then
            RTCD_OPTIONS="${RTCD_OPTIONS}${opt} "
        else
            die_unknown $opt
        fi
        ;;
        --force-enable-?*|--force-disable-?*)
        eval `echo "$opt" | sed 's/--force-/action=/;s/-/ option=/;s/-/_/g'`
        ${action}_feature $option
        ;;
        --libc=*)
        [ -d "${optval}" ] || die "Not a directory: ${optval}"
        disable_feature builtin_libc
        alt_libc="${optval}"
        ;;
        --as=*)
        [ "${optval}" = yasm -o "${optval}" = nasm -o "${optval}" = auto ] \
            || die "Must be yasm, nasm or auto: ${optval}"
        alt_as="${optval}"
        ;;
        --prefix=*)
        prefix="${optval}"
        ;;
        --libdir=*)
        libdir="${optval}"
        ;;
        --sdk-path=*)
        [ -d "${optval}" ] || die "Not a directory: ${optval}"
        sdk_path="${optval}"
        ;;
        --libc|--as|--prefix|--libdir|--sdk-path)
        die "Option ${opt} requires argument"
        ;;
        --help|-h) show_help
        ;;
        *) die_unknown $opt
        ;;
        esac
    done
}

process_cmdline() {
    for opt do
        optval="${opt#*=}"
        case "$opt" in
        *) process_common_cmdline $opt
        ;;
        esac
    done
}


post_process_common_cmdline() {
    prefix="${prefix:-/usr/local}"
    prefix="${prefix%/}"
    libdir="${libdir:-${prefix}/lib}"
    libdir="${libdir%/}"
    if [ "${libdir#${prefix}}" = "${libdir}" ]; then
        die "Libdir ${libdir} must be a subdirectory of ${prefix}"
    fi
}


post_process_cmdline() {
    true;
}

setup_gnu_toolchain() {
        CC=${CC:-${CROSS}gcc}
        CXX=${CXX:-${CROSS}g++}
        AR=${AR:-${CROSS}ar}
        LD=${LD:-${CROSS}${link_with_cc:-ld}}
        AS=${AS:-${CROSS}as}
    STRIP=${STRIP:-${CROSS}strip}
    NM=${NM:-${CROSS}nm}
        AS_SFX=.s
        EXE_SFX=
}

process_common_toolchain() {
    if [ -z "$toolchain" ]; then
        gcctarget="${CHOST:-$(gcc -dumpmachine 2> /dev/null)}"

        # detect tgt_isa
        case "$gcctarget" in
            armv6*)
                tgt_isa=armv6
                ;;
            armv7*-hardfloat*)
                tgt_isa=armv7
                float_abi=hard
                ;;
            armv7*)
                tgt_isa=armv7
                float_abi=softfp
                ;;
            armv5te*)
                tgt_isa=armv5te
                ;;
            *x86_64*|*amd64*)
                tgt_isa=x86_64
                ;;
            *i[3456]86*)
                tgt_isa=x86
                ;;
            *powerpc64*)
                tgt_isa=ppc64
                ;;
            *powerpc*)
                tgt_isa=ppc32
                ;;
            *sparc*)
                tgt_isa=sparc
                ;;
        esac

        # detect tgt_os
        case "$gcctarget" in
            *darwin8*)
                tgt_isa=universal
                tgt_os=darwin8
                ;;
            *darwin9*)
                tgt_isa=universal
                tgt_os=darwin9
                ;;
            *darwin10*)
                tgt_isa=x86_64
                tgt_os=darwin10
                ;;
            *darwin11*)
                tgt_isa=x86_64
                tgt_os=darwin11
                ;;
            *darwin12*)
                tgt_isa=x86_64
                tgt_os=darwin12
                ;;
            *darwin13*)
                tgt_isa=x86_64
                tgt_os=darwin13
                ;;
            x86_64*mingw32*)
                tgt_os=win64
                ;;
            *mingw32*|*cygwin*)
                [ -z "$tgt_isa" ] && tgt_isa=x86
                tgt_os=win32
                ;;
            *linux*|*bsd*)
                tgt_os=linux
                ;;
            *solaris2.10)
                tgt_os=solaris
                ;;
            *os2*)
                tgt_os=os2
                ;;
        esac

        if [ -n "$tgt_isa" ] && [ -n "$tgt_os" ]; then
            toolchain=${tgt_isa}-${tgt_os}-gcc
        fi
    fi

    toolchain=${toolchain:-generic-gnu}

    is_in ${toolchain} ${all_platforms} || enabled force_toolchain \
        || die "Unrecognized toolchain '${toolchain}'"

    enabled child || log_echo "Configuring for target '${toolchain}'"

    #
    # Set up toolchain variables
    #
    tgt_isa=$(echo ${toolchain} | awk 'BEGIN{FS="-"}{print $1}')
    tgt_os=$(echo ${toolchain} | awk 'BEGIN{FS="-"}{print $2}')
    tgt_cc=$(echo ${toolchain} | awk 'BEGIN{FS="-"}{print $3}')

    # Mark the specific ISA requested as enabled
    soft_enable ${tgt_isa}
    enable_feature ${tgt_os}
    enable_feature ${tgt_cc}

    # Enable the architecture family
    case ${tgt_isa} in
        arm*) enable_feature arm;;
        mips*) enable_feature mips;;
    esac

    # PIC is probably what we want when building shared libs
    enabled shared && soft_enable pic

    # Handle darwin variants. Newer SDKs allow targeting older
    # platforms, so find the newest SDK available.
    case ${toolchain} in
        *-darwin*)
            if [ -z "${DEVELOPER_DIR}" ]; then
                DEVELOPER_DIR=`xcode-select -print-path 2> /dev/null`
                [ $? -ne 0 ] && OSX_SKIP_DIR_CHECK=1
            fi
            if [ -z "${OSX_SKIP_DIR_CHECK}" ]; then
                OSX_SDK_ROOTS="${DEVELOPER_DIR}/SDKs"
                OSX_SDK_VERSIONS="MacOSX10.4u.sdk MacOSX10.5.sdk MacOSX10.6.sdk"
                OSX_SDK_VERSIONS="${OSX_SDK_VERSIONS} MacOSX10.7.sdk"
                for v in ${OSX_SDK_VERSIONS}; do
                    if [ -d "${OSX_SDK_ROOTS}/${v}" ]; then
                        osx_sdk_dir="${OSX_SDK_ROOTS}/${v}"
                    fi
                done
            fi
            ;;
    esac

    if [ -d "${osx_sdk_dir}" ]; then
        add_cflags  "-isysroot ${osx_sdk_dir}"
        add_ldflags "-isysroot ${osx_sdk_dir}"
    fi

    case ${toolchain} in
        *-darwin8-*)
            add_cflags  "-mmacosx-version-min=10.4"
            add_ldflags "-mmacosx-version-min=10.4"
            ;;
        *-darwin9-*)
            add_cflags  "-mmacosx-version-min=10.5"
            add_ldflags "-mmacosx-version-min=10.5"
            ;;
        *-darwin10-*)
            add_cflags  "-mmacosx-version-min=10.6"
            add_ldflags "-mmacosx-version-min=10.6"
            ;;
        *-darwin11-*)
            add_cflags  "-mmacosx-version-min=10.7"
            add_ldflags "-mmacosx-version-min=10.7"
            ;;
        *-darwin12-*)
            add_cflags  "-mmacosx-version-min=10.8"
            add_ldflags "-mmacosx-version-min=10.8"
            ;;
        *-darwin13-*)
            add_cflags  "-mmacosx-version-min=10.9"
            add_ldflags "-mmacosx-version-min=10.9"
            ;;
    esac

    # Handle Solaris variants. Solaris 10 needs -lposix4
    case ${toolchain} in
        sparc-solaris-*)
            add_extralibs -lposix4
            disable_feature fast_unaligned
            ;;
        *-solaris-*)
            add_extralibs -lposix4
            ;;
    esac

    # Process ARM architecture variants
    case ${toolchain} in
    arm*)
        # on arm, isa versions are supersets
        case ${tgt_isa} in
        armv7)
            soft_enable neon
            soft_enable media
            soft_enable edsp
            soft_enable fast_unaligned
            ;;
        armv6)
            soft_enable media
            soft_enable edsp
            soft_enable fast_unaligned
            ;;
        armv5te)
            soft_enable edsp
            disable_feature fast_unaligned
            ;;
        esac

        asm_conversion_cmd="cat"

        case ${tgt_cc} in
        gcc)
            CROSS=${CROSS:-arm-none-linux-gnueabi-}
            link_with_cc=gcc
            setup_gnu_toolchain
            arch_int=${tgt_isa##armv}
            arch_int=${arch_int%%te}
            check_add_asflags --defsym ARCHITECTURE=${arch_int}
            tune_cflags="-mtune="
            if [ ${tgt_isa} = "armv7" ]; then
                if [ -z "${float_abi}" ]; then
                    check_cpp <<EOF && float_abi=hard || float_abi=softfp
#ifndef __ARM_PCS_VFP
#error "not hardfp"
#endif
EOF
                fi
                check_add_cflags  -march=armv7-a -mfloat-abi=${float_abi}
                check_add_asflags -march=armv7-a -mfloat-abi=${float_abi}

                if enabled neon
                then
                    check_add_cflags -mfpu=neon #-ftree-vectorize
                    check_add_asflags -mfpu=neon
                fi

                if [ -z "${tune_cpu}" ]; then
                    tune_cpu=cortex-a8
                fi
            else
                check_add_cflags -march=${tgt_isa}
                check_add_asflags -march=${tgt_isa}
            fi

            enabled debug && add_asflags -g
            asm_conversion_cmd="${source_path}/build/make/ads2gas.pl"
            if enabled thumb; then
                asm_conversion_cmd="$asm_conversion_cmd -thumb"
                check_add_cflags -mthumb
                check_add_asflags -mthumb -mimplicit-it=always
            fi
            ;;
        vs*)
            asm_conversion_cmd="${source_path}/build/make/ads2armasm_ms.pl"
            AS_SFX=.s
            msvs_arch_dir=arm-msvs
            disable_feature multithread
            disable_feature unit_tests
            ;;
        rvct)
            CC=armcc
            AR=armar
            AS=armasm
            LD=${source_path}/build/make/armlink_adapter.sh
            STRIP=arm-none-linux-gnueabi-strip
            NM=arm-none-linux-gnueabi-nm
            tune_cflags="--cpu="
            tune_asflags="--cpu="
            if [ -z "${tune_cpu}" ]; then
                if [ ${tgt_isa} = "armv7" ]; then
                    if enabled neon
                    then
                        check_add_cflags --fpu=softvfp+vfpv3
                        check_add_asflags --fpu=softvfp+vfpv3
                    fi
                    check_add_cflags --cpu=Cortex-A8
                    check_add_asflags --cpu=Cortex-A8
                else
                    check_add_cflags --cpu=${tgt_isa##armv}
                    check_add_asflags --cpu=${tgt_isa##armv}
                fi
            fi
            arch_int=${tgt_isa##armv}
            arch_int=${arch_int%%te}
            check_add_asflags --pd "\"ARCHITECTURE SETA ${arch_int}\""
            enabled debug && add_asflags -g
            add_cflags --gnu
            add_cflags --enum_is_int
            add_cflags --wchar32
        ;;
        esac

        case ${tgt_os} in
        none*)
            disable_feature multithread
            disable_feature os_support
            ;;

        android*)
            SDK_PATH=${sdk_path}
            COMPILER_LOCATION=`find "${SDK_PATH}" \
                               -name "arm-linux-androideabi-gcc*" -print -quit`
            TOOLCHAIN_PATH=${COMPILER_LOCATION%/*}/arm-linux-androideabi-
            CC=${TOOLCHAIN_PATH}gcc
            CXX=${TOOLCHAIN_PATH}g++
            AR=${TOOLCHAIN_PATH}ar
            LD=${TOOLCHAIN_PATH}gcc
            AS=${TOOLCHAIN_PATH}as
            STRIP=${TOOLCHAIN_PATH}strip
            NM=${TOOLCHAIN_PATH}nm

            if [ -z "${alt_libc}" ]; then
                alt_libc=`find "${SDK_PATH}" -name arch-arm -print | \
                          awk '{n = split($0,a,"/"); \
                                split(a[n-1],b,"-"); \
                                print $0 " " b[2]}' | \
                          sort -g -k 2 | \
                          awk '{ print $1 }' | tail -1`
            fi

            add_cflags "--sysroot=${alt_libc}"
            add_ldflags "--sysroot=${alt_libc}"

            # linker flag that routes around a CPU bug in some
            # Cortex-A8 implementations (NDK Dev Guide)
            add_ldflags "-Wl,--fix-cortex-a8"

            enable_feature pic
            soft_enable realtime_only
            if [ ${tgt_isa} = "armv7" ]; then
                soft_enable runtime_cpu_detect
            fi
            if enabled runtime_cpu_detect; then
                add_cflags "-I${SDK_PATH}/sources/android/cpufeatures"
            fi
          ;;

        darwin*)

            XCRUN_FIND="xcrun --sdk iphoneos -find"
            CXX="$(${XCRUN_FIND} clang++)"
            CC="$(${XCRUN_FIND} clang)"
            AR="$(${XCRUN_FIND} ar)"
            LD="$(${XCRUN_FIND} ld)"
            AS="$(${XCRUN_FIND} as)"
            STRIP="$(${XCRUN_FIND} strip)"
            NM="$(${XCRUN_FIND} nm)"
            RANLIB="$(${XCRUN_FIND} ranlib)"
            AS_SFX=.s

            # ASFLAGS is written here instead of using check_add_asflags
            # because we need to overwrite all of ASFLAGS and purge the
            # options that were put in above
            ASFLAGS="-arch ${tgt_isa} -g"

            alt_libc="$(xcrun --sdk iphoneos --show-sdk-path)"
            add_cflags -arch ${tgt_isa} -isysroot ${alt_libc}
            add_ldflags -arch ${tgt_isa} -ios_version_min 7.0

            for d in lib usr/lib usr/lib/system; do
                try_dir="${alt_libc}/${d}"
                [ -d "${try_dir}" ] && add_ldflags -L"${try_dir}"
            done

            asm_conversion_cmd="${source_path}/build/make/ads2gas_apple.pl"
         ;;

        linux*)
            enable_feature linux
            if enabled rvct; then
                # Check if we have CodeSourcery GCC in PATH. Needed for
                # libraries
                hash arm-none-linux-gnueabi-gcc 2>&- || \
                  die "Couldn't find CodeSourcery GCC from PATH"

                # Use armcc as a linker to enable translation of
                # some gcc specific options such as -lm and -lpthread.
                LD="armcc --translate_gcc"

                # create configuration file (uses path to CodeSourcery GCC)
                armcc --arm_linux_configure --arm_linux_config_file=arm_linux.cfg

                add_cflags --arm_linux_paths --arm_linux_config_file=arm_linux.cfg
                add_asflags --no_hide_all --apcs=/interwork
                add_ldflags --arm_linux_paths --arm_linux_config_file=arm_linux.cfg
                enabled pic && add_cflags --apcs=/fpic
                enabled pic && add_asflags --apcs=/fpic
                enabled shared && add_cflags --shared
            fi
        ;;

        esac
    ;;
    mips*)
        link_with_cc=gcc
        setup_gnu_toolchain
        tune_cflags="-mtune="
        if enabled dspr2; then
            check_add_cflags -mips32r2 -mdspr2
            disable_feature fast_unaligned
        fi
        check_add_cflags -march=${tgt_isa}
        check_add_asflags -march=${tgt_isa}
        check_add_asflags -KPIC
    ;;
    ppc*)
        enable_feature ppc
        bits=${tgt_isa##ppc}
        link_with_cc=gcc
        setup_gnu_toolchain
        add_asflags -force_cpusubtype_ALL -I"\$(dir \$<)darwin"
        soft_enable altivec
        enabled altivec && add_cflags -maltivec

        case "$tgt_os" in
        linux*)
            add_asflags -maltivec -mregnames -I"\$(dir \$<)linux"
        ;;
        darwin*)
            darwin_arch="-arch ppc"
            enabled ppc64 && darwin_arch="${darwin_arch}64"
            add_cflags  ${darwin_arch} -m${bits} -fasm-blocks
            add_asflags ${darwin_arch} -force_cpusubtype_ALL -I"\$(dir \$<)darwin"
            add_ldflags ${darwin_arch} -m${bits}
            enabled altivec && add_cflags -faltivec
        ;;
        esac
    ;;
    x86*)
        bits=32
        enabled x86_64 && bits=64
        check_cpp <<EOF && bits=x32
#ifndef __ILP32__
#error "not x32"
#endif
EOF

        case  ${tgt_os} in
            win*)
                enabled gcc && add_cflags -fno-common
                ;;
            solaris*)
                CC=${CC:-${CROSS}gcc}
                CXX=${CXX:-${CROSS}g++}
                LD=${LD:-${CROSS}gcc}
                CROSS=${CROSS:-g}
                ;;
            os2)
                AS=${AS:-nasm}
                ;;
        esac

        AS="${alt_as:-${AS:-auto}}"
        case  ${tgt_cc} in
            icc*)
                CC=${CC:-icc}
                LD=${LD:-icc}
                setup_gnu_toolchain
                add_cflags -use-msasm  # remove -use-msasm too?
                # add -no-intel-extensions to suppress warning #10237
                # refer to http://software.intel.com/en-us/forums/topic/280199
                add_ldflags -i-static -no-intel-extensions
                enabled x86_64 && add_cflags -ipo -static -O3 -no-prec-div
                enabled x86_64 && AR=xiar
                case ${tune_cpu} in
                    atom*)
                        tune_cflags="-x"
                        tune_cpu="SSE3_ATOM"
                    ;;
                    *)
                        tune_cflags="-march="
                    ;;
                esac
            ;;
            gcc*)
                add_cflags -m${bits}
                add_ldflags -m${bits}
                link_with_cc=gcc
                tune_cflags="-march="
                setup_gnu_toolchain
                #for 32 bit x86 builds, -O3 did not turn on this flag
                enabled optimizations && disabled gprof && check_add_cflags -fomit-frame-pointer
            ;;
            vs*)
                # When building with Microsoft Visual Studio the assembler is
                # invoked directly. Checking at configure time is unnecessary.
                # Skip the check by setting AS arbitrarily
                AS=msvs
                msvs_arch_dir=x86-msvs
                vc_version=${tgt_cc##vs}
                case $vc_version in
                    7|8|9|10)
                         echo "${tgt_cc} does not support avx/avx2, disabling....."
                         RTCD_OPTIONS="${RTCD_OPTIONS}--disable-avx --disable-avx2 "
                         soft_disable avx
                         soft_disable avx2
                    ;;
                esac
            ;;
        esac

        soft_enable runtime_cpu_detect
        # We can't use 'check_cflags' until the compiler is configured and CC is
        # populated.
        check_gcc_machine_option mmx
        check_gcc_machine_option sse
        check_gcc_machine_option sse2
        check_gcc_machine_option sse3
        check_gcc_machine_option ssse3
        check_gcc_machine_option sse4 sse4_1
        check_gcc_machine_option avx
        check_gcc_machine_option avx2

        case "${AS}" in
            auto|"")
                which nasm >/dev/null 2>&1 && AS=nasm
                which yasm >/dev/null 2>&1 && AS=yasm
                [ "${AS}" = auto -o -z "${AS}" ] \
                    && die "Neither yasm nor nasm have been found"
            ;;
        esac
        log_echo "  using $AS"
        [ "${AS##*/}" = nasm ] && add_asflags -Ox
        AS_SFX=.asm
        case  ${tgt_os} in
            win32)
                add_asflags -f win32
                enabled debug && add_asflags -g cv8
                EXE_SFX=.exe
            ;;
            win64)
                add_asflags -f x64
                enabled debug && add_asflags -g cv8
                EXE_SFX=.exe
            ;;
            linux*|solaris*|android*)
                add_asflags -f elf${bits}
                enabled debug && [ "${AS}" = yasm ] && add_asflags -g dwarf2
                enabled debug && [ "${AS}" = nasm ] && add_asflags -g
                [ "${AS##*/}" = nasm ] && check_asm_align
            ;;
            darwin*)
                add_asflags -f macho${bits}
                enabled x86 && darwin_arch="-arch i386" || darwin_arch="-arch x86_64"
                add_cflags  ${darwin_arch}
                add_ldflags ${darwin_arch}
                # -mdynamic-no-pic is still a bit of voodoo -- it was required at
                # one time, but does not seem to be now, and it breaks some of the
                # code that still relies on inline assembly.
                # enabled icc && ! enabled pic && add_cflags -fno-pic -mdynamic-no-pic
                enabled icc && ! enabled pic && add_cflags -fno-pic
            ;;
            os2)
                add_asflags -f aout
                enabled debug && add_asflags -g
                EXE_SFX=.exe
            ;;
            *) log "Warning: Unknown os $tgt_os while setting up $AS flags"
            ;;
        esac
    ;;
    universal*|*-gcc|generic-gnu)
        link_with_cc=gcc
        enable_feature gcc
    setup_gnu_toolchain
    ;;
    esac

    # Try to enable CPU specific tuning
    if [ -n "${tune_cpu}" ]; then
        if [ -n "${tune_cflags}" ]; then
            check_add_cflags ${tune_cflags}${tune_cpu} || \
                die "Requested CPU '${tune_cpu}' not supported by compiler"
        fi
    if [ -n "${tune_asflags}" ]; then
            check_add_asflags ${tune_asflags}${tune_cpu} || \
                die "Requested CPU '${tune_cpu}' not supported by assembler"
        fi
    if [ -z "${tune_cflags}${tune_asflags}" ]; then
            log_echo "Warning: CPU tuning not supported by this toolchain"
        fi
    fi

    enabled debug && check_add_cflags -g && check_add_ldflags -g
    enabled gprof && check_add_cflags -pg && check_add_ldflags -pg
    enabled gcov &&
        check_add_cflags -fprofile-arcs -ftest-coverage &&
        check_add_ldflags -fprofile-arcs -ftest-coverage

    if enabled optimizations; then
        if enabled rvct; then
            enabled small && check_add_cflags -Ospace || check_add_cflags -Otime
        else
            enabled small && check_add_cflags -O2 ||  check_add_cflags -O3
        fi
    fi

    # default use_x86inc to yes if pic is no or 64bit or we are not on darwin
    if [ ${tgt_isa} = x86_64 -o ! "$pic" = "yes" -o \
         "${tgt_os#darwin}" = "${tgt_os}"  ]; then
      soft_enable use_x86inc
    fi

    # Position Independent Code (PIC) support, for building relocatable
    # shared objects
    enabled gcc && enabled pic && check_add_cflags -fPIC

    # Work around longjmp interception on glibc >= 2.11, to improve binary
    # compatibility. See http://code.google.com/p/webm/issues/detail?id=166
    enabled linux && check_add_cflags -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

    # Check for strip utility variant
    ${STRIP} -V 2>/dev/null | grep GNU >/dev/null && enable_feature gnu_strip

    # Try to determine target endianness
    check_cc <<EOF
    unsigned int e = 'O'<<24 | '2'<<16 | 'B'<<8 | 'E';
EOF
    [ -f "${TMP_O}" ] && od -A n -t x1 "${TMP_O}" | tr -d '\n' |
        grep '4f *32 *42 *45' >/dev/null 2>&1 && enable_feature big_endian

    # Try to find which inline keywords are supported
    check_cc <<EOF && INLINE="inline"
    static inline function() {}
EOF
    check_cc <<EOF && INLINE="__inline__ __attribute__((always_inline))"
    static __attribute__((always_inline)) function() {}
EOF

    # Almost every platform uses pthreads.
    if enabled multithread; then
        case ${toolchain} in
            *-win*-vs*);;
            *-android-gcc);;
            *) check_header pthread.h && add_extralibs -lpthread
        esac
    fi

    # only for MIPS platforms
    case ${toolchain} in
        mips*)
            if enabled dspr2; then
                if enabled big_endian; then
                    echo "dspr2 optimizations are available only for little endian platforms"
                    disable_feature dspr2
                fi
            fi
        ;;
    esac

    # glibc needs these
    if enabled linux; then
        add_cflags -D_LARGEFILE_SOURCE
        add_cflags -D_FILE_OFFSET_BITS=64
    fi

    # append any user defined extra cflags
    if [ -n "${extra_cflags}" ] ; then
        check_add_cflags ${extra_cflags} || \
        die "Requested extra CFLAGS '${extra_cflags}' not supported by compiler"
    fi
}

process_toolchain() {
    process_common_toolchain
}

print_config_mk() {
    local prefix=$1
    local makefile=$2
    shift 2
    for cfg; do
        upname="`toupper $cfg`"
        if enabled $cfg; then
            echo "${prefix}_${upname}=yes" >> $makefile
        fi
    done
}

print_config_h() {
    local prefix=$1
    local header=$2
    shift 2
    for cfg; do
        upname="`toupper $cfg`"
        if enabled $cfg; then
            echo "#define ${prefix}_${upname} 1" >> $header
        else
            echo "#define ${prefix}_${upname} 0" >> $header
        fi
    done
}

print_webm_license() {
    local destination=$1
    local prefix="$2"
    local suffix="$3"
    shift 3
    cat <<EOF > ${destination}
${prefix} Copyright (c) 2011 The WebM project authors. All Rights Reserved.${suffix}
${prefix} ${suffix}
${prefix} Use of this source code is governed by a BSD-style license${suffix}
${prefix} that can be found in the LICENSE file in the root of the source${suffix}
${prefix} tree. An additional intellectual property rights grant can be found${suffix}
${prefix} in the file PATENTS.  All contributing project authors may${suffix}
${prefix} be found in the AUTHORS file in the root of the source tree.${suffix}
EOF
}

process_targets() {
    true;
}

process_detect() {
    true;
}

enable_feature logging
logfile="config.log"
self=$0
process() {
    cmdline_args="$@"
    process_cmdline "$@"
    if enabled child; then
        echo "# ${self} $@" >> ${logfile}
    else
        echo "# ${self} $@" > ${logfile}
    fi
    post_process_common_cmdline
    post_process_cmdline
    process_toolchain
    process_detect
    process_targets

    OOT_INSTALLS="${OOT_INSTALLS}"
    if enabled source_path_used; then
    # Prepare the PWD for building.
    for f in ${OOT_INSTALLS}; do
            install -D ${source_path}/$f $f
    done
    fi
    cp ${source_path}/build/make/Makefile .

    clean_temp_files
    true
}
