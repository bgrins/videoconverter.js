;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl save_platform_context
    .globl restore_platform_context

.macro W V P
    stvx    \V,  0, \P
    addi    \P, \P, 16
.endm

.macro R V P
    lvx     \V,  0, \P
    addi    \P, \P, 16
.endm

;# r3 context_ptr
    .align 2
save_platform_contex:
    W v20, r3
    W v21, r3
    W v22, r3
    W v23, r3
    W v24, r3
    W v25, r3
    W v26, r3
    W v27, r3
    W v28, r3
    W v29, r3
    W v30, r3
    W v31, r3

    blr

;# r3 context_ptr
    .align 2
restore_platform_context:
    R v20, r3
    R v21, r3
    R v22, r3
    R v23, r3
    R v24, r3
    R v25, r3
    R v26, r3
    R v27, r3
    R v28, r3
    R v29, r3
    R v30, r3
    R v31, r3

    blr
