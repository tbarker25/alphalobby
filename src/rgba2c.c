#include <stdio.h>
#include <inttypes.h>

#include <fcntl.h>

int main(int argc, char **argv)
{
	setmode(STDIN_FILENO, O_BINARY);

	puts("static const uint8_t iconData[] = {");
	uint8_t buff[16384];
	size_t len;
	while ((len = fread(buff, 1, sizeof(buff), stdin))) {
		// uint32_t *buff32 = (void *)buff;
		for (int i=0; i < len / 4; ++i)
			fprintf(stdout, "0x%X, 0x%X, 0x%X, 0x%X, \n", 
			buff[i*4+2], buff[i*4+1], buff[i*4], buff[i*4+3]);
			// buff32[i]);
					// (buff32[i] & 0x000000FF) << 16
					// | (buff32[i] & 0x00FF0000) >> 16
					// | (buff32[i] & 0xFF00FF00));
	}
	puts("};\n");
	return 0;
}
