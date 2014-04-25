/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_MEM_MEMORY_MANAGER_INCLUDE_CAVL_IMPL_H_
#define VPX_MEM_MEMORY_MANAGER_INCLUDE_CAVL_IMPL_H_

/* Abstract AVL Tree Generic C Package.
** Implementation generation header file.
**
** This code is in the public domain.  See cavl_tree.html for interface
** documentation.
**
** Version: 1.5  Author: Walt Karas
*/

#undef L_
#undef L_EST_LONG_BIT
#undef L_SIZE
#undef l_tree
#undef L_MASK_HIGH_BIT
#undef L_LONG_BIT
#undef L_BIT_ARR_DEFN
#undef L_BIT_ARR_VAL
#undef L_BIT_ARR_0
#undef L_BIT_ARR_1
#undef L_BIT_ARR_ALL
#undef L_BIT_ARR_LONGS
#undef L_IMPL_MASK
#undef L_CHECK_READ_ERROR
#undef L_CHECK_READ_ERROR_INV_DEPTH
#undef L_SC
#undef L_BALANCE_PARAM_PREFIX

#ifdef AVL_UNIQUE

#define L_ AVL_UNIQUE

#else

#define L_(X) X

#endif

/* Determine correct storage class for functions */
#ifdef AVL_PRIVATE

#define L_SC static

#else

#define L_SC

#endif

#ifdef AVL_SIZE

#define L_SIZE AVL_SIZE

#else

#define L_SIZE unsigned long

#endif

#define L_MASK_HIGH_BIT ((int) ~ ((~ (unsigned) 0) >> 1))

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

#define L_LONG_BIT (sizeof(long) * CHAR_BIT)

#if ((AVL_MAX_DEPTH) > L_EST_LONG_BIT)

/* The maximum depth may be greater than the number of bits in a long,
** so multiple longs are needed to hold a bit array indexed by node
** depth. */

#define L_BIT_ARR_LONGS (((AVL_MAX_DEPTH) + L_LONG_BIT - 1) / L_LONG_BIT)

#define L_BIT_ARR_DEFN(NAME) unsigned long NAME[L_BIT_ARR_LONGS];

#define L_BIT_ARR_VAL(BIT_ARR, BIT_NUM) \
  ((BIT_ARR)[(BIT_NUM) / L_LONG_BIT] & (1L << ((BIT_NUM) % L_LONG_BIT)))

#define L_BIT_ARR_0(BIT_ARR, BIT_NUM) \
  (BIT_ARR)[(BIT_NUM) / L_LONG_BIT] &= ~(1L << ((BIT_NUM) % L_LONG_BIT));

#define L_BIT_ARR_1(BIT_ARR, BIT_NUM) \
  (BIT_ARR)[(BIT_NUM) / L_LONG_BIT] |= 1L << ((BIT_NUM) % L_LONG_BIT);

#define L_BIT_ARR_ALL(BIT_ARR, BIT_VAL) \
  { int i = L_BIT_ARR_LONGS; do (BIT_ARR)[--i] = 0L - (BIT_VAL); while(i); }

#else /* The bit array can definitely fit in one long */

#define L_BIT_ARR_DEFN(NAME) unsigned long NAME;

#define L_BIT_ARR_VAL(BIT_ARR, BIT_NUM) ((BIT_ARR) & (1L << (BIT_NUM)))

#define L_BIT_ARR_0(BIT_ARR, BIT_NUM) (BIT_ARR) &= ~(1L << (BIT_NUM));

#define L_BIT_ARR_1(BIT_ARR, BIT_NUM) (BIT_ARR) |= 1L << (BIT_NUM);

#define L_BIT_ARR_ALL(BIT_ARR, BIT_VAL) (BIT_ARR) = 0L - (BIT_VAL);

#endif

#ifdef AVL_READ_ERRORS_HAPPEN

#define L_CHECK_READ_ERROR(ERROR_RETURN) \
  { if (AVL_READ_ERROR) return(ERROR_RETURN); }

#else

#define L_CHECK_READ_ERROR(ERROR_RETURN)

#endif

/* The presumed reason that an instantiation places additional fields
** inside the AVL tree structure is that the SET_ and GET_ macros
** need these fields.  The "balance" function does not explicitly use
** any fields in the AVL tree structure, so only pass an AVL tree
** structure pointer to "balance" if it has instantiation-specific
** fields that are (presumably) needed by the SET_/GET_ calls within
** "balance".
*/
#ifdef AVL_INSIDE_STRUCT

#define L_BALANCE_PARAM_CALL_PREFIX l_tree,
#define L_BALANCE_PARAM_DECL_PREFIX L_(avl) *l_tree,

#else

#define L_BALANCE_PARAM_CALL_PREFIX
#define L_BALANCE_PARAM_DECL_PREFIX

#endif

#ifdef AVL_IMPL_MASK

#define L_IMPL_MASK (AVL_IMPL_MASK)

#else

/* Define all functions. */
#define L_IMPL_MASK AVL_IMPL_ALL

#endif

#if (L_IMPL_MASK & AVL_IMPL_INIT)

L_SC void L_(init)(L_(avl) *l_tree) {
  l_tree->root = AVL_NULL;
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_IS_EMPTY)

L_SC int L_(is_empty)(L_(avl) *l_tree) {
  return(l_tree->root == AVL_NULL);
}

#endif

/* Put the private balance function in the same compilation module as
** the insert function.  */
#if (L_IMPL_MASK & AVL_IMPL_INSERT)

/* Balances subtree, returns handle of root node of subtree after balancing.
*/
L_SC AVL_HANDLE L_(balance)(L_BALANCE_PARAM_DECL_PREFIX AVL_HANDLE bal_h) {
  AVL_HANDLE deep_h;

  /* Either the "greater than" or the "less than" subtree of
  ** this node has to be 2 levels deeper (or else it wouldn't
  ** need balancing).
  */
  if (AVL_GET_BALANCE_FACTOR(bal_h) > 0) {
    /* "Greater than" subtree is deeper. */

    deep_h = AVL_GET_GREATER(bal_h, 1);

    L_CHECK_READ_ERROR(AVL_NULL)

    if (AVL_GET_BALANCE_FACTOR(deep_h) < 0) {
      int bf;

      AVL_HANDLE old_h = bal_h;
      bal_h = AVL_GET_LESS(deep_h, 1);
      L_CHECK_READ_ERROR(AVL_NULL)
      AVL_SET_GREATER(old_h, AVL_GET_LESS(bal_h, 1))
      AVL_SET_LESS(deep_h, AVL_GET_GREATER(bal_h, 1))
      AVL_SET_LESS(bal_h, old_h)
      AVL_SET_GREATER(bal_h, deep_h)

      bf = AVL_GET_BALANCE_FACTOR(bal_h);

      if (bf != 0) {
        if (bf > 0) {
          AVL_SET_BALANCE_FACTOR(old_h, -1)
          AVL_SET_BALANCE_FACTOR(deep_h, 0)
        } else {
          AVL_SET_BALANCE_FACTOR(deep_h, 1)
          AVL_SET_BALANCE_FACTOR(old_h, 0)
        }

        AVL_SET_BALANCE_FACTOR(bal_h, 0)
      } else {
        AVL_SET_BALANCE_FACTOR(old_h, 0)
        AVL_SET_BALANCE_FACTOR(deep_h, 0)
      }
    } else {
      AVL_SET_GREATER(bal_h, AVL_GET_LESS(deep_h, 0))
      AVL_SET_LESS(deep_h, bal_h)

      if (AVL_GET_BALANCE_FACTOR(deep_h) == 0) {
        AVL_SET_BALANCE_FACTOR(deep_h, -1)
        AVL_SET_BALANCE_FACTOR(bal_h, 1)
      } else {
        AVL_SET_BALANCE_FACTOR(deep_h, 0)
        AVL_SET_BALANCE_FACTOR(bal_h, 0)
      }

      bal_h = deep_h;
    }
  } else {
    /* "Less than" subtree is deeper. */

    deep_h = AVL_GET_LESS(bal_h, 1);
    L_CHECK_READ_ERROR(AVL_NULL)

    if (AVL_GET_BALANCE_FACTOR(deep_h) > 0) {
      int bf;
      AVL_HANDLE old_h = bal_h;
      bal_h = AVL_GET_GREATER(deep_h, 1);
      L_CHECK_READ_ERROR(AVL_NULL)
      AVL_SET_LESS(old_h, AVL_GET_GREATER(bal_h, 0))
      AVL_SET_GREATER(deep_h, AVL_GET_LESS(bal_h, 0))
      AVL_SET_GREATER(bal_h, old_h)
      AVL_SET_LESS(bal_h, deep_h)

      bf = AVL_GET_BALANCE_FACTOR(bal_h);

      if (bf != 0) {
        if (bf < 0) {
          AVL_SET_BALANCE_FACTOR(old_h, 1)
          AVL_SET_BALANCE_FACTOR(deep_h, 0)
        } else {
          AVL_SET_BALANCE_FACTOR(deep_h, -1)
          AVL_SET_BALANCE_FACTOR(old_h, 0)
        }

        AVL_SET_BALANCE_FACTOR(bal_h, 0)
      } else {
        AVL_SET_BALANCE_FACTOR(old_h, 0)
        AVL_SET_BALANCE_FACTOR(deep_h, 0)
      }
    } else {
      AVL_SET_LESS(bal_h, AVL_GET_GREATER(deep_h, 0))
      AVL_SET_GREATER(deep_h, bal_h)

      if (AVL_GET_BALANCE_FACTOR(deep_h) == 0) {
        AVL_SET_BALANCE_FACTOR(deep_h, 1)
        AVL_SET_BALANCE_FACTOR(bal_h, -1)
      } else {
        AVL_SET_BALANCE_FACTOR(deep_h, 0)
        AVL_SET_BALANCE_FACTOR(bal_h, 0)
      }

      bal_h = deep_h;
    }
  }

  return(bal_h);
}

L_SC AVL_HANDLE L_(insert)(L_(avl) *l_tree, AVL_HANDLE h) {
  AVL_SET_LESS(h, AVL_NULL)
  AVL_SET_GREATER(h, AVL_NULL)
  AVL_SET_BALANCE_FACTOR(h, 0)

  if (l_tree->root == AVL_NULL)
    l_tree->root = h;
  else {
    /* Last unbalanced node encountered in search for insertion point. */
    AVL_HANDLE unbal = AVL_NULL;
    /* Parent of last unbalanced node. */
    AVL_HANDLE parent_unbal = AVL_NULL;
    /* Balance factor of last unbalanced node. */
    int unbal_bf;

    /* Zero-based depth in tree. */
    unsigned depth = 0, unbal_depth = 0;

    /* Records a path into the tree.  If bit n is true, indicates
    ** take greater branch from the nth node in the path, otherwise
    ** take the less branch.  bit 0 gives branch from root, and
    ** so on. */
    L_BIT_ARR_DEFN(branch)

    AVL_HANDLE hh = l_tree->root;
    AVL_HANDLE parent = AVL_NULL;
    int cmp;

    do {
      if (AVL_GET_BALANCE_FACTOR(hh) != 0) {
        unbal = hh;
        parent_unbal = parent;
        unbal_depth = depth;
      }

      cmp = AVL_COMPARE_NODE_NODE(h, hh);

      if (cmp == 0)
        /* Duplicate key. */
        return(hh);

      parent = hh;

      if (cmp > 0) {
        hh = AVL_GET_GREATER(hh, 1);
        L_BIT_ARR_1(branch, depth)
      } else {
        hh = AVL_GET_LESS(hh, 1);
        L_BIT_ARR_0(branch, depth)
      }

      L_CHECK_READ_ERROR(AVL_NULL)
      depth++;
    } while (hh != AVL_NULL);

    /*  Add node to insert as leaf of tree. */
    if (cmp < 0)
      AVL_SET_LESS(parent, h)
      else
        AVL_SET_GREATER(parent, h)

        depth = unbal_depth;

    if (unbal == AVL_NULL)
      hh = l_tree->root;
    else {
      cmp = L_BIT_ARR_VAL(branch, depth) ? 1 : -1;
      depth++;
      unbal_bf = AVL_GET_BALANCE_FACTOR(unbal);

      if (cmp < 0)
        unbal_bf--;
      else  /* cmp > 0 */
        unbal_bf++;

      hh = cmp < 0 ? AVL_GET_LESS(unbal, 1) : AVL_GET_GREATER(unbal, 1);
      L_CHECK_READ_ERROR(AVL_NULL)

      if ((unbal_bf != -2) && (unbal_bf != 2)) {
        /* No rebalancing of tree is necessary. */
        AVL_SET_BALANCE_FACTOR(unbal, unbal_bf)
        unbal = AVL_NULL;
      }
    }

    if (hh != AVL_NULL)
      while (h != hh) {
        cmp = L_BIT_ARR_VAL(branch, depth) ? 1 : -1;
        depth++;

        if (cmp < 0) {
          AVL_SET_BALANCE_FACTOR(hh, -1)
          hh = AVL_GET_LESS(hh, 1);
        } else { /* cmp > 0 */
          AVL_SET_BALANCE_FACTOR(hh, 1)
          hh = AVL_GET_GREATER(hh, 1);
        }

        L_CHECK_READ_ERROR(AVL_NULL)
      }

    if (unbal != AVL_NULL) {
      unbal = L_(balance)(L_BALANCE_PARAM_CALL_PREFIX unbal);
      L_CHECK_READ_ERROR(AVL_NULL)

      if (parent_unbal == AVL_NULL)
        l_tree->root = unbal;
      else {
        depth = unbal_depth - 1;
        cmp = L_BIT_ARR_VAL(branch, depth) ? 1 : -1;

        if (cmp < 0)
          AVL_SET_LESS(parent_unbal, unbal)
          else  /* cmp > 0 */
            AVL_SET_GREATER(parent_unbal, unbal)
          }
    }

  }

  return(h);
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_SEARCH)

L_SC AVL_HANDLE L_(search)(L_(avl) *l_tree, AVL_KEY k, avl_search_type st) {
  int cmp, target_cmp;
  AVL_HANDLE match_h = AVL_NULL;
  AVL_HANDLE h = l_tree->root;

  if (st & AVL_LESS)
    target_cmp = 1;
  else if (st & AVL_GREATER)
    target_cmp = -1;
  else
    target_cmp = 0;

  while (h != AVL_NULL) {
    cmp = AVL_COMPARE_KEY_NODE(k, h);

    if (cmp == 0) {
      if (st & AVL_EQUAL) {
        match_h = h;
        break;
      }

      cmp = -target_cmp;
    } else if (target_cmp != 0)
      if (!((cmp ^ target_cmp) & L_MASK_HIGH_BIT))
        /* cmp and target_cmp are both positive or both negative. */
        match_h = h;

    h = cmp < 0 ? AVL_GET_LESS(h, 1) : AVL_GET_GREATER(h, 1);
    L_CHECK_READ_ERROR(AVL_NULL)
  }

  return(match_h);
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_SEARCH_LEAST)

L_SC AVL_HANDLE L_(search_least)(L_(avl) *l_tree) {
  AVL_HANDLE h = l_tree->root;
  AVL_HANDLE parent = AVL_NULL;

  while (h != AVL_NULL) {
    parent = h;
    h = AVL_GET_LESS(h, 1);
    L_CHECK_READ_ERROR(AVL_NULL)
  }

  return(parent);
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_SEARCH_GREATEST)

L_SC AVL_HANDLE L_(search_greatest)(L_(avl) *l_tree) {
  AVL_HANDLE h = l_tree->root;
  AVL_HANDLE parent = AVL_NULL;

  while (h != AVL_NULL) {
    parent = h;
    h = AVL_GET_GREATER(h, 1);
    L_CHECK_READ_ERROR(AVL_NULL)
  }

  return(parent);
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_REMOVE)

/* Prototype of balance function (called by remove) in case not in
** same compilation unit.
*/
L_SC AVL_HANDLE L_(balance)(L_BALANCE_PARAM_DECL_PREFIX AVL_HANDLE bal_h);

L_SC AVL_HANDLE L_(remove)(L_(avl) *l_tree, AVL_KEY k) {
  /* Zero-based depth in tree. */
  unsigned depth = 0, rm_depth;

  /* Records a path into the tree.  If bit n is true, indicates
  ** take greater branch from the nth node in the path, otherwise
  ** take the less branch.  bit 0 gives branch from root, and
  ** so on. */
  L_BIT_ARR_DEFN(branch)

  AVL_HANDLE h = l_tree->root;
  AVL_HANDLE parent = AVL_NULL;
  AVL_HANDLE child;
  AVL_HANDLE path;
  int cmp, cmp_shortened_sub_with_path;
  int reduced_depth;
  int bf;
  AVL_HANDLE rm;
  AVL_HANDLE parent_rm;

  for (;;) {
    if (h == AVL_NULL)
      /* No node in tree with given key. */
      return(AVL_NULL);

    cmp = AVL_COMPARE_KEY_NODE(k, h);

    if (cmp == 0)
      /* Found node to remove. */
      break;

    parent = h;

    if (cmp > 0) {
      h = AVL_GET_GREATER(h, 1);
      L_BIT_ARR_1(branch, depth)
    } else {
      h = AVL_GET_LESS(h, 1);
      L_BIT_ARR_0(branch, depth)
    }

    L_CHECK_READ_ERROR(AVL_NULL)
    depth++;
    cmp_shortened_sub_with_path = cmp;
  }

  rm = h;
  parent_rm = parent;
  rm_depth = depth;

  /* If the node to remove is not a leaf node, we need to get a
  ** leaf node, or a node with a single leaf as its child, to put
  ** in the place of the node to remove.  We will get the greatest
  ** node in the less subtree (of the node to remove), or the least
  ** node in the greater subtree.  We take the leaf node from the
  ** deeper subtree, if there is one. */

  if (AVL_GET_BALANCE_FACTOR(h) < 0) {
    child = AVL_GET_LESS(h, 1);
    L_BIT_ARR_0(branch, depth)
    cmp = -1;
  } else {
    child = AVL_GET_GREATER(h, 1);
    L_BIT_ARR_1(branch, depth)
    cmp = 1;
  }

  L_CHECK_READ_ERROR(AVL_NULL)
  depth++;

  if (child != AVL_NULL) {
    cmp = -cmp;

    do {
      parent = h;
      h = child;

      if (cmp < 0) {
        child = AVL_GET_LESS(h, 1);
        L_BIT_ARR_0(branch, depth)
      } else {
        child = AVL_GET_GREATER(h, 1);
        L_BIT_ARR_1(branch, depth)
      }

      L_CHECK_READ_ERROR(AVL_NULL)
      depth++;
    } while (child != AVL_NULL);

    if (parent == rm)
      /* Only went through do loop once.  Deleted node will be replaced
      ** in the tree structure by one of its immediate children. */
      cmp_shortened_sub_with_path = -cmp;
    else
      cmp_shortened_sub_with_path = cmp;

    /* Get the handle of the opposite child, which may not be null. */
    child = cmp > 0 ? AVL_GET_LESS(h, 0) : AVL_GET_GREATER(h, 0);
  }

  if (parent == AVL_NULL)
    /* There were only 1 or 2 nodes in this tree. */
    l_tree->root = child;
  else if (cmp_shortened_sub_with_path < 0)
    AVL_SET_LESS(parent, child)
    else
      AVL_SET_GREATER(parent, child)

      /* "path" is the parent of the subtree being eliminated or reduced
      ** from a depth of 2 to 1.  If "path" is the node to be removed, we
      ** set path to the node we're about to poke into the position of the
      ** node to be removed. */
      path = parent == rm ? h : parent;

  if (h != rm) {
    /* Poke in the replacement for the node to be removed. */
    AVL_SET_LESS(h, AVL_GET_LESS(rm, 0))
    AVL_SET_GREATER(h, AVL_GET_GREATER(rm, 0))
    AVL_SET_BALANCE_FACTOR(h, AVL_GET_BALANCE_FACTOR(rm))

    if (parent_rm == AVL_NULL)
      l_tree->root = h;
    else {
      depth = rm_depth - 1;

      if (L_BIT_ARR_VAL(branch, depth))
        AVL_SET_GREATER(parent_rm, h)
        else
          AVL_SET_LESS(parent_rm, h)
        }
  }

  if (path != AVL_NULL) {
    /* Create a temporary linked list from the parent of the path node
    ** to the root node. */
    h = l_tree->root;
    parent = AVL_NULL;
    depth = 0;

    while (h != path) {
      if (L_BIT_ARR_VAL(branch, depth)) {
        child = AVL_GET_GREATER(h, 1);
        AVL_SET_GREATER(h, parent)
      } else {
        child = AVL_GET_LESS(h, 1);
        AVL_SET_LESS(h, parent)
      }

      L_CHECK_READ_ERROR(AVL_NULL)
      depth++;
      parent = h;
      h = child;
    }

    /* Climb from the path node to the root node using the linked
    ** list, restoring the tree structure and rebalancing as necessary.
    */
    reduced_depth = 1;
    cmp = cmp_shortened_sub_with_path;

    for (;;) {
      if (reduced_depth) {
        bf = AVL_GET_BALANCE_FACTOR(h);

        if (cmp < 0)
          bf++;
        else  /* cmp > 0 */
          bf--;

        if ((bf == -2) || (bf == 2)) {
          h = L_(balance)(L_BALANCE_PARAM_CALL_PREFIX h);
          L_CHECK_READ_ERROR(AVL_NULL)
          bf = AVL_GET_BALANCE_FACTOR(h);
        } else
          AVL_SET_BALANCE_FACTOR(h, bf)
          reduced_depth = (bf == 0);
      }

      if (parent == AVL_NULL)
        break;

      child = h;
      h = parent;
      depth--;
      cmp = L_BIT_ARR_VAL(branch, depth) ? 1 : -1;

      if (cmp < 0) {
        parent = AVL_GET_LESS(h, 1);
        AVL_SET_LESS(h, child)
      } else {
        parent = AVL_GET_GREATER(h, 1);
        AVL_SET_GREATER(h, child)
      }

      L_CHECK_READ_ERROR(AVL_NULL)
    }

    l_tree->root = h;
  }

  return(rm);
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_SUBST)

L_SC AVL_HANDLE L_(subst)(L_(avl) *l_tree, AVL_HANDLE new_node) {
  AVL_HANDLE h = l_tree->root;
  AVL_HANDLE parent = AVL_NULL;
  int cmp, last_cmp;

  /* Search for node already in tree with same key. */
  for (;;) {
    if (h == AVL_NULL)
      /* No node in tree with same key as new node. */
      return(AVL_NULL);

    cmp = AVL_COMPARE_NODE_NODE(new_node, h);

    if (cmp == 0)
      /* Found the node to substitute new one for. */
      break;

    last_cmp = cmp;
    parent = h;
    h = cmp < 0 ? AVL_GET_LESS(h, 1) : AVL_GET_GREATER(h, 1);
    L_CHECK_READ_ERROR(AVL_NULL)
  }

  /* Copy tree housekeeping fields from node in tree to new node. */
  AVL_SET_LESS(new_node, AVL_GET_LESS(h, 0))
  AVL_SET_GREATER(new_node, AVL_GET_GREATER(h, 0))
  AVL_SET_BALANCE_FACTOR(new_node, AVL_GET_BALANCE_FACTOR(h))

  if (parent == AVL_NULL)
    /* New node is also new root. */
    l_tree->root = new_node;
  else {
    /* Make parent point to new node. */
    if (last_cmp < 0)
      AVL_SET_LESS(parent, new_node)
      else
        AVL_SET_GREATER(parent, new_node)
      }

  return(h);
}

#endif

#ifdef AVL_BUILD_ITER_TYPE

#if (L_IMPL_MASK & AVL_IMPL_BUILD)

L_SC int L_(build)(
  L_(avl) *l_tree, AVL_BUILD_ITER_TYPE p, L_SIZE num_nodes) {
  /* Gives path to subtree being built.  If bit n is false, branch
  ** less from the node at depth n, if true branch greater. */
  L_BIT_ARR_DEFN(branch)

  /* If bit n is true, then for the current subtree at depth n, its
  ** greater subtree has one more node than its less subtree. */
  L_BIT_ARR_DEFN(rem)

  /* Depth of root node of current subtree. */
  unsigned depth = 0;

  /* Number of nodes in current subtree. */
  L_SIZE num_sub = num_nodes;

  /* The algorithm relies on a stack of nodes whose less subtree has
  ** been built, but whose greater subtree has not yet been built.
  ** The stack is implemented as linked list.  The nodes are linked
  ** together by having the "greater" handle of a node set to the
  ** next node in the list.  "less_parent" is the handle of the first
  ** node in the list. */
  AVL_HANDLE less_parent = AVL_NULL;

  /* h is root of current subtree, child is one of its children. */
  AVL_HANDLE h;
  AVL_HANDLE child;

  if (num_nodes == 0) {
    l_tree->root = AVL_NULL;
    return(1);
  }

  for (;;) {
    while (num_sub > 2) {
      /* Subtract one for root of subtree. */
      num_sub--;

      if (num_sub & 1)
        L_BIT_ARR_1(rem, depth)
        else
          L_BIT_ARR_0(rem, depth)
          L_BIT_ARR_0(branch, depth)
          depth++;

      num_sub >>= 1;
    }

    if (num_sub == 2) {
      /* Build a subtree with two nodes, slanting to greater.
      ** I arbitrarily chose to always have the extra node in the
      ** greater subtree when there is an odd number of nodes to
      ** split between the two subtrees. */

      h = AVL_BUILD_ITER_VAL(p);
      L_CHECK_READ_ERROR(0)
      AVL_BUILD_ITER_INCR(p)
      child = AVL_BUILD_ITER_VAL(p);
      L_CHECK_READ_ERROR(0)
      AVL_BUILD_ITER_INCR(p)
      AVL_SET_LESS(child, AVL_NULL)
      AVL_SET_GREATER(child, AVL_NULL)
      AVL_SET_BALANCE_FACTOR(child, 0)
      AVL_SET_GREATER(h, child)
      AVL_SET_LESS(h, AVL_NULL)
      AVL_SET_BALANCE_FACTOR(h, 1)
    } else { /* num_sub == 1 */
      /* Build a subtree with one node. */

      h = AVL_BUILD_ITER_VAL(p);
      L_CHECK_READ_ERROR(0)
      AVL_BUILD_ITER_INCR(p)
      AVL_SET_LESS(h, AVL_NULL)
      AVL_SET_GREATER(h, AVL_NULL)
      AVL_SET_BALANCE_FACTOR(h, 0)
    }

    while (depth) {
      depth--;

      if (!L_BIT_ARR_VAL(branch, depth))
        /* We've completed a less subtree. */
        break;

      /* We've completed a greater subtree, so attach it to
      ** its parent (that is less than it).  We pop the parent
      ** off the stack of less parents. */
      child = h;
      h = less_parent;
      less_parent = AVL_GET_GREATER(h, 1);
      L_CHECK_READ_ERROR(0)
      AVL_SET_GREATER(h, child)
      /* num_sub = 2 * (num_sub - rem[depth]) + rem[depth] + 1 */
      num_sub <<= 1;
      num_sub += L_BIT_ARR_VAL(rem, depth) ? 0 : 1;

      if (num_sub & (num_sub - 1))
        /* num_sub is not a power of 2. */
        AVL_SET_BALANCE_FACTOR(h, 0)
        else
          /* num_sub is a power of 2. */
          AVL_SET_BALANCE_FACTOR(h, 1)
        }

    if (num_sub == num_nodes)
      /* We've completed the full tree. */
      break;

    /* The subtree we've completed is the less subtree of the
    ** next node in the sequence. */

    child = h;
    h = AVL_BUILD_ITER_VAL(p);
    L_CHECK_READ_ERROR(0)
    AVL_BUILD_ITER_INCR(p)
    AVL_SET_LESS(h, child)

    /* Put h into stack of less parents. */
    AVL_SET_GREATER(h, less_parent)
    less_parent = h;

    /* Proceed to creating greater than subtree of h. */
    L_BIT_ARR_1(branch, depth)
    num_sub += L_BIT_ARR_VAL(rem, depth) ? 1 : 0;
    depth++;

  } /* end for (;; ) */

  l_tree->root = h;

  return(1);
}

#endif

#endif

#if (L_IMPL_MASK & AVL_IMPL_INIT_ITER)

/* Initialize depth to invalid value, to indicate iterator is
** invalid.   (Depth is zero-base.)  It's not necessary to initialize
** iterators prior to passing them to the "start" function.
*/
L_SC void L_(init_iter)(L_(iter) *iter) {
  iter->depth = ~0;
}

#endif

#ifdef AVL_READ_ERRORS_HAPPEN

#define L_CHECK_READ_ERROR_INV_DEPTH \
  { if (AVL_READ_ERROR) { iter->depth = ~0; return; } }

#else

#define L_CHECK_READ_ERROR_INV_DEPTH

#endif

#if (L_IMPL_MASK & AVL_IMPL_START_ITER)

L_SC void L_(start_iter)(
  L_(avl) *l_tree, L_(iter) *iter, AVL_KEY k, avl_search_type st) {
  AVL_HANDLE h = l_tree->root;
  unsigned d = 0;
  int cmp, target_cmp;

  /* Save the tree that we're going to iterate through in a
  ** member variable. */
  iter->tree_ = l_tree;

  iter->depth = ~0;

  if (h == AVL_NULL)
    /* Tree is empty. */
    return;

  if (st & AVL_LESS)
    /* Key can be greater than key of starting node. */
    target_cmp = 1;
  else if (st & AVL_GREATER)
    /* Key can be less than key of starting node. */
    target_cmp = -1;
  else
    /* Key must be same as key of starting node. */
    target_cmp = 0;

  for (;;) {
    cmp = AVL_COMPARE_KEY_NODE(k, h);

    if (cmp == 0) {
      if (st & AVL_EQUAL) {
        /* Equal node was sought and found as starting node. */
        iter->depth = d;
        break;
      }

      cmp = -target_cmp;
    } else if (target_cmp != 0)
      if (!((cmp ^ target_cmp) & L_MASK_HIGH_BIT))
        /* cmp and target_cmp are both negative or both positive. */
        iter->depth = d;

    h = cmp < 0 ? AVL_GET_LESS(h, 1) : AVL_GET_GREATER(h, 1);
    L_CHECK_READ_ERROR_INV_DEPTH

    if (h == AVL_NULL)
      break;

    if (cmp > 0)
      L_BIT_ARR_1(iter->branch, d)
      else
        L_BIT_ARR_0(iter->branch, d)
        iter->path_h[d++] = h;
  }
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_START_ITER_LEAST)

L_SC void L_(start_iter_least)(L_(avl) *l_tree, L_(iter) *iter) {
  AVL_HANDLE h = l_tree->root;

  iter->tree_ = l_tree;

  iter->depth = ~0;

  L_BIT_ARR_ALL(iter->branch, 0)

  while (h != AVL_NULL) {
    if (iter->depth != ~0)
      iter->path_h[iter->depth] = h;

    iter->depth++;
    h = AVL_GET_LESS(h, 1);
    L_CHECK_READ_ERROR_INV_DEPTH
  }
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_START_ITER_GREATEST)

L_SC void L_(start_iter_greatest)(L_(avl) *l_tree, L_(iter) *iter) {
  AVL_HANDLE h = l_tree->root;

  iter->tree_ = l_tree;

  iter->depth = ~0;

  L_BIT_ARR_ALL(iter->branch, 1)

  while (h != AVL_NULL) {
    if (iter->depth != ~0)
      iter->path_h[iter->depth] = h;

    iter->depth++;
    h = AVL_GET_GREATER(h, 1);
    L_CHECK_READ_ERROR_INV_DEPTH
  }
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_GET_ITER)

L_SC AVL_HANDLE L_(get_iter)(L_(iter) *iter) {
  if (iter->depth == ~0)
    return(AVL_NULL);

  return(iter->depth == 0 ?
         iter->tree_->root : iter->path_h[iter->depth - 1]);
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_INCR_ITER)

L_SC void L_(incr_iter)(L_(iter) *iter) {
#define l_tree (iter->tree_)

  if (iter->depth != ~0) {
    AVL_HANDLE h =
      AVL_GET_GREATER((iter->depth == 0 ?
                       iter->tree_->root : iter->path_h[iter->depth - 1]), 1);
    L_CHECK_READ_ERROR_INV_DEPTH

    if (h == AVL_NULL)
      do {
        if (iter->depth == 0) {
          iter->depth = ~0;
          break;
        }

        iter->depth--;
      } while (L_BIT_ARR_VAL(iter->branch, iter->depth));
    else {
      L_BIT_ARR_1(iter->branch, iter->depth)
      iter->path_h[iter->depth++] = h;

      for (;;) {
        h = AVL_GET_LESS(h, 1);
        L_CHECK_READ_ERROR_INV_DEPTH

        if (h == AVL_NULL)
          break;

        L_BIT_ARR_0(iter->branch, iter->depth)
        iter->path_h[iter->depth++] = h;
      }
    }
  }

#undef l_tree
}

#endif

#if (L_IMPL_MASK & AVL_IMPL_DECR_ITER)

L_SC void L_(decr_iter)(L_(iter) *iter) {
#define l_tree (iter->tree_)

  if (iter->depth != ~0) {
    AVL_HANDLE h =
      AVL_GET_LESS((iter->depth == 0 ?
                    iter->tree_->root : iter->path_h[iter->depth - 1]), 1);
    L_CHECK_READ_ERROR_INV_DEPTH

    if (h == AVL_NULL)
      do {
        if (iter->depth == 0) {
          iter->depth = ~0;
          break;
        }

        iter->depth--;
      } while (!L_BIT_ARR_VAL(iter->branch, iter->depth));
    else {
      L_BIT_ARR_0(iter->branch, iter->depth)
      iter->path_h[iter->depth++] = h;

      for (;;) {
        h = AVL_GET_GREATER(h, 1);
        L_CHECK_READ_ERROR_INV_DEPTH

        if (h == AVL_NULL)
          break;

        L_BIT_ARR_1(iter->branch, iter->depth)
        iter->path_h[iter->depth++] = h;
      }
    }
  }

#undef l_tree
}

#endif

/* Tidy up the preprocessor symbol name space. */
#undef L_
#undef L_EST_LONG_BIT
#undef L_SIZE
#undef L_MASK_HIGH_BIT
#undef L_LONG_BIT
#undef L_BIT_ARR_DEFN
#undef L_BIT_ARR_VAL
#undef L_BIT_ARR_0
#undef L_BIT_ARR_1
#undef L_BIT_ARR_ALL
#undef L_CHECK_READ_ERROR
#undef L_CHECK_READ_ERROR_INV_DEPTH
#undef L_BIT_ARR_LONGS
#undef L_IMPL_MASK
#undef L_CHECK_READ_ERROR
#undef L_CHECK_READ_ERROR_INV_DEPTH
#undef L_SC
#undef L_BALANCE_PARAM_CALL_PREFIX
#undef L_BALANCE_PARAM_DECL_PREFIX

#endif  // VPX_MEM_MEMORY_MANAGER_INCLUDE_CAVL_IMPL_H_
