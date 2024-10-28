#define DEF_LOG
#include "c-http-server/inc/httpserv.h"
#include "c-http-server/src/get_parameters.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

const char* detectMime(const char* path);
bool respondWithFile(struct HttpResponse* r, const char* filePath, const char* mimeOrAuto);
void urldecode2(char *dst, size_t dstlen, const char *src);
