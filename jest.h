#ifndef JEST_H_
#define JEST_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if defined(__clang__) || defined(__GNUC__)
#   define jest__gnuc_attribute(x) __attribute__(x)
#else
#   define jest__gnuc_attribute(x)
#endif

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
    size_t offset;
    char data[1 /* flexible array */];
} jest_arenabucket_t;

typedef struct {
    size_t default_bucket_size;
    jest_arenabucket_t *head;
    jest_arenabucket_t *tail;
} jest_arena_t;

jest_arena_t *jest_arena_create(size_t default_bucket_size);
void jest_arena_destroy(jest_arena_t *arena);
void jest_arena_reset(jest_arena_t *arena);

jest__gnuc_attribute((__malloc__, __alloc_size__(2), __alloc_align__(3)))
void *jest_arena_alloc_aligned(jest_arena_t *arena, size_t size, size_t align);

jest__gnuc_attribute((__malloc__, __alloc_size__(2)))
void *jest_arena_alloc(jest_arena_t *arena, size_t size);

#endif // !JEST_H_

#ifdef JEST_IMPL
#endif // JEST_IMPL
