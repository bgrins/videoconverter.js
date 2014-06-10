/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_WEBM_VIDEO_SOURCE_H_
#define TEST_WEBM_VIDEO_SOURCE_H_
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include "third_party/nestegg/include/nestegg/nestegg.h"
#include "test/video_source.h"

namespace libvpx_test {

static int
nestegg_read_cb(void *buffer, size_t length, void *userdata) {
  FILE *f = reinterpret_cast<FILE *>(userdata);

  if (fread(buffer, 1, length, f) < length) {
    if (ferror(f))
      return -1;
    if (feof(f))
      return 0;
  }
  return 1;
}


static int
nestegg_seek_cb(int64_t offset, int whence, void *userdata) {
  FILE *f = reinterpret_cast<FILE *>(userdata);
  switch (whence) {
    case NESTEGG_SEEK_SET:
      whence = SEEK_SET;
      break;
    case NESTEGG_SEEK_CUR:
      whence = SEEK_CUR;
      break;
    case NESTEGG_SEEK_END:
      whence = SEEK_END;
      break;
  };
  return fseek(f, (long)offset, whence) ? -1 : 0;
}


static int64_t
nestegg_tell_cb(void *userdata) {
  FILE *f = reinterpret_cast<FILE *>(userdata);
  return ftell(f);
}


static void
nestegg_log_cb(nestegg *context, unsigned int severity, char const *format,
               ...) {
  va_list ap;

  va_start(ap, format);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}

// This class extends VideoSource to allow parsing of WebM files,
// so that we can do actual file decodes.
class WebMVideoSource : public CompressedVideoSource {
 public:
  explicit WebMVideoSource(const std::string &file_name)
      : file_name_(file_name),
        input_file_(NULL),
        nestegg_ctx_(NULL),
        pkt_(NULL),
        video_track_(0),
        chunk_(0),
        chunks_(0),
        buf_(NULL),
        buf_sz_(0),
        frame_(0),
        end_of_file_(false) {
  }

  virtual ~WebMVideoSource() {
    if (input_file_)
      fclose(input_file_);
    if (nestegg_ctx_ != NULL) {
      if (pkt_ != NULL) {
        nestegg_free_packet(pkt_);
      }
      nestegg_destroy(nestegg_ctx_);
    }
  }

  virtual void Init() {
  }

  virtual void Begin() {
    input_file_ = OpenTestDataFile(file_name_);
    ASSERT_TRUE(input_file_ != NULL) << "Input file open failed. Filename: "
        << file_name_;

    nestegg_io io = {nestegg_read_cb, nestegg_seek_cb, nestegg_tell_cb,
                     input_file_};
    ASSERT_FALSE(nestegg_init(&nestegg_ctx_, io, NULL, -1))
        << "nestegg_init failed";

    unsigned int n;
    ASSERT_FALSE(nestegg_track_count(nestegg_ctx_, &n))
        << "failed to get track count";

    for (unsigned int i = 0; i < n; i++) {
      int track_type = nestegg_track_type(nestegg_ctx_, i);
      ASSERT_GE(track_type, 0) << "failed to get track type";

      if (track_type == NESTEGG_TRACK_VIDEO) {
        video_track_ = i;
        break;
      }
    }

    FillFrame();
  }

  virtual void Next() {
    ++frame_;
    FillFrame();
  }

  void FillFrame() {
    ASSERT_TRUE(input_file_ != NULL);
    if (chunk_ >= chunks_) {
      unsigned int track;

      do {
        /* End of this packet, get another. */
        if (pkt_ != NULL) {
          nestegg_free_packet(pkt_);
          pkt_ = NULL;
        }

        int again = nestegg_read_packet(nestegg_ctx_, &pkt_);
        ASSERT_GE(again, 0) << "nestegg_read_packet failed";
        if (!again) {
          end_of_file_ = true;
          return;
        }

        ASSERT_FALSE(nestegg_packet_track(pkt_, &track))
            << "nestegg_packet_track failed";
      } while (track != video_track_);

      ASSERT_FALSE(nestegg_packet_count(pkt_, &chunks_))
          << "nestegg_packet_count failed";
      chunk_ = 0;
    }

    ASSERT_FALSE(nestegg_packet_data(pkt_, chunk_, &buf_, &buf_sz_))
        << "nestegg_packet_data failed";
    chunk_++;
  }

  virtual const uint8_t *cxdata() const {
    return end_of_file_ ? NULL : buf_;
  }
  virtual size_t frame_size() const { return buf_sz_; }
  virtual unsigned int frame_number() const { return frame_; }

 protected:
  std::string file_name_;
  FILE *input_file_;
  nestegg *nestegg_ctx_;
  nestegg_packet *pkt_;
  unsigned int video_track_;
  unsigned int chunk_;
  unsigned int chunks_;
  uint8_t *buf_;
  size_t buf_sz_;
  unsigned int frame_;
  bool end_of_file_;
};

}  // namespace libvpx_test

#endif  // TEST_WEBM_VIDEO_SOURCE_H_
