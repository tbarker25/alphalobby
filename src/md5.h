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

#ifndef MD5_H
#define MD5_H

#define BASE64_MD5_LENGTH 24
#define BASE16_MD5_LENGTH 32
#define MD5_LENGTH 16
extern __thread uint8_t _md5Checksum[16];

void MD5_calc_checksum(const void *restrict bytes, size_t len, uint8_t *restrict buf);
#define MD5_calc_checksum_unsafe(__bytes, _len) ({MD5_calc_checksum((__bytes), (_len), _md5Checksum); _md5Checksum;})

const char *MD5_to_base_64(const uint8_t *s);
void MD5_from_base_16(const char *restrict in, uint8_t *restrict out);

#define MD5_calc_checksum_base_64(s, len) MD5_to_base_64(MD5_calc_checksum_unsafe(s, len))

#endif /* end of include guard: MD5_H */
