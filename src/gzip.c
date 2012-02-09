#include <stdlib.h>
#include <zlib.h>
#include <assert.h>

void InflateGzip(void *src, size_t srcLen, void *dst, size_t dstLen)
{
	z_stream stream = {
		.avail_in = srcLen,
		.next_in = src,
		.avail_out = dstLen,
		.next_out =  dst,
	};
	inflateInit2(&stream, 15 + 16);
	inflate(&stream, Z_FINISH);
	inflateEnd(&stream);
	assert(!stream.avail_out && !stream.avail_in);
}

void *DeflateGzip(void *src, size_t *len)
{
	z_stream stream = {
		.avail_in = *len,
		.next_in = src,
	};
	deflateInit2(&stream, 9, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
	size_t dstLen = deflateBound(&stream, *len);
	void *dst = malloc(dstLen);
	stream.avail_out = dstLen;
	stream.next_out = dst;
	deflate(&stream, Z_FINISH);
	*len = dstLen - stream.avail_out;
	deflateEnd(&stream);
	return dst;
}
