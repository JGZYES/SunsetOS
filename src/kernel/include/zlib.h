#pragma once

#include <stddef.h>
#include <stdint.h>

#define Z_DEFLATED 8
#define Z_MAX_WBITS 15
#define Z_DEF_MEM_LEVEL 8

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4

#define Z_DEFAULT_COMPRESSION (-1)
#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2

#define Z_DEFAULT_STRATEGY    0
#define Z_BINARY   0
#define Z_TEXT     1
#define Z_ASCII    Z_TEXT
#define Z_UNKNOWN  2

#define Z_NULL  0

#define MAX_WBITS 15
#define DEF_MEM_LEVEL 8

typedef unsigned char Byte;
typedef uint32_t uLong;
typedef uint32_t uInt;
typedef long unsigned int ULONG;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Byte* next_in;
    uInt avail_in;
    uLong total_in;
    Byte* next_out;
    uInt avail_out;
    uLong total_out;
    char* msg;
    void* state;
    void* workspace;
    int data_type;
    uLong adler;
    uLong reserved;
} z_stream;

typedef z_stream* z_streamp;

unsigned long adler32(unsigned long adler, const Byte* buf, uInt len);
unsigned long adler32_combine(unsigned long, unsigned long, long);
int compress(Byte* dest, uLong* destLen, const Byte* source, uLong sourceLen);
int compress2(Byte* dest, uLong* destLen, const Byte* source, uLong sourceLen, int level);
int uncompress(Byte* dest, uLong* destLen, const Byte* source, uLong sourceLen);
int uncompress2(Byte* dest, uLong* destLen, const Byte* source, uLong* sourceLen);

int deflateInit(z_streamp strm, int level);
int deflateInit2(z_streamp strm, int level, int method, int windowBits, int memLevel, int strategy);
int deflate(z_streamp strm, int flush);
int deflateEnd(z_streamp strm);
int deflateReset(z_streamp strm);
uLong deflateBound(z_streamp strm, uLong sourceLen);

int inflateInit(z_streamp strm);
int inflateInit2(z_streamp strm, int windowBits);
int inflate(z_streamp strm, int flush);
int inflateEnd(z_streamp strm);
int inflateReset(z_streamp strm);

const char* zlibVersion();
uLong crc32(uLong crc, const Byte* buf, uInt len);

#ifdef __cplusplus
}
#endif