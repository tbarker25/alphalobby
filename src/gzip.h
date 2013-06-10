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

#ifndef GZIP_H
#define GZIP_H

#define ALLOC_AND_INFLATE_GZIP(_out, _in, _len)\
	char (_out)[*(uint32_t *)((_in) + (_len) - 4)];\
	Gzip_inflate(_in, _len, _out, sizeof(_out));

void   Gzip_inflate(void *restrict src, size_t srcLen, void *restrict dst, size_t dst_len);
void * Gzip_deflate(void *src, size_t *len);

#endif /* end of include guard: GZIP_H */
