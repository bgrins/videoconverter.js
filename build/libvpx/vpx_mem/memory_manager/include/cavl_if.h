/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_MEM_MEMORY_MANAGER_INCLUDE_CAVL_IF_H_
#define VPX_MEM_MEMORY_MANAGER_INCLUDE_CAVL_IF_H_

/* Abstract AVL Tree Generic C Package.
** Interface generation header file.
**
** This code is in the public domain.  See cavl_tree.html for interface
** documentation.
**
** Version: 1.5  Author: Walt Karas
*/

/* This header contains the definition of CHAR_BIT (number of bits in a
** char). */
#include <limits.h>

#undef L_
#undef L_EST_LONG_BIT
#undef L_SIZE
#undef L_SC
#undef L_LONG_BIT
#undef L_BIT_ARR_DEFN

#ifndef AVL_SEARCH_TYPE_DEFINED_
#define AVL_SEARCH_TYPE_DEFINED_

typedef enum {
  AVL_EQUAL = 1,
  AVL_LESS = 2,
  AVL_GREATER = 4,
  AVL_LESS_EQUAL = AVL_EQUAL | AVL_LESS,
  AVL_GREATER_EQUAL = AVL_EQUAL | AVL_GREATER
}
avl_search_type;

#endif

#ifdef AVL_UNIQUE

#define L_ AVL_UNIQUE

#else

#define L_(X) X

#endif

/* Determine storage class for function prototypes. */
#ifdef AVL_PRIVATE

#define L_SC static

#else

#define L_SC extern

#endif

#ifdef AVL_SIZE

#define L_SIZE AVL_SIZE

#else

#define L_SIZE unsigned long

#endif

typedef struct {
#ifdef AVL_INSIDE_STRUCT

  AVL_INSIDE_STRUCT

#endif

  AVL_HANDLE root;
}
L_(avl);

/* Function prototypes. */

L_SC void L_(init)(L_(avl) *tree);

L_SC int L_(is_empty)(L_(avl) *tree);

L_SC AVL_HANDLE L_(insert)(L_(avl) *tree, AVL_HANDLE h);

L_SC AVL_HANDLE L_(search)(L_(avl) *tree, AVL_KEY k, avl_search_type st);

L_SC AVL_HANDLE L_(search_least)(L_(avl) *tree);

L_SC AVL_HANDLE L_(search_greatest)(L_(avl) *tree);

L_SC AVL_HANDLE L_(remove)(L_(avl) *tree, AVL_KEY k);

L_SC AVL_HANDLE L_(subst)(L_(avl) *tree, AVL_HANDLE new_node);

#ifdef AVL_BUILD_ITER_TYPE

L_SC int L_(build)(
  L_(avl) *tree, AVL_BUILD_ITER_TYPE p, L_SIZE num_nodes);

#endif

/* ANSI C/ISO C++ require that a long have at least 32 bits.  Set
** L_EST_LONG_BIT to be the greatest multiple of 8 in the range
** 32 - 64 (inclusive) that is less than or equal to the number of
** bits in a long.
*/

#if (((LONG_MAX >> 31) >> 7) == 0)

#define L_EST_LONG_BIT 32

#elif (((LONG_MAX >> 31) >> 15) == 0)

#define L_EST_LONG_BIT 40

#elif (((LONG_MAX >> 31) >> 23) == 0)

#define L_EST_LONG_BIT 48

#elif (((LONG_MAX >> 31) >> 31) == 0)

#define L_EST_LONG_BIT 56

#else

#define L_EST_LONG_BIT 64

#endif

/* Number of bits in a long. */
#define L_LONG_BIT (sizeof(long) * CHAR_BIT)

/* The macro L_BIT_ARR_DEFN defines a bit array whose index is a (0-based)
** node depth.  The definition depends on whether the maximum depth is more
** or less than the number of bits in a single long.
*/

#if ((AVL_MAX_DEPTH) > L_EST_LONG_BIT)

/* Maximum depth may be more than number of bits in a long. */

#define L_BIT_ARR_DEFN(NAME) \
  unsigned long NAME[((AVL_MAX_DEPTH) + L_LONG_BIT - 1) / L_LONG_BIT];

#else

/* Maximum depth is definitely less than number of bits in a long. */

#define L_BIT_ARR_DEFN(NAME) unsigned long NAME;

#endif

/* Iterator structure. */
typedef struct {
  /* Tree being iterated over. */
  L_(avl) *tree_;

  /* Records a path into the tree.  If bit n is true, indicates
  ** take greater branch from the nth node in the path, otherwise
  ** take the less branch.  bit 0 gives branch from root, and
  ** so on. */
  L_BIT_ARR_DEFN(branch)

  /* Zero-based depth of path into tree. */
  unsigned depth;

  /* Handles of nodes in path from root to current node (returned by *). */
  AVL_HANDLE path_h[(AVL_MAX_DEPTH) - 1];
}
L_(iter);

/* Iterator function prototypes. */

L_SC void L_(start_iter)(
  L_(avl) *tree, L_(iter) *iter, AVL_KEY k, avl_search_type st);

L_SC void L_(start_iter_least)(L_(avl) *tree, L_(iter) *iter);

L_SC void L_(start_iter_greatest)(L_(avl) *tree, L_(iter) *iter);

L_SC AVL_HANDLE L_(get_iter)(L_(iter) *iter);

L_SC void L_(incr_iter)(L_(iter) *iter);

L_SC void L_(decr_iter)(L_(iter) *iter);

L_SC void L_(init_iter)(L_(iter) *iter);

#define AVL_IMPL_INIT           1
#define AVL_IMPL_IS_EMPTY       (1 << 1)
#define AVL_IMPL_INSERT         (1 << 2)
#define AVL_IMPL_SEARCH         (1 << 3)
#define AVL_IMPL_SEARCH_LEAST       (1 << 4)
#define AVL_IMPL_SEARCH_GREATEST    (1 << 5)
#define AVL_IMPL_REMOVE         (1 << 6)
#define AVL_IMPL_BUILD          (1 << 7)
#define AVL_IMPL_START_ITER     (1 << 8)
#define AVL_IMPL_START_ITER_LEAST   (1 << 9)
#define AVL_IMPL_START_ITER_GREATEST    (1 << 10)
#define AVL_IMPL_GET_ITER       (1 << 11)
#define AVL_IMPL_INCR_ITER      (1 << 12)
#define AVL_IMPL_DECR_ITER      (1 << 13)
#define AVL_IMPL_INIT_ITER      (1 << 14)
#define AVL_IMPL_SUBST          (1 << 15)

#define AVL_IMPL_ALL            (~0)

#undef L_
#undef L_EST_LONG_BIT
#undef L_SIZE
#undef L_SC
#undef L_LONG_BIT
#undef L_BIT_ARR_DEFN

#endif  // VPX_MEM_MEMORY_MANAGER_INCLUDE_CAVL_IF_H_
