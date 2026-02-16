#include <stdio.h>

#define JEST_IMPL 1
#include "../jest.h"

#if 0
struct Foo {
    double bar;
    bool baz;
};

void serialize_test(void)
{
    struct Foo foo = {
        -JEST_NAN, true
    };

    // write foo to out.json5
    FILE *foo_out = fopen("out.json5", "w+");
    if (!foo_out) return;

    Jest_JsonVal root = Jest_jsonObj();
    Jest_jsonObjAdd(&root, "bar", Jest_jsonNumber(foo.bar));
    Jest_jsonObjAdd(&root, "baz", Jest_jsonBool(foo.baz));
    Jest_printJsonVal(foo_out, &root, true);

    printf("serialized foo: ");
    Jest_printJsonVal(stdout, &root, true);
    printf("\n\n");

    Jest_destroyJsonVal(&root);
    fclose(foo_out);
}

int main(void)
{
    Jest_JsonVal v;
    Jest_parseJsonFileFromPath(&v, "test.json5");

    Jest_Error err;
    Jest_JsonVal *v2 = Jest_jsonIdx(&v, "['foo 🌿/'][bar][0]", &err);

    *v2 = Jest_jsonObj();
    Jest_jsonObjAdd(v2, "object creation!", Jest_jsonBool(true));

    serialize_test();

    printf("v: ");
    Jest_printJsonVal(stdout, &v, true);
    printf("\n\nv2: ");
    Jest_printJsonVal(stdout, v2, true);
    printf("\n\n");

    v2 = Jest_jsonIdx(&v, "['foo 🌿/'][bar][]", &err); // intentionally bad syntax
    if (!v2) { // v2 will be null because of the syntax error
        printf("ERR: %d\n", (int)err); // print the erorr type (should be JEST_ERROR_SYNTAX (3))
    }

    Jest_destroyJsonVal(&v);
    return 0;
}
#endif

#include <assert.h>
#include <limits.h>

int main(void)
{
    jest_arena_t *arena = jest_arena_create(65536);
    if (!arena) return 1;

    int *x = jest_arena_alloc_aligned(arena, 4, 4);
    int *y = jest_arena_alloc_aligned(arena, 4, 4);

    jest_sb_t sb = jest_sb_create(arena, 256);
    jest_sb_append(arena, &sb, JEST_STR("test string"));
    jest_sb_append(arena, &sb, JEST_STR(", test string 2"));

    *x = INT_MAX;
    *y = 250;

    printf("sb.cap: %zu, sb.len: %zu, sb.data: \"%s\"\n", sb.cap, sb.buffer.len, sb.buffer.data);
    assert(*x == INT_MAX && *y == 250);

    jest_writer_t writer = jest_writer_create(arena);
    
    jestw_begin_obejct(arena, &writer, JEST_STR_NULL);
        jestw_comment(arena, &writer, JEST_STR("comments are allowed to be serialized!"));
        jestw_null(arena, &writer, JEST_STR("null"));
        jestw_number(arena, &writer, JEST_STR("key1"), -JEST_INFINITY);
        jestw_number(arena, &writer, JEST_STR("key2"), -JEST_NAN);
        jestw_newline(arena, &writer); // extra newlines can be added to make the resulting file more readable
        jestw_comment(arena, &writer, JEST_STR("strings are escaped"));
        jestw_string(arena, &writer, JEST_STR("in both key names \U0001f33f"), JEST_STR("and values \U0001f340"));
        jestw_string(arena, &writer, JEST_STR("invalid encodings get reduced to raw bytes"), JEST_STR("\\xf0\\xd0 -> \xf0\xd0"));
        jestw_string(arena, &writer, JEST_STR("overlong encodings count as invalid encodings"), JEST_STR("\\u0000 as a 4-byte overlong becomes \xf0\x80\x80\x80"));
        jestw_newline(arena, &writer);
        jestw_comment(arena, &writer, JEST_STR("serialize array"));
        jestw_begin_array(arena, &writer, JEST_STR("array!"));
            jestw_comment(arena, &writer, JEST_STR("this string came from the string builder `sb` in main"));
            jestw_string(arena, &writer, JEST_STR_NULL, sb.buffer);
            jestw_string(arena, &writer, JEST_STR_NULL, JEST_STR("string1"));
            jestw_boolean(arena, &writer, JEST_STR_NULL, true);
            jestw_boolean(arena, &writer, JEST_STR_NULL, false);
        jestw_end_array(arena, &writer);
    jestw_end_object(arena, &writer);
    jestw_write(&writer, "./out.json5");

    jest_arena_destroy(arena);
    return 0;
}
