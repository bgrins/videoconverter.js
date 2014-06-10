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

void *U(alloc)(U(descriptor) *desc, U(size_aau) n) {
#ifdef HMM_AUDIT_FAIL

  if (desc->avl_tree_root)
    AUDIT_BLOCK(PTR_REC_TO_HEAD(desc->avl_tree_root))
#endif

    if (desc->last_freed) {
#ifdef HMM_AUDIT_FAIL
      AUDIT_BLOCK(desc->last_freed)
#endif

      U(into_free_collection)(desc, (head_record *)(desc->last_freed));

      desc->last_freed = 0;
    }

  /* Add space for block header. */
  n += HEAD_AAUS;

  /* Convert n from number of address alignment units to block alignment
  ** units. */
  n = DIV_ROUND_UP(n, HMM_BLOCK_ALIGN_UNIT);

  if (n < MIN_BLOCK_BAUS)
    n = MIN_BLOCK_BAUS;

  {
    /* Search for the first node of the bin containing the smallest
    ** block big enough to satisfy request. */
    ptr_record *ptr_rec_ptr =
      U(avl_search)(
        (U(avl_avl) *) & (desc->avl_tree_root), (U(size_bau)) n,
        AVL_GREATER_EQUAL);

    /* If an approprate bin is found, satisfy the allocation request,
    ** otherwise return null pointer. */
    return(ptr_rec_ptr ?
           U(alloc_from_bin)(desc, ptr_rec_ptr, (U(size_bau)) n) : 0);
  }
}
