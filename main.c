#include "internal.h"
#include <signal.h>
#include <sparkey/sparkey.h>
#include <pthread.h>

static Http* server = NULL;

#define MAX_REQUEST_SIZE (1 * 1000 * 1000) // 1 gig

typedef uint64_t paste_key_t;

#define PASTE_KEY_INVALID ((paste_key_t) 0)

#define SERVER_ADDR "http://localhost/"

struct {
    pthread_rwlock_t   lock;
    sparkey_logwriter* writer;
    sparkey_logreader* reader;
} db;

static paste_key_t upload(const char * title, HttpPostValue content)
{
    pthread_rwlock_wrlock(&db.lock);

    pthread_rwlock_unlock(&db.lock);
}

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
        HttpPostData post;
        if (http_reqAsPost(&post, &request))
        {
            ERRF("oom during processing upload req");
        }
        else 
        {
            int status = 0;

            HttpPostValue title; 
            status += http_postGet(&title, &post, "p_title");
            HttpPostValue content; 
            status += http_postGet(&content, &post, "p_content");

            char* heap = malloc(128);
            if (status == 0 && heap)
            {
                LOGF("uploading \"%s\"", title.nt);
                paste_key_t key = upload(title.nt, content);
                // TODO: cmp with PASTE_KEY_INVALID

                r.status = 200;
                r.status_msg = "OK";
                r.content_type = "text/plain";

                sprintf(heap, "%sview?id=%zu", SERVER_ADDR, key);
                r.content = heap;
                r.free_content = true;
            }
        
            http_postFree(post);
        }
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
    pthread_rwlock_init(&db.lock, NULL);

    // TODO: check the returncodes of sparkey fns
    // TODO: make compression configurable
    sparkey_returncode returncode = sparkey_logwriter_create(&db.writer, "mylog.spl", SPARKEY_COMPRESSION_ZSTD, 0);

    const char *mykey = "mykey";
 * const char *myvalue = "this is my value";
 * sparkey_returncode returncode = sparkey_logwriter_put(mywriter, strlen(mykey), (uint8_t*)mykey, strlen(myvalue), (uint8_t*)myvalue);
 * // TODO: check the returncode
 * \endcode
 * - Close it when you're done:
 * \code
 * sparkey_returncode returncode = sparkey_logwriter_close(&mywriter);
 * // TODO: check the returncode
 * \endcode

    LOGF("Database setup done");

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
    pthread_rwlock_destroy(&db.lock);
	return 0;
}
