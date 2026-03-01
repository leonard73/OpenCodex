#include "file_ops.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

char *file_read_all_mmap(const char *path, size_t *out_size) {
  int fd;
  struct stat st;
  void *mapped;
  char *data;

  if (path == NULL) {
    fprintf(stderr, "file_read_all_mmap: path is NULL\n");
    return NULL;
  }

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Failed to open '%s' for reading: %s\n", path, strerror(errno));
    return NULL;
  }

  if (fstat(fd, &st) != 0) {
    fprintf(stderr, "Failed to stat '%s': %s\n", path, strerror(errno));
    close(fd);
    return NULL;
  }

  if (st.st_size == 0) {
    data = calloc(1, 1);
    if (out_size != NULL) {
      *out_size = 0;
    }
    close(fd);
    return data;
  }

  mapped = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped == MAP_FAILED) {
    fprintf(stderr, "mmap failed while reading '%s': %s\n", path, strerror(errno));
    close(fd);
    return NULL;
  }

  data = malloc((size_t)st.st_size + 1);
  if (data == NULL) {
    fprintf(stderr, "Out of memory while reading '%s'\n", path);
    munmap(mapped, (size_t)st.st_size);
    close(fd);
    return NULL;
  }

  memcpy(data, mapped, (size_t)st.st_size);
  data[st.st_size] = '\0';
  if (out_size != NULL) {
    *out_size = (size_t)st.st_size;
  }

  munmap(mapped, (size_t)st.st_size);
  close(fd);
  return data;
}

int file_write_all_mmap(const char *path, const char *data, size_t len, int overwrite) {
  int fd;
  int flags;
  void *mapped;

  if (path == NULL || data == NULL) {
    fprintf(stderr, "file_write_all_mmap: invalid arguments\n");
    return -1;
  }

  flags = O_RDWR | O_CREAT;
  if (!overwrite) {
    flags |= O_EXCL;
  }

  fd = open(path, flags, 0644);
  if (fd < 0) {
    fprintf(stderr, "Failed to open '%s' for writing: %s\n", path, strerror(errno));
    return -1;
  }

  if (ftruncate(fd, (off_t)len) != 0) {
    fprintf(stderr, "Failed to resize '%s': %s\n", path, strerror(errno));
    close(fd);
    return -1;
  }

  if (len == 0) {
    close(fd);
    return 0;
  }

  mapped = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED) {
    fprintf(stderr, "mmap failed while writing '%s': %s\n", path, strerror(errno));
    close(fd);
    return -1;
  }

  memcpy(mapped, data, len);
  if (msync(mapped, len, MS_SYNC) != 0) {
    fprintf(stderr, "msync failed for '%s': %s\n", path, strerror(errno));
    munmap(mapped, len);
    close(fd);
    return -1;
  }

  munmap(mapped, len);
  close(fd);
  return 0;
}

int file_exists(const char *path) {
  return path != NULL && access(path, F_OK) == 0;
}
