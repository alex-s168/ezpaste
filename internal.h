#define DEF_LOG
#include "c-http-server/inc/httpserv.h"
#include "c-http-server/inc/get_parameters.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

const char* detectMime(const char* path);
bool respondWithFile(struct HttpResponse* r, const char* filePath, const char* mimeOrAuto);
size_t urldecode2(char *dst, size_t dstlen, const char *src);

#define HTTP_POST_KEY_LEN (32)
typedef struct {
    const char * nt;
    size_t len;
} HttpPostValue;
typedef struct {
    char* _allocptr;

    size_t count;
    struct {
        char key[HTTP_POST_KEY_LEN];
        HttpPostValue val;
    }* items;
} HttpPostData;
/** 0 if ok */
int http_reqAsPost(HttpPostData* out, struct HttpRequest* r);
/** 0 if ok */
int http_postGet(HttpPostValue* out, HttpPostData const* data, const char * key);
void http_postFree(HttpPostData p);

// from: https://github.com/alex-s168/allib/blob/main/kallok/virtual.h

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

typedef struct {
    void  *data;
    size_t size;

#ifdef _WIN32
    HANDLE hFile;
    HANDLE hMap;
#else
    int fd;
#endif
} VirtAlloc;

VirtAlloc virtualMap(int *err, const char *path);
VirtAlloc virtualMapInit(int *err, const char *path, size_t initLen, void* initData);
void virtualUnmap(VirtAlloc alloc);

