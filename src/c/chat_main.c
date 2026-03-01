#include "llm_helper.h"
#include "model_loader.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_MODEL_PATH "models/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
#define FALLBACK_MODEL_PATH "/home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
#define DEFAULT_LOCAL_MODEL_NAME "opencodex-local-chat"
#define DEFAULT_LOCAL_SYSTEM_PROMPT \
  "You are OpenCodex chat assistant. Be natural and concise. Do not output internal reasoning."

#define LONGOPT_LOCAL_MODEL 1000
#define LONGOPT_LOCAL_MODEL_NAME 1001
#define LONGOPT_LOCAL_SYSTEM 1002
#define LONGOPT_LOCAL_RECREATE 1003
#define LONGOPT_TIMEOUT 1004
#define LONGOPT_PROFILE 1005

#define PROFILE_GENERAL 1
#define PROFILE_CODEGEN 2
#define PROFILE_FILEFORMAT 3

static void print_bar(void) {
  printf("+------------------------------------------------------------+\n");
}

static void print_banner(void) {
  print_bar();
  printf("| OpenCodex Chat Terminal                                   |\n");
  printf("| Local model chat with selectable profile                  |\n");
  print_bar();
}

static void strip_newline(char *s) {
  if (s != NULL) {
    s[strcspn(s, "\r\n")] = '\0';
  }
}

static char *read_line_prompt(const char *prompt) {
  char *line = NULL;
  size_t cap = 0;
  ssize_t nread;

  if (prompt != NULL) {
    printf("%s", prompt);
    fflush(stdout);
  }

  nread = getline(&line, &cap, stdin);
  if (nread < 0) {
    free(line);
    return NULL;
  }

  strip_newline(line);
  return line;
}

static char *read_line_default(const char *prompt, const char *default_value) {
  char msg[512];
  char *line;

  if (default_value != NULL && default_value[0] != '\0') {
    snprintf(msg, sizeof(msg), "%s [%s]: ", prompt, default_value);
  } else {
    snprintf(msg, sizeof(msg), "%s: ", prompt);
  }

  line = read_line_prompt(msg);
  if (line == NULL) {
    return NULL;
  }

  if (line[0] == '\0' && default_value != NULL) {
    free(line);
    return strdup(default_value);
  }

  return line;
}

static int read_int_default(const char *prompt, int default_value) {
  char buf[32];
  char *line;
  char *endptr = NULL;
  long parsed;

  snprintf(buf, sizeof(buf), "%d", default_value);
  line = read_line_default(prompt, buf);
  if (line == NULL) {
    return default_value;
  }

  parsed = strtol(line, &endptr, 10);
  if (endptr == line || *endptr != '\0') {
    free(line);
    return default_value;
  }

  free(line);
  return (int)parsed;
}

static long read_long_default(const char *prompt, long default_value) {
  char buf[32];
  char *line;
  char *endptr = NULL;
  long parsed;

  snprintf(buf, sizeof(buf), "%ld", default_value);
  line = read_line_default(prompt, buf);
  if (line == NULL) {
    return default_value;
  }

  parsed = strtol(line, &endptr, 10);
  if (endptr == line || *endptr != '\0' || parsed <= 0) {
    free(line);
    return default_value;
  }

  free(line);
  return parsed;
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

static const char *best_default_local_model_path(void) {
  if (access(FALLBACK_MODEL_PATH, R_OK) == 0) {
    return FALLBACK_MODEL_PATH;
  }
  return DEFAULT_MODEL_PATH;
}

static const char *profile_hint(int profile) {
  switch (profile) {
    case PROFILE_CODEGEN:
      return "You are a code generation assistant. Return runnable code and short practical notes.";
    case PROFILE_FILEFORMAT:
      return "You are a file formatting assistant. Return clean formatted content only.";
    default:
      return "";
  }
}

static char *with_hint(const char *hint, const char *input) {
  size_t need;
  char *out;

  if (hint == NULL || hint[0] == '\0') {
    return strdup(input);
  }

  need = strlen(hint) + strlen(input) + 32;
  out = malloc(need);
  if (out == NULL) {
    return NULL;
  }

  snprintf(out, need, "%s\n\nUser request:\n%s", hint, input);
  return out;
}

static int parse_profile_value(const char *value) {
  if (value == NULL) {
    return PROFILE_GENERAL;
  }
  if (strcmp(value, "general") == 0 || strcmp(value, "1") == 0) {
    return PROFILE_GENERAL;
  }
  if (strcmp(value, "codegen") == 0 || strcmp(value, "2") == 0) {
    return PROFILE_CODEGEN;
  }
  if (strcmp(value, "fileformat") == 0 || strcmp(value, "3") == 0) {
    return PROFILE_FILEFORMAT;
  }
  return PROFILE_GENERAL;
}

static int run_single_prompt(const char *prompt,
                             const char *model_name,
                             long timeout_seconds,
                             int profile) {
  const char *hint = profile_hint(profile);
  char *effective_prompt = with_hint(hint, prompt);
  char *response;

  if (effective_prompt == NULL) {
    fprintf(stderr, "Out of memory\n");
    return 1;
  }

  response = llm_generate_response(effective_prompt, model_name, timeout_seconds);
  free(effective_prompt);
  if (response == NULL) {
    return 1;
  }

  printf("%s\n", response);
  free(response);
  return 0;
}

static int run_repl(const char *model_name, long timeout_seconds, int profile) {
  char *line = NULL;
  size_t cap = 0;
  ssize_t nread;

  printf("[ocx] chat profile: %s\n",
         profile == PROFILE_CODEGEN ? "codegen" :
         (profile == PROFILE_FILEFORMAT ? "fileformat" : "general"));
  printf("[ocx] type 'exit' to leave.\n");

  while (1) {
    printf("ocx/chat> ");
    fflush(stdout);

    nread = getline(&line, &cap, stdin);
    if (nread < 0) {
      break;
    }

    strip_newline(line);
    if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
      break;
    }

    if (line[0] == '\0') {
      continue;
    }

    if (run_single_prompt(line, model_name, timeout_seconds, profile) != 0) {
      fprintf(stderr, "Request failed\n");
    }
  }

  free(line);
  return 0;
}

static int interactive_setup(char **model_name,
                             long *timeout_seconds,
                             int *profile,
                             const char **local_model_path,
                             const char **local_system,
                             int *local_recreate) {
  char *path = read_line_default("[ocx] local GGUF path", best_default_local_model_path());
  char *name = read_line_default("[ocx] local model name", DEFAULT_LOCAL_MODEL_NAME);
  char *system = read_line_default("[ocx] system prompt", DEFAULT_LOCAL_SYSTEM_PROMPT);
  int recreate = read_int_default("[ocx] recreate model each setup? (1=yes,0=no)", 0);

  if (path == NULL || name == NULL || system == NULL) {
    free(path);
    free(name);
    free(system);
    return 1;
  }

  *local_model_path = path;
  *local_system = system;
  *local_recreate = recreate;

  free(*model_name);
  *model_name = name;
  *timeout_seconds = read_long_default("[ocx] request timeout seconds", 1200);
  *profile = read_int_default("[ocx] profile: 1) general 2) codegen 3) fileformat", PROFILE_GENERAL);
  if (*profile < PROFILE_GENERAL || *profile > PROFILE_FILEFORMAT) {
    *profile = PROFILE_GENERAL;
  }

  return 0;
}

static void print_usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [options] [prompt]\n"
          "Run without arguments for interactive local-only setup + chat REPL.\n"
          "Options:\n"
          "  --timeout SEC        Request timeout in seconds (default: 1200)\n"
          "  --profile NAME       general|codegen|fileformat\n"
          "  --local-model PATH   GGUF path for local model creation\n"
          "  --local-model-name N Local model name (default: %s)\n"
          "  --local-system TEXT  System prompt for model creation\n"
          "  --local-recreate     Recreate model even if it exists\n"
          "  -h, --help           Show help\n",
          argv0,
          DEFAULT_LOCAL_MODEL_NAME);
}

int main(int argc, char **argv) {
  char *model_name = strdup(DEFAULT_LOCAL_MODEL_NAME);
  const char *local_model_path = NULL;
  const char *local_system = DEFAULT_LOCAL_SYSTEM_PROMPT;
  int local_recreate = 0;
  long timeout_seconds = 1200;
  int profile = PROFILE_GENERAL;
  int option_index = 0;
  int c;

  static struct option long_options[] = {
      {"timeout", required_argument, 0, LONGOPT_TIMEOUT},
      {"profile", required_argument, 0, LONGOPT_PROFILE},
      {"local-model", required_argument, 0, LONGOPT_LOCAL_MODEL},
      {"local-model-name", required_argument, 0, LONGOPT_LOCAL_MODEL_NAME},
      {"local-system", required_argument, 0, LONGOPT_LOCAL_SYSTEM},
      {"local-recreate", no_argument, 0, LONGOPT_LOCAL_RECREATE},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  if (model_name == NULL) {
    return 1;
  }

  while ((c = getopt_long(argc, argv, "h", long_options, &option_index)) != -1) {
    switch (c) {
      case LONGOPT_TIMEOUT: {
        char *endptr = NULL;
        timeout_seconds = strtol(optarg, &endptr, 10);
        if (endptr == optarg || *endptr != '\0' || timeout_seconds <= 0) {
          fprintf(stderr, "Invalid --timeout value: %s\n", optarg);
          free(model_name);
          return 1;
        }
        break;
      }
      case LONGOPT_PROFILE:
        profile = parse_profile_value(optarg);
        break;
      case LONGOPT_LOCAL_MODEL:
        local_model_path = optarg;
        break;
      case LONGOPT_LOCAL_MODEL_NAME:
        free(model_name);
        model_name = strdup(optarg);
        if (model_name == NULL) {
          return 1;
        }
        break;
      case LONGOPT_LOCAL_SYSTEM:
        local_system = optarg;
        break;
      case LONGOPT_LOCAL_RECREATE:
        local_recreate = 1;
        break;
      case 'h':
      default:
        print_usage(argv[0]);
        free(model_name);
        return c == 'h' ? 0 : 1;
    }
  }

  if (argc == 1) {
    print_banner();
    if (interactive_setup(&model_name,
                          &timeout_seconds,
                          &profile,
                          &local_model_path,
                          &local_system,
                          &local_recreate) != 0) {
      free(model_name);
      return 1;
    }
  }

  if (local_model_path == NULL) {
    local_model_path = best_default_local_model_path();
  }

  if (ensure_local_ollama_model(local_model_path,
                                model_name,
                                local_system,
                                local_recreate,
                                0.2,
                                0.9,
                                40,
                                42,
                                4096,
                                700) != 0) {
    free(model_name);
    return 1;
  }

  if (optind < argc) {
    char *prompt = join_args(argc, argv, optind);
    int rc;
    if (prompt == NULL) {
      free(model_name);
      return 1;
    }
    rc = run_single_prompt(prompt, model_name, timeout_seconds, profile);
    free(prompt);
    free(model_name);
    return rc;
  }

  c = run_repl(model_name, timeout_seconds, profile);
  free(model_name);
  return c;
}
