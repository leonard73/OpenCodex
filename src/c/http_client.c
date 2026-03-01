#include "http_client.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t len;
} response_buffer_t;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t chunk_size = size * nmemb;
  response_buffer_t *buffer = (response_buffer_t *)userp;

  char *grown = realloc(buffer->data, buffer->len + chunk_size + 1);
  if (grown == NULL) {
    return 0;
  }

  buffer->data = grown;
  memcpy(buffer->data + buffer->len, contents, chunk_size);
  buffer->len += chunk_size;
  buffer->data[buffer->len] = '\0';
  return chunk_size;
}

char *http_post_json(const char *url, const char *json_payload, long timeout_seconds) {
  static int curl_initialized = 0;
  CURL *curl = NULL;
  CURLcode result;
  long status_code = 0;
  response_buffer_t buffer = {0};
  struct curl_slist *headers = NULL;

  if (url == NULL || json_payload == NULL) {
    fprintf(stderr, "http_post_json: invalid arguments\n");
    return NULL;
  }

  if (!curl_initialized) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
      fprintf(stderr, "libcurl global init failed\n");
      return NULL;
    }
    curl_initialized = 1;
  }

  curl = curl_easy_init();
  if (curl == NULL) {
    fprintf(stderr, "Failed to initialize CURL handle\n");
    return NULL;
  }

  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(json_payload));
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds > 0 ? timeout_seconds : 120L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

  result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    fprintf(stderr, "HTTP request failed: %s\n", curl_easy_strerror(result));
    free(buffer.data);
    buffer.data = NULL;
  } else {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code < 200 || status_code >= 300) {
      fprintf(stderr, "HTTP request returned status %ld\n", status_code);
      free(buffer.data);
      buffer.data = NULL;
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return buffer.data;
}
