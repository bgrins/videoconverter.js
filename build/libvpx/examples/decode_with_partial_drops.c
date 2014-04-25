/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Decode With Partial Drops Example
// =========================
//
// This is an example utility which drops a series of frames (or parts of
// frames), as specified on the command line. This is useful for observing the
// error recovery features of the codec.
//
// Usage
// -----
// This example adds a single argument to the `simple_decoder` example,
// which specifies the range or pattern of frames to drop. The parameter is
// parsed as follows.
//
// Dropping A Range Of Frames
// --------------------------
// To drop a range of frames, specify the starting frame and the ending
// frame to drop, separated by a dash. The following command will drop
// frames 5 through 10 (base 1).
//
//  $ ./decode_with_partial_drops in.ivf out.i420 5-10
//
//
// Dropping A Pattern Of Frames
// ----------------------------
// To drop a pattern of frames, specify the number of frames to drop and
// the number of frames after which to repeat the pattern, separated by
// a forward-slash. The following command will drop 3 of 7 frames.
// Specifically, it will decode 4 frames, then drop 3 frames, and then
// repeat.
//
//  $ ./decode_with_partial_drops in.ivf out.i420 3/7
//
// Dropping Random Parts Of Frames
// -------------------------------
// A third argument tuple is available to split the frame into 1500 bytes pieces
// and randomly drop pieces rather than frames. The frame will be split at
// partition boundaries where possible. The following example will seed the RNG
// with the seed 123 and drop approximately 5% of the pieces. Pieces which
// are depending on an already dropped piece will also be dropped.
//
//  $ ./decode_with_partial_drops in.ivf out.i420 5,123
//
// Extra Variables
// ---------------
// This example maintains the pattern passed on the command line in the
// `n`, `m`, and `is_range` variables:
//
// Making The Drop Decision
// ------------------------
// The example decides whether to drop the frame based on the current
// frame number, immediately before decoding the frame.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VPX_CODEC_DISABLE_COMPAT 1
#include "./vpx_config.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"
#define interface (vpx_codec_vp8_dx())
#include <time.h>


#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)

static unsigned int mem_get_le32(const unsigned char *mem) {
    return (mem[3] << 24)|(mem[2] << 16)|(mem[1] << 8)|(mem[0]);
}

static void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    if(fmt[strlen(fmt)-1] != '\n')
        printf("\n");
    exit(EXIT_FAILURE);
}

static void die_codec(vpx_codec_ctx_t *ctx, const char *s) {
    const char *detail = vpx_codec_error_detail(ctx);

    printf("%s: %s\n", s, vpx_codec_error(ctx));
    if(detail)
        printf("    %s\n",detail);
    exit(EXIT_FAILURE);
}

struct parsed_header
{
    char key_frame;
    int version;
    char show_frame;
    int first_part_size;
};

int next_packet(struct parsed_header* hdr, int pos, int length, int mtu)
{
    int size = 0;
    int remaining = length - pos;
    /* Uncompressed part is 3 bytes for P frames and 10 bytes for I frames */
    int uncomp_part_size = (hdr->key_frame ? 10 : 3);
    /* number of bytes yet to send from header and the first partition */
    int remainFirst = uncomp_part_size + hdr->first_part_size - pos;
    if (remainFirst > 0)
    {
        if (remainFirst <= mtu)
        {
            size = remainFirst;
        }
        else
        {
            size = mtu;
        }

        return size;
    }

    /* second partition; just slot it up according to MTU */
    if (remaining <= mtu)
    {
        size = remaining;
        return size;
    }
    return mtu;
}

void throw_packets(unsigned char* frame, int* size, int loss_rate,
                   int* thrown, int* kept)
{
    unsigned char loss_frame[256*1024];
    int pkg_size = 1;
    int pos = 0;
    int loss_pos = 0;
    struct parsed_header hdr;
    unsigned int tmp;
    int mtu = 1500;

    if (*size < 3)
    {
        return;
    }
    putc('|', stdout);
    /* parse uncompressed 3 bytes */
    tmp = (frame[2] << 16) | (frame[1] << 8) | frame[0];
    hdr.key_frame = !(tmp & 0x1); /* inverse logic */
    hdr.version = (tmp >> 1) & 0x7;
    hdr.show_frame = (tmp >> 4) & 0x1;
    hdr.first_part_size = (tmp >> 5) & 0x7FFFF;

    /* don't drop key frames */
    if (hdr.key_frame)
    {
        int i;
        *kept = *size/mtu + ((*size % mtu > 0) ? 1 : 0); /* approximate */
        for (i=0; i < *kept; i++)
            putc('.', stdout);
        return;
    }

    while ((pkg_size = next_packet(&hdr, pos, *size, mtu)) > 0)
    {
        int loss_event = ((rand() + 1.0)/(RAND_MAX + 1.0) < loss_rate/100.0);
        if (*thrown == 0 && !loss_event)
        {
            memcpy(loss_frame + loss_pos, frame + pos, pkg_size);
            loss_pos += pkg_size;
            (*kept)++;
            putc('.', stdout);
        }
        else
        {
            (*thrown)++;
            putc('X', stdout);
        }
        pos += pkg_size;
    }
    memcpy(frame, loss_frame, loss_pos);
    memset(frame + loss_pos, 0, *size - loss_pos);
    *size = loss_pos;
}

int main(int argc, char **argv) {
    FILE            *infile, *outfile;
    vpx_codec_ctx_t  codec;
    int              flags = 0, frame_cnt = 0;
    unsigned char    file_hdr[IVF_FILE_HDR_SZ];
    unsigned char    frame_hdr[IVF_FRAME_HDR_SZ];
    unsigned char    frame[256*1024];
    vpx_codec_err_t  res;
    int              n, m, mode;
    unsigned int     seed;
    int              thrown=0, kept=0;
    int              thrown_frame=0, kept_frame=0;
    vpx_codec_dec_cfg_t  dec_cfg = {0};

    (void)res;
    /* Open files */
    if(argc < 4 || argc > 6)
        die("Usage: %s <infile> <outfile> [-t <num threads>] <N-M|N/M|L,S>\n",
            argv[0]);
    {
        char *nptr;
        int arg_num = 3;
        if (argc == 6 && strncmp(argv[arg_num++], "-t", 2) == 0)
            dec_cfg.threads = strtol(argv[arg_num++], NULL, 0);
        n = strtol(argv[arg_num], &nptr, 0);
        mode = (*nptr == '\0' || *nptr == ',') ? 2 : (*nptr == '-') ? 1 : 0;

        m = strtol(nptr+1, NULL, 0);
        if((!n && !m) || (*nptr != '-' && *nptr != '/' &&
            *nptr != '\0' && *nptr != ','))
            die("Couldn't parse pattern %s\n", argv[3]);
    }
    seed = (m > 0) ? m : (unsigned int)time(NULL);
    srand(seed);thrown_frame = 0;
    printf("Seed: %u\n", seed);
    printf("Threads: %d\n", dec_cfg.threads);
    if(!(infile = fopen(argv[1], "rb")))
        die("Failed to open %s for reading", argv[1]);
    if(!(outfile = fopen(argv[2], "wb")))
        die("Failed to open %s for writing", argv[2]);

    /* Read file header */
    if(!(fread(file_hdr, 1, IVF_FILE_HDR_SZ, infile) == IVF_FILE_HDR_SZ
         && file_hdr[0]=='D' && file_hdr[1]=='K' && file_hdr[2]=='I'
         && file_hdr[3]=='F'))
        die("%s is not an IVF file.", argv[1]);

    printf("Using %s\n",vpx_codec_iface_name(interface));
    /* Initialize codec */
    flags = VPX_CODEC_USE_ERROR_CONCEALMENT;
    res = vpx_codec_dec_init(&codec, interface, &dec_cfg, flags);
    if(res)
        die_codec(&codec, "Failed to initialize decoder");


    /* Read each frame */
    while(fread(frame_hdr, 1, IVF_FRAME_HDR_SZ, infile) == IVF_FRAME_HDR_SZ) {
        int               frame_sz = mem_get_le32(frame_hdr);
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t      *img;


        frame_cnt++;
        if(frame_sz > sizeof(frame))
            die("Frame %d data too big for example code buffer", frame_sz);
        if(fread(frame, 1, frame_sz, infile) != frame_sz)
            die("Frame %d failed to read complete frame", frame_cnt);

        /* Decide whether to throw parts of the frame or the whole frame
           depending on the drop mode */
        thrown_frame = 0;
        kept_frame = 0;
        switch (mode)
        {
        case 0:
            if (m - (frame_cnt-1)%m <= n)
            {
                frame_sz = 0;
            }
            break;
        case 1:
            if (frame_cnt >= n && frame_cnt <= m)
            {
                frame_sz = 0;
            }
            break;
        case 2:
            throw_packets(frame, &frame_sz, n, &thrown_frame, &kept_frame);
            break;
        default: break;
        }
        if (mode < 2)
        {
            if (frame_sz == 0)
            {
                putc('X', stdout);
                thrown_frame++;
            }
            else
            {
                putc('.', stdout);
                kept_frame++;
            }
        }
        thrown += thrown_frame;
        kept += kept_frame;
        fflush(stdout);
        /* Decode the frame */
        if(vpx_codec_decode(&codec, frame, frame_sz, NULL, 0))
            die_codec(&codec, "Failed to decode frame");

        /* Write decoded data to disk */
        while((img = vpx_codec_get_frame(&codec, &iter))) {
            unsigned int plane, y;

            for(plane=0; plane < 3; plane++) {
                unsigned char *buf =img->planes[plane];
            
                for(y=0; y < (plane ? (img->d_h + 1) >> 1 : img->d_h); y++) {
                    (void) fwrite(buf, 1, (plane ? (img->d_w + 1) >> 1 : img->d_w),
                                  outfile);
                    buf += img->stride[plane];
                }
            }
        }
    }
    printf("Processed %d frames.\n",frame_cnt);
    if(vpx_codec_destroy(&codec))
        die_codec(&codec, "Failed to destroy codec");

    fclose(outfile);
    fclose(infile);
    return EXIT_SUCCESS;
}
