/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/* This code is in the public domain.
** Version: 1.1  Author: Walt Karas
*/

#include "hmm_intrnl.h"

void U(init)(U(descriptor) *desc) {
  desc->avl_tree_root = 0;
  desc->last_freed = 0;
}

/* Remove a free block from a bin's doubly-linked list when it is not,
** the first block in the bin.
*/
void U(dll_remove)(
  /* Pointer to pointer record in the block to be removed. */
  ptr_record *to_remove) {
  to_remove->prev->next = to_remove->next;

  if (to_remove->next)
    to_remove->next->prev = to_remove->prev;
}

/* Put a block into the free collection of a heap.
*/
void U(into_free_collection)(
  /* Pointer to heap descriptor. */
  U(descriptor) *desc,
  /* Pointer to head record of block. */
  head_record *head_ptr) {
  ptr_record *ptr_rec_ptr = HEAD_TO_PTR_REC(head_ptr);

  ptr_record *bin_front_ptr =
    U(avl_insert)((U(avl_avl) *) & (desc->avl_tree_root), ptr_rec_ptr);

  if (bin_front_ptr != ptr_rec_ptr) {
    /* The block was not inserted into the AVL tree because there is
    ** already a bin for the size of the block. */

    MARK_SUCCESSIVE_BLOCK_IN_FREE_BIN(head_ptr)
    ptr_rec_ptr->self = ptr_rec_ptr;

    /* Make the block the new second block in the bin's doubly-linked
    ** list. */
    ptr_rec_ptr->prev = bin_front_ptr;
    ptr_rec_ptr->next = bin_front_ptr->next;
    bin_front_ptr->next = ptr_rec_ptr;

    if (ptr_rec_ptr->next)
      ptr_rec_ptr->next->prev = ptr_rec_ptr;
  } else
    /* Block is first block in new bin. */
    ptr_rec_ptr->next = 0;
}

/* Allocate a block from a given bin.  Returns a pointer to the payload
** of the removed block.  The "last freed" pointer must be null prior
** to calling this function.
*/
void *U(alloc_from_bin)(
  /* Pointer to heap descriptor. */
  U(descriptor) *desc,
  /* Pointer to pointer record of first block in bin. */
  ptr_record *bin_front_ptr,
  /* Number of BAUs needed in the allocated block.  If the block taken
  ** from the bin is significantly larger than the number of BAUs needed,
  ** the "extra" BAUs are split off to form a new free block. */
  U(size_bau) n_baus) {
  head_record *head_ptr;
  U(size_bau) rem_baus;

  if (bin_front_ptr->next) {
    /* There are multiple blocks in this bin.  Use the 2nd block in
    ** the bin to avoid needless change to the AVL tree.
    */

    ptr_record *ptr_rec_ptr = bin_front_ptr->next;
    head_ptr = PTR_REC_TO_HEAD(ptr_rec_ptr);

#ifdef AUDIT_FAIL
    AUDIT_BLOCK(head_ptr)
#endif

    U(dll_remove)(ptr_rec_ptr);
  } else {
    /* There is only one block in the bin, so it has to be removed
    ** from the AVL tree.
    */

    head_ptr = PTR_REC_TO_HEAD(bin_front_ptr);

    U(avl_remove)(
      (U(avl_avl) *) & (desc->avl_tree_root), BLOCK_BAUS(head_ptr));
  }

  MARK_BLOCK_ALLOCATED(head_ptr)

  rem_baus = BLOCK_BAUS(head_ptr) - n_baus;

  if (rem_baus >= MIN_BLOCK_BAUS) {
    /* Since there are enough "extra" BAUs, split them off to form
    ** a new free block.
    */

    head_record *rem_head_ptr =
      (head_record *) BAUS_FORWARD(head_ptr, n_baus);

    /* Change the next block's header to reflect the fact that the
    ** block preceeding it is now smaller.
    */
    SET_PREV_BLOCK_BAUS(
      BAUS_FORWARD(head_ptr, head_ptr->block_size), rem_baus)

    head_ptr->block_size = n_baus;

    rem_head_ptr->previous_block_size = n_baus;
    rem_head_ptr->block_size = rem_baus;

    desc->last_freed = rem_head_ptr;
  }

  return(HEAD_TO_PTR_REC(head_ptr));
}

/* Take a block out of the free collection.
*/
void U(out_of_free_collection)(
  /* Descriptor of heap that block is in. */
  U(descriptor) *desc,
  /* Pointer to head of block to take out of free collection. */
  head_record *head_ptr) {
  ptr_record *ptr_rec_ptr = HEAD_TO_PTR_REC(head_ptr);

  if (ptr_rec_ptr->self == ptr_rec_ptr)
    /* Block is not the front block in its bin, so all we have to
    ** do is take it out of the bin's doubly-linked list. */
    U(dll_remove)(ptr_rec_ptr);
  else {
    ptr_record *next = ptr_rec_ptr->next;

    if (next)
      /* Block is the front block in its bin, and there is at least
      ** one other block in the bin.  Substitute the next block for
      ** the front block. */
      U(avl_subst)((U(avl_avl) *) & (desc->avl_tree_root), next);
    else
      /* Block is the front block in its bin, but there is no other
      ** block in the bin.  Eliminate the bin. */
      U(avl_remove)(
        (U(avl_avl) *) & (desc->avl_tree_root), BLOCK_BAUS(head_ptr));
  }
}

void U(free)(U(descriptor) *desc, void *payload_ptr) {
  /* Flags if coalesce with adjacent block. */
  int coalesce;

  head_record *fwd_head_ptr;
  head_record *free_head_ptr = PTR_REC_TO_HEAD(payload_ptr);

  desc->num_baus_can_shrink = 0;

#ifdef HMM_AUDIT_FAIL

  AUDIT_BLOCK(free_head_ptr)

  /* Make sure not freeing an already free block. */
  if (!IS_BLOCK_ALLOCATED(free_head_ptr))
    HMM_AUDIT_FAIL

    if (desc->avl_tree_root)
      /* Audit root block in AVL tree. */
      AUDIT_BLOCK(PTR_REC_TO_HEAD(desc->avl_tree_root))

#endif

      fwd_head_ptr =
        (head_record *) BAUS_FORWARD(free_head_ptr, free_head_ptr->block_size);

  if (free_head_ptr->previous_block_size) {
    /* Coalesce with backward block if possible. */

    head_record *bkwd_head_ptr =
      (head_record *) BAUS_BACKWARD(
        free_head_ptr, free_head_ptr->previous_block_size);

#ifdef HMM_AUDIT_FAIL
    AUDIT_BLOCK(bkwd_head_ptr)
#endif

    if (bkwd_head_ptr == (head_record *)(desc->last_freed)) {
      desc->last_freed = 0;
      coalesce = 1;
    } else if (IS_BLOCK_ALLOCATED(bkwd_head_ptr))
      coalesce = 0;
    else {
      U(out_of_free_collection)(desc, bkwd_head_ptr);
      coalesce = 1;
    }

    if (coalesce) {
      bkwd_head_ptr->block_size += free_head_ptr->block_size;
      SET_PREV_BLOCK_BAUS(fwd_head_ptr, BLOCK_BAUS(bkwd_head_ptr))
      free_head_ptr = bkwd_head_ptr;
    }
  }

  if (fwd_head_ptr->block_size == 0) {
    /* Block to be freed is last block before dummy end-of-chunk block. */
    desc->end_of_shrinkable_chunk =
      BAUS_FORWARD(fwd_head_ptr, DUMMY_END_BLOCK_BAUS);
    desc->num_baus_can_shrink = BLOCK_BAUS(free_head_ptr);

    if (PREV_BLOCK_BAUS(free_head_ptr) == 0)
      /* Free block is the entire chunk, so shrinking can eliminate
      ** entire chunk including dummy end block. */
      desc->num_baus_can_shrink += DUMMY_END_BLOCK_BAUS;
  } else {
    /* Coalesce with forward block if possible. */

#ifdef HMM_AUDIT_FAIL
    AUDIT_BLOCK(fwd_head_ptr)
#endif

    if (fwd_head_ptr == (head_record *)(desc->last_freed)) {
      desc->last_freed = 0;
      coalesce = 1;
    } else if (IS_BLOCK_ALLOCATED(fwd_head_ptr))
      coalesce = 0;
    else {
      U(out_of_free_collection)(desc, fwd_head_ptr);
      coalesce = 1;
    }

    if (coalesce) {
      free_head_ptr->block_size += fwd_head_ptr->block_size;

      fwd_head_ptr =
        (head_record *) BAUS_FORWARD(
          fwd_head_ptr, BLOCK_BAUS(fwd_head_ptr));

      SET_PREV_BLOCK_BAUS(fwd_head_ptr, BLOCK_BAUS(free_head_ptr))

      if (fwd_head_ptr->block_size == 0) {
        /* Coalesced block to be freed is last block before dummy
        ** end-of-chunk block. */
        desc->end_of_shrinkable_chunk =
          BAUS_FORWARD(fwd_head_ptr, DUMMY_END_BLOCK_BAUS);
        desc->num_baus_can_shrink = BLOCK_BAUS(free_head_ptr);

        if (PREV_BLOCK_BAUS(free_head_ptr) == 0)
          /* Free block is the entire chunk, so shrinking can
          ** eliminate entire chunk including dummy end block. */
          desc->num_baus_can_shrink += DUMMY_END_BLOCK_BAUS;
      }
    }
  }

  if (desc->last_freed) {
    /* There is a last freed block, but it is not adjacent to the
    ** block being freed by this call to free, so put the last
    ** freed block into the free collection.
    */

#ifdef HMM_AUDIT_FAIL
    AUDIT_BLOCK(desc->last_freed)
#endif

    U(into_free_collection)(desc, (head_record *)(desc->last_freed));
  }

  desc->last_freed = free_head_ptr;
}

void U(new_chunk)(U(descriptor) *desc, void *start, U(size_bau) n_baus) {
#ifdef HMM_AUDIT_FAIL

  if (desc->avl_tree_root)
    /* Audit root block in AVL tree. */
    AUDIT_BLOCK(PTR_REC_TO_HEAD(desc->avl_tree_root))
#endif

#undef HEAD_PTR
#define HEAD_PTR ((head_record *) start)

    /* Make the chunk one big free block followed by a dummy end block.
    */

    n_baus -= DUMMY_END_BLOCK_BAUS;

  HEAD_PTR->previous_block_size = 0;
  HEAD_PTR->block_size = n_baus;

  U(into_free_collection)(desc, HEAD_PTR);

  /* Set up the dummy end block. */
  start = BAUS_FORWARD(start, n_baus);
  HEAD_PTR->previous_block_size = n_baus;
  HEAD_PTR->block_size = 0;

#undef HEAD_PTR
}

#ifdef HMM_AUDIT_FAIL

/* Function that does audit fail actions defined my preprocessor symbol,
** and returns a dummy integer value.
*/
int U(audit_block_fail_dummy_return)(void) {
  HMM_AUDIT_FAIL

  /* Dummy return. */
  return(0);
}

#endif

/* AVL Tree instantiation. */

#ifdef HMM_AUDIT_FAIL

/* The AVL tree generic package passes an ACCESS of 1 when it "touches"
** a child node for the first time during a particular operation.  I use
** this feature to audit only one time (per operation) the free blocks
** that are tree nodes.  Since the root node is not a child node, it has
** to be audited directly.
*/

/* The pain you feel while reading these macros will not be in vain.  It
** will remove all doubt from you mind that C++ inline functions are
** a very good thing.
*/

#define AVL_GET_LESS(H, ACCESS) \
  (((ACCESS) ? AUDIT_BLOCK_AS_EXPR(PTR_REC_TO_HEAD(H)) : 0), (H)->self)
#define AVL_GET_GREATER(H, ACCESS) \
  (((ACCESS) ? AUDIT_BLOCK_AS_EXPR(PTR_REC_TO_HEAD(H)) : 0), (H)->prev)

#else

#define AVL_GET_LESS(H, ACCESS) ((H)->self)
#define AVL_GET_GREATER(H, ACCESS) ((H)->prev)

#endif

#define AVL_SET_LESS(H, LH) (H)->self = (LH);
#define AVL_SET_GREATER(H, GH) (H)->prev = (GH);

/*  high bit of high bit of
**  block_size  previous_block_size balance factor
**  ----------- ------------------- --------------
**  0       0           n/a (block allocated)
**  0       1           1
**  1       0           -1
**  1       1           0
*/

#define AVL_GET_BALANCE_FACTOR(H) \
  ((((head_record *) (PTR_REC_TO_HEAD(H)))->block_size & \
    HIGH_BIT_BAU_SIZE) ? \
   (((head_record *) (PTR_REC_TO_HEAD(H)))->previous_block_size & \
    HIGH_BIT_BAU_SIZE ? 0 : -1) : 1)

#define AVL_SET_BALANCE_FACTOR(H, BF) \
  {                         \
    register head_record *p =               \
                                            (head_record *) PTR_REC_TO_HEAD(H);       \
    register int bal_f = (BF);              \
    \
    if (bal_f <= 0)                 \
      p->block_size |= HIGH_BIT_BAU_SIZE;       \
    else                        \
      p->block_size &= ~HIGH_BIT_BAU_SIZE;      \
    if (bal_f >= 0)                 \
      p->previous_block_size |= HIGH_BIT_BAU_SIZE;  \
    else                        \
      p->previous_block_size &= ~HIGH_BIT_BAU_SIZE; \
  }

#define COMPARE_KEY_KEY(K1, K2) ((K1) == (K2) ? 0 : ((K1) > (K2) ? 1 : -1))

#define AVL_COMPARE_KEY_NODE(K, H) \
  COMPARE_KEY_KEY(K, BLOCK_BAUS(PTR_REC_TO_HEAD(H)))

#define AVL_COMPARE_NODE_NODE(H1, H2) \
  COMPARE_KEY_KEY(BLOCK_BAUS(PTR_REC_TO_HEAD(H1)), \
                  BLOCK_BAUS(PTR_REC_TO_HEAD(H2)))

#define AVL_NULL ((ptr_record *) 0)

#define AVL_IMPL_MASK \
  ( AVL_IMPL_INSERT | AVL_IMPL_SEARCH | AVL_IMPL_REMOVE | AVL_IMPL_SUBST )

#include "cavl_impl.h"
