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
 * This is an example demonstrating multi-resolution encoding in VP8.
 * High-resolution input video is down-sampled to lower-resolutions. The
 * encoder then encodes the video and outputs multiple bitstreams with
 * different resolutions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "vpx_ports/mem_ops.h"
#include "./tools_common.h"
#define interface (vpx_codec_vp8_cx())
#define fourcc    0x30385056

#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)

/*
 * The input video frame is downsampled several times to generate a multi-level
 * hierarchical structure. NUM_ENCODERS is defined as the number of encoding
 * levels required. For example, if the size of input video is 1280x720,
 * NUM_ENCODERS is 3, and down-sampling factor is 2, the encoder outputs 3
 * bitstreams with resolution of 1280x720(level 0), 640x360(level 1), and
 * 320x180(level 2) respectively.
 */
#define NUM_ENCODERS 3

/* This example uses the scaler function in libyuv. */
#include "third_party/libyuv/include/libyuv/basic_types.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/libyuv/include/libyuv/cpu_id.h"

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

int (*read_frame_p)(FILE *f, vpx_image_t *img);

static int read_frame(FILE *f, vpx_image_t *img) {
    size_t nbytes, to_read;
    int    res = 1;

    to_read = img->w*img->h*3/2;
    nbytes = fread(img->planes[0], 1, to_read, f);
    if(nbytes != to_read) {
        res = 0;
        if(nbytes > 0)
            printf("Warning: Read partial frame. Check your width & height!\n");
    }
    return res;
}

static int read_frame_by_row(FILE *f, vpx_image_t *img) {
    size_t nbytes, to_read;
    int    res = 1;
    int plane;

    for (plane = 0; plane < 3; plane++)
    {
        unsigned char *ptr;
        int w = (plane ? (1 + img->d_w) / 2 : img->d_w);
        int h = (plane ? (1 + img->d_h) / 2 : img->d_h);
        int r;

        /* Determine the correct plane based on the image format. The for-loop
         * always counts in Y,U,V order, but this may not match the order of
         * the data on disk.
         */
        switch (plane)
        {
        case 1:
            ptr = img->planes[img->fmt==VPX_IMG_FMT_YV12? VPX_PLANE_V : VPX_PLANE_U];
            break;
        case 2:
            ptr = img->planes[img->fmt==VPX_IMG_FMT_YV12?VPX_PLANE_U : VPX_PLANE_V];
            break;
        default:
            ptr = img->planes[plane];
        }

        for (r = 0; r < h; r++)
        {
            to_read = w;

            nbytes = fread(ptr, 1, to_read, f);
            if(nbytes != to_read) {
                res = 0;
                if(nbytes > 0)
                    printf("Warning: Read partial frame. Check your width & height!\n");
                break;
            }

            ptr += img->stride[plane];
        }
        if (!res)
            break;
    }

    return res;
}

static void write_ivf_file_header(FILE *outfile,
                                  const vpx_codec_enc_cfg_t *cfg,
                                  int frame_cnt) {
    char header[32];

    if(cfg->g_pass != VPX_RC_ONE_PASS && cfg->g_pass != VPX_RC_LAST_PASS)
        return;
    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';
    mem_put_le16(header+4,  0);                   /* version */
    mem_put_le16(header+6,  32);                  /* headersize */
    mem_put_le32(header+8,  fourcc);              /* headersize */
    mem_put_le16(header+12, cfg->g_w);            /* width */
    mem_put_le16(header+14, cfg->g_h);            /* height */
    mem_put_le32(header+16, cfg->g_timebase.den); /* rate */
    mem_put_le32(header+20, cfg->g_timebase.num); /* scale */
    mem_put_le32(header+24, frame_cnt);           /* length */
    mem_put_le32(header+28, 0);                   /* unused */

    (void) fwrite(header, 1, 32, outfile);
}

static void write_ivf_frame_header(FILE *outfile,
                                   const vpx_codec_cx_pkt_t *pkt)
{
    char             header[12];
    vpx_codec_pts_t  pts;

    if(pkt->kind != VPX_CODEC_CX_FRAME_PKT)
        return;

    pts = pkt->data.frame.pts;
    mem_put_le32(header, pkt->data.frame.sz);
    mem_put_le32(header+4, pts&0xFFFFFFFF);
    mem_put_le32(header+8, pts >> 32);

    (void) fwrite(header, 1, 12, outfile);
}

int main(int argc, char **argv)
{
    FILE                *infile, *outfile[NUM_ENCODERS];
    vpx_codec_ctx_t      codec[NUM_ENCODERS];
    vpx_codec_enc_cfg_t  cfg[NUM_ENCODERS];
    vpx_codec_pts_t      frame_cnt = 0;
    vpx_image_t          raw[NUM_ENCODERS];
    vpx_codec_err_t      res[NUM_ENCODERS];

    int                  i;
    long                 width;
    long                 height;
    int                  frame_avail;
    int                  got_data;
    int                  flags = 0;

    /*Currently, only realtime mode is supported in multi-resolution encoding.*/
    int                  arg_deadline = VPX_DL_REALTIME;

    /* Set show_psnr to 1/0 to show/not show PSNR. Choose show_psnr=0 if you
       don't need to know PSNR, which will skip PSNR calculation and save
       encoding time. */
    int                  show_psnr = 0;
    uint64_t             psnr_sse_total[NUM_ENCODERS] = {0};
    uint64_t             psnr_samples_total[NUM_ENCODERS] = {0};
    double               psnr_totals[NUM_ENCODERS][4] = {{0,0}};
    int                  psnr_count[NUM_ENCODERS] = {0};

    /* Set the required target bitrates for each resolution level.
     * If target bitrate for highest-resolution level is set to 0,
     * (i.e. target_bitrate[0]=0), we skip encoding at that level.
     */
    unsigned int         target_bitrate[NUM_ENCODERS]={1000, 500, 100};
    /* Enter the frame rate of the input video */
    int                  framerate = 30;
    /* Set down-sampling factor for each resolution level.
       dsf[0] controls down sampling from level 0 to level 1;
       dsf[1] controls down sampling from level 1 to level 2;
       dsf[2] is not used. */
    vpx_rational_t dsf[NUM_ENCODERS] = {{2, 1}, {2, 1}, {1, 1}};

    if(argc!= (5+NUM_ENCODERS))
        die("Usage: %s <width> <height> <infile> <outfile(s)> <output psnr?>\n",
            argv[0]);

    printf("Using %s\n",vpx_codec_iface_name(interface));

    width = strtol(argv[1], NULL, 0);
    height = strtol(argv[2], NULL, 0);

    if(width < 16 || width%2 || height <16 || height%2)
        die("Invalid resolution: %ldx%ld", width, height);

    /* Open input video file for encoding */
    if(!(infile = fopen(argv[3], "rb")))
        die("Failed to open %s for reading", argv[3]);

    /* Open output file for each encoder to output bitstreams */
    for (i=0; i< NUM_ENCODERS; i++)
    {
        if(!target_bitrate[i])
        {
            outfile[i] = NULL;
            continue;
        }

        if(!(outfile[i] = fopen(argv[i+4], "wb")))
            die("Failed to open %s for writing", argv[i+4]);
    }

    show_psnr = strtol(argv[NUM_ENCODERS + 4], NULL, 0);

    /* Populate default encoder configuration */
    for (i=0; i< NUM_ENCODERS; i++)
    {
        res[i] = vpx_codec_enc_config_default(interface, &cfg[i], 0);
        if(res[i]) {
            printf("Failed to get config: %s\n", vpx_codec_err_to_string(res[i]));
            return EXIT_FAILURE;
        }
    }

    /*
     * Update the default configuration according to needs of the application.
     */
    /* Highest-resolution encoder settings */
    cfg[0].g_w = width;
    cfg[0].g_h = height;
    cfg[0].g_threads = 1;                           /* number of threads used */
    cfg[0].rc_dropframe_thresh = 30;
    cfg[0].rc_end_usage = VPX_CBR;
    cfg[0].rc_resize_allowed = 0;
    cfg[0].rc_min_quantizer = 4;
    cfg[0].rc_max_quantizer = 56;
    cfg[0].rc_undershoot_pct = 98;
    cfg[0].rc_overshoot_pct = 100;
    cfg[0].rc_buf_initial_sz = 500;
    cfg[0].rc_buf_optimal_sz = 600;
    cfg[0].rc_buf_sz = 1000;
    cfg[0].g_error_resilient = 1;              /* Enable error resilient mode */
    cfg[0].g_lag_in_frames   = 0;

    /* Disable automatic keyframe placement */
    /* Note: These 3 settings are copied to all levels. But, except the lowest
     * resolution level, all other levels are set to VPX_KF_DISABLED internally.
     */
    //cfg[0].kf_mode           = VPX_KF_DISABLED;
    cfg[0].kf_mode           = VPX_KF_AUTO;
    cfg[0].kf_min_dist = 3000;
    cfg[0].kf_max_dist = 3000;

    cfg[0].rc_target_bitrate = target_bitrate[0];       /* Set target bitrate */
    cfg[0].g_timebase.num = 1;                          /* Set fps */
    cfg[0].g_timebase.den = framerate;

    /* Other-resolution encoder settings */
    for (i=1; i< NUM_ENCODERS; i++)
    {
        memcpy(&cfg[i], &cfg[0], sizeof(vpx_codec_enc_cfg_t));

        cfg[i].g_threads = 1;                       /* number of threads used */
        cfg[i].rc_target_bitrate = target_bitrate[i];

        /* Note: Width & height of other-resolution encoders are calculated
         * from the highest-resolution encoder's size and the corresponding
         * down_sampling_factor.
         */
        {
            unsigned int iw = cfg[i-1].g_w*dsf[i-1].den + dsf[i-1].num - 1;
            unsigned int ih = cfg[i-1].g_h*dsf[i-1].den + dsf[i-1].num - 1;
            cfg[i].g_w = iw/dsf[i-1].num;
            cfg[i].g_h = ih/dsf[i-1].num;
        }

        /* Make width & height to be multiplier of 2. */
        // Should support odd size ???
        if((cfg[i].g_w)%2)cfg[i].g_w++;
        if((cfg[i].g_h)%2)cfg[i].g_h++;
    }

    /* Allocate image for each encoder */
    for (i=0; i< NUM_ENCODERS; i++)
        if(!vpx_img_alloc(&raw[i], VPX_IMG_FMT_I420, cfg[i].g_w, cfg[i].g_h, 32))
            die("Failed to allocate image", cfg[i].g_w, cfg[i].g_h);

    if (raw[0].stride[VPX_PLANE_Y] == raw[0].d_w)
        read_frame_p = read_frame;
    else
        read_frame_p = read_frame_by_row;

    for (i=0; i< NUM_ENCODERS; i++)
        if(outfile[i])
            write_ivf_file_header(outfile[i], &cfg[i], 0);

    /* Initialize multi-encoder */
    if(vpx_codec_enc_init_multi(&codec[0], interface, &cfg[0], NUM_ENCODERS,
                                (show_psnr ? VPX_CODEC_USE_PSNR : 0), &dsf[0]))
        die_codec(&codec[0], "Failed to initialize encoder");

    /* The extra encoding configuration parameters can be set as follows. */
    /* Set encoding speed */
    for ( i=0; i<NUM_ENCODERS; i++)
    {
        int speed = -6;
        if(vpx_codec_control(&codec[i], VP8E_SET_CPUUSED, speed))
            die_codec(&codec[i], "Failed to set cpu_used");
    }

    /* Set static threshold. */
    for ( i=0; i<NUM_ENCODERS; i++)
    {
        unsigned int static_thresh = 1;
        if(vpx_codec_control(&codec[i], VP8E_SET_STATIC_THRESHOLD, static_thresh))
            die_codec(&codec[i], "Failed to set static threshold");
    }

    /* Set NOISE_SENSITIVITY to do TEMPORAL_DENOISING */
    /* Enable denoising for the highest-resolution encoder. */
    if(vpx_codec_control(&codec[0], VP8E_SET_NOISE_SENSITIVITY, 1))
        die_codec(&codec[0], "Failed to set noise_sensitivity");
    for ( i=1; i< NUM_ENCODERS; i++)
    {
        if(vpx_codec_control(&codec[i], VP8E_SET_NOISE_SENSITIVITY, 0))
            die_codec(&codec[i], "Failed to set noise_sensitivity");
    }


    frame_avail = 1;
    got_data = 0;

    while(frame_avail || got_data)
    {
        vpx_codec_iter_t iter[NUM_ENCODERS]={NULL};
        const vpx_codec_cx_pkt_t *pkt[NUM_ENCODERS];

        flags = 0;
        frame_avail = read_frame_p(infile, &raw[0]);

        if(frame_avail)
        {
            for ( i=1; i<NUM_ENCODERS; i++)
            {
                /*Scale the image down a number of times by downsampling factor*/
                /* FilterMode 1 or 2 give better psnr than FilterMode 0. */
                I420Scale(raw[i-1].planes[VPX_PLANE_Y], raw[i-1].stride[VPX_PLANE_Y],
                          raw[i-1].planes[VPX_PLANE_U], raw[i-1].stride[VPX_PLANE_U],
                          raw[i-1].planes[VPX_PLANE_V], raw[i-1].stride[VPX_PLANE_V],
                          raw[i-1].d_w, raw[i-1].d_h,
                          raw[i].planes[VPX_PLANE_Y], raw[i].stride[VPX_PLANE_Y],
                          raw[i].planes[VPX_PLANE_U], raw[i].stride[VPX_PLANE_U],
                          raw[i].planes[VPX_PLANE_V], raw[i].stride[VPX_PLANE_V],
                          raw[i].d_w, raw[i].d_h, 1);
            }
        }

        /* Encode each frame at multi-levels */
        if(vpx_codec_encode(&codec[0], frame_avail? &raw[0] : NULL,
            frame_cnt, 1, flags, arg_deadline))
            die_codec(&codec[0], "Failed to encode frame");

        for (i=NUM_ENCODERS-1; i>=0 ; i--)
        {
            got_data = 0;

            while( (pkt[i] = vpx_codec_get_cx_data(&codec[i], &iter[i])) )
            {
                got_data = 1;
                switch(pkt[i]->kind) {
                    case VPX_CODEC_CX_FRAME_PKT:
                        write_ivf_frame_header(outfile[i], pkt[i]);
                        (void) fwrite(pkt[i]->data.frame.buf, 1,
                                      pkt[i]->data.frame.sz, outfile[i]);
                    break;
                    case VPX_CODEC_PSNR_PKT:
                        if (show_psnr)
                        {
                            int j;

                            psnr_sse_total[i] += pkt[i]->data.psnr.sse[0];
                            psnr_samples_total[i] += pkt[i]->data.psnr.samples[0];
                            for (j = 0; j < 4; j++)
                            {
                                //fprintf(stderr, "%.3lf ", pkt[i]->data.psnr.psnr[j]);
                                psnr_totals[i][j] += pkt[i]->data.psnr.psnr[j];
                            }
                            psnr_count[i]++;
                        }

                        break;
                    default:
                        break;
                }
                printf(pkt[i]->kind == VPX_CODEC_CX_FRAME_PKT
                       && (pkt[i]->data.frame.flags & VPX_FRAME_IS_KEY)? "K":".");
                fflush(stdout);
            }
        }
        frame_cnt++;
    }
    printf("\n");

    fclose(infile);

    printf("Processed %ld frames.\n",(long int)frame_cnt-1);
    for (i=0; i< NUM_ENCODERS; i++)
    {
        /* Calculate PSNR and print it out */
        if ( (show_psnr) && (psnr_count[i]>0) )
        {
            int j;
            double ovpsnr = sse_to_psnr(psnr_samples_total[i], 255.0,
                                        psnr_sse_total[i]);

            fprintf(stderr, "\n ENC%d PSNR (Overall/Avg/Y/U/V)", i);

            fprintf(stderr, " %.3lf", ovpsnr);
            for (j = 0; j < 4; j++)
            {
                fprintf(stderr, " %.3lf", psnr_totals[i][j]/psnr_count[i]);
            }
        }

        if(vpx_codec_destroy(&codec[i]))
            die_codec(&codec[i], "Failed to destroy codec");

        vpx_img_free(&raw[i]);

        if(!outfile[i])
            continue;

        /* Try to rewrite the file header with the actual frame count */
        if(!fseek(outfile[i], 0, SEEK_SET))
            write_ivf_file_header(outfile[i], &cfg[i], frame_cnt-1);
        fclose(outfile[i]);
    }
    printf("\n");

    return EXIT_SUCCESS;
}
