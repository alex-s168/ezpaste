#include "internal.h"

void templ_init(TemplCtx* out)
{
    out->out = NULL;
    out->out_len = 0;
    out->out_cap = 0;
    out->vars_count = 0;
    out->vars_items = NULL;
}

/** 0 = ok */
int templ_addVar(TemplCtx* ctx, const char * key, const char * value)
{
    TemplVar* new = realloc(ctx->vars_items,
                            sizeof(TemplVar) * (ctx->vars_count + 1));
    if (new == NULL) return 1;
    ctx->vars_items = new;
    ctx->vars_items[ctx->vars_count ++] = (TemplVar) { .key = key, .value = value };
    return 0;
}

char * templ_getAndOwnContent(TemplCtx* ctx)
{
    char * p = ctx->out;
    ctx->out = NULL;
    ctx->out_len = 0;
    ctx->out_cap = 0;
    return p;
}

void templ_free(TemplCtx* ctx)
{
    if (ctx->out) free(ctx->out);
    if (ctx->vars_items) free(ctx->vars_items);
    templ_init(ctx);
}

/** 0 = ok */
static int output(TemplCtx* ctx, char c)
{
    if (ctx->out_cap <= ctx->out_len) {
        char* new = realloc(ctx->out, ctx->out_cap + 64);
        if (new == NULL) return 1;
        ctx->out = new;
        ctx->out_cap += 64;
    }

    ctx->out[ctx->out_len ++] = c;

    return 0;
}

static int outputAll(TemplCtx* ctx, const char * str)
{
    for (; *str; str ++)
        if (output(ctx, *str))
            return 1;
    return 0;
}

static const char * getVar(TemplCtx const* ctx, const char * key)
{
    for (size_t i = 0; i < ctx->vars_count; i ++)
        if (!strcmp(ctx->vars_items[i].key, key))
            return ctx->vars_items[i].value;
    return NULL;
}

TemplErr templ_run(TemplCtx* ctx, const char * templ)
{
    for (; *templ; templ ++) {
        if (*templ == '$') {
            templ ++;
            if (*templ == '{') {
                templ ++;
                char* end = strchr(templ, '}');
                if (end == NULL) return TEMPL_UNCLOSED_CURLY;
                size_t len = end - templ;
                if (len == 0) return TEMPL_VAR_NOT_FOUND;
                char cpy[len + 1];
                memcpy(cpy, templ, len);
                cpy[len] = '\0';
                const char * var = getVar(ctx, cpy);
                if (var == NULL) return TEMPL_VAR_NOT_FOUND;
                if (outputAll(ctx, var))
                    return TEMPL_OOM;
                templ = end;
            } else {
                if (output(ctx, '$'))
                    return TEMPL_OOM;
            }
        } else {
            if (output(ctx, *templ))
                return TEMPL_OOM;
        }
    }

    if (output(ctx, '\0'))
        return TEMPL_OOM;

    return TEMPL_OK;
}
