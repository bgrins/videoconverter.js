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

U(size_aau) U(largest_available)(U(descriptor) *desc) {
  U(size_bau) largest;

  if (!(desc->avl_tree_root))
    largest = 0;
  else {
#ifdef HMM_AUDIT_FAIL
    /* Audit root block in AVL tree. */
    AUDIT_BLOCK(PTR_REC_TO_HEAD(desc->avl_tree_root))
#endif

    largest =
      BLOCK_BAUS(
        PTR_REC_TO_HEAD(
          U(avl_search)(
            (U(avl_avl) *) & (desc->avl_tree_root),
            (U(size_bau)) ~(U(size_bau)) 0, AVL_LESS)));
  }

  if (desc->last_freed) {
    /* Size of last freed block. */
    register U(size_bau) lf_size;

#ifdef HMM_AUDIT_FAIL
    AUDIT_BLOCK(desc->last_freed)
#endif

    lf_size = BLOCK_BAUS(desc->last_freed);

    if (lf_size > largest)
      largest = lf_size;
  }

  /* Convert largest size to AAUs and subract head size leaving payload
  ** size.
  */
  return(largest ?
         ((largest * ((U(size_aau)) HMM_BLOCK_ALIGN_UNIT)) - HEAD_AAUS) :
         0);
}
