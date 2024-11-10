#include "internal.h"
#include <signal.h>
#include <sparkey/sparkey.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static Http* server = NULL;

#define MAX_REQUEST_SIZE (1 * 1000 * 1000) // 1 gig

typedef uint64_t paste_key_t;

#define PASTE_KEY_INVALID ((paste_key_t) 0)

#define SERVER_ADDR "http://localhost/"

#define DB_FILE_PREFIX "data"

struct {
    pthread_rwlock_t    lock;
    VirtAlloc           nextKey;
    sparkey_hashreader* hashread;
    sparkey_logiter   * iter;
} db = {0};

typedef struct {
    struct tm creation_date;
    char      tite[64];
    char      data[];
} DbEntry;

static void closeDb();
static sparkey_returncode initDb();
static sparkey_returncode rehashDb(bool rehash);

static const char * mime_text = "text/plain";

static paste_key_t upload(const char * title, HttpPostValue content)
{
    pthread_rwlock_wrlock(&db.lock);

    DbEntry* ent = malloc(sizeof(DbEntry) + content.len);
    if (ent == NULL) {
        pthread_rwlock_unlock(&db.lock);
        return PASTE_KEY_INVALID;
    }

    memset(ent, 0, sizeof(DbEntry));
    strncpy(ent->tite, title, sizeof(ent->tite));
    memcpy(ent->data, content.nt, content.len);

    {time_t now = time(NULL);
     struct tm* p = localtime(&now);
     if(p){ ent->creation_date = *p; }
    }

    paste_key_t* nextkey = (paste_key_t *) db.nextKey.data;
    paste_key_t key = (*nextkey) ++;

    sparkey_returncode skret;

    sparkey_logwriter* wr;
    skret = sparkey_logwriter_append(&wr, DB_FILE_PREFIX ".spl");

    if (skret == SPARKEY_SUCCESS) {
        skret = sparkey_logwriter_put(
                wr,
                sizeof(paste_key_t), (uint8_t*) &key,
                sizeof(DbEntry) + content.len, (uint8_t*) ent);
    }

    free(ent);

    (void) sparkey_logwriter_close(&wr);
    if (skret == SPARKEY_SUCCESS) {
        skret = rehashDb(true);
    }

    pthread_rwlock_unlock(&db.lock);

    if (skret != SPARKEY_SUCCESS) {
        ERRF("sparkey error while uploading: %s", sparkey_errstring(skret));
        return PASTE_KEY_INVALID;
    }

    return key;
}

static struct HttpResponse serve(struct HttpRequest request, void* userdata) {
    (void) userdata;

    struct HttpResponse r = (struct HttpResponse) {
        .status = 404,
        .status_msg = "Not Found",
        .content_type = mime_text,
        .content = "Page not found!",
        .content_size = SIZE_MAX,
        .free_content = false,
    };

    if (!strcmp(request.path, "/"))
        request.path = "/index.html";

    if (request.body_size > MAX_REQUEST_SIZE)
    {
        r.status = 413,
        r.status_msg = "Content Too Large";
        r.content = "Request too large";
    }
    else if (respondWithFile(&r, request.path, NULL, NULL))
    {
    }
    else if (!memcmp(request.path, "/view", 5))
    {
        paste_key_t key = PASTE_KEY_INVALID;

        const char* start = urlGETStart(request.path);
        char *startd;
        if (start && (startd = strdup(start))) {
            struct urlGETParam p;
            urlGETPrepare(&p);
            while (urlGETNext(&p, startd)) {
                if (!strcmp(p.k, "id")) {
                    sscanf(p.v, "%zu", &key);
                    break;
                }
            }
            free(startd);
        }

        if (key == PASTE_KEY_INVALID)
        {
            r.status = 400;
            r.status_msg = "Bad Request";
            r.content_type = mime_text;
            r.content = "invalid / no key specified";
        }
        else 
        {
            pthread_rwlock_rdlock(&db.lock);

            sparkey_returncode sk;

            sk = sparkey_hash_get(db.hashread, (uint8_t*) &key, sizeof(paste_key_t), db.iter);

            if ((sk != SPARKEY_SUCCESS) || (sparkey_logiter_state(db.iter) != SPARKEY_ITER_ACTIVE))
            {
                r.status = 400;
                r.status_msg = "Bad Request";
                r.content_type = mime_text;
                r.content = "Item not found in database";
            }
            else
            {
                size_t datalen = sparkey_logiter_valuelen(db.iter);
                datalen -= sizeof(DbEntry);

                DbEntry header;
                size_t header_len;
                sk = sparkey_logiter_fill_value(
                            db.iter, sparkey_hash_getreader(db.hashread),
                            sizeof(DbEntry), (uint8_t*) &header, &header_len);
                if (header_len != sizeof(DbEntry))
                    sk = SPARKEY_INTERNAL_ERROR;

                uint8_t* valbuf = NULL;
                if (sk == SPARKEY_SUCCESS) {
                    valbuf = malloc(datalen + 1);
                    if (valbuf == NULL) {
                        sk = SPARKEY_INTERNAL_ERROR;
                    } else {
                        size_t actual_valuelen;
                        sk = sparkey_logiter_fill_value(
                                    db.iter, sparkey_hash_getreader(db.hashread),
                                    datalen, valbuf, &actual_valuelen);
                        valbuf[datalen] = '\0';
                    }
                }

                if (sk == SPARKEY_SUCCESS) {
                    if (!memcmp(request.path, "/view-fancy", 11)) {
                        int nok = 0;

                        TemplCtx ctx;
                        templ_init(&ctx);
                        nok |= templ_addVar(&ctx, "paste_title", header.tite);

                        char rawLink[128];
                        snprintf(rawLink, sizeof(rawLink), "%sview?id=%zu", SERVER_ADDR, key);
                        nok |= templ_addVar(&ctx, "paste_raw_link", rawLink);

                        nok |= templ_addVar(&ctx, "paste_content", (char*) valbuf);

                        if (nok || !respondWithFile(&r, "/_fancy_view.html", "text/html", &ctx)) {
                            r.status = 500;
                            r.status_msg = "Internal Server Error";
                            r.content_type = mime_text;
                            r.content = "failed to upload file: Server error";
                        }

                        templ_free(&ctx);
                    } 
                    else {
                        r.status = 200;
                        r.status_msg = "OK";
                        r.content_type = mime_text;
                        r.content =  (char *) valbuf;
                        r.free_content = true;
                        r.content_size = datalen;
                    }
                }
                else {
                    if (valbuf) {
                        free(valbuf);
                    }

                    r.status = 500;
                    r.status_msg = "Internal Server Error";
                    r.content_type = mime_text;
                    r.content = "failed to read file: Server error";
                }
            }

            pthread_rwlock_unlock(&db.lock);
        }
    }
    else if (!memcmp(request.path, "/upload-fancy", 6))
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

            if (status == 0)
            {
                LOGF("Uploading \"%s\"", title.nt);
                paste_key_t key = upload(title.nt, content);

                if (key == PASTE_KEY_INVALID)
                {
                    r.status = 500;
                    r.status_msg = "Internal Server Error";
                    r.content_type = mime_text;
                    r.content = "failed to upload file: Server error";
                }
                else
                {
                    int nok = 0;

                    TemplCtx ctx;
                    templ_init(&ctx);
                    nok |= templ_addVar(&ctx, "paste_title", title.nt);

                    char fancyLink[128];
                    snprintf(fancyLink, sizeof(fancyLink), "%sview-fancy?id=%zu", SERVER_ADDR, key);
                    nok |= templ_addVar(&ctx, "paste_fancy_link", fancyLink);

                    char rawLink[128];
                    snprintf(rawLink, sizeof(rawLink), "%sview?id=%zu", SERVER_ADDR, key);
                    nok |= templ_addVar(&ctx, "paste_raw_link", rawLink);

                    nok |= templ_addVar(&ctx, "paste_delete_link", "not yet implemented");

                    if (nok || !respondWithFile(&r, "/_fancy_upload.html", "text/html", &ctx)) {
                        r.status = 500;
                        r.status_msg = "Internal Server Error";
                        r.content_type = mime_text;
                        r.content = "failed to upload file: Server error";
                    }

                    templ_free(&ctx);
                }
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

static sparkey_returncode rehashDb(bool rehash)
{
    if (db.iter) {
        (void) sparkey_hash_close(&db.hashread);
        (void) sparkey_logiter_close(&db.iter);
    }

    sparkey_returncode skret;

    if (rehash) {
        skret = sparkey_hash_write(DB_FILE_PREFIX ".spi", DB_FILE_PREFIX ".spl", 4);
        if (skret != SPARKEY_SUCCESS) {
            db.hashread = NULL;
            db.iter = NULL;
            return skret;
        }
    }

    skret = sparkey_hash_open(&db.hashread, DB_FILE_PREFIX ".spi", DB_FILE_PREFIX ".spl");
    if (skret != SPARKEY_SUCCESS) {
        db.hashread = NULL;
        db.iter = NULL;
        return skret;
    }

    skret = sparkey_logiter_create(&db.iter, sparkey_hash_getreader(db.hashread));
    if (skret != SPARKEY_SUCCESS) {
        (void) sparkey_hash_close(&db.hashread);
        db.hashread = NULL;
        db.iter = NULL;
        return skret;
    }

    return SPARKEY_SUCCESS;
}

static sparkey_returncode initDb()
{
    bool exists = access(DB_FILE_PREFIX ".spi", F_OK) == 0;

    if (!exists) {
        sparkey_logwriter* wr;
        // TODO: make compression and compression block size configurable
        sparkey_returncode r = sparkey_logwriter_create(&wr, DB_FILE_PREFIX ".spl", SPARKEY_COMPRESSION_ZSTD, 512);
        if (r == SPARKEY_SUCCESS) {
            (void) sparkey_logwriter_close(&wr);
        }
        else return r;
    }

    return rehashDb(!exists);
}

static void closeDb()
{
    (void) sparkey_hash_close(&db.hashread);
    (void) sparkey_logiter_close(&db.iter);
}

int main(int argc, char **argv)
{
    sparkey_returncode skret;

    pthread_rwlock_init(&db.lock, NULL);

    {int code;
     paste_key_t def = 0;
     db.nextKey = virtualMapInit(&code, "nextkey.bin", sizeof(paste_key_t), &def);
     if (code) {
         ERRF("failed to open nextkey.bin");
         pthread_rwlock_destroy(&db.lock);
         return 1;
     }
    }

    // TODO: make compression and compression block size configurable
    skret = initDb();
    if (skret != SPARKEY_SUCCESS) {
        ERRF("sparkey database error: %s", sparkey_errstring(skret));
        pthread_rwlock_destroy(&db.lock);
        virtualUnmap(db.nextKey);
        return 1;
    }

    LOGF("Loaded database");

    HttpCfg cfg = (HttpCfg) {
        .port = 80,
        .reuse = 1,
        .num_threads = 2,
        .con_sleep_us = 1000 * (/*ms*/ 5),
        .max_enq_con = 128,
        .handler = serve,
    };

    server = http_open(cfg, NULL);
    if (server == NULL) {
        ERRF("failed to start HTTP server");
        pthread_rwlock_destroy(&db.lock);
        closeDb();
        virtualUnmap(db.nextKey);
        return 1;
    }

    signal(SIGINT, sigint_handler);

	LOGF("Waiting for clients to connect...");

    while (true)
    {
        http_tick(server);
        if (http_isStopping(server)) {
            LOGF("Waiting...");
            http_wait(server);
            break;
        }
    }

    LOGF("Stopping...");
    http_close(server);
    pthread_rwlock_destroy(&db.lock);
    closeDb();
    virtualUnmap(db.nextKey);
	return 0;
}
