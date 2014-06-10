// #include <strmif.h>
#include "EbmlBufferWriter.h"
#include "EbmlWriter.h"
// #include <cassert>
// #include <limits>
// #include <malloc.h>  //_alloca
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

void Ebml_Write(EbmlGlobal *glob, const void *buffer_in, unsigned long len) {
  unsigned char *src = glob->buf;
  src += glob->offset;
  memcpy(src, buffer_in, len);
  glob->offset += len;
}

static void _Serialize(EbmlGlobal *glob, const unsigned char *p, const unsigned char *q) {
  while (q != p) {
    --q;
    memcpy(&(glob->buf[glob->offset]), q, 1);
    glob->offset++;
  }
}

void Ebml_Serialize(EbmlGlobal *glob, const void *buffer_in, unsigned long len) {
  // assert(buf);

  const unsigned char *const p = (const unsigned char *)(buffer_in);
  const unsigned char *const q = p + len;

  _Serialize(glob, p, q);
}


void Ebml_StartSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc, unsigned long class_id) {
  Ebml_WriteID(glob, class_id);
  ebmlLoc->offset = glob->offset;
  // todo this is always taking 8 bytes, this may need later optimization
  unsigned long long unknownLen =  0x01FFFFFFFFFFFFFFLLU;
  Ebml_Serialize(glob, (void *)&unknownLen, 8); // this is a key that says lenght unknown
}

void Ebml_EndSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc) {
  unsigned long long size = glob->offset - ebmlLoc->offset - 8;
  unsigned long long curOffset = glob->offset;
  glob->offset = ebmlLoc->offset;
  size |=  0x0100000000000000LLU;
  Ebml_Serialize(glob, &size, 8);
  glob->offset = curOffset;
}

