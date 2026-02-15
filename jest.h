#ifndef JEST_H_
#define JEST_H_

#include <inttypes.h>
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

#define JEST_STR(s) JEST_LITERAL(jest_string_t) {sizeof("" s "") - 1, (uint8_t *)(void *)("" s "")}
typedef struct { size_t len; uint8_t *data; } jest_string_t;
typedef struct { size_t cap; jest_string_t buffer; } jest_string_builder_t;

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

#if JEST_REQUIRES_MATH_H
#   define jest_signbit signbit
#   define jest_isinf isinf
#   define jest_isnan isnan
#else
#   define jest_signbit jest__signbit
#   define jest_isinf jest__isinf
#   define jest_isnan jest__isnan

static int jest__signbit(double x)
{
    return *((uint64_t *)(void *)&x) >> 63;
}

static int jest__isinf(double x)
{
    return (*((uint64_t *)(void *)&x) & INT64_MAX) == UINT64_C(0x7FF0000000000000);
}

static int jest__isnan(double x)
{
    return (*((uint64_t *)(void *)&x) & INT64_MAX) > UINT64_C(0x7FF0000000000000);
}
#endif

typedef struct jest_arenabucket_t {
    struct jest_arenabucket_t *next;
    size_t size;
    char data[1 /* flexible array */];
} jest_arenabucket_t;

typedef struct {
    size_t default_bucket_size;
    jest_arenabucket_t *head;
    jest_arenabucket_t *current;
    size_t offset;
} jest_arena_t;

jest__gnuc_attribute((__warn_unused_result__))
jest_arena_t *jest_arena_create(size_t default_bucket_size);

void jest_arena_destroy(jest_arena_t *arena);
void jest_arena_reset(jest_arena_t *arena);

jest__gnuc_attribute((__warn_unused_result__))
void *jest_arena_alloc_aligned(jest_arena_t *arena, size_t size, size_t align);

jest__gnuc_attribute((__warn_unused_result__))
void *jest_arena_alloc(jest_arena_t *arena, size_t size);

#endif // !JEST_H_

#ifdef JEST_IMPL

static jest_arenabucket_t *jest__arena_alloc_bucket(size_t size)
{
    jest_arenabucket_t *ret = (jest_arenabucket_t *)malloc(sizeof(*ret) + size - 1);
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

    jest_arenabucket_t *next = arena->head->next;
    while (next) {
        arena->current = next;
        next = next->next;
        if (arena->current->size - arena->offset >= min_size) {
            return;
        }
    }

    arena->current->next = jest__arena_alloc_bucket(JEST_MAX(min_size, arena->default_bucket_size));
    arena->current = arena->current->next;
    return;
}

jest_arena_t *jest_arena_create(size_t default_bucket_size)
{
    jest_arena_t *ret = (jest_arena_t *)malloc(sizeof(*ret));
    if (!ret) JEST_PANIC();

    ret->default_bucket_size = default_bucket_size;
    ret->head = ret->current = NULL;
    return ret;
}

void jest_arena_destroy(jest_arena_t *arena)
{
    if (!arena) return;

    jest_arenabucket_t *next = arena->head->next;
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

    void *ret = arena->current->data + arena->offset + padding;
    arena->offset += size + padding;
    return ret;
}

void *jest_arena_alloc(jest_arena_t *arena, size_t size)
{
    return jest_arena_alloc_aligned(arena, size, 2 * sizeof(void *));
}

#endif // JEST_IMPL
