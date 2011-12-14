#include <stdio.h>
#include <inttypes.h>

#include <fcntl.h>

int main(int argc, char **argv)
{
	setmode(STDIN_FILENO, O_BINARY);
	setmode(STDOUT_FILENO, O_BINARY);

	uint8_t buff[16384];
	size_t len;
	while ((len = fread(buff, 1, sizeof(buff), stdin))) {
		uint32_t *buff32 = (void *)buff;
		for (int i=0; i < len / 4; ++i)
			buff32[i] = (buff32[i] & 0x000000FF) << 16
					| (buff32[i] & 0x00FF0000) >> 16
					| (buff32[i] & 0xFF00FF00);
		fwrite(buff, 1, len, stdout);
	}
	return 0;
}
