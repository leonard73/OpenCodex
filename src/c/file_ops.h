#ifndef FILE_OPS_H
#define FILE_OPS_H

#include <stddef.h>

char *file_read_all_mmap(const char *path, size_t *out_size);
int file_write_all_mmap(const char *path, const char *data, size_t len, int overwrite);
int file_exists(const char *path);

#endif
