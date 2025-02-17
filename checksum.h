#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

void generate_crc32_table();
uint32_t crc32(const char *data, size_t length);

#endif
