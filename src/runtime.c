#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
extern __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
#else
#include <unistd.h>
#endif

#include "love.h"

typedef enum {
    VALUE_NULL,
    VALUE_INT,
    VALUE_BOOL,
    VALUE_STRING,
    VALUE_LIST,
    VALUE_MAP
} ValueType;

typedef struct ListData ListData;
typedef struct MapData MapData;

typedef struct {
    ValueType type;
    long int_value;
    int bool_value;
    char *string_value;
    ListData *list_value;
    MapData *map_value;
} Value;

typedef struct {
    char *key;
    Value value;
} MapEntry;

struct ListData {
    int refcount;
    Value *items;
    size_t count;
    size_t cap;
};

struct MapData {
    int refcount;
    MapEntry *entries;
    size_t count;
    size_t cap;
};

typedef struct {
    char name[64];
    Value value;
    int is_const;
} Variable;

typedef struct Env {
    struct Env *parent;
    Variable vars[256];
    size_t count;
} Env;

typedef struct {
    char name[64];
    Node *decl;
} FunctionDef;

typedef struct {
    FunctionDef defs[128];
    size_t count;
} FunctionTable;

typedef struct {
    RuntimeConfig config;
    FunctionTable functions;
    int should_stop;
    int rng_seeded;
} RuntimeState;

typedef struct {
    int error;
    int has_return;
    Value return_value;
} ExecResult;

static char *xstrdup(const char *s) {
    size_t n;
    char *out;

    if (!s) {
        out = (char *)malloc(1);
        if (!out) {
            fprintf(stderr, "[lovelang] OOM\n");
            exit(1);
        }
        out[0] = '\0';
        return out;
    }

    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) {
        fprintf(stderr, "[lovelang] OOM\n");
        exit(1);
    }

    memcpy(out, s, n + 1);
    return out;
}

static void value_free(Value *v);

static ListData *list_data_new(void) {
    ListData *list = (ListData *)calloc(1, sizeof(ListData));
    if (!list) {
        fprintf(stderr, "[lovelang] OOM\n");
        exit(1);
    }
    list->refcount = 1;
    return list;
}

static MapData *map_data_new(void) {
    MapData *map = (MapData *)calloc(1, sizeof(MapData));
    if (!map) {
        fprintf(stderr, "[lovelang] OOM\n");
        exit(1);
    }
    map->refcount = 1;
    return map;
}

static void list_data_retain(ListData *list) {
    if (list) list->refcount++;
}

static void map_data_retain(MapData *map) {
    if (map) map->refcount++;
}

static void list_data_release(ListData *list) {
    size_t i;
    if (!list) return;
    list->refcount--;
    if (list->refcount > 0) return;

    for (i = 0; i < list->count; i++) {
        value_free(&list->items[i]);
    }
    free(list->items);
    free(list);
}

static void map_data_release(MapData *map) {
    size_t i;
    if (!map) return;
    map->refcount--;
    if (map->refcount > 0) return;

    for (i = 0; i < map->count; i++) {
        free(map->entries[i].key);
        value_free(&map->entries[i].value);
    }
    free(map->entries);
    free(map);
}

static Value value_null(void) {
    Value v;
    v.type = VALUE_NULL;
    v.int_value = 0;
    v.bool_value = 0;
    v.string_value = NULL;
    v.list_value = NULL;
    v.map_value = NULL;
    return v;
}

static Value value_int(long n) {
    Value v = value_null();
    v.type = VALUE_INT;
    v.int_value = n;
    return v;
}

static Value value_bool(int b) {
    Value v = value_null();
    v.type = VALUE_BOOL;
    v.bool_value = b ? 1 : 0;
    return v;
}

static Value value_string(const char *s) {
    Value v = value_null();
    v.type = VALUE_STRING;
    v.string_value = xstrdup(s ? s : "");
    return v;
}

static Value value_list_new(void) {
    Value v = value_null();
    v.type = VALUE_LIST;
    v.list_value = list_data_new();
    return v;
}

static Value value_map_new(void) {
    Value v = value_null();
    v.type = VALUE_MAP;
    v.map_value = map_data_new();
    return v;
}

static Value value_copy(Value in) {
    Value out = in;
    if (in.type == VALUE_STRING && in.string_value) {
        out.string_value = xstrdup(in.string_value);
    } else if (in.type == VALUE_LIST && in.list_value) {
        list_data_retain(in.list_value);
        out.list_value = in.list_value;
    } else if (in.type == VALUE_MAP && in.map_value) {
        map_data_retain(in.map_value);
        out.map_value = in.map_value;
    }
    return out;
}

static void value_free(Value *v) {
    if (!v) return;

    if (v->type == VALUE_STRING) {
        free(v->string_value);
        v->string_value = NULL;
    } else if (v->type == VALUE_LIST) {
        list_data_release(v->list_value);
        v->list_value = NULL;
    } else if (v->type == VALUE_MAP) {
        map_data_release(v->map_value);
        v->map_value = NULL;
    }

    v->type = VALUE_NULL;
    v->int_value = 0;
    v->bool_value = 0;
    v->string_value = NULL;
    v->list_value = NULL;
    v->map_value = NULL;
}

static void text_append(char **buf, size_t *len, size_t *cap, const char *text) {
    size_t add = strlen(text);
    if (*len + add + 1 > *cap) {
        while (*len + add + 1 > *cap) {
            *cap *= 2;
        }
        *buf = (char *)realloc(*buf, *cap);
        if (!*buf) {
            fprintf(stderr, "[lovelang] OOM\n");
            exit(1);
        }
    }
    memcpy(*buf + *len, text, add + 1);
    *len += add;
}

static char *value_to_string(Value v) {
    char buf[64];
    size_t i;

    switch (v.type) {
        case VALUE_INT:
            snprintf(buf, sizeof(buf), "%ld", v.int_value);
            return xstrdup(buf);
        case VALUE_BOOL:
            return xstrdup(v.bool_value ? "sach" : "jhooth");
        case VALUE_STRING:
            return xstrdup(v.string_value ? v.string_value : "");
        case VALUE_LIST: {
            char *out = (char *)malloc(64);
            size_t len = 0;
            size_t cap = 64;
            if (!out) {
                fprintf(stderr, "[lovelang] OOM\n");
                exit(1);
            }
            out[0] = '\0';
            text_append(&out, &len, &cap, "[");
            if (v.list_value) {
                for (i = 0; i < v.list_value->count; i++) {
                    char *item = value_to_string(v.list_value->items[i]);
                    if (i > 0) text_append(&out, &len, &cap, ", ");
                    text_append(&out, &len, &cap, item);
                    free(item);
                }
            }
            text_append(&out, &len, &cap, "]");
            return out;
        }
        case VALUE_MAP: {
            char *out = (char *)malloc(64);
            size_t len = 0;
            size_t cap = 64;
            if (!out) {
                fprintf(stderr, "[lovelang] OOM\n");
                exit(1);
            }
            out[0] = '\0';
            text_append(&out, &len, &cap, "{");
            if (v.map_value) {
                for (i = 0; i < v.map_value->count; i++) {
                    char *item = value_to_string(v.map_value->entries[i].value);
                    if (i > 0) text_append(&out, &len, &cap, ", ");
                    text_append(&out, &len, &cap, v.map_value->entries[i].key ? v.map_value->entries[i].key : "");
                    text_append(&out, &len, &cap, ": ");
                    text_append(&out, &len, &cap, item);
                    free(item);
                }
            }
            text_append(&out, &len, &cap, "}");
            return out;
        }
        case VALUE_NULL:
        default:
            return xstrdup("null");
    }
}

static long value_as_int(Value v) {
    char *end = NULL;

    if (v.type == VALUE_INT) {
        return v.int_value;
    }

    if (v.type == VALUE_BOOL) {
        return v.bool_value ? 1 : 0;
    }

    if (v.type == VALUE_STRING) {
        long n = strtol(v.string_value ? v.string_value : "0", &end, 10);
        if (!end || *end != '\0') {
            return 0;
        }
        return n;
    }

    if (v.type == VALUE_LIST && v.list_value) {
        return (long)v.list_value->count;
    }

    if (v.type == VALUE_MAP && v.map_value) {
        return (long)v.map_value->count;
    }

    return 0;
}

static int value_truthy(Value v) {
    switch (v.type) {
        case VALUE_INT:
            return v.int_value != 0;
        case VALUE_BOOL:
            return v.bool_value != 0;
        case VALUE_STRING:
            return v.string_value && v.string_value[0] != '\0';
        case VALUE_LIST:
            return v.list_value && v.list_value->count > 0;
        case VALUE_MAP:
            return v.map_value && v.map_value->count > 0;
        case VALUE_NULL:
        default:
            return 0;
    }
}

static int mode_is(const RuntimeState *state, const char *name) {
    return strcmp(state->config.mode, name) == 0;
}

static void runtime_error(RuntimeState *state, int line, const char *message) {
    if (mode_is(state, "toxic")) {
        fprintf(stderr, "[lovelang] line %d: reply to kar yaar - %s\n", line, message);
    } else if (mode_is(state, "shayari")) {
        fprintf(stderr, "[lovelang] line %d: dil toot gaya - %s\n", line, message);
    } else {
        fprintf(stderr, "[lovelang] line %d: pyaar se bolo, par syntax sahi rakho - %s\n", line, message);
    }
}

static void debug_log(RuntimeState *state, const char *event, const char *name, Value value) {
    char *s;
    if (!state->config.debug_love) {
        return;
    }

    s = value_to_string(value);
    printf("[debug-love] %s %s = %s\n", event, name, s);
    free(s);
}

static int env_find_local(Env *env, const char *name) {
    size_t i;
    for (i = 0; i < env->count; i++) {
        if (strcmp(env->vars[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int env_find_any(Env *env, const char *name, Env **owner, int *index) {
    Env *cur = env;
    while (cur) {
        int idx = env_find_local(cur, name);
        if (idx >= 0) {
            *owner = cur;
            *index = idx;
            return 1;
        }
        cur = cur->parent;
    }
    return 0;
}

static Value *env_get(Env *env, const char *name) {
    Env *owner = NULL;
    int idx = -1;
    if (env_find_any(env, name, &owner, &idx)) {
        return &owner->vars[idx].value;
    }
    return NULL;
}

static int env_define(Env *env, RuntimeState *state, int line, const char *name, Value v, int is_const) {
    int idx = env_find_local(env, name);

    if (idx >= 0) {
        if (env->vars[idx].is_const) {
            runtime_error(state, line, "vada variable immutable hai");
            value_free(&v);
            return 0;
        }

        value_free(&env->vars[idx].value);
        env->vars[idx].value = v;
        env->vars[idx].is_const = is_const;
        return 1;
    }

    if (env->count >= 256) {
        runtime_error(state, line, "too many variables in scope");
        value_free(&v);
        return 0;
    }

    strncpy(env->vars[env->count].name, name, sizeof(env->vars[env->count].name) - 1);
    env->vars[env->count].name[sizeof(env->vars[env->count].name) - 1] = '\0';
    env->vars[env->count].value = v;
    env->vars[env->count].is_const = is_const;
    env->count++;
    return 1;
}

static int env_assign(Env *env, RuntimeState *state, int line, const char *name, Value v) {
    Env *owner = NULL;
    int idx = -1;

    if (env_find_any(env, name, &owner, &idx)) {
        if (owner->vars[idx].is_const) {
            runtime_error(state, line, "vada variable ko change nahi kar sakte");
            value_free(&v);
            return 0;
        }

        value_free(&owner->vars[idx].value);
        owner->vars[idx].value = v;
        return 1;
    }

    return env_define(env, state, line, name, v, 0);
}

static void env_clear(Env *env) {
    size_t i;
    for (i = 0; i < env->count; i++) {
        value_free(&env->vars[i].value);
    }
    env->count = 0;
}

static ExecResult exec_ok(void) {
    ExecResult r;
    r.error = 0;
    r.has_return = 0;
    r.return_value = value_null();
    return r;
}

static ExecResult exec_error(void) {
    ExecResult r = exec_ok();
    r.error = 1;
    return r;
}

static ExecResult exec_return(Value value) {
    ExecResult r = exec_ok();
    r.has_return = 1;
    r.return_value = value;
    return r;
}

static Node *find_function(RuntimeState *state, const char *name) {
    size_t i;
    for (i = 0; i < state->functions.count; i++) {
        if (strcmp(state->functions.defs[i].name, name) == 0) {
            return state->functions.defs[i].decl;
        }
    }
    return NULL;
}

static void register_function(RuntimeState *state, Node *decl) {
    size_t i;

    for (i = 0; i < state->functions.count; i++) {
        if (strcmp(state->functions.defs[i].name, decl->text ? decl->text : "") == 0) {
            state->functions.defs[i].decl = decl;
            return;
        }
    }

    if (state->functions.count >= 128) {
        return;
    }

    strncpy(state->functions.defs[state->functions.count].name,
            decl->text ? decl->text : "",
            sizeof(state->functions.defs[state->functions.count].name) - 1);
    state->functions.defs[state->functions.count].name[
        sizeof(state->functions.defs[state->functions.count].name) - 1] = '\0';
    state->functions.defs[state->functions.count].decl = decl;
    state->functions.count++;
}

static void print_style(RuntimeState *state, NodeType type, const char *text) {
    const char *safe = text ? text : "";
    (void)state;

    switch (type) {
        case NODE_PRINT:
            printf("%s\n", safe);
            break;

        case NODE_TYPING:
            printf("typing...\n");
            break;

        default:
            printf("%s\n", safe);
            break;
    }
}

static Value eval_expr(Node *expr, RuntimeState *state, Env *env, int *ok);
static ExecResult execute_stmt_list(Node *stmt, RuntimeState *state, Env *env);

static int count_args(Node *args) {
    int count = 0;
    while (args) {
        count++;
        args = args->next;
    }
    return count;
}

static void sleep_ms(long ms) {
    if (ms <= 0) {
        return;
    }

#ifdef _WIN32
    Sleep((unsigned long)ms);
#else
    while (ms > 0) {
        struct timespec req;
        struct timespec rem;
        long chunk = ms > 1000 ? 1000 : ms;

        req.tv_sec = chunk / 1000;
        req.tv_nsec = (long)(chunk % 1000) * 1000000L;

        while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
            req = rem;
        }

        ms -= chunk;
    }
#endif
}

static int list_ensure(ListData *list, size_t need) {
    if (!list) return 0;
    if (need <= list->cap) return 1;

    if (list->cap == 0) list->cap = 4;
    while (list->cap < need) {
        list->cap *= 2;
    }

    list->items = (Value *)realloc(list->items, list->cap * sizeof(Value));
    if (!list->items) {
        fprintf(stderr, "[lovelang] OOM\n");
        exit(1);
    }
    return 1;
}

static int list_push(ListData *list, Value v) {
    if (!list_ensure(list, list->count + 1)) return 0;
    list->items[list->count++] = v;
    return 1;
}

static Value list_pop(ListData *list, int *ok) {
    Value out;
    if (!list || list->count == 0) {
        *ok = 0;
        return value_null();
    }
    out = list->items[list->count - 1];
    list->count--;
    return out;
}

static Value list_get_copy(ListData *list, long index, int *ok) {
    if (!list || index < 0 || (size_t)index >= list->count) {
        *ok = 0;
        return value_null();
    }
    return value_copy(list->items[index]);
}

static int list_set(ListData *list, long index, Value v) {
    if (!list || index < 0 || (size_t)index >= list->count) {
        return 0;
    }
    value_free(&list->items[index]);
    list->items[index] = v;
    return 1;
}

static int map_ensure(MapData *map, size_t need) {
    if (!map) return 0;
    if (need <= map->cap) return 1;

    if (map->cap == 0) map->cap = 4;
    while (map->cap < need) {
        map->cap *= 2;
    }

    map->entries = (MapEntry *)realloc(map->entries, map->cap * sizeof(MapEntry));
    if (!map->entries) {
        fprintf(stderr, "[lovelang] OOM\n");
        exit(1);
    }
    return 1;
}

static int map_find(MapData *map, const char *key) {
    size_t i;
    if (!map || !key) return -1;
    for (i = 0; i < map->count; i++) {
        if (map->entries[i].key && strcmp(map->entries[i].key, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int map_set(MapData *map, const char *key, Value v) {
    int idx;
    if (!map || !key) return 0;

    idx = map_find(map, key);
    if (idx >= 0) {
        value_free(&map->entries[idx].value);
        map->entries[idx].value = v;
        return 1;
    }

    if (!map_ensure(map, map->count + 1)) return 0;
    map->entries[map->count].key = xstrdup(key);
    map->entries[map->count].value = v;
    map->count++;
    return 1;
}

static Value map_get_copy(MapData *map, const char *key, int *found) {
    int idx = map_find(map, key);
    if (idx < 0) {
        *found = 0;
        return value_null();
    }
    *found = 1;
    return value_copy(map->entries[idx].value);
}

static char *str_trim_dup(const char *in) {
    const char *start;
    const char *end;
    size_t n;
    char *out;

    if (!in) return xstrdup("");
    start = in;
    while (*start && isspace((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;

    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) {
        fprintf(stderr, "[lovelang] OOM\n");
        exit(1);
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

static char *str_lower_dup(const char *in) {
    size_t i;
    char *out = xstrdup(in ? in : "");
    for (i = 0; out[i]; i++) {
        out[i] = (char)tolower((unsigned char)out[i]);
    }
    return out;
}

static char *str_upper_dup(const char *in) {
    size_t i;
    char *out = xstrdup(in ? in : "");
    for (i = 0; out[i]; i++) {
        out[i] = (char)toupper((unsigned char)out[i]);
    }
    return out;
}

static char *str_replace_all_dup(const char *text, const char *from, const char *to) {
    size_t from_len;
    size_t to_len;
    size_t cap = strlen(text ? text : "") + 1;
    size_t len = 0;
    const char *cur;
    char *out;

    if (!text) return xstrdup("");
    if (!from || !from[0]) return xstrdup(text);
    if (!to) to = "";

    from_len = strlen(from);
    to_len = strlen(to);
    out = (char *)malloc(cap);
    if (!out) {
        fprintf(stderr, "[lovelang] OOM\n");
        exit(1);
    }
    out[0] = '\0';

    cur = text;
    while (*cur) {
        const char *hit = strstr(cur, from);
        if (!hit) {
            size_t tail = strlen(cur);
            if (len + tail + 1 > cap) {
                while (len + tail + 1 > cap) cap *= 2;
                out = (char *)realloc(out, cap);
                if (!out) {
                    fprintf(stderr, "[lovelang] OOM\n");
                    exit(1);
                }
            }
            memcpy(out + len, cur, tail + 1);
            len += tail;
            break;
        }

        {
            size_t chunk = (size_t)(hit - cur);
            size_t needed = len + chunk + to_len + 1;
            if (needed > cap) {
                while (needed > cap) cap *= 2;
                out = (char *)realloc(out, cap);
                if (!out) {
                    fprintf(stderr, "[lovelang] OOM\n");
                    exit(1);
                }
            }
            memcpy(out + len, cur, chunk);
            len += chunk;
            memcpy(out + len, to, to_len);
            len += to_len;
            out[len] = '\0';
            cur = hit + from_len;
        }
    }

    return out;
}

static int file_exists_path(const char *path) {
    FILE *fp;
    if (!path || !path[0]) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static char *file_read_text(const char *path) {
    FILE *fp;
    long size;
    char *buf;

    if (!path || !path[0]) return NULL;
    fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fprintf(stderr, "[lovelang] OOM\n");
        exit(1);
    }
    if (size > 0) {
        fread(buf, 1, (size_t)size, fp);
    }
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static int file_write_text(const char *path, const char *text, int append) {
    FILE *fp;
    if (!path || !path[0]) return 0;
    fp = fopen(path, append ? "ab" : "wb");
    if (!fp) return 0;
    if (text && text[0]) {
        fwrite(text, 1, strlen(text), fp);
    }
    fclose(fp);
    return 1;
}

static Value call_function(Node *call, RuntimeState *state, Env *env, int *ok) {
    const char *name = call->text ? call->text : "";

    if (strcmp(name, "dil_se_pucho") == 0 || strcmp(name, "input") == 0) {
        int argc = count_args(call->args);
        char buffer[2048];
        char *prompt = NULL;

        if (argc > 1) {
            runtime_error(state, call->line, "dil_se_pucho(prompt?) expects at most 1 argument");
            *ok = 0;
            return value_null();
        }

        if (call->args) {
            Value arg = eval_expr(call->args, state, env, ok);
            if (!*ok) {
                return value_null();
            }
            prompt = value_to_string(arg);
            value_free(&arg);
        }

        if (prompt && prompt[0]) {
            fputs(prompt, stdout);
            fflush(stdout);
        }

        if (!fgets(buffer, sizeof(buffer), stdin)) {
            free(prompt);
            return value_string("");
        }

        {
            size_t n = strlen(buffer);
            while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r')) {
                buffer[n - 1] = '\0';
                n--;
            }
        }

        free(prompt);
        return value_string(buffer);
    }

    if (strcmp(name, "kismat") == 0) {
        int argc = count_args(call->args);
        Value min_arg;
        Value max_arg;
        long min;
        long max;

        if (argc != 2) {
            runtime_error(state, call->line, "kismat(min, max) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }

        min_arg = eval_expr(call->args, state, env, ok);
        if (!*ok) {
            return value_null();
        }
        max_arg = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&min_arg);
            return value_null();
        }

        min = value_as_int(min_arg);
        max = value_as_int(max_arg);
        value_free(&min_arg);
        value_free(&max_arg);

        if (min > max) {
            long tmp = min;
            min = max;
            max = tmp;
        }

        if (!state->rng_seeded) {
            srand((unsigned int)time(NULL));
            state->rng_seeded = 1;
        }

        if (min == max) {
            return value_int(min);
        }

        {
            long double span = ((long double)max - (long double)min) + 1.0L;
            long double roll = ((long double)rand() / ((long double)RAND_MAX + 1.0L)) * span;
            long out = min + (long)roll;
            if (out < min) out = min;
            if (out > max) out = max;
            return value_int(out);
        }
    }

    if (strcmp(name, "lafz_len") == 0) {
        int argc = count_args(call->args);
        Value arg;
        char *as_text;
        Value out;

        if (argc != 1) {
            runtime_error(state, call->line, "lafz_len(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }

        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) {
            return value_null();
        }

        as_text = value_to_string(arg);
        out = value_int((long)strlen(as_text));
        free(as_text);
        value_free(&arg);
        return out;
    }

    if (strcmp(name, "lambai") == 0 || strcmp(name, "len") == 0) {
        int argc = count_args(call->args);
        Value arg;
        if (argc != 1) {
            runtime_error(state, call->line, "lambai(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        if (arg.type == VALUE_STRING) {
            Value out = value_int((long)strlen(arg.string_value ? arg.string_value : ""));
            value_free(&arg);
            return out;
        }
        if (arg.type == VALUE_LIST && arg.list_value) {
            Value out = value_int((long)arg.list_value->count);
            value_free(&arg);
            return out;
        }
        if (arg.type == VALUE_MAP && arg.map_value) {
            Value out = value_int((long)arg.map_value->count);
            value_free(&arg);
            return out;
        }
        value_free(&arg);
        return value_int(0);
    }

    if (strcmp(name, "type_of") == 0 || strcmp(name, "kya_type") == 0) {
        Value arg;
        const char *kind = "null";
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "type_of(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        if (arg.type == VALUE_INT) kind = "int";
        else if (arg.type == VALUE_BOOL) kind = "bool";
        else if (arg.type == VALUE_STRING) kind = "string";
        else if (arg.type == VALUE_LIST) kind = "list";
        else if (arg.type == VALUE_MAP) kind = "map";
        value_free(&arg);
        return value_string(kind);
    }

    if (strcmp(name, "to_text") == 0 || strcmp(name, "text_banao") == 0) {
        Value arg;
        char *txt;
        Value out;
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "to_text(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        txt = value_to_string(arg);
        out = value_string(txt);
        free(txt);
        value_free(&arg);
        return out;
    }

    if (strcmp(name, "to_int") == 0 || strcmp(name, "int_banao") == 0) {
        Value arg;
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "to_int(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        {
            Value out = value_int(value_as_int(arg));
            value_free(&arg);
            return out;
        }
    }

    if (strcmp(name, "to_bool") == 0 || strcmp(name, "bool_banao") == 0) {
        Value arg;
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "to_bool(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        {
            Value out = value_bool(value_truthy(arg));
            value_free(&arg);
            return out;
        }
    }

    if (strcmp(name, "list_nayi") == 0 || strcmp(name, "pyaar_list") == 0) {
        if (count_args(call->args) != 0) {
            runtime_error(state, call->line, "list_nayi() expects no arguments");
            *ok = 0;
            return value_null();
        }
        return value_list_new();
    }

    if (strcmp(name, "map_naya") == 0 || strcmp(name, "raaz_map") == 0) {
        if (count_args(call->args) != 0) {
            runtime_error(state, call->line, "map_naya() expects no arguments");
            *ok = 0;
            return value_null();
        }
        return value_map_new();
    }

    if (strcmp(name, "list_daal") == 0 || strcmp(name, "pyaar_daal") == 0) {
        int argc = count_args(call->args);
        Value listv;
        Value item;
        if (argc != 2) {
            runtime_error(state, call->line, "list_daal(list, value) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        listv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        item = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&listv);
            return value_null();
        }
        if (listv.type != VALUE_LIST || !listv.list_value) {
            runtime_error(state, call->line, "list_daal first argument must be list");
            value_free(&listv);
            value_free(&item);
            *ok = 0;
            return value_null();
        }
        list_push(listv.list_value, item);
        {
            Value out = value_int((long)listv.list_value->count);
            value_free(&listv);
            return out;
        }
    }

    if (strcmp(name, "list_nikaal") == 0) {
        int argc = count_args(call->args);
        Value listv;
        Value out;
        if (argc != 1) {
            runtime_error(state, call->line, "list_nikaal(list) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        listv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        if (listv.type != VALUE_LIST || !listv.list_value) {
            runtime_error(state, call->line, "list_nikaal argument must be list");
            value_free(&listv);
            *ok = 0;
            return value_null();
        }
        out = list_pop(listv.list_value, ok);
        if (!*ok) {
            runtime_error(state, call->line, "list_nikaal on empty list");
            value_free(&listv);
            *ok = 0;
            return value_null();
        }
        value_free(&listv);
        return out;
    }

    if (strcmp(name, "list_lao") == 0) {
        int argc = count_args(call->args);
        Value listv;
        Value idxv;
        Value out;
        if (argc != 2) {
            runtime_error(state, call->line, "list_lao(list, index) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        listv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        idxv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&listv);
            return value_null();
        }
        if (listv.type != VALUE_LIST || !listv.list_value) {
            runtime_error(state, call->line, "list_lao first argument must be list");
            value_free(&listv);
            value_free(&idxv);
            *ok = 0;
            return value_null();
        }
        out = list_get_copy(listv.list_value, value_as_int(idxv), ok);
        value_free(&listv);
        value_free(&idxv);
        if (!*ok) {
            runtime_error(state, call->line, "list_lao index out of range");
            *ok = 0;
            return value_null();
        }
        return out;
    }

    if (strcmp(name, "list_set") == 0) {
        int argc = count_args(call->args);
        Value listv;
        Value idxv;
        Value item;
        if (argc != 3) {
            runtime_error(state, call->line, "list_set(list, index, value) expects exactly 3 arguments");
            *ok = 0;
            return value_null();
        }
        listv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        idxv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&listv);
            return value_null();
        }
        item = eval_expr(call->args->next->next, state, env, ok);
        if (!*ok) {
            value_free(&listv);
            value_free(&idxv);
            return value_null();
        }
        if (listv.type != VALUE_LIST || !listv.list_value) {
            runtime_error(state, call->line, "list_set first argument must be list");
            value_free(&listv);
            value_free(&idxv);
            value_free(&item);
            *ok = 0;
            return value_null();
        }
        if (!list_set(listv.list_value, value_as_int(idxv), item)) {
            runtime_error(state, call->line, "list_set index out of range");
            value_free(&listv);
            value_free(&idxv);
            value_free(&item);
            *ok = 0;
            return value_null();
        }
        value_free(&listv);
        value_free(&idxv);
        return value_bool(1);
    }

    if (strcmp(name, "map_set") == 0) {
        int argc = count_args(call->args);
        Value mapv;
        Value keyv;
        Value valv;
        char *key;
        if (argc != 3) {
            runtime_error(state, call->line, "map_set(map, key, value) expects exactly 3 arguments");
            *ok = 0;
            return value_null();
        }
        mapv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        keyv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&mapv);
            return value_null();
        }
        valv = eval_expr(call->args->next->next, state, env, ok);
        if (!*ok) {
            value_free(&mapv);
            value_free(&keyv);
            return value_null();
        }
        if (mapv.type != VALUE_MAP || !mapv.map_value) {
            runtime_error(state, call->line, "map_set first argument must be map");
            value_free(&mapv);
            value_free(&keyv);
            value_free(&valv);
            *ok = 0;
            return value_null();
        }
        key = value_to_string(keyv);
        map_set(mapv.map_value, key, valv);
        free(key);
        value_free(&mapv);
        value_free(&keyv);
        return value_bool(1);
    }

    if (strcmp(name, "map_get") == 0) {
        int argc = count_args(call->args);
        Value mapv;
        Value keyv;
        char *key;
        int found = 0;
        Value out;
        if (argc != 2) {
            runtime_error(state, call->line, "map_get(map, key) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        mapv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        keyv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&mapv);
            return value_null();
        }
        if (mapv.type != VALUE_MAP || !mapv.map_value) {
            runtime_error(state, call->line, "map_get first argument must be map");
            value_free(&mapv);
            value_free(&keyv);
            *ok = 0;
            return value_null();
        }
        key = value_to_string(keyv);
        out = map_get_copy(mapv.map_value, key, &found);
        free(key);
        value_free(&mapv);
        value_free(&keyv);
        if (!found) {
            return value_null();
        }
        return out;
    }

    if (strcmp(name, "map_has") == 0) {
        int argc = count_args(call->args);
        Value mapv;
        Value keyv;
        char *key;
        int has;
        if (argc != 2) {
            runtime_error(state, call->line, "map_has(map, key) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        mapv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        keyv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&mapv);
            return value_null();
        }
        if (mapv.type != VALUE_MAP || !mapv.map_value) {
            runtime_error(state, call->line, "map_has first argument must be map");
            value_free(&mapv);
            value_free(&keyv);
            *ok = 0;
            return value_null();
        }
        key = value_to_string(keyv);
        has = map_find(mapv.map_value, key) >= 0;
        free(key);
        value_free(&mapv);
        value_free(&keyv);
        return value_bool(has);
    }

    if (strcmp(name, "map_keys") == 0) {
        int argc = count_args(call->args);
        Value mapv;
        Value out;
        size_t i;
        if (argc != 1) {
            runtime_error(state, call->line, "map_keys(map) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        mapv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        if (mapv.type != VALUE_MAP || !mapv.map_value) {
            runtime_error(state, call->line, "map_keys argument must be map");
            value_free(&mapv);
            *ok = 0;
            return value_null();
        }
        out = value_list_new();
        for (i = 0; i < mapv.map_value->count; i++) {
            Value keyv = value_string(mapv.map_value->entries[i].key ? mapv.map_value->entries[i].key : "");
            list_push(out.list_value, keyv);
        }
        value_free(&mapv);
        return out;
    }

    if (strcmp(name, "lafz_trim") == 0) {
        Value arg;
        char *txt;
        char *trim;
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "lafz_trim(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        txt = value_to_string(arg);
        trim = str_trim_dup(txt);
        free(txt);
        value_free(&arg);
        {
            Value out = value_string(trim);
            free(trim);
            return out;
        }
    }

    if (strcmp(name, "lafz_lower") == 0) {
        Value arg;
        char *txt;
        char *lower;
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "lafz_lower(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        txt = value_to_string(arg);
        lower = str_lower_dup(txt);
        free(txt);
        value_free(&arg);
        {
            Value out = value_string(lower);
            free(lower);
            return out;
        }
    }

    if (strcmp(name, "lafz_upper") == 0) {
        Value arg;
        char *txt;
        char *upper;
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "lafz_upper(value) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        txt = value_to_string(arg);
        upper = str_upper_dup(txt);
        free(txt);
        value_free(&arg);
        {
            Value out = value_string(upper);
            free(upper);
            return out;
        }
    }

    if (strcmp(name, "lafz_contains") == 0) {
        Value textv;
        Value needlev;
        char *text;
        char *needle;
        int has;
        if (count_args(call->args) != 2) {
            runtime_error(state, call->line, "lafz_contains(text, needle) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        textv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        needlev = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&textv);
            return value_null();
        }
        text = value_to_string(textv);
        needle = value_to_string(needlev);
        has = strstr(text, needle) != NULL;
        free(text);
        free(needle);
        value_free(&textv);
        value_free(&needlev);
        return value_bool(has);
    }

    if (strcmp(name, "lafz_replace") == 0) {
        Value textv;
        Value fromv;
        Value tov;
        char *text;
        char *from;
        char *to;
        char *replaced;
        Value out;
        if (count_args(call->args) != 3) {
            runtime_error(state, call->line, "lafz_replace(text, from, to) expects exactly 3 arguments");
            *ok = 0;
            return value_null();
        }
        textv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        fromv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&textv);
            return value_null();
        }
        tov = eval_expr(call->args->next->next, state, env, ok);
        if (!*ok) {
            value_free(&textv);
            value_free(&fromv);
            return value_null();
        }
        text = value_to_string(textv);
        from = value_to_string(fromv);
        to = value_to_string(tov);
        replaced = str_replace_all_dup(text, from, to);
        out = value_string(replaced);
        free(text);
        free(from);
        free(to);
        free(replaced);
        value_free(&textv);
        value_free(&fromv);
        value_free(&tov);
        return out;
    }

    if (strcmp(name, "lafz_split") == 0) {
        Value textv;
        Value sepv;
        char *text;
        char *sep;
        Value out;
        if (count_args(call->args) != 2) {
            runtime_error(state, call->line, "lafz_split(text, sep) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        textv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        sepv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&textv);
            return value_null();
        }
        text = value_to_string(textv);
        sep = value_to_string(sepv);
        out = value_list_new();

        if (!sep[0]) {
            size_t i;
            for (i = 0; text[i]; i++) {
                char tmp[2] = { text[i], '\0' };
                list_push(out.list_value, value_string(tmp));
            }
        } else {
            const char *cur = text;
            size_t sep_len = strlen(sep);
            while (1) {
                const char *hit = strstr(cur, sep);
                if (!hit) {
                    list_push(out.list_value, value_string(cur));
                    break;
                }
                {
                    size_t chunk = (size_t)(hit - cur);
                    char *piece = (char *)malloc(chunk + 1);
                    if (!piece) {
                        fprintf(stderr, "[lovelang] OOM\n");
                        exit(1);
                    }
                    memcpy(piece, cur, chunk);
                    piece[chunk] = '\0';
                    list_push(out.list_value, value_string(piece));
                    free(piece);
                }
                cur = hit + sep_len;
            }
        }

        free(text);
        free(sep);
        value_free(&textv);
        value_free(&sepv);
        return out;
    }

    if (strcmp(name, "lafz_join") == 0) {
        Value listv;
        Value sepv;
        char *sep;
        char *joined;
        Value out;
        size_t i;
        if (count_args(call->args) != 2) {
            runtime_error(state, call->line, "lafz_join(list, sep) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        listv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        sepv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&listv);
            return value_null();
        }
        if (listv.type != VALUE_LIST || !listv.list_value) {
            runtime_error(state, call->line, "lafz_join first argument must be list");
            value_free(&listv);
            value_free(&sepv);
            *ok = 0;
            return value_null();
        }
        sep = value_to_string(sepv);
        {
            size_t cap = 64;
            size_t len = 0;
            joined = (char *)malloc(cap);
            if (!joined) {
                fprintf(stderr, "[lovelang] OOM\n");
                exit(1);
            }
            joined[0] = '\0';
            for (i = 0; i < listv.list_value->count; i++) {
                char *part = value_to_string(listv.list_value->items[i]);
                if (i > 0) text_append(&joined, &len, &cap, sep);
                text_append(&joined, &len, &cap, part);
                free(part);
            }
        }
        out = value_string(joined);
        free(joined);
        free(sep);
        value_free(&listv);
        value_free(&sepv);
        return out;
    }

    if (strcmp(name, "dil_khol_ke_padho") == 0 || strcmp(name, "file_padho") == 0) {
        Value pathv;
        char *path;
        char *text;
        Value out;
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "dil_khol_ke_padho(path) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        pathv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        path = value_to_string(pathv);
        text = file_read_text(path);
        if (!text) {
            runtime_error(state, call->line, "file read failed");
            free(path);
            value_free(&pathv);
            *ok = 0;
            return value_null();
        }
        out = value_string(text);
        free(text);
        free(path);
        value_free(&pathv);
        return out;
    }

    if (strcmp(name, "ishq_likhdo") == 0 || strcmp(name, "file_likho") == 0) {
        Value pathv;
        Value textv;
        char *path;
        char *text;
        int ok_write;
        if (count_args(call->args) != 2) {
            runtime_error(state, call->line, "ishq_likhdo(path, text) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        pathv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        textv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&pathv);
            return value_null();
        }
        path = value_to_string(pathv);
        text = value_to_string(textv);
        ok_write = file_write_text(path, text, 0);
        free(path);
        free(text);
        value_free(&pathv);
        value_free(&textv);
        return value_bool(ok_write);
    }

    if (strcmp(name, "ishq_joddo") == 0 || strcmp(name, "file_jodo") == 0) {
        Value pathv;
        Value textv;
        char *path;
        char *text;
        int ok_write;
        if (count_args(call->args) != 2) {
            runtime_error(state, call->line, "ishq_joddo(path, text) expects exactly 2 arguments");
            *ok = 0;
            return value_null();
        }
        pathv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        textv = eval_expr(call->args->next, state, env, ok);
        if (!*ok) {
            value_free(&pathv);
            return value_null();
        }
        path = value_to_string(pathv);
        text = value_to_string(textv);
        ok_write = file_write_text(path, text, 1);
        free(path);
        free(text);
        value_free(&pathv);
        value_free(&textv);
        return value_bool(ok_write);
    }

    if (strcmp(name, "raasta_hai_kya") == 0 || strcmp(name, "file_hai_kya") == 0) {
        Value pathv;
        char *path;
        int exists;
        if (count_args(call->args) != 1) {
            runtime_error(state, call->line, "raasta_hai_kya(path) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }
        pathv = eval_expr(call->args, state, env, ok);
        if (!*ok) return value_null();
        path = value_to_string(pathv);
        exists = file_exists_path(path);
        free(path);
        value_free(&pathv);
        return value_bool(exists);
    }

    if (strcmp(name, "abhi_time") == 0) {
        int argc = count_args(call->args);
        if (argc != 0) {
            runtime_error(state, call->line, "abhi_time() expects no arguments");
            *ok = 0;
            return value_null();
        }
        return value_int((long)time(NULL));
    }

    if (strcmp(name, "thoda_ruko") == 0) {
        int argc = count_args(call->args);
        Value arg;
        long ms;

        if (argc != 1) {
            runtime_error(state, call->line, "thoda_ruko(ms) expects exactly 1 argument");
            *ok = 0;
            return value_null();
        }

        arg = eval_expr(call->args, state, env, ok);
        if (!*ok) {
            return value_null();
        }

        ms = value_as_int(arg);
        value_free(&arg);
        if (ms < 0) {
            ms = 0;
        }

        sleep_ms(ms);
        return value_null();
    }

    if (strcmp(name, "love_byeee") == 0 || strcmp(name, "love_you_baby_byeee") == 0) {
        if (mode_is(state, "toxic")) {
            printf("bye bolke ja rahe ho? theek hai, take care\n");
        } else if (mode_is(state, "shayari")) {
            printf("love you baby, byeee - milenge phir se alfaazon mein\n");
        } else {
            printf("love you baby byeee\n");
        }
        state->should_stop = 1;
        return value_null();
    }

    {
        Node *decl = find_function(state, name);
        Node *param;
        Node *arg_node;
        Env local;
        ExecResult result;
        Value positional_args[64];
        int positional_count = 0;
        int positional_idx = 0;
        struct {
            char name[64];
            Value value;
            int used;
        } named_args[64];
        int named_count = 0;
        int i;
        Value out = value_null();

        if (!decl) {
            runtime_error(state, call->line, "unknown dhadkan/function call");
            *ok = 0;
            return value_null();
        }

        memset(&local, 0, sizeof(local));
        local.parent = env;
        for (i = 0; i < 64; i++) {
            positional_args[i] = value_null();
            named_args[i].name[0] = '\0';
            named_args[i].value = value_null();
            named_args[i].used = 0;
        }

        arg_node = call->args;
        while (arg_node) {
            if (arg_node->type == NODE_ASSIGN && arg_node->text) {
                if (named_count >= 64) {
                    runtime_error(state, call->line, "too many named arguments");
                    *ok = 0;
                    break;
                }
                named_args[named_count].value = eval_expr(arg_node->left, state, env, ok);
                if (!*ok) break;
                strncpy(named_args[named_count].name, arg_node->text, sizeof(named_args[named_count].name) - 1);
                named_args[named_count].name[sizeof(named_args[named_count].name) - 1] = '\0';
                named_args[named_count].used = 0;
                named_count++;
            } else {
                if (positional_count >= 64) {
                    runtime_error(state, call->line, "too many positional arguments");
                    *ok = 0;
                    break;
                }
                positional_args[positional_count] = eval_expr(arg_node, state, env, ok);
                if (!*ok) break;
                positional_count++;
            }
            arg_node = arg_node->next;
        }

        if (!*ok) {
            for (i = 0; i < positional_count; i++) value_free(&positional_args[i]);
            for (i = 0; i < named_count; i++) value_free(&named_args[i].value);
            return value_null();
        }

        param = decl->params;
        while (param) {
            Value bind = value_null();
            int matched_named = -1;

            if (positional_idx < positional_count) {
                bind = positional_args[positional_idx];
                positional_args[positional_idx] = value_null();
                positional_idx++;
            } else {
                for (i = 0; i < named_count; i++) {
                    if (!named_args[i].used && param->text && strcmp(named_args[i].name, param->text) == 0) {
                        matched_named = i;
                        break;
                    }
                }

                if (matched_named >= 0) {
                    bind = named_args[matched_named].value;
                    named_args[matched_named].value = value_null();
                    named_args[matched_named].used = 1;
                } else if (param->type == NODE_ASSIGN && param->left) {
                    bind = eval_expr(param->left, state, &local, ok);
                    if (!*ok) {
                        for (i = positional_idx; i < positional_count; i++) value_free(&positional_args[i]);
                        for (i = 0; i < named_count; i++) value_free(&named_args[i].value);
                        env_clear(&local);
                        return value_null();
                    }
                }
            }

            if (!env_define(&local, state, call->line, param->text ? param->text : "", bind, 0)) {
                for (i = 0; i < positional_count; i++) value_free(&positional_args[i]);
                for (i = 0; i < named_count; i++) value_free(&named_args[i].value);
                env_clear(&local);
                *ok = 0;
                return value_null();
            }

            param = param->next;
        }

        for (i = positional_idx; i < positional_count; i++) {
            value_free(&positional_args[i]);
        }

        for (i = 0; i < named_count; i++) {
            if (!named_args[i].used) {
                runtime_error(state, call->line, "unknown named argument");
                value_free(&named_args[i].value);
                env_clear(&local);
                *ok = 0;
                return value_null();
            }
            value_free(&named_args[i].value);
        }

        if (positional_idx < positional_count) {
            runtime_error(state, call->line, "too many positional arguments");
            env_clear(&local);
            *ok = 0;
            return value_null();
        }

        result = execute_stmt_list(decl->body ? decl->body->body : NULL, state, &local);
        if (result.error) {
            env_clear(&local);
            *ok = 0;
            return value_null();
        }

        if (result.has_return) {
            out = value_copy(result.return_value);
            value_free(&result.return_value);
        }

        env_clear(&local);
        return out;
    }
}

static Value eval_expr(Node *expr, RuntimeState *state, Env *env, int *ok) {
    Value left;
    Value right;

    if (!expr) {
        *ok = 0;
        return value_null();
    }

    switch (expr->type) {
        case NODE_INT:
            return value_int(expr->int_value);

        case NODE_STRING:
            return value_string(expr->text ? expr->text : "");

        case NODE_BOOL:
            return value_bool(expr->bool_value);

        case NODE_IDENT: {
            Value *v = env_get(env, expr->text ? expr->text : "");
            if (!v) {
                runtime_error(state, expr->line, "unknown variable");
                *ok = 0;
                return value_null();
            }
            return value_copy(*v);
        }

        case NODE_CALL:
            return call_function(expr, state, env, ok);

        case NODE_UNARY:
            left = eval_expr(expr->left, state, env, ok);
            if (!*ok) {
                return value_null();
            }

            if (strcmp(expr->text ? expr->text : "", "-") == 0) {
                Value out = value_int(-value_as_int(left));
                value_free(&left);
                return out;
            }

            if (strcmp(expr->text ? expr->text : "", "!") == 0 ||
                strcmp(expr->text ? expr->text : "", "nahi") == 0) {
                Value out = value_bool(!value_truthy(left));
                value_free(&left);
                return out;
            }

            runtime_error(state, expr->line, "unsupported unary operator");
            value_free(&left);
            *ok = 0;
            return value_null();

        case NODE_BINARY:
            left = eval_expr(expr->left, state, env, ok);
            if (!*ok) return value_null();
            right = eval_expr(expr->right, state, env, ok);
            if (!*ok) {
                value_free(&left);
                return value_null();
            }

            if (strcmp(expr->text, "+") == 0) {
                if (left.type == VALUE_STRING || right.type == VALUE_STRING) {
                    char *ls = value_to_string(left);
                    char *rs = value_to_string(right);
                    size_t len = strlen(ls) + strlen(rs) + 1;
                    char *merged = (char *)malloc(len);
                    Value out;
                    if (!merged) {
                        fprintf(stderr, "[lovelang] OOM\n");
                        exit(1);
                    }
                    strcpy(merged, ls);
                    strcat(merged, rs);
                    out = value_string(merged);
                    free(merged);
                    free(ls);
                    free(rs);
                    value_free(&left);
                    value_free(&right);
                    return out;
                }
                {
                    Value out = value_int(value_as_int(left) + value_as_int(right));
                    value_free(&left);
                    value_free(&right);
                    return out;
                }
            }

            if (strcmp(expr->text, "-") == 0) {
                Value out = value_int(value_as_int(left) - value_as_int(right));
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, "*") == 0) {
                Value out = value_int(value_as_int(left) * value_as_int(right));
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, "/") == 0) {
                long rv = value_as_int(right);
                Value out;
                if (rv == 0) {
                    runtime_error(state, expr->line, "division by zero");
                    value_free(&left);
                    value_free(&right);
                    *ok = 0;
                    return value_null();
                }
                out = value_int(value_as_int(left) / rv);
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, "%") == 0) {
                long rv = value_as_int(right);
                Value out;
                if (rv == 0) {
                    runtime_error(state, expr->line, "modulo by zero");
                    value_free(&left);
                    value_free(&right);
                    *ok = 0;
                    return value_null();
                }
                out = value_int(value_as_int(left) % rv);
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, "==") == 0) {
                int eq;
                if (left.type == VALUE_STRING || right.type == VALUE_STRING) {
                    char *ls = value_to_string(left);
                    char *rs = value_to_string(right);
                    eq = strcmp(ls, rs) == 0;
                    free(ls);
                    free(rs);
                } else {
                    eq = value_as_int(left) == value_as_int(right);
                }
                value_free(&left);
                value_free(&right);
                return value_bool(eq);
            }

            if (strcmp(expr->text, "!=") == 0) {
                int ne;
                if (left.type == VALUE_STRING || right.type == VALUE_STRING) {
                    char *ls = value_to_string(left);
                    char *rs = value_to_string(right);
                    ne = strcmp(ls, rs) != 0;
                    free(ls);
                    free(rs);
                } else {
                    ne = value_as_int(left) != value_as_int(right);
                }
                value_free(&left);
                value_free(&right);
                return value_bool(ne);
            }

            if (strcmp(expr->text, "<") == 0) {
                Value out = value_bool(value_as_int(left) < value_as_int(right));
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, "<=") == 0) {
                Value out = value_bool(value_as_int(left) <= value_as_int(right));
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, ">") == 0) {
                Value out = value_bool(value_as_int(left) > value_as_int(right));
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, ">=") == 0) {
                Value out = value_bool(value_as_int(left) >= value_as_int(right));
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, "&&") == 0 || strcmp(expr->text, "aur") == 0) {
                Value out = value_bool(value_truthy(left) && value_truthy(right));
                value_free(&left);
                value_free(&right);
                return out;
            }

            if (strcmp(expr->text, "||") == 0 || strcmp(expr->text, "ya") == 0) {
                Value out = value_bool(value_truthy(left) || value_truthy(right));
                value_free(&left);
                value_free(&right);
                return out;
            }

            runtime_error(state, expr->line, "unsupported binary operator");
            value_free(&left);
            value_free(&right);
            *ok = 0;
            return value_null();

        default:
            runtime_error(state, expr->line, "invalid expression node");
            *ok = 0;
            return value_null();
    }
}

static ExecResult execute_one(Node *stmt, RuntimeState *state, Env *env) {
    int ok = 1;

    if (!stmt) {
        return exec_ok();
    }

    switch (stmt->type) {
        case NODE_VAR_DECL:
        case NODE_CONST_DECL: {
            Value v = eval_expr(stmt->left, state, env, &ok);
            if (!ok) return exec_error();
            if (!env_define(env, state, stmt->line, stmt->text ? stmt->text : "", v, stmt->type == NODE_CONST_DECL)) {
                return exec_error();
            }
            debug_log(state,
                      stmt->type == NODE_CONST_DECL ? "vada set" : "yaad set",
                      stmt->text ? stmt->text : "",
                      v);
            return exec_ok();
        }

        case NODE_ASSIGN: {
            Value v = eval_expr(stmt->left, state, env, &ok);
            if (!ok) return exec_error();
            if (!env_assign(env, state, stmt->line, stmt->text ? stmt->text : "", v)) {
                return exec_error();
            }
            debug_log(state, "update", stmt->text ? stmt->text : "", v);
            return exec_ok();
        }

        case NODE_PRINT: {
            Value v = eval_expr(stmt->left, state, env, &ok);
            char *s;
            if (!ok) return exec_error();
            s = value_to_string(v);
            print_style(state, stmt->type, s);
            free(s);
            value_free(&v);
            return exec_ok();
        }

        case NODE_TYPING:
            print_style(state, NODE_TYPING, "");
            return exec_ok();

        case NODE_IF: {
            Value cond = eval_expr(stmt->cond, state, env, &ok);
            int take_then;
            ExecResult r;
            if (!ok) return exec_error();
            take_then = value_truthy(cond);
            value_free(&cond);

            if (take_then) {
                r = execute_stmt_list(stmt->then_branch ? stmt->then_branch->body : NULL, state, env);
                return r;
            }

            r = execute_stmt_list(stmt->else_branch ? stmt->else_branch->body : NULL, state, env);
            return r;
        }

        case NODE_WHILE: {
            int guard = 0;
            while (1) {
                Value cond = eval_expr(stmt->cond, state, env, &ok);
                int truth;
                ExecResult r;

                if (!ok) return exec_error();
                truth = value_truthy(cond);
                value_free(&cond);

                if (!truth) break;

                r = execute_stmt_list(stmt->body ? stmt->body->body : NULL, state, env);
                if (r.error || r.has_return) {
                    return r;
                }

                guard++;
                if (guard > 1000000) {
                    runtime_error(state, stmt->line, "loop guard triggered");
                    return exec_error();
                }
            }
            return exec_ok();
        }

        case NODE_FUNC_DECL:
            register_function(state, stmt);
            return exec_ok();

        case NODE_RETURN: {
            Value v;
            if (!env->parent) {
                runtime_error(state, stmt->line, "ehsaas return sirf dhadkan ke andar allowed hai");
                return exec_error();
            }
            v = eval_expr(stmt->left, state, env, &ok);
            if (!ok) return exec_error();
            return exec_return(v);
        }

        case NODE_CALL: {
            Value out = call_function(stmt, state, env, &ok);
            if (!ok) return exec_error();
            value_free(&out);
            return exec_ok();
        }

        case NODE_FESTIVAL: {
            ExecResult r;
            if (stmt->text && stmt->text[0]) {
                printf("festival mode: %s\n", stmt->text);
            }
            r = execute_stmt_list(stmt->body ? stmt->body->body : NULL, state, env);
            return r;
        }

        case NODE_BLOCK:
            return execute_stmt_list(stmt->body, state, env);

        default:
            runtime_error(state, stmt->line, "unsupported statement node");
            return exec_error();
    }
}

static ExecResult execute_stmt_list(Node *stmt, RuntimeState *state, Env *env) {
    Node *cur = stmt;
    while (cur) {
        if (state->should_stop) {
            return exec_ok();
        }
        ExecResult r = execute_one(cur, state, env);
        if (r.error || r.has_return) {
            return r;
        }
        cur = cur->next;
    }
    return exec_ok();
}

int runtime_execute(Node *program, const RuntimeConfig *config) {
    RuntimeState state;
    Env global_env;
    ExecResult result;

    memset(&state, 0, sizeof(state));
    memset(&global_env, 0, sizeof(global_env));

    if (config) {
        state.config = *config;
    }

    if (!state.config.mode[0]) {
        strncpy(state.config.mode, "romantic", sizeof(state.config.mode) - 1);
    }

    if (!program || program->type != NODE_BLOCK) {
        fprintf(stderr, "[lovelang] runtime error: invalid program root\n");
        return 1;
    }

    result = execute_stmt_list(program->body, &state, &global_env);

    if (result.has_return) {
        value_free(&result.return_value);
        runtime_error(&state, program->line, "top-level ehsaas return allowed nahi hai");
        env_clear(&global_env);
        return 1;
    }

    env_clear(&global_env);
    return result.error ? 1 : 0;
}
