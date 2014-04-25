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

void U(grow_chunk)(U(descriptor) *desc, void *end, U(size_bau) n_baus) {
#undef HEAD_PTR
#define HEAD_PTR ((head_record *) end)

  end = BAUS_BACKWARD(end, DUMMY_END_BLOCK_BAUS);

#ifdef HMM_AUDIT_FAIL

  if (HEAD_PTR->block_size != 0)
    /* Chunk does not have valid dummy end block. */
    HMM_AUDIT_FAIL

#endif

    /* Create a new block that absorbs the old dummy end block. */
    HEAD_PTR->block_size = n_baus;

  /* Set up the new dummy end block. */
  {
    head_record *dummy = (head_record *) BAUS_FORWARD(end, n_baus);
    dummy->previous_block_size = n_baus;
    dummy->block_size = 0;
  }

  /* Simply free the new block, allowing it to coalesce with any
  ** free block at that was the last block in the chunk prior to
  ** growth.
  */
  U(free)(desc, HEAD_TO_PTR_REC(end));

#undef HEAD_PTR
}
