#include "file_ops.h"
#include "llm_helper.h"
#include "model_loader.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_MODEL_PATH "models/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
#define FALLBACK_MODEL_PATH "/home/pan/work/github_ai/llm_modes/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf"
#define DEFAULT_LOCAL_MODEL_NAME "opencodex-local-coding"
#define DEFAULT_LOCAL_SYSTEM_PROMPT \
  "You are OpenCodex coding assistant. Return direct final answers and accurate code. No internal reasoning."

#define LONGOPT_LOCAL_MODEL 1000
#define LONGOPT_LOCAL_MODEL_NAME 1001
#define LONGOPT_LOCAL_SYSTEM 1002
#define LONGOPT_LOCAL_RECREATE 1003
#define LONGOPT_TIMEOUT 1004

#define MODE_CODEGEN 1
#define MODE_CHAT 2
#define MODE_FILEFORMAT 3
#define MODE_RECONFIGURE 4
#define MODE_EXIT 5

static void print_bar(void) {
  printf("+------------------------------------------------------------+\n");
}

static void print_banner(void) {
  print_bar();
  printf("| OpenCodex Local Terminal                                  |\n");
  printf("| Local GGUF only, interactive coding/chat/file-format      |\n");
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

static const char *best_default_local_model_path(void) {
  if (access(FALLBACK_MODEL_PATH, R_OK) == 0) {
    return FALLBACK_MODEL_PATH;
  }
  return DEFAULT_MODEL_PATH;
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

static int ensure_output_allowed(const char *path, int force, int assume_yes) {
  char question[512];

  if (path == NULL) {
    return 0;
  }

  if (file_exists(path) && !force) {
    snprintf(question, sizeof(question), "Overwrite '%s'?", path);
    if (!ask_confirmation(question, assume_yes)) {
      fprintf(stderr, "Write cancelled\n");
      return 0;
    }
  }

  return 1;
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

static const char *profile_hint(int profile) {
  switch (profile) {
    case 2:
      return "You are a precise coding assistant. Return runnable code and short practical notes.";
    case 3:
      return "You are a formatter assistant. Return clean formatted content without commentary.";
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

static int run_codegen_request(const char *prompt,
                               const char *output,
                               const char *model_name,
                               long timeout_seconds,
                               int dry_run,
                               int force,
                               int assume_yes) {
  char *response;

  if (prompt == NULL || output == NULL) {
    fprintf(stderr, "Missing prompt or output\n");
    return 1;
  }

  if (!ensure_output_allowed(output, force, assume_yes)) {
    return 1;
  }

  response = llm_generate_response(prompt, model_name, timeout_seconds);
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

  printf("Saved: %s\n", output);
  free(response);
  return 0;
}

static int run_chat_loop(const char *model_name, long timeout_seconds) {
  int profile = read_int_default("[ocx] chat profile: 1) general 2) codegen 3) fileformat", 1);
  const char *hint = profile_hint(profile);
  char *line = NULL;
  size_t cap = 0;
  ssize_t nread;

  printf("[ocx] chat started. type 'exit' to leave.\n");

  while (1) {
    char *prompt = NULL;
    char *response = NULL;

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

    prompt = with_hint(hint, line);
    if (prompt == NULL) {
      fprintf(stderr, "Out of memory\n");
      continue;
    }

    response = llm_generate_response(prompt, model_name, timeout_seconds);
    free(prompt);

    if (response == NULL) {
      fprintf(stderr, "Request failed\n");
      continue;
    }

    printf("\n%s\n\n", response);
    free(response);
  }

  free(line);
  return 0;
}

static int run_fileformat_task(const char *model_name,
                               long timeout_seconds,
                               int force,
                               int assume_yes) {
  char *input_path = read_line_prompt("[ocx] input file path: ");
  char *output_path = NULL;
  char *input_data = NULL;
  char *prompt = NULL;
  char *response = NULL;
  size_t input_size = 0;
  int rc = 1;

  if (input_path == NULL || input_path[0] == '\0') {
    fprintf(stderr, "No input path provided\n");
    goto cleanup;
  }

  input_data = file_read_all_mmap(input_path, &input_size);
  if (input_data == NULL) {
    goto cleanup;
  }

  output_path = read_line_default("[ocx] output file path (blank=overwrite input)", input_path);
  if (output_path == NULL || output_path[0] == '\0') {
    free(output_path);
    output_path = strdup(input_path);
  }
  if (output_path == NULL) {
    goto cleanup;
  }

  if (!ensure_output_allowed(output_path, force, assume_yes)) {
    goto cleanup;
  }

  {
    const char *instruction =
        "You are a strict formatter. Return only formatted content with no explanation.\n"
        "Preserve meaning and behavior.\n\n"
        "Content:\n";
    size_t need = strlen(instruction) + input_size + 8;
    prompt = malloc(need);
    if (prompt == NULL) {
      goto cleanup;
    }
    snprintf(prompt, need, "%s%s", instruction, input_data);
  }

  response = llm_generate_response(prompt, model_name, timeout_seconds);
  if (response == NULL) {
    fprintf(stderr, "Formatting request failed\n");
    goto cleanup;
  }

  if (file_write_all_mmap(output_path, response, strlen(response), 1) != 0) {
    fprintf(stderr, "Failed writing '%s'\n", output_path);
    goto cleanup;
  }

  printf("Saved formatted file: %s\n", output_path);
  rc = 0;

cleanup:
  free(input_path);
  free(output_path);
  free(input_data);
  free(prompt);
  free(response);
  return rc;
}

static int prepare_local_model_interactive(char **model_name,
                                           long *timeout_seconds,
                                           int *force,
                                           int *assume_yes) {
  char *model_path = read_line_default("[ocx] local GGUF path", best_default_local_model_path());
  char *name = read_line_default("[ocx] local model name", DEFAULT_LOCAL_MODEL_NAME);
  char *system = read_line_default("[ocx] system prompt", DEFAULT_LOCAL_SYSTEM_PROMPT);
  int recreate = read_int_default("[ocx] recreate model each setup? (1=yes,0=no)", 0);

  if (model_path == NULL || name == NULL || system == NULL) {
    free(model_path);
    free(name);
    free(system);
    return 1;
  }

  if (ensure_local_ollama_model(model_path,
                                name,
                                system,
                                recreate,
                                0.05,
                                0.85,
                                40,
                                7,
                                8192,
                                900) != 0) {
    free(model_path);
    free(name);
    free(system);
    return 1;
  }

  free(*model_name);
  *model_name = name;
  *timeout_seconds = read_long_default("[ocx] request timeout seconds", 1200);
  *force = read_int_default("[ocx] auto-overwrite files? (1=yes,0=no)", 0);
  *assume_yes = read_int_default("[ocx] auto-confirm prompts? (1=yes,0=no)", 0);

  free(model_path);
  free(system);
  return 0;
}

static int run_interactive_menu(void) {
  char *model_name = strdup(DEFAULT_LOCAL_MODEL_NAME);
  long timeout_seconds = 1200;
  int force = 0;
  int assume_yes = 0;

  if (model_name == NULL) {
    return 1;
  }

  print_banner();
  if (prepare_local_model_interactive(&model_name, &timeout_seconds, &force, &assume_yes) != 0) {
    free(model_name);
    return 1;
  }

  while (1) {
    int mode = read_int_default("[ocx] mode: 1) codegen 2) chat 3) fileformat 4) reconfigure 5) exit", MODE_CODEGEN);

    if (mode == MODE_RECONFIGURE) {
      if (prepare_local_model_interactive(&model_name, &timeout_seconds, &force, &assume_yes) != 0) {
        break;
      }
      continue;
    }

    if (mode == MODE_EXIT) {
      break;
    }

    if (mode == MODE_CODEGEN) {
      char *prompt = read_line_prompt("ocx/codegen> prompt: ");
      char *output = read_line_prompt("ocx/codegen> output file: ");
      if (prompt != NULL && output != NULL && prompt[0] != '\0' && output[0] != '\0') {
        (void)run_codegen_request(prompt, output, model_name, timeout_seconds, 0, force, assume_yes);
      } else {
        fprintf(stderr, "Invalid prompt/output\n");
      }
      free(prompt);
      free(output);
      continue;
    }

    if (mode == MODE_CHAT) {
      (void)run_chat_loop(model_name, timeout_seconds);
      continue;
    }

    if (mode == MODE_FILEFORMAT) {
      (void)run_fileformat_task(model_name, timeout_seconds, force, assume_yes);
      continue;
    }

    fprintf(stderr, "Unknown mode\n");
  }

  free(model_name);
  return 0;
}

static void print_usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s --prompt TEXT --output FILE [options]\n"
          "Run without arguments for interactive local-only terminal mode.\n"
          "Options:\n"
          "  --prompt TEXT        Code generation prompt\n"
          "  --output FILE        Output file path\n"
          "  --force              Overwrite output without confirmation\n"
          "  --yes                Auto-confirm prompts\n"
          "  --dry-run            Print output only\n"
          "  --delete FILE        Delete file with confirmation\n"
          "  --local-model PATH   GGUF path for local model creation\n"
          "  --local-model-name N Local model name (default: %s)\n"
          "  --local-system TEXT  System prompt for model creation\n"
          "  --local-recreate     Recreate model even if it exists\n"
          "  --timeout SEC        Request timeout in seconds (default: 1200)\n"
          "  -h, --help           Show help\n",
          argv0,
          DEFAULT_LOCAL_MODEL_NAME);
}

int main(int argc, char **argv) {
  const char *prompt = NULL;
  const char *output = NULL;
  const char *delete_target = NULL;
  const char *local_model_path = NULL;
  const char *local_model_name = DEFAULT_LOCAL_MODEL_NAME;
  const char *local_system_prompt = DEFAULT_LOCAL_SYSTEM_PROMPT;
  int force = 0;
  int assume_yes = 0;
  int dry_run = 0;
  int local_recreate = 0;
  long timeout_seconds = 1200;
  int option_index = 0;
  int c;

  static struct option long_options[] = {
      {"prompt", required_argument, 0, 'p'},
      {"output", required_argument, 0, 'o'},
      {"force", no_argument, 0, 'f'},
      {"yes", no_argument, 0, 'y'},
      {"dry-run", no_argument, 0, 'd'},
      {"delete", required_argument, 0, 'x'},
      {"local-model", required_argument, 0, LONGOPT_LOCAL_MODEL},
      {"local-model-name", required_argument, 0, LONGOPT_LOCAL_MODEL_NAME},
      {"local-system", required_argument, 0, LONGOPT_LOCAL_SYSTEM},
      {"local-recreate", no_argument, 0, LONGOPT_LOCAL_RECREATE},
      {"timeout", required_argument, 0, LONGOPT_TIMEOUT},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  if (argc == 1) {
    return run_interactive_menu();
  }

  while ((c = getopt_long(argc, argv, "hp:o:fydx:", long_options, &option_index)) != -1) {
    switch (c) {
      case 'p':
        prompt = optarg;
        break;
      case 'o':
        output = optarg;
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
      case LONGOPT_LOCAL_MODEL:
        local_model_path = optarg;
        break;
      case LONGOPT_LOCAL_MODEL_NAME:
        local_model_name = optarg;
        break;
      case LONGOPT_LOCAL_SYSTEM:
        local_system_prompt = optarg;
        break;
      case LONGOPT_LOCAL_RECREATE:
        local_recreate = 1;
        break;
      case LONGOPT_TIMEOUT: {
        char *endptr = NULL;
        timeout_seconds = strtol(optarg, &endptr, 10);
        if (endptr == optarg || *endptr != '\0' || timeout_seconds <= 0) {
          fprintf(stderr, "Invalid --timeout value: %s\n", optarg);
          return 1;
        }
        break;
      }
      case 'h':
      default:
        print_usage(argv[0]);
        return c == 'h' ? 0 : 1;
    }
  }

  if (delete_target != NULL) {
    return delete_file_with_confirmation(delete_target, assume_yes);
  }

  if (prompt == NULL || output == NULL) {
    print_usage(argv[0]);
    return 1;
  }

  if (local_model_path == NULL) {
    local_model_path = best_default_local_model_path();
  }

  if (ensure_local_ollama_model(local_model_path,
                                local_model_name,
                                local_system_prompt,
                                local_recreate,
                                0.05,
                                0.85,
                                40,
                                7,
                                8192,
                                900) != 0) {
    return 1;
  }

  return run_codegen_request(prompt,
                             output,
                             local_model_name,
                             timeout_seconds,
                             dry_run,
                             force,
                             assume_yes);
}
