#include "internal.h"
#include <signal.h>

static Http* server = NULL;

#define MAX_REQUEST_SIZE (1 * 1000 * 1000) // 1 gig

static struct HttpResponse serve(struct HttpRequest request, void* userdata) {
    (void) userdata;

    struct HttpResponse r = (struct HttpResponse) {
        .status = 404,
        .status_msg = "Not Found",
        .content_type = "text/plain",
        .content = "Page not found!",
        .content_size = SIZE_MAX,
        .free_content = false,
    };

    if (request.body_size > MAX_REQUEST_SIZE)
    {
        r.status = 413,
        r.status_msg = "Content Too Large";
        r.content = "Request too large";
    }
    else if (respondWithFile(&r, request.path, NULL))
    {
    }
    else if (memcmp(request.path, "/upload-fancy", 6) == 0)
    {
        char* bodycpy = malloc(request.body_size + 1);
        if (bodycpy)
        {
            memcpy(bodycpy, request.body, request.body_size);
            bodycpy[request.body_size] = '\0';

            struct urlGETParam p;
            urlGETPrepare(&p);
            while (urlGETNext(&p, bodycpy)) {
                char key[64];
                urldecode2(key, 64, p.k);
                char* value = request.body; // works because of buf size and we don't need buf anymore afterwards
                urldecode2(value, request.body_size, p.v);

                LOGF("%s=%s", key, value);
            }

            r.status = 200;
            r.status_msg = "OK";
            r.content_type = "text/plain";
            r.content = "hey";
        }

        free(bodycpy);
    }

    if (r.content_size == SIZE_MAX) {
        r.content_size = strlen(r.content);
    }

    return r;
}

static void sigint_handler(int sig) {
    if (server && !http_isStopping(server)) {
        http_slowlyStop(server);
    } else {
        exit(130);
    }
}

int main(int argc, char **argv)
{
    HttpCfg cfg = (HttpCfg) {
        .port = 80,
        .reuse = 1,
        .num_threads = 4,
        .con_sleep_us = 1000 * (/*ms*/ 5),
        .max_enq_con = 128,
        .handler = serve,
    };

    server = http_open(cfg, NULL);

    signal(SIGINT, sigint_handler);

	LOGF("Waiting for clients to connect...");

    while (true)
    {
        http_tick(server);
        if (http_isStopping(server)) {
            LOGF("Stopping...");
            http_wait(server);
            break;
        }
    }

    http_close(server);
	return 0;
}
