#ifndef JEST_H_
#define JEST_H_ 1

/* c/c++ version check */
#if defined(__cplusplus) && __cplusplus < 201103L
#   error C++11 or later is required
#elif !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#   error C99 or later is required
#endif

#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__clang__) || defined(__GNUC__)
#   define JEST_GNUC_ATTRIBUTE(x) __attribute__(x)
#   define JEST_TRAP() __builtin_trap()
#   define JEST_LIKELY(expr) __builtin_expect(!!(expr), 1)
#else
#   define JEST_GNUC_ATTRIBUTE(x)
#   define JEST_TRAP() abort()
#   define JEST_LIKELY(expr) (expr)
#endif

#if JEST_NO_ASSERT
#   define JEST_ASSERT(expr, ...)
#else
#   define JEST_ASSERT(expr, ...) \
    do { \
        if (JEST_LIKELY(expr)) break; \
        fprintf(stderr, "[%s:%d: %s] Assertion failed: ", __FILE__, __LINE__, __func__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } while (0)
#endif

#define JEST_UNREACHABLE() \
do { \
    fprintf(stderr, "[%s:%d: %s] JEST_UNREACHABLE() reached.\n", __FILE__, __LINE__, __func__); \
    JEST_TRAP(); \
} while (0)

#define JEST_TODO(msg) \
do { \
    fprintf(stderr, "[%s:%d: %s] TODO: " msg "\n", __FILE__, __LINE__, __func__); \
    JEST_TRAP(); \
} while (0)

#define JEST_MIN(x, y) (((x) < (y))? (x) : (y))
#define JEST_MAX(x, y) (((x) > (y))? (x) : (y))

#ifndef JEST_NAN
#   if (defined(__clang__) || defined(__GNUC__)) && !JEST_USE_MATH_H
#       define JEST_NAN __builtin_nanf("")
#   else
#       include <math.h>
#       define JEST_REQUIRES_MATH_H 1
#       define JEST_NAN NAN
#   endif
#endif // !JEST_NAN

#ifndef JEST_INFINITY
#   if (defined(__clang__) || defined(__GNUC__)) && !JEST_USE_MATH_H
#       define JEST_INFINITY __builtin_inff()
#   else
#       include <math.h>
#       define JEST_REQUIRES_MATH_H 1
#       define JEST_INFINITY INFINITY
#   endif
#endif // !JEST_INF

#ifdef __cplusplus
#   define JEST_LITERAL type
#else
#   define JEST_LITERAL(type) (type)
#endif

typedef struct jest_arena_t jest_arena_t;

#define JEST_STR(s) JEST_LITERAL(jest_string_t) {sizeof("" s "") - 1, (char *)(void *)"" s ""}
#define JEST_STR_NULL JEST_LITERAL(jest_string_t) {0, NULL}
#define JESTW_NO_KEY  JEST_STR_NULL
typedef struct jest_string_t { size_t len; char *data; } jest_string_t;
typedef struct {
    size_t cap; 
    jest_string_t buffer;
} jest_sb_t;

JEST_GNUC_ATTRIBUTE((__warn_unused_result__))
void *jest_arena_alloc_aligned(jest_arena_t *arena, size_t size, size_t align);

JEST_GNUC_ATTRIBUTE((__warn_unused_result__))
void *jest_arena_alloc(jest_arena_t *arena, size_t size);

jest_string_t jest_str(jest_arena_t *arena, const char *cstr);
jest_string_t jest_strn(jest_arena_t *arena, const char *cstr, size_t len);

JEST_GNUC_ATTRIBUTE((__format__(__printf__, 2, 3)))
jest_string_t jest_fstr(jest_arena_t *arena, const char *fmt, ...);

bool jest_is_str_null(jest_string_t str);
jest_string_t jest_char(jest_arena_t *arena, char c);

typedef struct jest_typed_value_t jest_typed_value_t;

typedef struct jest_arena_bucket_t {
    struct jest_arena_bucket_t *next;
    size_t size;
    char data[1 /* flexible array */];
} jest_arena_bucket_t;

struct jest_arena_t {
    size_t default_bucket_size;
    jest_arena_bucket_t *head;
    jest_arena_bucket_t *current;
    size_t offset;
};

JEST_GNUC_ATTRIBUTE((__warn_unused_result__))
jest_arena_t *jest_arena_create(size_t default_bucket_size);

void jest_arena_destroy(jest_arena_t *arena);
void jest_arena_reset(jest_arena_t *arena);

jest_sb_t jest_sb_create(jest_arena_t *arena, size_t start_cap);
void jest_sb_reserve(jest_arena_t *arena, jest_sb_t *sb, size_t amount);
void jest_sb_append(jest_arena_t *arena, jest_sb_t *sb, jest_string_t str);
char jest_sb_pop(jest_sb_t *sb, size_t idx);

enum jest_scope_type {
    JEST_SCOPE_INVALID = 0,
    JEST_SCOPE_OBJ,
    JEST_SCOPE_ARR
};

enum jest_writer_error {
    JEST_WRITER_SUCCESS = 0,
    JEST_WRITER_ERR_MISMATCHED_SCOPE,
    JEST_WRITER_ERR_BAD_PARAM,
    JEST_WRITER_ERR_IO
};

typedef struct {
    jest_sb_t data;
    size_t idx_last_comma; // index of last comma, used to remove trailing commas
    size_t scope_stack_cap;
    size_t scope_stack_len;
    enum jest_scope_type *scope_stack;
} jestw_t;

// jest writer/serializer functions
jestw_t jestw_create(jest_arena_t *arena);
enum jest_writer_error jestw_kv_begin_object(jest_arena_t *arena, jestw_t *writer, jest_string_t key);
enum jest_writer_error jestw_end_object(jest_arena_t *arena, jestw_t *writer);
enum jest_writer_error jestw_kv_begin_array(jest_arena_t *arena, jestw_t *writer, jest_string_t key);
enum jest_writer_error jestw_end_array(jest_arena_t *arena, jestw_t *writer);
enum jest_writer_error jestw_kv_null(jest_arena_t *arena, jestw_t *writer, jest_string_t key);
enum jest_writer_error jestw_kv_bool(jest_arena_t *arena, jestw_t *writer, jest_string_t key, bool val);
enum jest_writer_error jestw_kv_uint(jest_arena_t *arena, jestw_t *writer, jest_string_t key, uintmax_t val);
enum jest_writer_error jestw_kv_int(jest_arena_t *arena, jestw_t *writer, jest_string_t key, intmax_t val);
enum jest_writer_error jestw_kv_float(jest_arena_t *arena, jestw_t *writer, jest_string_t key, double val);
enum jest_writer_error jestw_kv_string(jest_arena_t *arena, jestw_t *writer, jest_string_t key, jest_string_t val);
enum jest_writer_error jestw_comment(jest_arena_t *arena, jestw_t *writer, jest_string_t text);
enum jest_writer_error jestw_newline(jest_arena_t *arena, jestw_t *writer);
enum jest_writer_error jestw_writef(const jestw_t *writer, FILE *file);
enum jest_writer_error jestw_write(const jestw_t *writer, const char *path);

extern inline enum jest_writer_error jestw_begin_object(jest_arena_t *arena, jestw_t *writer);
extern inline enum jest_writer_error jestw_begin_array(jest_arena_t *arena, jestw_t *writer);
extern inline enum jest_writer_error jestw_null(jest_arena_t *arena, jestw_t *writer);
extern inline enum jest_writer_error jestw_bool(jest_arena_t *arena, jestw_t *writer, bool val);
extern inline enum jest_writer_error jestw_uint(jest_arena_t *arena, jestw_t *writer, uint64_t val);
extern inline enum jest_writer_error jestw_int(jest_arena_t *arena, jestw_t *writer, int64_t val);
extern inline enum jest_writer_error jestw_float(jest_arena_t *arena, jestw_t *writer, double val);
extern inline enum jest_writer_error jestw_string(jest_arena_t *arena, jestw_t *writer, jest_string_t val);

typedef struct { int line, col; } jest_file_pos_t;

// all single-char tokens are represented as their ascii value
enum jest_lexeme_type {
    JEST_LEXEME_INVALID = 0,
    JEST_LEXEME_IDENT = 256,
    JEST_LEXEME_UINT,
    JEST_LEXEME_INT,
    JEST_LEXEME_FLOAT,
    JEST_LEXEME_STRING
};

enum jest_lexer_error {
    JEST_LEXER_SUCCESS = 0,
    JEST_LEXER_ERR_MISMATCHED_SCOPE,
    JEST_LEXER_ERR_BAD_PARAM,
    JEST_LEXER_ERR_IO,
    JEST_LEXER_ERR_UNEXPECTED_TOKEN
};

typedef struct {
    jest_file_pos_t pos;
    enum jest_lexeme_type lexeme_type;
    const char *current;
    const char *end;
    const char *lexeme_start;
    const char *lexeme_end;

    union {
        uintmax_t as_uint;
        intmax_t as_int;
        double as_float;
    } lexeme_val;
} jest_lexer_t;

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
    JEST_TYPE_BOOL,

    // numeric types
    JEST_TYPE_UINT,
    JEST_TYPE_INT,
    JEST_TYPE_FLOAT,

    JEST_TYPE_STRING,
    JEST_TYPE_ARRAY,
    JEST_TYPE_OBJECT
};

struct jest_typed_value_t {
    enum jest_type type;
    union {
        bool           as_bool;
        uint64_t       as_uint;
        int64_t        as_int;
        double         as_float;
        jest_string_t  as_string;
        jest_array_t   as_array;
        jest_object_t  as_object;
    } v;
};

#if JEST_REQUIRES_MATH_H
#   define JEST_SIGNBIT signbit
#   define JEST_ISINF isinf
#   define JEST_ISNAN isnan
#else
#   define JEST_SIGNBIT jest__signbit
#   define JEST_ISINF jest__isinf
#   define JEST_ISNAN jest__isnan

JEST_GNUC_ATTRIBUTE((__unused__))
static int jest__signbit(double x)
{
    return *((uint64_t *)(void *)&x) >> 63;
}

JEST_GNUC_ATTRIBUTE((__unused__))
static int jest__isinf(double x)
{
    return (*((uint64_t *)(void *)&x) & INT64_MAX) == UINT64_C(0x7FF0000000000000);
}

JEST_GNUC_ATTRIBUTE((__unused__))
static int jest__isnan(double x)
{
    return (*((uint64_t *)(void *)&x) & INT64_MAX) > UINT64_C(0x7FF0000000000000);
}
#endif

#endif // !JEST_H_

#ifdef JEST_IMPL

// helper structs for utf-8 handling
typedef struct { bool valid; int len; uint32_t value; } jest__codepoint_info;
typedef struct { bool valid; int len; char value[4]; }  jest__utf8_info;

// helper functions for converting codepoints to/from utf-8 bytes
static jest__codepoint_info jest__utf8_to_codepoint_info(const char *utf8_sequence, size_t max_len);
static jest_string_t jest__escape_string(jest_arena_t *arena, jest_string_t str);

// helper functions escaping/unescaping strings so that they can be properly written to/read from a file
static jest_string_t jest__escape_string(jest_arena_t *arena, jest_string_t str);
static jest_string_t jest__unescape_string(jest_arena_t *arena, jest_string_t str);

// helper functions for arenas
static jest_arena_bucket_t *jest__arena_alloc_bucket(size_t size);
static void jest__arena_next(jest_arena_t *arena, size_t min_size);

// helper functions for managing a writer's scope stack
static void jest__writer_push_scope(jest_arena_t *arena, jestw_t *writer, enum jest_scope_type scope);
static enum jest_scope_type jest__writer_pop_scope(jestw_t *writer, enum jest_scope_type target);

// helper function for managing commas in writers
static void jest__writer_comma(jest_arena_t *arena, jestw_t *writer);

jest_string_t jest_str(jest_arena_t *arena, const char *cstr)
{
    if (!arena || !cstr) {
        return JEST_STR_NULL;
    }

    return jest_strn(arena, cstr, strlen(cstr));
}

jest_string_t jest_strn(jest_arena_t *arena, const char *cstr, size_t len)
{
    if (!arena || !cstr) {
        return JEST_STR_NULL;
    }

    jest_string_t ret;
    ret.len = len;
    ret.data = (char *)jest_arena_alloc_aligned(arena, ret.len + 1, 1);
    JEST_ASSERT(ret.data, "failed to allocate character buffer in arena");
    memcpy(ret.data, cstr, ret.len);

    return ret;
}

jest_string_t jest_fstr(jest_arena_t *arena, const char *fmt, ...)
{
    if (!arena || !fmt) {
        return JEST_STR_NULL;
    }

    va_list args;
    va_start(args, fmt);

    int n = vsnprintf(NULL, 0, fmt, args);

    va_end(args);
    va_start(args, fmt);

    jest_string_t ret;
    ret.len = n;
    ret.data = (char *)jest_arena_alloc_aligned(arena, ret.len + 1, 1);
    JEST_ASSERT(ret.data, "failed to allocate character buffer in arena");

    vsnprintf((char *)(void *)ret.data, n + 1, fmt, args);
    va_end(args);

    return ret;
}

bool jest_is_str_null(jest_string_t str)
{
    return !str.data;
}

jest_string_t jest_char(jest_arena_t *arena, char c)
{
    if (!arena) return JEST_STR_NULL;

    jest_string_t ret = jest_str(arena, " ");
    ret.data[0] = c;

    return ret;
}

jest_sb_t jest_sb_create(jest_arena_t *arena, size_t start_cap)
{
    if (!arena || !start_cap) {
        return JEST_LITERAL(jest_sb_t) {0, JEST_STR_NULL};
    }

    jest_sb_t ret;
    ret.cap = start_cap;
    ret.buffer.len = 0;
    ret.buffer.data = (char *)jest_arena_alloc_aligned(arena, ret.cap, 1);
    JEST_ASSERT(ret.buffer.data, "failed to allocate buffer for string builder");

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
    if (sb->cap == 0 || !sb->buffer.data) jest_sb_reserve(arena, sb, 256);

    if (sb->buffer.len + str.len >= sb->cap) {
        jest_sb_reserve(arena, sb, 2 * sb->cap);
    }

    // NOLINTNEXTLINE(clang-analyzer-unix.cstring.NullArg)
    memcpy(sb->buffer.data + sb->buffer.len, str.data, str.len);
    sb->buffer.len += str.len;
}

char jest_sb_pop(jest_sb_t *sb, size_t idx)
{
    if (!sb) return (char)0xff;

    if (idx == sb->buffer.len - 1) {
        --sb->buffer.len;
        return sb->buffer.data[sb->buffer.len];
    }

    const char ret = sb->buffer.data[idx];
    memmove(sb->buffer.data + idx, sb->buffer.data + idx + 1, sb->buffer.len - idx);
    --sb->buffer.len;
    return ret;
}

jest_arena_t *jest_arena_create(size_t default_bucket_size)
{
    jest_arena_t *ret = (jest_arena_t *)malloc(sizeof(*ret));
    JEST_ASSERT(ret, "nomem");

    ret->default_bucket_size = default_bucket_size;
    ret->head = ret->current = NULL;
    ret->offset = 0;
    return ret;
}

void jest_arena_destroy(jest_arena_t *arena)
{
    if (!arena) return;

    jest_arena_bucket_t *next = (arena->head)? arena->head->next : NULL;
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
    arena->offset = 0;
}

void *jest_arena_alloc_aligned(jest_arena_t *arena, size_t size, size_t align)
{
    if (!arena || !size || !align) return NULL;
    if (!arena->current) jest__arena_next(arena, size + align - 1);

    const uintptr_t current = (uintptr_t)(arena->current->data + arena->offset);
    const uintptr_t mask    = (uintptr_t)(align - 1);
    const uintptr_t padding = (current & mask)? align - (current & mask) : 0;

    // printf("arena->current->size - arena->offset = %zu\n", arena->current->size - arena->offset);
    if (size + padding > arena->current->size - arena->offset) jest__arena_next(arena, size);
    JEST_ASSERT(arena->current, "failed to allocate new bucket for arena");

    void *ret = arena->current->data + arena->offset + padding;
    arena->offset += size + padding;

    // NOLINTNEXTLINE(clang-analyzer-unix.cstring.NullArg)
    memset(ret, 0, size);
    return ret;
}

void *jest_arena_alloc(jest_arena_t *arena, size_t size)
{
    return jest_arena_alloc_aligned(arena, size, 2 * sizeof(void *));
}

jestw_t jestw_create(jest_arena_t *arena)
{
    jestw_t ret;
    memset(&ret, 0, sizeof(ret));
    ret.data = jest_sb_create(arena, 1024);
    JEST_ASSERT(ret.data.buffer.data, "failed to create writer");
    return ret;
}

enum jest_writer_error jestw_kv_begin_object(jest_arena_t *arena, jestw_t *writer, jest_string_t key)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", jest__escape_string(arena, key).data));
    jest__writer_push_scope(arena, writer, JEST_SCOPE_OBJ);
    jest_sb_append(arena, &writer->data, JEST_STR("{"));
    jestw_newline(arena, writer);

    return JEST_WRITER_ERR_BAD_PARAM;
}

enum jest_writer_error jestw_end_object(jest_arena_t *arena, jestw_t *writer)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;

    if (jest__writer_pop_scope(writer, JEST_SCOPE_OBJ) == JEST_SCOPE_INVALID) {
        return JEST_WRITER_ERR_MISMATCHED_SCOPE;
    }

    jest_sb_pop(&writer->data, writer->idx_last_comma);

    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));
    jest_sb_append(arena, &writer->data, JEST_STR("}"));
    jest__writer_comma(arena, writer);
    jestw_newline(arena, writer);
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_kv_begin_array(jest_arena_t *arena, jestw_t *writer, jest_string_t key)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", jest__escape_string(arena, key).data));
    jest__writer_push_scope(arena, writer, JEST_SCOPE_ARR);
    jest_sb_append(arena, &writer->data, JEST_STR("["));
    jestw_newline(arena, writer);

    return JEST_WRITER_ERR_BAD_PARAM;
}

enum jest_writer_error jestw_end_array(jest_arena_t *arena, jestw_t *writer)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;

    if (jest__writer_pop_scope(writer, JEST_SCOPE_ARR) == JEST_SCOPE_INVALID) {
        return JEST_WRITER_ERR_MISMATCHED_SCOPE;
    }

    jest_sb_pop(&writer->data, writer->idx_last_comma);

    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));
    jest_sb_append(arena, &writer->data, JEST_STR("]"));
    jest__writer_comma(arena, writer);
    jestw_newline(arena, writer);
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_kv_null(jest_arena_t *arena, jestw_t *writer, jest_string_t key)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", jest__escape_string(arena, key).data));
    jest_sb_append(arena, &writer->data, JEST_STR("null"));
    jest__writer_comma(arena, writer);
    jestw_newline(arena, writer);

    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_kv_bool(jest_arena_t *arena, jestw_t *writer, jest_string_t key, bool val)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", jest__escape_string(arena, key).data));
    jest_sb_append(arena, &writer->data, (val)? JEST_STR("true") : JEST_STR("false"));
    jest__writer_comma(arena, writer);
    jestw_newline(arena, writer);

    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_kv_uint(jest_arena_t *arena, jestw_t *writer, jest_string_t key, uintmax_t val)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", jest__escape_string(arena, key).data));
    if (JEST_ISINF(val) || JEST_ISNAN(val)) {
        if (JEST_SIGNBIT(val)) jest_sb_append(arena, &writer->data, JEST_STR("-"));
        jest_sb_append(arena, &writer->data, JEST_ISINF(val)? JEST_STR("Infinity") : JEST_STR("NaN"));
        jest__writer_comma(arena, writer);
        jestw_newline(arena, writer);
        return JEST_WRITER_SUCCESS;
    }

    jest_sb_append(arena, &writer->data, jest_fstr(arena, "%ju", val));
    jest__writer_comma(arena, writer);
    jestw_newline(arena, writer);
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_kv_int(jest_arena_t *arena, jestw_t *writer, jest_string_t key, intmax_t val)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", jest__escape_string(arena, key).data));
    if (JEST_ISINF(val) || JEST_ISNAN(val)) {
        if (JEST_SIGNBIT(val)) jest_sb_append(arena, &writer->data, JEST_STR("-"));
        jest_sb_append(arena, &writer->data, JEST_ISINF(val)? JEST_STR("Infinity") : JEST_STR("NaN"));
        jest__writer_comma(arena, writer);
        jestw_newline(arena, writer);
        return JEST_WRITER_SUCCESS;
    }

    jest_sb_append(arena, &writer->data, jest_fstr(arena, "%jd", val));
    jest__writer_comma(arena, writer);
    jestw_newline(arena, writer);
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_kv_float(jest_arena_t *arena, jestw_t *writer, jest_string_t key, double val)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", jest__escape_string(arena, key).data));
    if (JEST_ISINF(val) || JEST_ISNAN(val)) {
        if (JEST_SIGNBIT(val)) jest_sb_append(arena, &writer->data, JEST_STR("-"));
        jest_sb_append(arena, &writer->data, JEST_ISINF(val)? JEST_STR("Infinity") : JEST_STR("NaN"));
        jest__writer_comma(arena, writer);
        jestw_newline(arena, writer);
        return JEST_WRITER_SUCCESS;
    }

    jest_sb_append(arena, &writer->data, jest_fstr(arena, "%.15g", val));
    jest__writer_comma(arena, writer);
    jestw_newline(arena, writer);
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_kv_string(jest_arena_t *arena, jestw_t *writer, jest_string_t key, jest_string_t val)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    if (!jest_is_str_null(key)) jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\": ", jest__escape_string(arena, key).data));
    jest_sb_append(arena, &writer->data, jest_fstr(arena, "\"%s\"", jest__escape_string(arena, val).data));
    jest__writer_comma(arena, writer);
    jestw_newline(arena, writer);

    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_comment(jest_arena_t *arena, jestw_t *writer, jest_string_t text)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    for (size_t i = 0; i < writer->scope_stack_len; ++i) jest_sb_append(arena, &writer->data, JEST_STR("\t"));

    jest_sb_append(arena, &writer->data, jest_fstr(arena, "/* %s */\n", text.data));

    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_newline(jest_arena_t *arena, jestw_t *writer)
{
    if (!arena || !writer) return JEST_WRITER_ERR_BAD_PARAM;
    jest_sb_append(arena, &writer->data, JEST_STR("\n"));
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_writef(const jestw_t *writer, FILE *file)
{
    if (!writer || !file) return JEST_WRITER_ERR_BAD_PARAM;
    if (writer->scope_stack_len) return JEST_WRITER_ERR_MISMATCHED_SCOPE;

    fwrite(writer->data.buffer.data, 1, writer->data.buffer.len, file);
    return JEST_WRITER_SUCCESS;
}

enum jest_writer_error jestw_write(const jestw_t *writer, const char *path)
{
    if (!writer || !path) return JEST_WRITER_ERR_BAD_PARAM;
    FILE *file = fopen(path, "w");
    if (!file) return JEST_WRITER_ERR_IO;

    const enum jest_writer_error err = jestw_writef(writer, file);
    fclose(file);
    return err;
}

extern inline enum jest_writer_error jestw_begin_object(jest_arena_t *arena, jestw_t *writer)
{
    return jestw_kv_begin_object(arena, writer, JESTW_NO_KEY);
}

extern inline enum jest_writer_error jestw_begin_array(jest_arena_t *arena, jestw_t *writer)
{
    return jestw_kv_begin_array(arena, writer, JESTW_NO_KEY);
}

extern inline enum jest_writer_error jestw_null(jest_arena_t *arena, jestw_t *writer)
{
    return jestw_kv_null(arena, writer, JESTW_NO_KEY);
}

extern inline enum jest_writer_error jestw_bool(jest_arena_t *arena, jestw_t *writer, bool val)
{
    return jestw_kv_bool(arena, writer, JESTW_NO_KEY, val);
}

extern inline enum jest_writer_error jestw_uint(jest_arena_t *arena, jestw_t *writer, uint64_t val)
{
    return jestw_kv_uint(arena, writer, JESTW_NO_KEY, val);
}

extern inline enum jest_writer_error jestw_int(jest_arena_t *arena, jestw_t *writer, int64_t val)
{
    return jestw_kv_int(arena, writer, JESTW_NO_KEY, val);
}

extern inline enum jest_writer_error jestw_float(jest_arena_t *arena, jestw_t *writer, double val)
{
    return jestw_kv_float(arena, writer, JESTW_NO_KEY, val);
}

extern inline enum jest_writer_error jestw_string(jest_arena_t *arena, jestw_t *writer, jest_string_t val)
{
    return jestw_kv_string(arena, writer, JESTW_NO_KEY, val);
}

static jest__codepoint_info jest__utf8_to_codepoint_info(const char *utf8_sequence, size_t max_len)
{
    int len = 0;
    uint32_t codepoint = 0;

    if (!utf8_sequence || !max_len) goto lbl_invalid;

    if      (max_len >= 1 && (uint8_t)utf8_sequence[0] < 0x80)           len = 1;
    else if (max_len >= 2 && ((uint8_t)utf8_sequence[0] & 0xf0)  < 0xe0) len = 2;
    else if (max_len >= 3 && ((uint8_t)utf8_sequence[0] & 0xf0)  < 0xf0) len = 3;
    else if (max_len >= 4 && ((uint8_t)utf8_sequence[0] & 0xf0) == 0xf0) len = 4;
    else goto lbl_invalid;

    codepoint = (len == 1)? (uint32_t)utf8_sequence[0] :
                (len == 2)? ((uint32_t)utf8_sequence[0] & 0x1f) :
                (len == 3)? ((uint32_t)utf8_sequence[0] & 0x0f) : ((uint32_t)utf8_sequence[0] & 0x07);

    for (int i = 1; i < len; ++i) {
        codepoint <<= 6;
        codepoint += (uint8_t)utf8_sequence[i] & 0x3f;
        if (((uint8_t)utf8_sequence[i] & 0xf0) >= 0xc0) goto lbl_invalid;
        if (!((uint8_t)utf8_sequence[i] & 0xf0)) goto lbl_invalid;
    }

    // handle overlong encodings
    if (len == 2 && codepoint < 0x80)    goto lbl_invalid;
    if (len == 3 && codepoint < 0x800)   goto lbl_invalid;
    if (len == 4 && codepoint < 0x10000) goto lbl_invalid;

    return JEST_LITERAL(jest__codepoint_info) {true, len, codepoint};

lbl_invalid:
    return JEST_LITERAL(jest__codepoint_info) {false, 1, 0xfffd};
}

static jest__utf8_info jest__codepoint_to_utf8_info(uint32_t codepoint)
{
    if (codepoint > 0x10ffff) {
        return JEST_LITERAL(jest__utf8_info) {
            true, 1, {(char)0xef, (char)0xbf, (char)0xbd} // U+FFFD as utf-8 bytes
        };
    }

    if (codepoint <= 0x7f) {
        return JEST_LITERAL(jest__utf8_info) {
            true, 1, {(char)codepoint}
        }; 
    }

    if (codepoint <= 0x7ff) {
        return JEST_LITERAL(jest__utf8_info) {
            true, 2,
            {
                (char)(((codepoint >> 6) & 0x1f) | 0xc0),
                (char)((codepoint        & 0x3f) | 0x80)
            }
        };
    }

    if (codepoint <= 0xffff) {
        return JEST_LITERAL(jest__utf8_info) {
            true, 3,
            {
                (char)(((codepoint >> 12) & 0x0f) | 0xe0),
                (char)(((codepoint >>  6) & 0x3f) | 0x80),
                (char)((codepoint         & 0x3f) | 0x80)
            }
        };
    }

    return JEST_LITERAL(jest__utf8_info) {
        true, 4,
        {
            (char)(((codepoint >> 18) & 0x07) | 0xf0),
            (char)(((codepoint >> 12) & 0x3f) | 0x80),
            (char)(((codepoint >>  6) & 0x3f) | 0x80),
            (char)((codepoint         & 0x3f) | 0x80)
        }
    };
}

static jest_string_t jest__escape_string(jest_arena_t *arena, jest_string_t str)
{
    if (!arena || jest_is_str_null(str)) return JEST_STR_NULL;

    jest_sb_t sb = jest_sb_create(arena, str.len * 2);

    for (size_t i = 0; i < str.len; ++i) {
        switch (str.data[i]) {
            case '\'': jest_sb_append(arena, &sb, JEST_STR("\\\"")); continue;
            case '"':  jest_sb_append(arena, &sb, JEST_STR("\\'"));  continue;
            case '\\': jest_sb_append(arena, &sb, JEST_STR("\\\\")); continue;
            case '\b': jest_sb_append(arena, &sb, JEST_STR("\\b"));  continue;
            case '\f': jest_sb_append(arena, &sb, JEST_STR("\\f"));  continue;
            case '\n': jest_sb_append(arena, &sb, JEST_STR("\\n"));  continue;
            case '\r': jest_sb_append(arena, &sb, JEST_STR("\\r"));  continue;
            case '\t': jest_sb_append(arena, &sb, JEST_STR("\\t"));  continue;
            case '\v': jest_sb_append(arena, &sb, JEST_STR("\\v"));  continue;
            case '\0': jest_sb_append(arena, &sb, JEST_STR("\\0"));  continue;
            case '/':  jest_sb_append(arena, &sb, JEST_STR("\\/"));  continue;
            default: break;
        }

        jest__codepoint_info info = jest__utf8_to_codepoint_info(&str.data[i], str.len - 1);
        if (info.value > 0x7f || iscntrl(str.data[i]) || !isprint(str.data[i])) {
            if (!info.valid || info.value <= 0xff) {
                jest_sb_append(arena, &sb, jest_fstr(arena, "\\x%.2" PRIx8 "", (uint8_t)str.data[i]));
                i += info.len - 1;
                continue;
            }

            if (info.value <= 0xffff) {
                jest_sb_append(arena, &sb, jest_fstr(arena, "\\u%.4" PRIx32 "", info.value));
                i += info.len - 1;
                continue;
            }

            info.value -= 0x10000;
            uint16_t hi = (info.value >> 10) + 0xd800;
            uint16_t lo = (info.value & 0x3ff) + 0xdc00;

            jest_sb_append(arena, &sb, jest_fstr(arena, "\\u%.4" PRIx16 "\\u%.4" PRIx16 "", hi, lo));
            i += info.len - 1;
            continue;
        }

        jest_sb_append(arena, &sb, jest_char(arena, str.data[i]));
    }

    return sb.buffer;
}

static jest_string_t jest__unescape_string(jest_arena_t *arena, jest_string_t str)
{
    if (!arena || jest_is_str_null(str)) return JEST_STR_NULL;

    jest_sb_t sb = jest_sb_create(arena, str.len);

    for (size_t i = 0; i < str.len; ++i) {
        if (str.data[i] != '\\' || str.len - i < 2) {
lbl_write_raw:
            jest_sb_append(arena, &sb, jest_char(arena, str.data[i]));
            continue;
        }

        ++i;
        switch (str.data[i]) {
            case '\'': jest_sb_append(arena, &sb, JEST_STR("'"));  continue;
            case '"':  jest_sb_append(arena, &sb, JEST_STR("\"")); continue;
            case '\\': jest_sb_append(arena, &sb, JEST_STR("\\")); continue;
            case 'b':  jest_sb_append(arena, &sb, JEST_STR("\b")); continue;
            case 'f':  jest_sb_append(arena, &sb, JEST_STR("\f")); continue;
            case 'n':  jest_sb_append(arena, &sb, JEST_STR("\n")); continue;
            case 'r':  jest_sb_append(arena, &sb, JEST_STR("\r")); continue;
            case 't':  jest_sb_append(arena, &sb, JEST_STR("\t")); continue;
            case 'v':  jest_sb_append(arena, &sb, JEST_STR("\v")); continue;
            case '0':  jest_sb_append(arena, &sb, JEST_STR("\0")); continue;
            case '\r':
            case '\n': { while (isspace(str.data[i])) ++i; } --i;  continue;
            default: break;
        }

        int expected = (str.data[i] == 'x')? 4 :
                       (str.data[i] == 'u')? 6 : 0;
        if (!expected || str.len - i < (unsigned)expected) goto lbl_write_raw;

        --i;
        int n = 0;
        uint32_t codepoint = 0;
        if (expected == 4) {
            sscanf(&str.data[i], "\\x%2" SCNx32 "%n", &codepoint, &n);
            if (n < expected) {
                ++i;
                goto lbl_write_raw;
            }
        } else {
            sscanf(&str.data[i], "\\u%4" SCNx32 "%n", &codepoint, &n);
            if (n < expected) {
                ++i;
                goto lbl_write_raw;
            }

            // handle utf-16 surrogate pairs
            if (0xd800 <= codepoint && codepoint <= 0xdbff && str.len - i - n >= 6) { // codepoint is high surrogate
                uint16_t lo;
                sscanf(&str.data[i + 6], "\\u%4" SCNx16 "%n", &lo, &n);
                if (n < 6) goto lbl_write_unicode;
                if (0xdc00 <= lo && lo <= 0xdfff) { // lo is valid low surrogate
                    codepoint = ((codepoint - 0xd800) << 10) + ((lo - 0xdc00)) + 0x10000;
                }

                n += 6; // make sure to add the 6 back on to the n because its old value was overwritten
            }
        }

        jest__utf8_info info;

lbl_write_unicode:
        info = jest__codepoint_to_utf8_info(codepoint);
        if (!info.valid) goto lbl_write_raw;

        jest_sb_append(arena, &sb, jest_strn(arena, info.value, info.len));
        i += n - 1;
    }

    return sb.buffer;
}

static jest_arena_bucket_t *jest__arena_alloc_bucket(size_t size)
{
    jest_arena_bucket_t *ret = (jest_arena_bucket_t *)malloc(sizeof(*ret) + size - 1);
    JEST_ASSERT(ret, "nomem");

    ret->size = size;
    ret->next = NULL;

    return ret;
}

static void jest__arena_next(jest_arena_t *arena, size_t min_size)
{
    if (!arena) return;

    arena->offset = 0;
    jest_arena_bucket_t *next = (arena->head)? arena->head->next : NULL;
    while (next) {
        arena->current = next;
        next = next->next;
        if (arena->current->size >= min_size) {
            return;
        }
    }

    next = jest__arena_alloc_bucket(JEST_MAX(min_size, arena->default_bucket_size));
    if (!arena->current) {
        arena->head = arena->current = next;
        return;
    }

    arena->current->next = next;
    arena->current = next;
}

static void jest__writer_push_scope(jest_arena_t *arena, jestw_t *writer, enum jest_scope_type scope)
{
    if (!arena || !writer) return;

    if (writer->scope_stack_len + 1 > writer->scope_stack_cap) {
        writer->scope_stack_cap = (writer->scope_stack_cap)? 2 * writer->scope_stack_cap : 32;
        enum jest_scope_type *new_mem = (enum jest_scope_type *)jest_arena_alloc(arena, sizeof(*new_mem) * writer->scope_stack_cap);
        JEST_ASSERT(new_mem, "failed to allocate new memory for writer scope stack");
        memcpy(new_mem, writer->scope_stack, writer->scope_stack_len * sizeof(*writer->scope_stack));
        writer->scope_stack = new_mem;
    }

    writer->scope_stack[writer->scope_stack_len++] = scope;
}

static enum jest_scope_type jest__writer_pop_scope(jestw_t *writer, enum jest_scope_type target)
{
    if (!writer) return JEST_SCOPE_INVALID;
    if (!writer->scope_stack_len) return JEST_SCOPE_INVALID;
    if (target != writer->scope_stack[writer->scope_stack_len - 1]) return JEST_SCOPE_INVALID;
    return writer->scope_stack[--writer->scope_stack_len];
}

static void jest__writer_comma(jest_arena_t *arena, jestw_t *writer)
{
    if (!writer->scope_stack_len) return;
    writer->idx_last_comma = writer->data.buffer.len;
    jest_sb_append(arena, &writer->data, JEST_STR(","));
}

#endif // JEST_IMPL
