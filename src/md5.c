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

/*
 **********************************************************************
 ** Copyright (C) 1990, RSA Data Security, Inc. All rights reserved. **
 **                                                                  **
 ** License to copy and use this software is granted provided that   **
 ** it is identified as the "RSA Data Security, Inc. MD5 Message     **
 ** Digest Algorithm" in all material mentioning or referencing this **
 ** software or this function.                                       **
 **                                                                  **
 ** License is also granted to make and use derivative works         **
 ** provided that such works are identified as "derived from the RSA **
 ** Data Security, Inc. MD5 Message Digest Algorithm" in all         **
 ** material mentioning or referencing the derived work.             **
 **                                                                  **
 ** RSA Data Security, Inc. makes no representations concerning      **
 ** either the merchantability of this software or the suitability   **
 ** of this software for any particular purpose.  It is provided "as **
 ** is" without express or implied warranty of any kind.             **
 **                                                                  **
 ** These notices must be retained in any copies of any part of this **
 ** documentation and/or software.                                   **
 **********************************************************************
 */

#include <inttypes.h>
#include <stdio.h>
#include "md5.h"

typedef struct {
	uint32_t i[2];
	uint32_t buf[4];
	uint8_t in[64];
	uint8_t *digest;
} MD5_CTX;

#define MD5_INITIAL_BUFF {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}

static void update(MD5_CTX *restrict md_context, const void *restrict in_buf, unsigned int in_len);
static void final(MD5_CTX *md_context);
static void transform(uint32_t *restrict buf, uint32_t *restrict in);

static const uint8_t PADDING[64] = {0x80};

/* F, G and H are basic MD5 functions: selection, majority, parity */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s, ac) \
  {(a) += F ((b), (c), (d)) + (x) + (uint32_t)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) \
  {(a) += G ((b), (c), (d)) + (x) + (uint32_t)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) \
  {(a) += H ((b), (c), (d)) + (x) + (uint32_t)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) \
  {(a) += I ((b), (c), (d)) + (x) + (uint32_t)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }

static void
update(MD5_CTX *restrict md_context, const void *restrict in_buf, unsigned int in_len)
{
	uint32_t in[16];
	const uint8_t *in_buff = in_buf;
	int mdi = (int)((md_context->i[0] >> 3) & 0x3F);

	/* update number of bits */
	if ((md_context->i[0] + ((uint32_t)in_len << 3)) < md_context->i[0])
		md_context->i[1]++;
	md_context->i[0] += ((uint32_t)in_len << 3);
	md_context->i[1] += ((uint32_t)in_len >> 29);

	while (in_len--) {
		/* add new character to buf, increment mdi */
		md_context->in[mdi++] = *in_buff++;

		/* transform if necessary */
		if (mdi == 0x40) {
			for (int i = 0, ii = 0; i < 16; i++, ii += 4)
				in[i] = (((uint32_t)md_context->in[ii+3]) << 24) |
					(((uint32_t)md_context->in[ii+2]) << 16) |
					(((uint32_t)md_context->in[ii+1]) << 8) |
					((uint32_t)md_context->in[ii]);
			transform (md_context->buf, in);
			mdi = 0;
		}
	}
}

static void
final(MD5_CTX *md_context)
{
	uint32_t in[16] = {
		[14] = md_context->i[0],
		[15] = md_context->i[1],
	};

	/* compute number of bytes mod 64 */
	int mdi = (int)((md_context->i[0] >> 3) & 0x3F);

	/* pad out to 56 mod 64 */
	unsigned int pad_len = (mdi < 56) ? (unsigned int)(56 - mdi) : (unsigned int)(120 - mdi);
	update (md_context, PADDING, pad_len);

	/* append length in bits and transform */
	for (int i = 0, ii = 0; i < 14; i++, ii += 4)
		in[i] = (((uint32_t)md_context->in[ii+3]) << 24) |
			(((uint32_t)md_context->in[ii+2]) << 16) |
			(((uint32_t)md_context->in[ii+1]) << 8) |
			((uint32_t)md_context->in[ii]);
	transform (md_context->buf, in);

	/* store buf in digest */
	for (int i=0, ii=0; i<4; i++, ii+=4) {
		md_context->digest[ii] = (uint8_t)(md_context->buf[i] & 0xFF);
		md_context->digest[ii+1] =
			(uint8_t)((md_context->buf[i] >> 8) & 0xFF);
		md_context->digest[ii+2] =
			(uint8_t)((md_context->buf[i] >> 16) & 0xFF);
		md_context->digest[ii+3] =
			(uint8_t)((md_context->buf[i] >> 24) & 0xFF);
	}
}

/* Basic MD5 step. transform buf based on in.
 */
static void
transform(uint32_t *restrict buf, uint32_t *restrict in)
{
	uint32_t a = buf[0], b = buf[1], c = buf[2], d = buf[3];

	/* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
	FF ( a, b, c, d, in[ 0], S11, 3614090360); /* 1 */
	FF ( d, a, b, c, in[ 1], S12, 3905402710); /* 2 */
	FF ( c, d, a, b, in[ 2], S13,  606105819); /* 3 */
	FF ( b, c, d, a, in[ 3], S14, 3250441966); /* 4 */
	FF ( a, b, c, d, in[ 4], S11, 4118548399); /* 5 */
	FF ( d, a, b, c, in[ 5], S12, 1200080426); /* 6 */
	FF ( c, d, a, b, in[ 6], S13, 2821735955); /* 7 */
	FF ( b, c, d, a, in[ 7], S14, 4249261313); /* 8 */
	FF ( a, b, c, d, in[ 8], S11, 1770035416); /* 9 */
	FF ( d, a, b, c, in[ 9], S12, 2336552879); /* 10 */
	FF ( c, d, a, b, in[10], S13, 4294925233); /* 11 */
	FF ( b, c, d, a, in[11], S14, 2304563134); /* 12 */
	FF ( a, b, c, d, in[12], S11, 1804603682); /* 13 */
	FF ( d, a, b, c, in[13], S12, 4254626195); /* 14 */
	FF ( c, d, a, b, in[14], S13, 2792965006); /* 15 */
	FF ( b, c, d, a, in[15], S14, 1236535329); /* 16 */

	/* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
	GG ( a, b, c, d, in[ 1], S21, 4129170786); /* 17 */
	GG ( d, a, b, c, in[ 6], S22, 3225465664); /* 18 */
	GG ( c, d, a, b, in[11], S23,  643717713); /* 19 */
	GG ( b, c, d, a, in[ 0], S24, 3921069994); /* 20 */
	GG ( a, b, c, d, in[ 5], S21, 3593408605); /* 21 */
	GG ( d, a, b, c, in[10], S22,   38016083); /* 22 */
	GG ( c, d, a, b, in[15], S23, 3634488961); /* 23 */
	GG ( b, c, d, a, in[ 4], S24, 3889429448); /* 24 */
	GG ( a, b, c, d, in[ 9], S21,  568446438); /* 25 */
	GG ( d, a, b, c, in[14], S22, 3275163606); /* 26 */
	GG ( c, d, a, b, in[ 3], S23, 4107603335); /* 27 */
	GG ( b, c, d, a, in[ 8], S24, 1163531501); /* 28 */
	GG ( a, b, c, d, in[13], S21, 2850285829); /* 29 */
	GG ( d, a, b, c, in[ 2], S22, 4243563512); /* 30 */
	GG ( c, d, a, b, in[ 7], S23, 1735328473); /* 31 */
	GG ( b, c, d, a, in[12], S24, 2368359562); /* 32 */

	/* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
	HH ( a, b, c, d, in[ 5], S31, 4294588738); /* 33 */
	HH ( d, a, b, c, in[ 8], S32, 2272392833); /* 34 */
	HH ( c, d, a, b, in[11], S33, 1839030562); /* 35 */
	HH ( b, c, d, a, in[14], S34, 4259657740); /* 36 */
	HH ( a, b, c, d, in[ 1], S31, 2763975236); /* 37 */
	HH ( d, a, b, c, in[ 4], S32, 1272893353); /* 38 */
	HH ( c, d, a, b, in[ 7], S33, 4139469664); /* 39 */
	HH ( b, c, d, a, in[10], S34, 3200236656); /* 40 */
	HH ( a, b, c, d, in[13], S31,  681279174); /* 41 */
	HH ( d, a, b, c, in[ 0], S32, 3936430074); /* 42 */
	HH ( c, d, a, b, in[ 3], S33, 3572445317); /* 43 */
	HH ( b, c, d, a, in[ 6], S34,   76029189); /* 44 */
	HH ( a, b, c, d, in[ 9], S31, 3654602809); /* 45 */
	HH ( d, a, b, c, in[12], S32, 3873151461); /* 46 */
	HH ( c, d, a, b, in[15], S33,  530742520); /* 47 */
	HH ( b, c, d, a, in[ 2], S34, 3299628645); /* 48 */

	/* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
	II ( a, b, c, d, in[ 0], S41, 4096336452); /* 49 */
	II ( d, a, b, c, in[ 7], S42, 1126891415); /* 50 */
	II ( c, d, a, b, in[14], S43, 2878612391); /* 51 */
	II ( b, c, d, a, in[ 5], S44, 4237533241); /* 52 */
	II ( a, b, c, d, in[12], S41, 1700485571); /* 53 */
	II ( d, a, b, c, in[ 3], S42, 2399980690); /* 54 */
	II ( c, d, a, b, in[10], S43, 4293915773); /* 55 */
	II ( b, c, d, a, in[ 1], S44, 2240044497); /* 56 */
	II ( a, b, c, d, in[ 8], S41, 1873313359); /* 57 */
	II ( d, a, b, c, in[15], S42, 4264355552); /* 58 */
	II ( c, d, a, b, in[ 6], S43, 2734768916); /* 59 */
	II ( b, c, d, a, in[13], S44, 1309151649); /* 60 */
	II ( a, b, c, d, in[ 4], S41, 4149444226); /* 61 */
	II ( d, a, b, c, in[11], S42, 3174756917); /* 62 */
	II ( c, d, a, b, in[ 2], S43,  718787259); /* 63 */
	II ( b, c, d, a, in[ 9], S44, 3951481745); /* 64 */

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}


void
MD5_to_base_64(char *out, const uint8_t *in)
{
	const char *conv = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	uint32_t c;

	for (uint8_t i=0; i<5; ++i) {
		uint32_t c2;

		c2 = ((uint32_t)in[i*3] << 16) + ((uint32_t)in[i*3+1] << 8) + ((uint32_t)in[i*3+2]);
		sprintf(&out[i*4], "%c%c%c%c", conv[(c2 >> 18) % (1 << 6)], conv[(c2 >> 12) % (1 << 6)], conv[(c2 >> 6) % (1 << 6)], conv[(c2 >> 0) % (1 << 6)]);
	}
	c = (uint32_t)in[15] << 16;
	sprintf(&out[20], "%c%c%s", conv[(c >> 18) % (1 << 6)], conv[(c >> 12) % (1 << 6)], "==");
}

void
MD5_calc_checksum(const void *restrict bytes, size_t len, void *restrict buf)
{
	MD5_CTX md_context = {
		.buf = MD5_INITIAL_BUFF,
		.digest = buf,
	};
	update(&md_context, bytes, len);
	final(&md_context);
}

void
MD5_from_base_16(const char *restrict in, uint8_t *restrict out)
{
	#define FROM_XCHR(c) (c - '0' + (c > '9') * (10 - 'a' + '0'))
	for (int i=0; i < 16; ++i)
		out[i] = (uint8_t)(FROM_XCHR(in[2*i]) << 4 | FROM_XCHR(in[2*i+1]));
	#undef FROM_XCHR
}
