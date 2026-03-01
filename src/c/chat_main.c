#include "llm_helper.h"
#include "model_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_MODEL_NAME "deepseek-r1:1.5b"
#define DEFAULT_MODEL_PATH "models/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
#define FALLBACK_MODEL_PATH "/home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
#define DEFAULT_URL "http://localhost:11434/api/generate"

static void print_usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--model NAME] [--url URL] [prompt]\n"
          "If no prompt is passed, REPL mode starts.\n",
          argv0);
}

static char *join_args(int argc, char **argv, int start) {
  size_t total = 0;
  int i;
  char *joined;
  char *p;

  for (i = start; i < argc; ++i) {
    total += strlen(argv[i]) + 1;
  }

  if (total == 0) {
    return NULL;
  }

  joined = malloc(total);
  if (joined == NULL) {
    return NULL;
  }

  p = joined;
  for (i = start; i < argc; ++i) {
    size_t len = strlen(argv[i]);
    memcpy(p, argv[i], len);
    p += len;
    if (i != argc - 1) {
      *p++ = ' ';
    }
  }
  *p = '\0';
  return joined;
}

static int run_single_prompt(const char *prompt, const char *model, const char *url) {
  char *response = llm_generate_response(prompt, model, url);
  if (response == NULL) {
    return 1;
  }

  printf("%s\n", response);
  free(response);
  return 0;
}

static int run_repl(const char *model, const char *url) {
  char *line = NULL;
  size_t cap = 0;
  ssize_t nread;

  printf("OpenCodex chat REPL. Type 'exit' or 'quit' to leave.\n");
  while (1) {
    printf("> ");
    fflush(stdout);

    nread = getline(&line, &cap, stdin);
    if (nread < 0) {
      break;
    }

    if (nread > 0 && line[nread - 1] == '\n') {
      line[nread - 1] = '\0';
    }

    if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
      break;
    }

    if (line[0] == '\0') {
      continue;
    }

    if (run_single_prompt(line, model, url) != 0) {
      fprintf(stderr, "Failed to get response from LLM\n");
    }
  }

  free(line);
  return 0;
}

int main(int argc, char **argv) {
  const char *model = DEFAULT_MODEL_NAME;
  const char *url = DEFAULT_URL;
  int i = 1;

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

  while (i < argc) {
    if (strcmp(argv[i], "--model") == 0) {
      if (i + 1 >= argc) {
        print_usage(argv[0]);
        return 1;
      }
      model = argv[i + 1];
      i += 2;
      continue;
    }

    if (strcmp(argv[i], "--url") == 0) {
      if (i + 1 >= argc) {
        print_usage(argv[0]);
        return 1;
      }
      url = argv[i + 1];
      i += 2;
      continue;
    }

    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    }

    break;
  }

  if (i < argc) {
    char *prompt = join_args(argc, argv, i);
    int rc;

    if (prompt == NULL) {
      fprintf(stderr, "Failed to read prompt\n");
      return 1;
    }

    rc = run_single_prompt(prompt, model, url);
    free(prompt);
    return rc;
  }

  return run_repl(model, url);
}
