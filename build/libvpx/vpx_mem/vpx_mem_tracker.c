/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
  vpx_mem_tracker.c

  jwz 2003-09-30:
   Stores a list of addreses, their size, and file and line they came from.
   All exposed lib functions are prefaced by vpx_ and allow the global list
   to be thread safe.
   Current supported platforms are:
    Linux, Win32, win_ce and vx_works
   Further support can be added by defining the platform specific mutex
   in the memory_tracker struct as well as calls to create/destroy/lock/unlock
   the mutex in vpx_memory_tracker_init/Destroy and memory_tracker_lock_mutex/unlock_mutex
*/
#include "./vpx_config.h"

#if defined(__uClinux__)
# include <lddk.h>
#endif

#if HAVE_PTHREAD_H
# include <pthread.h>
#elif defined(WIN32) || defined(_WIN32_WCE)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <winbase.h>
#elif defined(VXWORKS)
# include <sem_lib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // VXWORKS doesn't have a malloc/memory.h file,
// this should pull in malloc,free,etc.
#include <stdarg.h>

#include "include/vpx_mem_tracker.h"

#undef vpx_malloc   // undefine any vpx_mem macros that may affect calls to
#undef vpx_free     // memory functions in this file
#undef vpx_memcpy
#undef vpx_memset


#ifndef USE_GLOBAL_FUNCTION_POINTERS
# define USE_GLOBAL_FUNCTION_POINTERS   0  // use function pointers instead of compiled functions.
#endif

#if USE_GLOBAL_FUNCTION_POINTERS
static mem_track_malloc_func g_malloc   = malloc;
static mem_track_calloc_func g_calloc   = calloc;
static mem_track_realloc_func g_realloc = realloc;
static mem_track_free_func g_free       = free;
static mem_track_memcpy_func g_memcpy   = memcpy;
static mem_track_memset_func g_memset   = memset;
static mem_track_memmove_func g_memmove = memmove;
# define MEM_TRACK_MALLOC g_malloc
# define MEM_TRACK_FREE   g_free
# define MEM_TRACK_MEMCPY g_memcpy
# define MEM_TRACK_MEMSET g_memset
#else
# define MEM_TRACK_MALLOC vpx_malloc
# define MEM_TRACK_FREE   vpx_free
# define MEM_TRACK_MEMCPY vpx_memcpy
# define MEM_TRACK_MEMSET vpx_memset
#endif // USE_GLOBAL_FUNCTION_POINTERS

/* prototypes for internal library functions */
static void memtrack_log(const char *fmt, ...);
static void memory_tracker_dump();
static void memory_tracker_check_integrity(char *file, unsigned int line);
static void memory_tracker_add(size_t addr, unsigned int size,
                               char *file, unsigned int line,
                               int padded);
static int memory_tracker_remove(size_t addr);
static struct mem_block *memory_tracker_find(size_t addr);

#if defined(NO_MUTEX)
# define memory_tracker_lock_mutex() (!g_b_mem_tracker_inited)
# define memory_tracker_unlock_mutex()
#else
static int memory_tracker_lock_mutex();
static int memory_tracker_unlock_mutex();
#endif

#ifndef VPX_NO_GLOBALS
struct memory_tracker {
  struct mem_block *head,
      * tail;
  int len,
      totalsize;
  unsigned int current_allocated,
           max_allocated;

#if HAVE_PTHREAD_H
  pthread_mutex_t mutex;
#elif defined(WIN32) || defined(_WIN32_WCE)
  HANDLE mutex;
#elif defined(VXWORKS)
  SEM_ID mutex;
#elif defined(NO_MUTEX)
#else
#error "No mutex type defined for this platform!"
#endif

  int padding_size,
      pad_value;
};

static struct memory_tracker memtrack;   // our global memory allocation list
static int g_b_mem_tracker_inited = 0;     // indicates whether the global list has
// been initialized (1:yes/0:no)
static struct {
  FILE *file;
  int type;
  void (*func)(void *userdata, const char *fmt, va_list args);
  void *userdata;
} g_logging = {NULL, 0, NULL, NULL};
#else
# include "vpx_global_handling.h"
#define g_b_mem_tracker_inited vpxglobalm(vpxmem,g_b_mem_tracker_inited)
#define g_logging vpxglobalm(vpxmem,g_logging)
#define memtrack vpxglobalm(vpxmem,memtrack)
#endif // #ifndef VPX_NO_GLOBALS

extern void *vpx_malloc(size_t size);
extern void vpx_free(void *memblk);
extern void *vpx_memcpy(void *dest, const void *src, size_t length);
extern void *vpx_memset(void *dest, int val, size_t length);

/*
 *
 * Exposed library functions
 *
*/

/*
    vpx_memory_tracker_init(int padding_size, int pad_value)
      padding_size - the size of the padding before and after each mem addr.
                     Values > 0 indicate that integrity checks can be performed
                     by inspecting these areas.
      pad_value - the initial value within the padding area before and after
                  each mem addr.

    Initializes global memory tracker structure
    Allocates the head of the list
*/
int vpx_memory_tracker_init(int padding_size, int pad_value) {
  if (!g_b_mem_tracker_inited) {
    if ((memtrack.head = (struct mem_block *)
                         MEM_TRACK_MALLOC(sizeof(struct mem_block)))) {
      int ret;

      MEM_TRACK_MEMSET(memtrack.head, 0, sizeof(struct mem_block));

      memtrack.tail = memtrack.head;

      memtrack.current_allocated = 0;
      memtrack.max_allocated     = 0;

      memtrack.padding_size = padding_size;
      memtrack.pad_value    = pad_value;

#if HAVE_PTHREAD_H
      ret = pthread_mutex_init(&memtrack.mutex,
                               NULL);            /*mutex attributes (NULL=default)*/
#elif defined(WIN32) || defined(_WIN32_WCE)
      memtrack.mutex = CreateMutex(NULL,   /*security attributes*/
                                   FALSE,  /*we don't want initial ownership*/
                                   NULL);  /*mutex name*/
      ret = !memtrack.mutex;
#elif defined(VXWORKS)
      memtrack.mutex = sem_bcreate(SEM_Q_FIFO, /*SEM_Q_FIFO non-priority based mutex*/
                                   SEM_FULL);  /*SEM_FULL initial state is unlocked*/
      ret = !memtrack.mutex;
#elif defined(NO_MUTEX)
      ret = 0;
#endif

      if (ret) {
        memtrack_log("vpx_memory_tracker_init: Error creating mutex!\n");

        MEM_TRACK_FREE(memtrack.head);
        memtrack.head = NULL;
      } else {
        memtrack_log("Memory Tracker init'd, v."vpx_mem_tracker_version" pad_size:%d pad_val:0x%x %d\n"
, padding_size
, pad_value
, pad_value);
        g_b_mem_tracker_inited = 1;
      }
    }
  }

  return g_b_mem_tracker_inited;
}

/*
    vpx_memory_tracker_destroy()
    If our global struct was initialized zeros out all its members,
    frees memory and destroys it's mutex
*/
void vpx_memory_tracker_destroy() {
  if (!memory_tracker_lock_mutex()) {
    struct mem_block *p  = memtrack.head,
                          * p2 = memtrack.head;

    memory_tracker_dump();

    while (p) {
      p2 = p;
      p  = p->next;

      MEM_TRACK_FREE(p2);
    }

    memtrack.head              = NULL;
    memtrack.tail              = NULL;
    memtrack.len               = 0;
    memtrack.current_allocated = 0;
    memtrack.max_allocated     = 0;

    if (!g_logging.type && g_logging.file && g_logging.file != stderr) {
      fclose(g_logging.file);
      g_logging.file = NULL;
    }

    memory_tracker_unlock_mutex();

    g_b_mem_tracker_inited = 0;
  }
}

/*
    vpx_memory_tracker_add(size_t addr, unsigned int size,
                         char * file, unsigned int line)
      addr - memory address to be added to list
      size - size of addr
      file - the file addr was referenced from
      line - the line in file addr was referenced from
    Adds memory address addr, it's size, file and line it came from
    to the global list via the thread safe internal library function
*/
void vpx_memory_tracker_add(size_t addr, unsigned int size,
                            char *file, unsigned int line,
                            int padded) {
  memory_tracker_add(addr, size, file, line, padded);
}

/*
    vpx_memory_tracker_remove(size_t addr)
      addr - memory address to be removed from list
    Removes addr from the global list via the thread safe
    internal remove function
    Return:
      Same as described for memory_tracker_remove
*/
int vpx_memory_tracker_remove(size_t addr) {
  return memory_tracker_remove(addr);
}

/*
    vpx_memory_tracker_find(size_t addr)
      addr - address to be found in list
    Return:
        If found, pointer to the memory block that matches addr
        NULL otherwise
*/
struct mem_block *vpx_memory_tracker_find(size_t addr) {
  struct mem_block *p = NULL;

  if (!memory_tracker_lock_mutex()) {
    p = memory_tracker_find(addr);
    memory_tracker_unlock_mutex();
  }

  return p;
}

/*
    vpx_memory_tracker_dump()
    Locks the memory tracker's mutex and calls the internal
    library function to dump the current contents of the
    global memory allocation list
*/
void vpx_memory_tracker_dump() {
  if (!memory_tracker_lock_mutex()) {
    memory_tracker_dump();
    memory_tracker_unlock_mutex();
  }
}

/*
    vpx_memory_tracker_check_integrity(char* file, unsigned int line)
      file - The file name where the check was placed
      line - The line in file where the check was placed
    Locks the memory tracker's mutex and calls the internal
    integrity check function to inspect every address in the global
    memory allocation list
*/
void vpx_memory_tracker_check_integrity(char *file, unsigned int line) {
  if (!memory_tracker_lock_mutex()) {
    memory_tracker_check_integrity(file, line);
    memory_tracker_unlock_mutex();
  }
}

/*
    vpx_memory_tracker_set_log_type
    Sets the logging type for the memory tracker. Based on the value it will
    direct its output to the appropriate place.
    Return:
      0: on success
      -1: if the logging type could not be set, because the value was invalid
          or because a file could not be opened
*/
int vpx_memory_tracker_set_log_type(int type, char *option) {
  int ret = -1;

  switch (type) {
    case 0:
      g_logging.type = 0;

      if (!option) {
        g_logging.file = stderr;
        ret = 0;
      } else {
        if ((g_logging.file = fopen((char *)option, "w")))
          ret = 0;
      }

      break;
#if defined(WIN32) && !defined(_WIN32_WCE)
    case 1:
      g_logging.type = type;
      ret = 0;
      break;
#endif
    default:
      break;
  }

  // output the version to the new logging destination
  if (!ret)
    memtrack_log("Memory Tracker logging initialized, "
                 "Memory Tracker v."vpx_mem_tracker_version"\n");

  return ret;
}

/*
    vpx_memory_tracker_set_log_func
    Sets a logging function to be used by the memory tracker.
    Return:
      0: on success
      -1: if the logging type could not be set because logfunc was NULL
*/
int vpx_memory_tracker_set_log_func(void *userdata,
                                    void(*logfunc)(void *userdata,
                                                   const char *fmt, va_list args)) {
  int ret = -1;

  if (logfunc) {
    g_logging.type     = -1;
    g_logging.userdata = userdata;
    g_logging.func     = logfunc;
    ret = 0;
  }

  // output the version to the new logging destination
  if (!ret)
    memtrack_log("Memory Tracker logging initialized, "
                 "Memory Tracker v."vpx_mem_tracker_version"\n");

  return ret;
}

/*
 *
 * END - Exposed library functions
 *
*/


/*
 *
 * Internal library functions
 *
*/

static void memtrack_log(const char *fmt, ...) {
  va_list list;

  va_start(list, fmt);

  switch (g_logging.type) {
    case -1:

      if (g_logging.func)
        g_logging.func(g_logging.userdata, fmt, list);

      break;
    case 0:

      if (g_logging.file) {
        vfprintf(g_logging.file, fmt, list);
        fflush(g_logging.file);
      }

      break;
#if defined(WIN32) && !defined(_WIN32_WCE)
    case 1: {
      char temp[1024];
      _vsnprintf(temp, sizeof(temp) / sizeof(char) - 1, fmt, list);
      OutputDebugString(temp);
    }
    break;
#endif
    default:
      break;
  }

  va_end(list);
}

/*
    memory_tracker_dump()
    Dumps the current contents of the global memory allocation list
*/
static void memory_tracker_dump() {
  int i = 0;
  struct mem_block *p = (memtrack.head ? memtrack.head->next : NULL);

  memtrack_log("\n_currently Allocated= %d; Max allocated= %d\n",
               memtrack.current_allocated, memtrack.max_allocated);

  while (p) {
#if defined(WIN32) && !defined(_WIN32_WCE)

    /*when using outputdebugstring, output filenames so they
      can be clicked to be opened in visual studio*/
    if (g_logging.type == 1)
      memtrack_log("memblocks[%d].addr= 0x%.8x, memblocks[%d].size= %d, file:\n"
                   "  %s(%d):\n", i,
                   p->addr, i, p->size,
                   p->file, p->line);
    else
#endif
      memtrack_log("memblocks[%d].addr= 0x%.8x, memblocks[%d].size= %d, file: %s, line: %d\n", i,
                   p->addr, i, p->size,
                   p->file, p->line);

    p = p->next;
    ++i;
  }

  memtrack_log("\n");
}

/*
    memory_tracker_check_integrity(char* file, unsigned int file)
      file - the file name where the check was placed
      line - the line in file where the check was placed
    If a padding_size was supplied to vpx_memory_tracker_init()
    this function will check ea. addr in the list verifying that
    addr-padding_size and addr+padding_size is filled with pad_value
*/
static void memory_tracker_check_integrity(char *file, unsigned int line) {
  if (memtrack.padding_size) {
    int i,
        index = 0;
    unsigned char *p_show_me,
             * p_show_me2;
    unsigned int tempme = memtrack.pad_value,
                 dead1,
                 dead2;
    unsigned char *x_bounds;
    struct mem_block *p = memtrack.head->next;

    while (p) {
      // x_bounds = (unsigned char*)p->addr;
      // back up VPX_BYTE_ALIGNMENT
      // x_bounds -= memtrack.padding_size;

      if (p->padded) { // can the bounds be checked?
        /*yes, move to the address that was actually allocated
        by the vpx_* calls*/
        x_bounds = (unsigned char *)(((size_t *)p->addr)[-1]);

        for (i = 0; i < memtrack.padding_size; i += sizeof(unsigned int)) {
          p_show_me = (x_bounds + i);
          p_show_me2 = (unsigned char *)(p->addr + p->size + i);

          MEM_TRACK_MEMCPY(&dead1, p_show_me, sizeof(unsigned int));
          MEM_TRACK_MEMCPY(&dead2, p_show_me2, sizeof(unsigned int));

          if ((dead1 != tempme) || (dead2 != tempme)) {
            memtrack_log("\n[vpx_mem integrity check failed]:\n"
                         "    index[%d,%d] {%s:%d} addr=0x%x, size=%d,"
                         " file: %s, line: %d c0:0x%x c1:0x%x\n",
                         index, i, file, line, p->addr, p->size, p->file,
                         p->line, dead1, dead2);
          }
        }
      }

      ++index;
      p = p->next;
    }
  }
}

/*
    memory_tracker_add(size_t addr, unsigned int size,
                     char * file, unsigned int line)
    Adds an address (addr), it's size, file and line number to our list.
    Adjusts the total bytes allocated and max bytes allocated if necessary.
    If memory cannot be allocated the list will be destroyed.
*/
void memory_tracker_add(size_t addr, unsigned int size,
                        char *file, unsigned int line,
                        int padded) {
  if (!memory_tracker_lock_mutex()) {
    struct mem_block *p;

    p = MEM_TRACK_MALLOC(sizeof(struct mem_block));

    if (p) {
      p->prev       = memtrack.tail;
      p->prev->next = p;
      p->addr       = addr;
      p->size       = size;
      p->line       = line;
      p->file       = file;
      p->padded     = padded;
      p->next       = NULL;

      memtrack.tail = p;

      memtrack.current_allocated += size;

      if (memtrack.current_allocated > memtrack.max_allocated)
        memtrack.max_allocated = memtrack.current_allocated;

      // memtrack_log("memory_tracker_add: added addr=0x%.8x\n", addr);

      memory_tracker_unlock_mutex();
    } else {
      memtrack_log("memory_tracker_add: error allocating memory!\n");
      memory_tracker_unlock_mutex();
      vpx_memory_tracker_destroy();
    }
  }
}

/*
    memory_tracker_remove(size_t addr)
    Removes an address and its corresponding size (if they exist)
    from the memory tracker list and adjusts the current number
    of bytes allocated.
    Return:
      0: on success
      -1: if the mutex could not be locked
      -2: if the addr was not found in the list
*/
int memory_tracker_remove(size_t addr) {
  int ret = -1;

  if (!memory_tracker_lock_mutex()) {
    struct mem_block *p;

    if ((p = memory_tracker_find(addr))) {
      memtrack.current_allocated -= p->size;

      p->prev->next = p->next;

      if (p->next)
        p->next->prev = p->prev;
      else
        memtrack.tail = p->prev;

      ret = 0;
      MEM_TRACK_FREE(p);
    } else {
      if (addr)
        memtrack_log("memory_tracker_remove(): addr not found in list,"
                     " 0x%.8x\n", addr);

      ret = -2;
    }

    memory_tracker_unlock_mutex();
  }

  return ret;
}

/*
    memory_tracker_find(size_t addr)
    Finds an address in our addrs list
    NOTE: the mutex MUST be locked in the other internal
          functions before calling this one. This avoids
          the need for repeated locking and unlocking as in Remove
    Returns: pointer to the mem block if found, NULL otherwise
*/
static struct mem_block *memory_tracker_find(size_t addr) {
  struct mem_block *p = NULL;

  if (memtrack.head) {
    p = memtrack.head->next;

    while (p && (p->addr != addr))
      p = p->next;
  }

  return p;
}


#if !defined(NO_MUTEX)
/*
    memory_tracker_lock_mutex()
    Locks the memory tracker mutex with a platform specific call
    Returns:
        0: Success
       <0: Failure, either the mutex was not initialized
           or the call to lock the mutex failed
*/
static int memory_tracker_lock_mutex() {
  int ret = -1;

  if (g_b_mem_tracker_inited) {

#if HAVE_PTHREAD_H
    ret = pthread_mutex_lock(&memtrack.mutex);
#elif defined(WIN32) || defined(_WIN32_WCE)
    ret = WaitForSingleObject(memtrack.mutex, INFINITE);
#elif defined(VXWORKS)
    ret = sem_take(memtrack.mutex, WAIT_FOREVER);
#endif

    if (ret) {
      memtrack_log("memory_tracker_lock_mutex: mutex lock failed\n");
    }
  }

  return ret;
}

/*
    memory_tracker_unlock_mutex()
    Unlocks the memory tracker mutex with a platform specific call
    Returns:
        0: Success
       <0: Failure, either the mutex was not initialized
           or the call to unlock the mutex failed
*/
static int memory_tracker_unlock_mutex() {
  int ret = -1;

  if (g_b_mem_tracker_inited) {

#if HAVE_PTHREAD_H
    ret = pthread_mutex_unlock(&memtrack.mutex);
#elif defined(WIN32) || defined(_WIN32_WCE)
    ret = !ReleaseMutex(memtrack.mutex);
#elif defined(VXWORKS)
    ret = sem_give(memtrack.mutex);
#endif

    if (ret) {
      memtrack_log("memory_tracker_unlock_mutex: mutex unlock failed\n");
    }
  }

  return ret;
}
#endif

/*
    vpx_memory_tracker_set_functions

    Sets the function pointers for the standard library functions.

    Return:
      0: on success
      -1: if the use global function pointers is not set.
*/
int vpx_memory_tracker_set_functions(mem_track_malloc_func g_malloc_l
, mem_track_calloc_func g_calloc_l
, mem_track_realloc_func g_realloc_l
, mem_track_free_func g_free_l
, mem_track_memcpy_func g_memcpy_l
, mem_track_memset_func g_memset_l
, mem_track_memmove_func g_memmove_l) {
#if USE_GLOBAL_FUNCTION_POINTERS

  if (g_malloc_l)
    g_malloc = g_malloc_l;

  if (g_calloc_l)
    g_calloc = g_calloc_l;

  if (g_realloc_l)
    g_realloc = g_realloc_l;

  if (g_free_l)
    g_free = g_free_l;

  if (g_memcpy_l)
    g_memcpy = g_memcpy_l;

  if (g_memset_l)
    g_memset = g_memset_l;

  if (g_memmove_l)
    g_memmove = g_memmove_l;

  return 0;
#else
  (void)g_malloc_l;
  (void)g_calloc_l;
  (void)g_realloc_l;
  (void)g_free_l;
  (void)g_memcpy_l;
  (void)g_memset_l;
  (void)g_memmove_l;
  return -1;
#endif
}
