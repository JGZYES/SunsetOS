#include "zlib.h"
#include "libc.h"

extern "C" void* malloc(size_t size);
extern "C" void free(void* ptr);

#define Z_DEFLATED 8
#define MAX_WBITS 15
#define DEF_MEM_LEVEL 8

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES 2

#define BMAX 15
#define HEAP_SIZE (2 * 512)

typedef struct {
    unsigned short* freq;
    unsigned short* code;
    unsigned char* dad;
    unsigned char* len;
} huffman_table;

typedef struct {
    unsigned short l_tree[HEAP_SIZE];
    unsigned short d_tree[64];
    int last_lit;
    int last_dist;
    int lastbits;
    unsigned char l_buf[16384];
    unsigned char outbuf[16384 + 1];
    int outcnt;
    unsigned short bi_buf;
    int bi_valid;
} deflate_state;

typedef struct {
    unsigned char* next_in;
    unsigned int avail_in;
    unsigned long total_in;
    unsigned char* next_out;
    unsigned int avail_out;
    unsigned long total_out;
    char* msg;
    void* state;
    void* workspace;
    int data_type;
    unsigned long adler;
    unsigned long reserved;
    int eof;
    int method;
    int mode;
    unsigned int marker;
} inflate_state;

static const unsigned short base_length[29] = {
    0,1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64,80,96,112,128,160,192,224,0
};

static const unsigned short base_dist[30] = {
    0,1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144,8192,12288,16384,0
};

static const unsigned char order[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

const char* zlibVersion() {
    return "1.2.3";
}

unsigned long adler32(unsigned long adler, const Byte* buf, uInt len) {
    unsigned long s1 = adler & 0xffff;
    unsigned long s2 = (adler >> 16) & 0xffff;
    unsigned int k;

    if (buf == NULL) return 1L;

    while (len > 0) {
        k = len < 5552 ? len : 5552;
        len -= k;
        while (k >= 16) {
            s1 += buf[0]; s2 += s1;
            s1 += buf[1]; s2 += s1;
            s1 += buf[2]; s2 += s1;
            s1 += buf[3]; s2 += s1;
            s1 += buf[4]; s2 += s1;
            s1 += buf[5]; s2 += s1;
            s1 += buf[6]; s2 += s1;
            s1 += buf[7]; s2 += s1;
            s1 += buf[8]; s2 += s1;
            s1 += buf[9]; s2 += s1;
            s1 += buf[10]; s2 += s1;
            s1 += buf[11]; s2 += s1;
            s1 += buf[12]; s2 += s1;
            s1 += buf[13]; s2 += s1;
            s1 += buf[14]; s2 += s1;
            s1 += buf[15]; s2 += s1;
            buf += 16;
            k -= 16;
        }
        while (k--) {
            s1 += *buf++;
            s2 += s1;
        }
        s1 %= 65521;
        s2 %= 65521;
    }
    return (s2 << 16) | s1;
}

unsigned long adler32_combine(unsigned long a1, unsigned long a2, long offset) {
    unsigned long sum1;
    unsigned long sum2;
    unsigned rem;

    rem = (unsigned long)(offset % 65521);
    sum1 = a1 & 0xffff;
    sum2 = rem * (a1 & 0xffff);
    sum1 %= 65521;
    sum2 %= 65521;
    sum1 += (a2 & 0xffff) + 65521 - rem;
    sum2 += ((a2 >> 16) & 0xffff) + 65521 - rem;
    if (sum1 >= 65521) sum1 -= 65521;
    if (sum1 >= 65521) sum1 -= 65521;
    if (sum2 >= (unsigned long)(2 * 65521)) sum2 -= 2 * 65521;
    if (sum2 >= 65521) sum2 -= 65521;
    return sum1 | (sum2 << 16);
}

static void bi_flush(deflate_state* s, unsigned char* out, int* put) {
    if (s->bi_valid > 0) {
        if (*put < 16384) {
            out[(*put)++] = (unsigned char)(s->bi_buf & 0xff);
            if (s->bi_valid > 8) {
                out[(*put)++] = (unsigned char)((s->bi_buf >> 8) & 0xff);
            }
        }
        s->bi_buf = 0;
        s->bi_valid = 0;
    }
}

static void send_bits(deflate_state* s, int val, int width, unsigned char* out, int* put) {
    unsigned int t;
    if (s->bi_valid > (int)(32 - width)) {
        t = (unsigned int)(val & ((1u << s->bi_valid) - 1));
        s->bi_buf |= (unsigned short)(t << s->bi_valid);
        out[(*put)++] = (unsigned char)(s->bi_buf & 0xff);
        out[(*put)++] = (unsigned char)((s->bi_buf >> 8) & 0xff);
        s->bi_buf = (unsigned short)((unsigned int)val >> (32 - s->bi_valid));
        s->bi_valid -= 32 - s->bi_valid;
    } else {
        s->bi_buf |= (unsigned short)((unsigned int)val << s->bi_valid);
        s->bi_valid += width;
    }
}

int compress2(Byte* dest, uLong* destLen, const Byte* source, uLong sourceLen, int level) {
    deflate_state s;
    unsigned int i;
    unsigned int j;

    (void)level;

    if (sourceLen == 0) {
        *destLen = 0;
        return Z_OK;
    }

    if (*destLen == 0) {
        return Z_BUF_ERROR;
    }

    memset(&s, 0, sizeof(s));

    s.l_tree[256 * 2 + 1] = 1;

    for (i = 0; i < 512; i++) {
        s.l_tree[i * 2] = 0;
        s.d_tree[i * 2] = 0;
    }

    for (i = 0; i < sourceLen && i < 16384; i++) {
        s.l_buf[i] = source[i];
    }

    s.last_lit = 0;
    s.last_dist = 0;
    s.lastbits = 0;
    s.outcnt = 0;
    s.bi_buf = 0;
    s.bi_valid = 0;

    send_bits(&s, 2, 2, s.outbuf, &s.outcnt);

    i = 0;
    while (i < sourceLen && s.outcnt < (int)*destLen - 6) {
        unsigned char c = source[i];
        s.l_buf[s.last_lit++] = c;

        send_bits(&s, c, 8, s.outbuf, &s.outcnt);
        i++;

        if (s.last_lit >= 31) {
            break;
        }
    }

    send_bits(&s, 256, 9, s.outbuf, &s.outcnt);
    bi_flush(&s, dest, (int*)destLen);

    if (s.outcnt > (int)*destLen) {
        *destLen = s.outcnt;
        return Z_BUF_ERROR;
    }

    for (j = 0; j < (unsigned int)s.outcnt && j < *destLen; j++) {
        dest[j] = s.outbuf[j];
    }
    *destLen = s.outcnt;

    return Z_OK;
}

int compress(Byte* dest, uLong* destLen, const Byte* source, uLong sourceLen) {
    return compress2(dest, destLen, source, sourceLen, 6);
}

int uncompress2(Byte* dest, uLong* destLen, const Byte* source, uLong* sourceLen) {
    inflate_state s;
    unsigned int i;
    unsigned int total;

    if (sourceLen == NULL || destLen == NULL) {
        return Z_STREAM_ERROR;
    }

    if (*destLen == 0 && *sourceLen > 0) {
        return Z_BUF_ERROR;
    }

    memset(&s, 0, sizeof(s));

    s.method = 0;
    s.eof = 0;
    s.mode = 0;
    s.marker = 0;

    if (*sourceLen < 2) {
        return Z_DATA_ERROR;
    }

    unsigned int header = source[0] | (source[1] << 8);
    if ((header % 31) != 0) {
        return Z_DATA_ERROR;
    }

    if ((header & 0x0f) != 8) {
        return Z_DATA_ERROR;
    }

    s.method = header & 0x0f;
    if (s.method > 9) {
        return Z_DATA_ERROR;
    }

    i = 2;
    total = 0;

    while (i < *sourceLen && total < *destLen) {
        unsigned char c = source[i++];
        dest[total++] = c;
    }

    *destLen = total;
    *sourceLen = i;

    return Z_OK;
}

int uncompress(Byte* dest, uLong* destLen, const Byte* source, uLong sourceLen) {
    uLong len = sourceLen;
    return uncompress2(dest, destLen, source, &len);
}

int deflateInit(z_streamp strm, int level) {
    return deflateInit2(strm, level, 8, MAX_WBITS, DEF_MEM_LEVEL, 0);
}

int deflateInit2(z_streamp strm, int level, int method, int windowBits, int memLevel, int strategy) {
    deflate_state* s;

    (void)level;
    (void)method;
    (void)windowBits;
    (void)memLevel;
    (void)strategy;

    if (strm == NULL) return Z_STREAM_ERROR;

    strm->msg = NULL;

    s = (deflate_state*)malloc(sizeof(deflate_state));
    if (s == NULL) return Z_MEM_ERROR;

    memset(s, 0, sizeof(deflate_state));

    strm->state = (void*)s;

    return Z_OK;
}

int deflateReset(z_streamp strm) {
    deflate_state* s;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;

    s = (deflate_state*)strm->state;

    strm->total_in = strm->total_out = 0;
    strm->msg = NULL;
    strm->data_type = Z_UNKNOWN;

    s->last_lit = 0;
    s->last_dist = 0;
    s->lastbits = 0;
    s->outcnt = 0;
    s->bi_buf = 0;
    s->bi_valid = 0;

    return Z_OK;
}

uLong deflateBound(z_streamp strm, uLong sourceLen) {
    (void)strm;
    return sourceLen + ((sourceLen + 7) >> 3) + ((sourceLen + 63) >> 6) + 11;
}

int deflateEnd(z_streamp strm) {
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;

    free(strm->state);
    strm->state = NULL;

    return Z_OK;
}

int deflate(z_streamp strm, int flush) {
    deflate_state* s;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    if (flush > Z_FINISH || flush < 0) return Z_STREAM_ERROR;

    s = (deflate_state*)strm->state;

    if (strm->next_out == NULL || (strm->next_in == NULL && strm->avail_in != 0)) {
        return Z_STREAM_ERROR;
    }

    if (strm->avail_in == 0 && flush != Z_FINISH) {
        return Z_OK;
    }

    if (flush == Z_FINISH) {
        send_bits(s, 2, 3, s->outbuf, &s->outcnt);
        bi_flush(s, (unsigned char*)strm->next_out, (int*)&strm->total_out);
        return Z_STREAM_END;
    }

    return Z_OK;
}

int inflateInit(z_streamp strm) {
    return inflateInit2(strm, MAX_WBITS);
}

int inflateInit2(z_streamp strm, int windowBits) {
    inflate_state* s;

    (void)windowBits;

    if (strm == NULL) return Z_STREAM_ERROR;

    strm->msg = NULL;

    s = (inflate_state*)malloc(sizeof(inflate_state));
    if (s == NULL) return Z_MEM_ERROR;

    memset(s, 0, sizeof(inflate_state));

    strm->state = (void*)s;

    s->method = 0;
    s->eof = 0;
    s->mode = 0;
    s->marker = 0;

    return Z_OK;
}

int inflateReset(z_streamp strm) {
    inflate_state* s;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;

    s = (inflate_state*)strm->state;

    s->method = 0;
    s->eof = 0;
    s->mode = 0;
    s->marker = 0;

    return Z_OK;
}

int inflateEnd(z_streamp strm) {
    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;

    free(strm->state);
    strm->state = NULL;

    return Z_OK;
}

int inflate(z_streamp strm, int flush) {
    inflate_state* s;

    if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
    if (flush > Z_FINISH || flush < 0) return Z_STREAM_ERROR;

    s = (inflate_state*)strm->state;

    if (s->method == 0) {
        if (strm->avail_in < 2) return Z_BUF_ERROR;
        if (*(unsigned char*)strm->next_in != 0x78) {
            s->mode = 20;
            s->marker = 5;
            return Z_DATA_ERROR;
        }
        strm->avail_in--;
        strm->next_in++;
        s->method = 1;
    }

    if (strm->avail_in == 0) {
        if (flush == Z_FINISH) {
            s->eof = 1;
            return Z_STREAM_END;
        }
        return Z_BUF_ERROR;
    }

    if (flush == Z_FINISH) {
        s->eof = 1;
        return Z_STREAM_END;
    }

    return Z_OK;
}

unsigned long crc32(unsigned long crc, const Byte* buf, uInt len) {
    static unsigned long crc_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x5fdf5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe4635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d8c6, 0x126ba8c0, 0x8bbeb9ea, 0xfcb9897c, 0x62dd1cdf, 0x15da2c49, 0x8cd37df3, 0xfbd44d65,
        0x4db26058, 0x3ab550ce, 0xa3bc0074, 0xd4bb30e2, 0xd1bb6641, 0xa6bc56d7, 0x3fb5076d, 0x48b237fb,
        0xd80d2a6a, 0xaf0a1afc, 0x36034b46, 0x41047bd0, 0xdf60ee73, 0xa867dee5, 0x316e8f5f, 0x4669bfc9,
        0xcb61b23c, 0xbc6682aa, 0x256fd310, 0x5268e386, 0xcc0c7625, 0xbb0b46b3, 0x22621709, 0x5505279f,
        0xc5ba3a0e, 0xb2bd0a98, 0x2bb45b22, 0x5cb36bb4, 0xc2d7fe17, 0xb5d0ce81, 0x2cd99f3b, 0x5bdeafad,
        0xedb88290, 0x9abbf206, 0x03b6e3bc, 0x74b1d32a, 0xead54689, 0x9dd2761f, 0x04db27a5, 0x05dc1733,
        0x95bf4b82, 0xe2b87b14, 0x7bb12aae, 0x0cb61a38, 0x92d28f9b, 0xe5d5bf0d, 0x7cdceeb7, 0x0bdbde21,
        0xf00f92f4, 0x8708a262, 0x1e01f3d8, 0x6906c34e, 0x816e17cd, 0xf6b9275b, 0x6fb076e1, 0x18b74677,
        0x88065be6, 0xff0f6b70, 0x66063aca, 0x51010a5c, 0x8f659fff, 0xf862af69, 0x616bfed3, 0x166cce45,
        0xa00ae378, 0xd70dd3ee, 0x4e048254, 0x3903b2c2, 0xa7672761, 0xd06017f7, 0x4969464d, 0x3e6e76db,
        0xaed16b4a, 0xd9d65bdc, 0x40df0a66, 0x37d83af0, 0xa96baf53, 0xdedf9fc5, 0x47d6ce7f, 0x30d1fee9,
        0xbd9f316, 0xcaded88a, 0x53d39930, 0x24d4a9a6, 0xba0b3c05, 0xcdb70c93, 0x54be5d29, 0x23b96dbf,
        0xb306702e, 0xc40140b8, 0x5d081102, 0x2a0f2194, 0xb46bb437, 0xc36c84a1, 0x5a65d51b, 0x2d62e58d
    };

    if (buf == NULL) return 0L;

    crc = crc ^ 0xffffffff;
    while (len--) {
        crc = crc_table[(crc ^ *buf++) & 0xff] ^ (crc >> 8);
    }
    return crc ^ 0xffffffff;
}