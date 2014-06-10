halloc 1.2.1
============
      
	Hierarchical memory heap interface - an extension to standard
	malloc/free interface that simplifies tasks of memory disposal 
	when allocated structures exhibit hierarchical properties.

	http://swapped.cc/halloc
=
	To build libhalloc.a with GNU tools run
		make

	To install in /usr/include and /usr/lib
		make install

	To cleanup the build files 
		make clean
=
	halloc-1.2.1
		* fixed a double-free bug in _set_allocator() as per
		  Matthew Gregan comments

		* switched to using NULL instead of 0 where applicable

	halloc-1.2.0
		* added missing <string.h> include to halloc.c
		
		* improved standard compliance thanks to the feedback
		  received from Stan Tobias. Two things were fixed -
		  
		- hblock_t structure no longer uses zero-sized 'data'
		  array, which happened to be common, but non-standard
		  extension; 
		  
		- secondly, added the code to test the behaviour of 
		  realloc(ptr, 0). Standard allows it NOT to act as
		  free(), in which case halloc will use its own version
		  of allocator calling free() when neccessary.

	halloc-1.1.0
		* initial public release (rewrite of hhmalloc library)

=============================================================================
Copyright (c) 2004-2010, Alex Pankratov (ap@swapped.cc). All rights reserved.

