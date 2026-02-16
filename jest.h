#ifndef JEST_H_
#define JEST_H_

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__clang__) || defined(__GNUC__)
#   define jest__gnuc_attribute(x) __attribute__(x)
#   define jest__trap() __builtin_trap()
#else
#   define jest__gnuc_attribute(x)
#   define jest__trap() abort()
#endif

#ifndef JEST_WRITER_TAB
#   define JEST_WRITER_TAB "\t"
#endif

#define JEST_PANIC() \
do { \
    fprintf(stderr, "[%s:%d] panic!\n", __FILE__, __LINE__); \
    jest__trap(); \
} while (0)

#define JEST_UNREACHABLE() \
do { \
    fprintf(stderr, "[%s:%d] JEST_UNREACHABLE() reached.\n", __FILE__, __LINE__); \
    jest__trap(); \
} while (0)

#define JEST_TODO(msg) \
do { \
    fprintf(stderr, "[%s:%d] TODO: " msg "\n", __FILE__, __LINE__); \
    jest__trap(); \
} while (0)

#define JEST_MIN(x, y) (((x) < (y))? (x) : (y))
#define JEST_MAX(x, y) (((x) > (y))? (x) : (y))

#if JEST_USE_MATH_H
    #include <math.h>
    #define JEST_REQUIRES_MATH_H 1
    #define JEST_NAN NAN
    #define JEST_INFINITY INFINITY
#endif

#ifndef JEST_NAN
#   if (defined(__clang__) || defined(__GNUC__))
#       define JEST_NAN __builtin_nanf("")
#   else
#       include <math.h>
#       define JEST_REQUIRES_MATH_H 1
#       define JEST_NAN NAN
#   endif
#endif // !JEST_NAN

#ifndef JEST_INFINITY
#   if (defined(__clang__) || defined(__GNUC__))
#       define JEST_INFINITY __builtin_inff()
#   else
#       include <math.h>
#       define JEST_REQUIRES_MATH_H 1
#       define JEST_INFINITY INFINITY
#   endif
#endif // !JEST_INF

#ifdef __cplusplus
    #define JEST_LITERAL(type) type
#else
    #define JEST_LITERAL(type) (type)
#endif

typedef bool   jest_boolean_t;
typedef double jest_number_t;

#define JEST_STR(s) JEST_LITERAL(jest_string_t) {sizeof("" s "") - 1, "" s ""}
#define JEST_STR_NULL JEST_LITERAL(jest_string_t) {0, NULL}
typedef struct { size_t len; char *data; } jest_string_t;
typedef struct {
    size_t cap; 
    jest_string_t buffer;
} jest_sb_t;

typedef struct jest_typed_value_t jest_typed_value_t;

typedef struct { size_t len; jest_typed_value_t *elems; } jest_array_t;
typedef struct {
    size_t nelems;
    size_t nbuckets;
    uint32_t load_factor;
    struct {
        uint32_t hash; // hash of the key
        uint32_t seed; // seed used by the hashing algorithm
        jest_string_t key;
        jest_typed_value_t *v;
    } *buckets;
} jest_object_t;

enum jest_type {
    JEST_TYPE_NULL = 1,
    JEST_TYPE_BOOLEAN,
    JEST_TYPE_NUMBER,
    JEST_TYPE_STRING,
    JEST_TYPE_ARRAY,
    JEST_TYPE_OBJECT
};

struct jest_typed_value_t {
    enum jest_type type;
    union {
        jest_boolean_t as_boolean;
        jest_number_t  as_number;
        jest_string_t  as_string;
        jest_array_t   as_array;
        jest_object_t  as_object;
    } v;
};

typedef struct jest_arena_bucket_t {
    struct jest_arena_bucket_t *next;
    size_t size;
    char data[1 /* flexible array */];
} jest_arena_bucket_t;

typedef struct {
    size_t default_bucket_size;
    jest_arena_bucket_t *head;
    jest_arena_bucket_t *current;
    size_t offset;
} jest_arena_t;


enum jest_scope_type {
    JEST_SCOPE_INVALID = 0,
    JEST_SCOPE_OBJ,
    JEST_SCOPE_ARR
};

enum jest_writer_error {
    JEST_WRITER_SUCCESS = 0,
    JEST_WRITER_ERR_MISMATCHED_SCOPE,
    JEST_WRITER_ERR_BAD_PARAM,
    JEST_WRITER_ERR_IO_FAIL
};

typedef struct {
    jest_sb_t data;
    size_t scope_stack_cap;
    size_t scope_stack_len;
    enum jest_scope_type *scope_stack;
} jest_writer_t;

jest__gnuc_attribute((__warn_unused_result__))
jest_arena_t *jest_arena_create(size_t default_bucket_size);

void jest_arena_destroy(jest_arena_t *arena);
void jest_arena_reset(jest_arena_t *arena);

jest__gnuc_attribute((__warn_unused_result__))
void *jest_arena_alloc_aligned(jest_arena_t *arena, size_t size, size_t align);

jest__gnuc_attribute((__warn_unused_result__))
void *jest_arena_alloc(jest_arena_t *arena, size_t size);

jest_string_t jest_str(jest_arena_t *arena, const char *cstr);

jest__gnuc_attribute((__format__(__printf__, 2, 3)))
jest_string_t jest_fstr(jest_arena_t *arena, const char *fmt, ...);

jest_sb_t jest_sb_create(jest_arena_t *arena, size_t start_cap);
void jest_sb_reserve(jest_arena_t *arena, jest_sb_t *sb, size_t amount);
void jest_sb_append(jest_arena_t *arena, jest_sb_t *sb, jest_string_t str);

jest_writer_t jest_writer_create(jest_arena_t *arena);
enum jest_writer_error jestw_begin_obejct(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key);
enum jest_writer_error jestw_end_object(jest_arena_t *arena, jest_writer_t *writer);
enum jest_writer_error jestw_begin_array(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key);
enum jest_writer_error jestw_end_array(jest_arena_t *arena, jest_writer_t *writer);
enum jest_writer_error jestw_null(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key);
enum jest_writer_error jestw_boolean(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key, jest_boolean_t val);
enum jest_writer_error jestw_number(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key, jest_number_t val);
enum jest_writer_error jestw_string(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key, jest_string_t val);
enum jest_writer_error jestw_comment(jest_arena_t *arena, jest_writer_t *writer, jest_string_t text);
enum jest_writer_error jestw_newline(jest_arena_t *arena, jest_writer_t *writer);
enum jest_writer_error jestw_write(const jest_writer_t *writer, const char *path);

bool jest_is_str_null(jest_string_t str);

#if JEST_REQUIRES_MATH_H
#   define jest_signbit signbit
#   define jest_isinf isinf
#   define jest_isnan isnan
#else
#   define jest_signbit jest__signbit
#   define jest_isinf jest__isinf
#   define jest_isnan jest__isnan

jest__gnuc_attribute((__unused__))
static int jest__signbit(double x)
{
    return *((uint64_t *)(void *)&x) >> 63;
}

jest__gnuc_attribute((__unused__))
static int jest__isinf(double x)
{
    return (*((uint64_t *)(void *)&x) & INT64_MAX) == UINT64_C(0x7FF0000000000000);
}

jest__gnuc_attribute((__unused__))
static int jest__isnan(double x)
{
    return (*((uint64_t *)(void *)&x) & INT64_MAX) > UINT64_C(0x7FF0000000000000);
}
#endif

#endif // !JEST_H_

#ifdef JEST_IMPL

static jest_arena_bucket_t *jest__arena_alloc_bucket(size_t size)
{
    jest_arena_bucket_t *ret = (jest_arena_bucket_t *)malloc(sizeof(*ret) + size - 1);
    if (!ret) JEST_PANIC();

    ret->size = size;
    ret->next = NULL;

    return ret;
}

static void jest__arena_next(jest_arena_t *arena, size_t min_size)
{
    if (!arena) return;

    arena->offset = 0;
    if (!arena->head || !arena->current) {
        arena->head = arena->current = jest__arena_alloc_bucket(JEST_MAX(min_size, arena->default_bucket_size));
        return;
    }

    jest_arena_bucket_t *next = arena->head->next;
    while (next) {
        arena->current = next;
        next = next->next;
        if (arena->current->size >= min_size) {
            return;
        }
    }

    arena->current->next = jest__arena_alloc_bucket(JEST_MAX(min_size, arena->default_bucket_size));
    arena->current = arena->current->next;
    arena->current->next = NULL;
    return;
}

jest_arena_t *jest_arena_create(size_t default_bucket_size)
{
    jest_arena_t *ret = (jest_arena_t *)malloc(sizeof(*ret));
    if (!ret) JEST_PANIC();

    ret->default_bucket_size = default_bucket_size;
    ret->head = ret->current = NULL;
    ret->offset = 0;
    return ret;
}

void jest_arena_destroy(jest_arena_t *arena)
{
    if (!arena) return;

    jest_arena_bucket_t *next = arena->head->next;
    while (arena->head) {
        free(arena->head);
        arena->head = next;
        if (next) next = next->next;
    }

    free(arena);
}

void jest_arena_reset(jest_arena_t *arena)
{
    arena->current = arena->head;
}

void *jest_arena_alloc_aligned(jest_arena_t *arena, size_t size, size_t align)
{
    if (!arena || !size || !align) return NULL;
    if (!arena->current) jest__arena_next(arena, size + align - 1);

    const uintptr_t current = (uintptr_t)(arena->current->data + arena->offset);
    const uintptr_t mask    = (uintptr_t)(align - 1);
    const uintptr_t padding = ((current & mask) == current)? 0 : align - (current & mask);
    
    if (size + padding > arena->current->size - arena->offset) jest__arena_next(arena, size);
    if (!arena->current) JEST_PANIC();

    void *ret = arena->current->data + arena->offset + padding;
    arena->offset += size + padding;
    memset(ret, 0, size);
    return ret;
}

void *jest_arena_alloc(jest_arena_t *arena, size_t size)
{
    return jest_arena_alloc_aligned(arena, size, 2 * sizeof(void *));
}

jest_string_t jest_str(jest_arena_t *arena, const char *cstr)
{
    if (!arena || !cstr) {
        return JEST_LITERAL(jest_string_t) { 0, NULL };
    }

    jest_string_t ret;
    ret.len = strlen(cstr);
    ret.data = (char *)jest_arena_alloc_aligned(arena, ret.len + 1, 1);
    if (!ret.data) JEST_PANIC();
    memcpy(ret.data, cstr, ret.len + 1);

    return ret;
}

jest_string_t jest_fstr(jest_arena_t *arena, const char *fmt, ...)
{
    if (!arena || !fmt) {
        return JEST_LITERAL(jest_string_t) {0, NULL};
    }

    va_list args;
    va_start(args, fmt);

    int n = vsnprintf(NULL, 0, fmt, args);

    va_end(args);
    va_start(args, fmt);

    jest_string_t ret;
    ret.len = n;
    ret.data = (char *)jest_arena_alloc_aligned(arena, ret.len + 1, 1);
    if (!ret.data) JEST_PANIC();

    vsnprintf((char *)(void *)ret.data, n + 1, fmt, args);
    va_end(args);

    return ret;
}

jest_sb_t jest_sb_create(jest_arena_t *arena, size_t start_cap)
{
    if (!arena || !start_cap) {
        return JEST_LITERAL(jest_sb_t) {0, {0, NULL}};
    }

    jest_sb_t ret;
    ret.cap = start_cap;
    ret.buffer.len = 0;
    ret.buffer.data = jest_arena_alloc_aligned(arena, ret.cap, 1);
    if (!ret.buffer.data) JEST_PANIC();

    return ret;
}

void jest_sb_reserve(jest_arena_t *arena, jest_sb_t *sb, size_t amount)
{
    if (!arena || !amount || !sb) return;
    if (amount <= sb->cap) return;

    jest_sb_t out = jest_sb_create(arena, sb->cap * 2);
    if (sb->buffer.data) memcpy(out.buffer.data, sb->buffer.data, sb->buffer.len + 1);
    *sb = out;
}

void jest_sb_append(jest_arena_t *arena, jest_sb_t *sb, jest_string_t str)
{
    if (!arena || !sb) {
        return;
    }

    if (jest_is_str_null(str) || !str.len) return;

    if (sb->buffer.len + str.len >= sb->cap) {
        jest_sb_reserve(arena, sb, 2 * sb->cap);
    }

    memcpy(sb->buffer.data + sb->buffer.len, str.data, str.len + 1);
    sb->buffer.len += str.len;
}

static void jest__writer_push_scope(jest_arena_t *arena, jest_writer_t *writer, enum jest_scope_type scope)
{
    if (!arena || !writer) return;

    if (writer->scope_stack_len + 1 > writer->scope_stack_cap) {
        writer->scope_stack_cap = (writer->scope_stack_cap)? 2 * writer->scope_stack_cap : 32;
        enum jest_scope_type *new = (enum jest_scope_type *)jest_arena_alloc(arena, sizeof(*new) * writer->scope_stack_cap);
        if (!new) JEST_PANIC();
        memcpy(new, writer->scope_stack, writer->scope_stack_len * sizeof(*writer->scope_stack));
        writer->scope_stack = new;
    }

    writer->scope_stack[writer->scope_stack_len++] = scope;
}

static enum jest_scope_type jest__writer_pop_scope(jest_writer_t *writer, enum jest_scope_type target)
{
    if (!writer) return JEST_SCOPE_INVALID;
    if (!writer->scope_stack_len) return JEST_SCOPE_INVALID;
    if (target != writer->scope_stack[writer->scope_stack_len - 1]) return JEST_SCOPE_INVALID;
    return writer->scope_stack[--writer->scope_stack_len];
}

jest_writer_t jest_writer_create(jest_arena_t *arena)
{
    jest_writer_t ret;
    memset(&ret, 0, sizeof(ret));
    ret.data = jest_sb_create(arena, 1024);
    if (!ret.data.buffer.data) JEST_PANIC();
    return ret;
}

enum jest_writer_error jestw_begin_obejct(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", key.data));
    jest__writer_push_scope(arena, writer, JEST_SCOPE_OBJ);
    jest_sb_append(arena, &writer->data, JEST_STR("{\n"));

    return JEST_WRITER_ERR_BAD_PARAM;
}

enum jest_writer_error jestw_end_object(jest_arena_t *arena, jest_writer_t *writer)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;

    if (jest__writer_pop_scope(writer, JEST_SCOPE_OBJ) == JEST_SCOPE_INVALID) {
        return JEST_WRITER_ERR_MISMATCHED_SCOPE;
    }

    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));
    jest_sb_append(arena, &writer->data, JEST_STR("}"));
    if (writer->scope_stack_len) jest_sb_append(arena, &writer->data, JEST_STR(","));
    jest_sb_append(arena, &writer->data, JEST_STR("\n"));
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_begin_array(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", key.data));
    jest__writer_push_scope(arena, writer, JEST_SCOPE_ARR);
    jest_sb_append(arena, &writer->data, JEST_STR("[\n"));

    return JEST_WRITER_ERR_BAD_PARAM;
}

enum jest_writer_error jestw_end_array(jest_arena_t *arena, jest_writer_t *writer)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;

    if (jest__writer_pop_scope(writer, JEST_SCOPE_ARR) == JEST_SCOPE_INVALID) {
        return JEST_WRITER_ERR_MISMATCHED_SCOPE;
    }

    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));
    jest_sb_append(arena, &writer->data, JEST_STR("]"));
    if (writer->scope_stack_len) jest_sb_append(arena, &writer->data, JEST_STR(","));
    jest_sb_append(arena, &writer->data, JEST_STR("\n"));
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_null(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", key.data));
    jest_sb_append(arena, &writer->data, JEST_STR("null,\n"));

    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_boolean(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key, jest_boolean_t val)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", key.data));
    jest_sb_append(arena, &writer->data, (val)? JEST_STR("true,\n") : JEST_STR("false,\n"));

    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_number(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key, jest_number_t val)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", key.data));

    if (jest_isinf(val) || jest_isnan(val)) {
        if (jest_signbit(val)) jest_sb_append(arena, &writer->data, JEST_STR("-"));
        jest_sb_append(arena, &writer->data, jest_isinf(val)? JEST_STR("Infinity,\n") : JEST_STR("NaN,\n"));
        return JEST_WRITER_SUCCESS;
    }

    jest_sb_append(arena, &writer->data, jest_fstr(arena, "%.15g,\n", val));
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_string(jest_arena_t *arena, jest_writer_t *writer, jest_string_t key, jest_string_t val)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", key.data));
    JEST_TODO("make the resulting string escape special characters");
    // jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\",\n", val.data));

    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_comment(jest_arena_t *arena, jest_writer_t *writer, jest_string_t text)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR(JEST_WRITER_TAB));

    jest_sb_append(arena, &writer->data, jest_fstr(arena, "/* %s */\n", text.data));

    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_newline(jest_arena_t *arena, jest_writer_t *writer)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    jest_sb_append(arena, &writer->data, JEST_STR("\n"));
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_write(const jest_writer_t *writer, const char *path)
{
    if (!path || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    FILE *file = fopen(path, "w");
    if (!file) return JEST_WRITER_ERR_IO_FAIL;

    fwrite(writer->data.buffer.data, 1, writer->data.buffer.len, file);

    fclose(file);
    return JEST_WRITER_SUCCESS;
}

bool jest_is_str_null(jest_string_t str)
{
    return !str.data;
}

#endif // JEST_IMPL
