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

/* The function in this file performs default actions if self-auditing
** finds heap corruption.  Don't rely on this code to handle the
** case where HMM is being used to implement the malloc and free standard
** library functions.  Rewrite the function if necessary to avoid using
** I/O and execution termination functions that call malloc or free.
** In Unix, for example, you would replace the fputs calls with calls
** to the write system call using file handle number 2.
*/
#include "hmm_intrnl.h"
#include <stdio.h>
#include <stdlib.h>

static int entered = 0;

/* Print abort message, file and line.  Terminate execution.
*/
void hmm_dflt_abort(const char *file, const char *line) {
  /* Avoid use of printf(), which is more likely to use heap. */

  if (entered)

    /* The standard I/O functions called a heap function and caused
    ** an indirect recursive call to this function.  So we'll have
    ** to just exit without printing a message.  */
    while (1);

  entered = 1;

  fputs("\n_abort - Heap corruption\n" "File: ", stderr);
  fputs(file, stderr);
  fputs("  Line: ", stderr);
  fputs(line, stderr);
  fputs("\n\n", stderr);
  fputs("hmm_dflt_abort: while(1)!!!\n", stderr);
  fflush(stderr);

  while (1);
}
