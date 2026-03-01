#include <stdio.h>
#include <string.h>

int main(void) {
  FILE *fp = fopen("/proc/self/status", "r");
  char line[256];

  if (fp == NULL) {
    perror("fopen");
    return 1;
  }

  printf("OpenCodex memory baseline:\n");
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (strncmp(line, "VmSize:", 7) == 0 || strncmp(line, "VmRSS:", 6) == 0 ||
        strncmp(line, "VmPeak:", 7) == 0 || strncmp(line, "VmHWM:", 6) == 0) {
      fputs(line, stdout);
    }
  }

  fclose(fp);
  return 0;
}
