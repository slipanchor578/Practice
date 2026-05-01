#ifndef INTERNAL_H
#define INTERNAL_H

#include <stddef.h>

// リクエスト用
int read_request(int client, char* buf, size_t bufsize);
int normalize_path(const char* path, char* out, size_t outsize);
int check_method(int client, const char* method);
const char* get_host(const char* buf);
void parse_headers(const char* buf);

// レスポンス用
void send_html(int client, const char* status, const char* body);
void send_error(int client, int code, const char* msg);
int send_file(int client, const char* local_path);

// mime用
const char* get_mime_type(const char* path);

#endif