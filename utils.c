#include "internal.h"
#include <ctype.h>

const char* detectMime(const char* path)
{
    const char* lastSlash = strrchr(path, '/');
    if (lastSlash) path = lastSlash + 1;
    const char* ext = strrchr(path, '.');
    if (ext) {
        ext++;

        if (!strcmp(ext, "aac")) return "audio/aac";
        else if (!strcmp(ext, "abw")) return "application/x-abiword";
        else if (!strcmp(ext, "apng")) return "image/apng";
        else if (!strcmp(ext, "arc")) return "application/x-freearc";
        else if (!strcmp(ext, "avif")) return "image/avif";
        else if (!strcmp(ext, "avi")) return "video/x-msvideo";
        else if (!strcmp(ext, "azw")) return "application/vnd.amazon.ebook";
        else if (!strcmp(ext, "bin")) return "application/octet-stream";
        else if (!strcmp(ext, "bmp")) return "image/bmp";
        else if (!strcmp(ext, "bz")) return "application/x-bzip";
        else if (!strcmp(ext, "bz2")) return "application/x-bzip2";
        else if (!strcmp(ext, "cda")) return "application/x-cdf";
        else if (!strcmp(ext, "csh")) return "application/x-csh";
        else if (!strcmp(ext, "css")) return "text/css";
        else if (!strcmp(ext, "csv")) return "text/csv";
        else if (!strcmp(ext, "doc")) return "application/msword";
        else if (!strcmp(ext, "docx")) return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
        else if (!strcmp(ext, "eot")) return "application/vnd.ms-fontobject";
        else if (!strcmp(ext, "epub")) return "application/epub+zip";
        else if (!strcmp(ext, "gz")) return "application/x-gzip.";
        else if (!strcmp(ext, "gif")) return "image/gif";
        else if (!strcmp(ext, "html")) return "text/html";
        else if (!strcmp(ext, "ico")) return "image/vnd.microsoft.icon";
        else if (!strcmp(ext, "ics")) return "text/calendar";
        else if (!strcmp(ext, "jar")) return "application/java-archive";
        else if (!strcmp(ext, "jpeg")) return "image/jpeg";
        else if (!strcmp(ext, "js")) return "text/javascript";
        else if (!strcmp(ext, "json")) return "application/json";
        else if (!strcmp(ext, "mjs")) return "text/javascript";
        else if (!strcmp(ext, "mp3")) return "audio/mpeg";
        else if (!strcmp(ext, "mp4")) return "video/mp4";
        else if (!strcmp(ext, "mpeg")) return "video/mpeg";
        else if (!strcmp(ext, "mpkg")) return "application/vnd.apple.installer+xml";
        else if (!strcmp(ext, "odp")) return "application/vnd.oasis.opendocument.presentation";
        else if (!strcmp(ext, "ods")) return "application/vnd.oasis.opendocument.spreadsheet";
        else if (!strcmp(ext, "odt")) return "application/vnd.oasis.opendocument.text";
        else if (!strcmp(ext, "oga")) return "audio/ogg";
        else if (!strcmp(ext, "ogv")) return "video/ogg";
        else if (!strcmp(ext, "ogx")) return "application/ogg";
        else if (!strcmp(ext, "opus")) return "audio/ogg";
        else if (!strcmp(ext, "otf")) return "font/otf";
        else if (!strcmp(ext, "png")) return "image/png";
        else if (!strcmp(ext, "pdf")) return "application/pdf";
        else if (!strcmp(ext, "php")) return "application/x-httpd-php";
        else if (!strcmp(ext, "ppt")) return "application/vnd.ms-powerpoint";
        else if (!strcmp(ext, "pptx")) return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
        else if (!strcmp(ext, "rar")) return "application/vnd.rar";
        else if (!strcmp(ext, "rtf")) return "application/rtf";
        else if (!strcmp(ext, "sh")) return "application/x-sh";
        else if (!strcmp(ext, "svg")) return "image/svg+xml";
        else if (!strcmp(ext, "tar")) return "application/x-tar";
        else if (!strcmp(ext, "tiff")) return "image/tiff";
        else if (!strcmp(ext, "ts")) return "video/mp2t";
        else if (!strcmp(ext, "ttf")) return "font/ttf";
        else if (!strcmp(ext, "txt")) return "text/plain";
        else if (!strcmp(ext, "vsd")) return "application/vnd.visio";
        else if (!strcmp(ext, "wav")) return "audio/wav";
        else if (!strcmp(ext, "weba")) return "audio/webm";
        else if (!strcmp(ext, "webm")) return "video/webm";
        else if (!strcmp(ext, "webp")) return "image/webp";
        else if (!strcmp(ext, "woff")) return "font/woff";
        else if (!strcmp(ext, "woff2")) return "font/woff2";
        else if (!strcmp(ext, "xhtml")) return "application/xhtml+xml";
        else if (!strcmp(ext, "xls")) return "application/vnd.ms-excel";
        else if (!strcmp(ext, "xlsx")) return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
        else if (!strcmp(ext, "xml")) return "application/xml";
        else if (!strcmp(ext, "zip")) return "application/zip";
        else if (!strcmp(ext, "7z")) return "application/x-7z-compressed";

    }
    return "text/plain";
}

static const char* file_path = "html";
bool respondWithFile(struct HttpResponse* r, const char* filePath, const char* mime)
{
    while (*filePath == '/')
        filePath ++;

    char newpath[128];
    sprintf(newpath, "%s/%s", file_path, filePath);

    if (strstr(newpath, ".."))
        return false;

    if (mime == NULL)
        mime = detectMime(newpath);

    FILE* f = fopen(newpath, "r");
    if (f == NULL) return false;

    bool status = false;

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    char* buf = malloc(len);
    if (buf) {
        fread(buf, 1, len, f);

        r->status = 200;
        r->status_msg = "OK";
        r->content_type = mime;
        r->content = buf;
        r->content_size = len;
        LOGF("content is %zu", len);
        r->free_content = true;
        status = true;
    } else {
        ERRF("oom while loading static file");
    }

    fclose(f);

    return status;
}

// modified from: https://gist.github.com/jmsaavedra/7964251
size_t urldecode2(char *dst, size_t dstlen, const char *src)
{
  size_t olddlen = dstlen;
  if (dstlen == 0) return 0;
  char a, b;
  while (*src) {
    if (!dstlen) break;
    if ((*src == '%') &&
      ((a = src[1]) && (b = src[2])) &&
      (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a')
        a -= 'a'-'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a'-'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16*a+b;
      dstlen--;
      src+=3;
    } 
    else if (*src == '+') {
        src++;
        *dst++ = ' ';
        dstlen--;
    }
    else {
      *dst++ = *src++;
      dstlen--;
    }
  }
  if (dstlen)
    *dst++ = '\0';
  else {
    dst --;
    *dst = '\0';
  }
  dstlen ++;
  return olddlen - dstlen;
}

/** 0 if ok */
int http_reqAsPost(HttpPostData* out, struct HttpRequest* request)
{
    char* bodycpy = malloc(request->body_size + 1);
    if (bodycpy == NULL)
        return 1;

    memcpy(bodycpy, request->body, request->body_size);
    bodycpy[request->body_size] = '\0';

    size_t num_and = 0;
    for (size_t i = 0; i < request->body_size; i ++)
        if (request->body[i] == '&')
            num_and ++;
    out->items = malloc(sizeof(*out->items) * (num_and + 1));

    if (out->items == NULL) {
        free(bodycpy);
        return 1;
    }

    out->_allocptr = bodycpy;

    struct urlGETParam p;
    urlGETPrepare(&p);
    while (urlGETNext(&p, bodycpy)) {
        urldecode2(out->items[out->count].key, 64, p.k);
        size_t n =
            urldecode2(bodycpy, request->body_size - (bodycpy - out->_allocptr), p.v);
        HttpPostValue* v = &out->items[out->count ++].val;
        v->len = n;
        v->nt = bodycpy;
        bodycpy += n;
    }

    return 0;
}

/** 0 if ok */
int http_postGet(HttpPostValue* out, HttpPostData const* data, const char * key)
{
    for (size_t i = 0; i < data->count; i ++) {
        if (!strcmp(data->items[i].key, key)) {
            *out = data->items[i].val;
            return 0;
        }
    }
    return 1;
}

void http_postFree(HttpPostData p)
{
    free(p.items);
    free(p._allocptr);
}
