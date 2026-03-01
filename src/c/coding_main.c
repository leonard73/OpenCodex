#include "file_ops.h"
#include "llm_helper.h"
#include "model_loader.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_MODEL_NAME "deepseek-r1:1.5b"
#define DEFAULT_MODEL_PATH "models/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
#define FALLBACK_MODEL_PATH "/home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
#define DEFAULT_URL "http://localhost:11434/api/generate"

static void print_usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s --prompt TEXT --output FILE [options]\n"
          "Options:\n"
          "  --model NAME         Ollama model name (default: %s)\n"
          "  --url URL            Ollama endpoint (default: %s)\n"
          "  --force              Allow overwrite without existence check\n"
          "  --yes                Skip interactive confirmations\n"
          "  --dry-run            Print output only, do not write file\n"
          "  --delete FILE        Delete target file with confirmation\n"
          "  -h, --help           Show help\n",
          argv0,
          DEFAULT_MODEL_NAME,
          DEFAULT_URL);
}

static int ask_confirmation(const char *question, int assume_yes) {
  char answer[16];

  if (assume_yes) {
    return 1;
  }

  printf("%s [y/N]: ", question);
  fflush(stdout);

  if (fgets(answer, sizeof(answer), stdin) == NULL) {
    return 0;
  }

  return answer[0] == 'y' || answer[0] == 'Y';
}

static int delete_file_with_confirmation(const char *path, int assume_yes) {
  char question[512];

  if (path == NULL) {
    return 0;
  }

  if (!file_exists(path)) {
    fprintf(stderr, "Delete target '%s' does not exist\n", path);
    return 1;
  }

  snprintf(question, sizeof(question), "Delete '%s'?", path);
  if (!ask_confirmation(question, assume_yes)) {
    fprintf(stderr, "Deletion cancelled\n");
    return 1;
  }

  if (unlink(path) != 0) {
    perror("unlink");
    return 1;
  }

  printf("Deleted %s\n", path);
  return 0;
}

int main(int argc, char **argv) {
  const char *prompt = NULL;
  const char *output = NULL;
  const char *model = DEFAULT_MODEL_NAME;
  const char *url = DEFAULT_URL;
  const char *delete_target = NULL;
  int force = 0;
  int assume_yes = 0;
  int dry_run = 0;
  int option_index = 0;
  int c;
  char *response;

  static struct option long_options[] = {
      {"prompt", required_argument, 0, 'p'},
      {"output", required_argument, 0, 'o'},
      {"model", required_argument, 0, 'm'},
      {"url", required_argument, 0, 'u'},
      {"force", no_argument, 0, 'f'},
      {"yes", no_argument, 0, 'y'},
      {"dry-run", no_argument, 0, 'd'},
      {"delete", required_argument, 0, 'x'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  while ((c = getopt_long(argc, argv, "hp:o:m:u:fydx:", long_options, &option_index)) != -1) {
    switch (c) {
      case 'p':
        prompt = optarg;
        break;
      case 'o':
        output = optarg;
        break;
      case 'm':
        model = optarg;
        break;
      case 'u':
        url = optarg;
        break;
      case 'f':
        force = 1;
        break;
      case 'y':
        assume_yes = 1;
        break;
      case 'd':
        dry_run = 1;
        break;
      case 'x':
        delete_target = optarg;
        break;
      case 'h':
      default:
        print_usage(argv[0]);
        return c == 'h' ? 0 : 1;
    }
  }

  if (access(DEFAULT_MODEL_PATH, R_OK) == 0) {
    (void)model_file_is_readable(DEFAULT_MODEL_PATH);
  } else if (access(FALLBACK_MODEL_PATH, R_OK) == 0) {
    (void)model_file_is_readable(FALLBACK_MODEL_PATH);
  } else {
    (void)model_file_is_readable(DEFAULT_MODEL_PATH);
    fprintf(stderr,
            "Warning: local GGUF model file check failed. 'make download' can place it at %s\n"
            "or you can place it at %s\n",
            DEFAULT_MODEL_PATH,
            FALLBACK_MODEL_PATH);
  }

  if (delete_target != NULL) {
    return delete_file_with_confirmation(delete_target, assume_yes);
  }

  if (prompt == NULL || output == NULL) {
    print_usage(argv[0]);
    return 1;
  }

  if (file_exists(output) && !force) {
    char question[512];
    snprintf(question, sizeof(question), "Overwrite '%s'?", output);
    if (!ask_confirmation(question, assume_yes)) {
      fprintf(stderr, "Write cancelled\n");
      return 1;
    }
  }

  response = llm_generate_response(prompt, model, url);
  if (response == NULL) {
    fprintf(stderr, "Failed to generate content\n");
    return 1;
  }

  if (dry_run) {
    printf("%s\n", response);
    free(response);
    return 0;
  }

  if (file_write_all_mmap(output, response, strlen(response), 1) != 0) {
    fprintf(stderr, "Failed to write output file '%s'\n", output);
    free(response);
    return 1;
  }

  printf("Wrote generated content to %s\n", output);
  free(response);
  return 0;
}
