
#include "zlib.h"

#define ALLOC_AND_INFLATE_GZIP(_out, _in, _len)\
	char (_out)[*(uint32_t *)((_in) + (_len) - 4)];\
	InflateGzip(_in, _len, _out, sizeof(_out));

#define GET_DECOMPRESSED_SIZE(_gzBytes, _len) (*(uint32_t *)((_gzBytes) + (_len) - 4))
void InflateGzip(void *src, size_t srcLen, void *dst, size_t dstLen);
void *DeflateGzip(void *src, size_t *len);
