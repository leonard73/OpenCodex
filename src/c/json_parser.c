#include "json_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#define OPENCODEX_HAS_CJSON 1
#elif __has_include(<cJSON.h>)
#include <cJSON.h>
#define OPENCODEX_HAS_CJSON 1
#else
#define OPENCODEX_HAS_CJSON 0
#endif

#if !OPENCODEX_HAS_CJSON
static char *fallback_extract_response(const char *json_text) {
  const char *needle = "\"response\"";
  const char *key_pos = strstr(json_text, needle);
  const char *colon;
  const char *start;
  const char *p;
  size_t max_len = strlen(json_text) + 1;
  char *out;
  size_t out_len = 0;

  if (key_pos == NULL) {
    fprintf(stderr, "Fallback parser: no 'response' key in JSON\n");
    return NULL;
  }

  colon = strchr(key_pos + strlen(needle), ':');
  if (colon == NULL) {
    fprintf(stderr, "Fallback parser: malformed JSON near 'response'\n");
    return NULL;
  }

  start = strchr(colon, '"');
  if (start == NULL) {
    fprintf(stderr, "Fallback parser: response is not a JSON string\n");
    return NULL;
  }
  start++;

  out = malloc(max_len);
  if (out == NULL) {
    return NULL;
  }

  for (p = start; *p != '\0'; ++p) {
    if (*p == '\\' && p[1] != '\0') {
      ++p;
      switch (*p) {
        case 'n':
          out[out_len++] = '\n';
          break;
        case 'r':
          out[out_len++] = '\r';
          break;
        case 't':
          out[out_len++] = '\t';
          break;
        case '"':
          out[out_len++] = '"';
          break;
        case '\\':
          out[out_len++] = '\\';
          break;
        default:
          out[out_len++] = *p;
          break;
      }
      continue;
    }

    if (*p == '"') {
      out[out_len] = '\0';
      return out;
    }

    out[out_len++] = *p;
  }

  free(out);
  fprintf(stderr, "Fallback parser: unterminated response string\n");
  return NULL;
}
#endif

char *json_extract_response(const char *json_text) {
#if OPENCODEX_HAS_CJSON
  cJSON *root = NULL;
  cJSON *response = NULL;
  char *copy = NULL;

  if (json_text == NULL) {
    fprintf(stderr, "json_extract_response: json_text is NULL\n");
    return NULL;
  }

  root = cJSON_Parse(json_text);
  if (root == NULL) {
    fprintf(stderr, "Failed to parse JSON from Ollama response\n");
    return NULL;
  }

  response = cJSON_GetObjectItemCaseSensitive(root, "response");
  if (!cJSON_IsString(response) || response->valuestring == NULL) {
    fprintf(stderr, "JSON response does not contain string field 'response'\n");
    cJSON_Delete(root);
    return NULL;
  }

  copy = strdup(response->valuestring);
  if (copy == NULL) {
    fprintf(stderr, "Out of memory while copying response text\n");
  }

  cJSON_Delete(root);
  return copy;
#else
  fprintf(stderr, "cJSON headers not found; using fallback parser\n");
  return fallback_extract_response(json_text);
#endif
}
