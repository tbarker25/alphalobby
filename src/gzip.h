#define ALLOC_AND_INFLATE_GZIP(_out, _in, _len)\
	char (_out)[*(uint32_t *)((_in) + (_len) - 4)];\
	InflateGzip(_in, _len, _out, sizeof(_out));

void InflateGzip(void *restrict src, size_t srcLen, void *restrict dst, size_t dstLen);
void *DeflateGzip(void *src, size_t *len);
