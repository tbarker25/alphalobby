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

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	setmode(STDIN_FILENO, O_BINARY);

	puts("static const uint8_t iconData[] = {");
	uint8_t buf[16384];
	size_t len;
	while ((len = fread(buf, 1, sizeof(buf), stdin))) {
		for (int i=0; i < len / 4; ++i)
			fprintf(stdout, "0x%X, 0x%X, 0x%X, 0x%X, \n",
			buf[i*4+2], buf[i*4+1], buf[i*4], buf[i*4+3]);
	}
	puts("};\n");
	return 0;
}
