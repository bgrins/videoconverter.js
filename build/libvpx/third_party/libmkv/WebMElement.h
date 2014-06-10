// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.


#ifndef MKV_CONTEXT_HPP
#define MKV_CONTEXT_HPP 1

// these are helper functions
void writeHeader(EbmlGlobal *ebml);
void writeSegmentInformation(EbmlGlobal *ebml, EbmlLoc *startInfo,
                             unsigned long timeCodeScale, double duration);
// this function is a helper only, it assumes a lot of defaults
void writeVideoTrack(EbmlGlobal *ebml, unsigned int trackNumber,
                     int flagLacing, const char *codecId,
                     unsigned int pixelWidth, unsigned int pixelHeight,
                     double frameRate);
void writeAudioTrack(EbmlGlobal *glob, unsigned int trackNumber,
                     int flagLacing, const char *codecId,
                     double samplingFrequency, unsigned int channels,
                     unsigned char *private, unsigned long privateSize);

void writeSimpleBlock(EbmlGlobal *ebml, unsigned char trackNumber,
                      short timeCode, int isKeyframe,
                      unsigned char lacingFlag, int discardable,
                      unsigned char *data, unsigned long dataLength);

#endif
