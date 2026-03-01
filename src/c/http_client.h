#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

char *http_post_json(const char *url, const char *json_payload, long timeout_seconds);

#endif
