#include "llm_helper.h"

#include "http_client.h"
#include "json_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *escape_json_string(const char *input) {
  size_t i;
  size_t out_len = 0;
  char *out;
  char *p;

  if (input == NULL) {
    return NULL;
  }

  for (i = 0; input[i] != '\0'; ++i) {
    switch (input[i]) {
      case '\\':
      case '"':
      case '\n':
      case '\r':
      case '\t':
        out_len += 2;
        break;
      default:
        out_len += 1;
        break;
    }
  }

  out = malloc(out_len + 1);
  if (out == NULL) {
    return NULL;
  }

  p = out;
  for (i = 0; input[i] != '\0'; ++i) {
    switch (input[i]) {
      case '\\':
        *p++ = '\\';
        *p++ = '\\';
        break;
      case '"':
        *p++ = '\\';
        *p++ = '"';
        break;
      case '\n':
        *p++ = '\\';
        *p++ = 'n';
        break;
      case '\r':
        *p++ = '\\';
        *p++ = 'r';
        break;
      case '\t':
        *p++ = '\\';
        *p++ = 't';
        break;
      default:
        *p++ = input[i];
        break;
    }
  }
  *p = '\0';

  return out;
}

char *llm_generate_response(const char *prompt, const char *model_name, const char *ollama_url) {
  const char *resolved_model = model_name != NULL ? model_name : "deepseek-r1:1.5b";
  const char *resolved_url = ollama_url != NULL ? ollama_url : "http://localhost:11434/api/generate";
  char *escaped_prompt = NULL;
  char *payload = NULL;
  char *http_response = NULL;
  char *parsed_response = NULL;
  size_t payload_size;

  if (prompt == NULL || prompt[0] == '\0') {
    fprintf(stderr, "Prompt cannot be empty\n");
    return NULL;
  }

  escaped_prompt = escape_json_string(prompt);
  if (escaped_prompt == NULL) {
    fprintf(stderr, "Failed to escape prompt\n");
    return NULL;
  }

  payload_size = strlen(escaped_prompt) + strlen(resolved_model) + 64;
  payload = malloc(payload_size);
  if (payload == NULL) {
    fprintf(stderr, "Out of memory while building JSON payload\n");
    free(escaped_prompt);
    return NULL;
  }

  snprintf(payload, payload_size,
           "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}",
           resolved_model, escaped_prompt);

  http_response = http_post_json(resolved_url, payload, 180);
  if (http_response == NULL) {
    free(escaped_prompt);
    free(payload);
    return NULL;
  }

  parsed_response = json_extract_response(http_response);

  free(http_response);
  free(escaped_prompt);
  free(payload);
  return parsed_response;
}
