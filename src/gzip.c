/*
 * Copyright (c) 2013, Thomas Barker
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * It under the terms of the GNU General Public License as published by
 * The Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * Along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdlib.h>
#include <zlib.h>

#include "gzip.h"

void
Gzip_inflate(void *restrict src, size_t src_len, void *restrict dst, size_t dst_len)
{
	z_stream stream = {
		.avail_in = src_len,
		.next_in = src,
		.avail_out = dst_len,
		.next_out =  dst,
	};

	inflateInit2(&stream, 15 + 16);
	inflate(&stream, Z_FINISH);
	inflateEnd(&stream);
	assert(!stream.avail_out && !stream.avail_in);
}

void *
Gzip_deflate(void *src, size_t *len)
{
	z_stream stream = {
		.avail_in = *len,
		.next_in = src,
	};
	size_t dst_len;
	void *dst;

	deflateInit2(&stream, 9, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
	dst_len = deflateBound(&stream, *len);
	dst = malloc(dst_len);
	stream.avail_out = dst_len;
	stream.next_out = dst;
	deflate(&stream, Z_FINISH);
	*len = dst_len - stream.avail_out;
	deflateEnd(&stream);
	return dst;
}
