#include "model_loader.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int model_file_is_readable(const char *model_path) {
  struct stat st;

  if (model_path == NULL || model_path[0] == '\0') {
    fprintf(stderr, "Model path is empty\n");
    return 0;
  }

  if (access(model_path, R_OK) != 0) {
    fprintf(stderr, "Model file '%s' is not readable: %s\n", model_path, strerror(errno));
    return 0;
  }

  if (stat(model_path, &st) != 0) {
    fprintf(stderr, "Unable to stat model file '%s': %s\n", model_path, strerror(errno));
    return 0;
  }

  if (!S_ISREG(st.st_mode)) {
    fprintf(stderr, "Model path '%s' is not a regular file\n", model_path);
    return 0;
  }

  return 1;
}
