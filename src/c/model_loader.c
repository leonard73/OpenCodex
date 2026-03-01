#include "model_loader.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

static int run_command(char *const argv[], int quiet) {
  pid_t pid = fork();
  int status = 0;

  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    if (quiet) {
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0) {
        (void)dup2(devnull, STDOUT_FILENO);
        (void)dup2(devnull, STDERR_FILENO);
        close(devnull);
      }
    }
    execvp(argv[0], argv);
    perror("execvp");
    _exit(127);
  }

  if (waitpid(pid, &status, 0) < 0) {
    perror("waitpid");
    return -1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  return -1;
}

static int ollama_model_exists(const char *model_name) {
  char *const cmd[] = {"ollama", "show", (char *)model_name, NULL};
  return run_command(cmd, 1) == 0;
}

int ensure_local_ollama_model(const char *model_path,
                              const char *model_name,
                              const char *system_prompt,
                              int recreate,
                              double temperature,
                              double top_p,
                              int top_k,
                              int seed,
                              int num_ctx,
                              int num_predict) {
  char modelfile_template[] = "/tmp/opencodex_modelfile_XXXXXX";
  int fd = -1;
  FILE *fp = NULL;
  int create_rc = -1;
  char *const ollama_check[] = {"ollama", "--version", NULL};
  char *const ollama_create_prefix[] = {"ollama", "create", (char *)model_name, "-f", modelfile_template, NULL};

  if (model_path == NULL || model_name == NULL || model_path[0] == '\0' || model_name[0] == '\0') {
    fprintf(stderr, "ensure_local_ollama_model: invalid arguments\n");
    return -1;
  }

  if (!model_file_is_readable(model_path)) {
    return -1;
  }

  if (run_command(ollama_check, 1) != 0) {
    fprintf(stderr, "Error: 'ollama' command is required for local GGUF model usage\n");
    return -1;
  }

  if (!recreate && ollama_model_exists(model_name)) {
    return 0;
  }

  fd = mkstemp(modelfile_template);
  if (fd < 0) {
    fprintf(stderr, "Failed to create temporary Modelfile: %s\n", strerror(errno));
    return -1;
  }

  fp = fdopen(fd, "w");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open temporary Modelfile stream: %s\n", strerror(errno));
    close(fd);
    unlink(modelfile_template);
    return -1;
  }

  fprintf(fp, "FROM %s\n", model_path);
  fprintf(fp, "SYSTEM %s\n",
          (system_prompt != NULL && system_prompt[0] != '\0')
              ? system_prompt
              : "You are OpenCodex assistant. Be concise and practical.");
  fprintf(fp, "PARAMETER temperature %.6f\n", temperature);
  fprintf(fp, "PARAMETER top_p %.6f\n", top_p);
  fprintf(fp, "PARAMETER top_k %d\n", top_k);
  fprintf(fp, "PARAMETER seed %d\n", seed);
  fprintf(fp, "PARAMETER num_ctx %d\n", num_ctx);
  fprintf(fp, "PARAMETER num_predict %d\n", num_predict);
  fprintf(fp, "PARAMETER repeat_penalty 1.1\n");

  if (fclose(fp) != 0) {
    fprintf(stderr, "Failed to write temporary Modelfile: %s\n", strerror(errno));
    unlink(modelfile_template);
    return -1;
  }
  fp = NULL;

  fprintf(stderr, "Preparing local model '%s' from '%s'...\n", model_name, model_path);
  create_rc = run_command(ollama_create_prefix, 0);
  unlink(modelfile_template);
  if (create_rc != 0) {
    fprintf(stderr, "Failed to create Ollama local model '%s'\n", model_name);
    return -1;
  }

  return 0;
}
