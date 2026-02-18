#include <stdio.h>

#define JEST_IMPL 1
#include "../jest.h"

#include <assert.h>
#include <limits.h>

int main(void)
{
    jest_arena_t *arena = jest_arena_create(65536);
    if (!arena) return 1;

    int *x = (int *)jest_arena_alloc_aligned(arena, 4, 4);
    int *y = (int *)jest_arena_alloc_aligned(arena, 4, 4);

    jest_sb_t sb = jest_sb_create(arena, 256);
    jest_sb_append(arena, &sb, JEST_STR("test string"));
    jest_sb_append(arena, &sb, JEST_STR(", test string 2"));

    *x = INT_MAX;
    *y = 250;

    printf("sb.cap: %zu, sb.len: %zu, sb.data: \"%s\"\n", sb.cap, sb.buffer.len, sb.buffer.data);
    assert(*x == INT_MAX && *y == 250);

    jestw_t writer = jestw_create(arena);
    
    jestw_begin_object(arena, &writer);
        jestw_comment(arena, &writer, JEST_STR("comments are allowed to be serialized!"));
        jestw_kv_null(arena, &writer, JEST_STR("null"));
        jestw_kv_float(arena, &writer, JEST_STR("key1"), -JEST_INFINITY);
        jestw_kv_float(arena, &writer, JEST_STR("key2"), -JEST_NAN);
        jestw_newline(arena, &writer); // extra newlines can be added to make the resulting file more readable
        jestw_comment(arena, &writer, JEST_STR("strings are escaped"));
        jestw_kv_string(arena, &writer, JEST_STR("in both key names \U0001f33f"), JEST_STR("and values \U0001f340"));
        jestw_kv_string(arena, &writer, JEST_STR("invalid utf-8 gets reduced to raw bytes"), JEST_STR("\\xf0\\xd0 -> \xf0\xd0"));
        jestw_kv_string(arena, &writer, JEST_STR("overlong encodings count as invalid utf-8"), JEST_STR("\\u0000 as a 4-byte overlong becomes \xf0\x80\x80\x80"));
        jestw_newline(arena, &writer);
        jestw_comment(arena, &writer, JEST_STR("serialize array"));
        jestw_kv_begin_array(arena, &writer, JEST_STR("array!"));
            jestw_comment(arena, &writer, JEST_STR("this string came from the string builder `sb` in main"));
            jestw_string(arena, &writer, sb.buffer);
            jestw_string(arena, &writer, JEST_STR("string1"));
            jestw_bool(arena, &writer, true);
            jestw_bool(arena, &writer, false);
        jestw_end_array(arena, &writer);
        jestw_newline(arena, &writer);
        jestw_comment(arena, &writer, JEST_STR("jest also supports serialization of integer types"));
        jestw_kv_begin_object(arena, &writer, JEST_STR("datatypes"));
            jestw_kv_uint(arena, &writer, JEST_STR("uint"), UINT64_MAX);
            jestw_kv_int(arena, &writer, JEST_STR("int"), INT64_MIN);
        jestw_end_object(arena, &writer);
    jestw_end_object(arena, &writer);
    jestw_write(&writer, "./out.json5");

    printf("./out.json5: ");
    jestw_writef(&writer, stdout);
    printf("\n");

    jest_string_t str = jest__unescape_string(arena, JEST_STR("\\ud83c\\udf3f \\\ntest"));
    printf("str: '%s'\n", str.data);

    jest_arena_destroy(arena);
    return 0;
}
