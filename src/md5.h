#pragma once
#define BASE64_MD5_LENGTH 24
#define BASE16_MD5_LENGTH 32
#define MD5_LENGTH 16
extern __thread uint8_t _md5Checksum[16];

void GetMD5Sum(const void *restrict bytes, size_t len, uint8_t *restrict buff);
#define GetMD5Sum_unsafe(_bytes, _len) ({GetMD5Sum((_bytes), (_len), _md5Checksum); _md5Checksum;})

const char *ToBase64(const uint8_t *s);
void FromBase16(const char *restrict in, uint8_t *restrict out);

#define GetBase64MD5sum(s, len) ToBase64(GetMD5Sum_unsafe(s, len))


