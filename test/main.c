#include <stdio.h>

#define JEST_IMPL 1
#include "../jest.h"

struct Foo {
    double bar;
    bool baz;
};

void serialize_test(void)
{
    static char strbuf1[32] = {0};
    static char strbuf2[32] = {0};
    struct Foo foo = {
        1.0, false
    };

    // write foo to out.json5
    FILE *foo_out = fopen("out.json5", "w+");
    fprintf(
        foo_out,
        "{\n"
            "\tbar: %s,\n"
            "\tbaz: %s\n"
        "}",
        Jest_dblToStr(strbuf1, 32, foo.bar),
        Jest_boolToStr(strbuf2, 32, foo.baz)
    );

    // print serialized foo to stdout
    char *foo_tmp = NULL;
    Jest_readEntireFile(&foo_tmp, foo_out);
    printf("serialized foo:\n%s\n\n", foo_tmp);

    free(foo_tmp);
    fclose(foo_out);
}

int main(void)
{
    Jest_JsonVal v;
    Jest_parseJsonFileFromPath(&v, "test.json5");

    Jest_Error err;
    Jest_JsonVal *v2 = Jest_jsonIdx(&v, "['foo 🌿/'][bar][0]", &err);

    serialize_test();

    printf("v: ");
    Jest_printJsonVal(stdout, &v, true);
    printf("\nv2: ");
    Jest_printJsonVal(stdout, v2, true);
    printf("\n\n");

    v2 = Jest_jsonIdx(&v, "['foo 🌿/'][bar][]", &err); // intentionally bad syntax
    if (!v2) { // v2 will be null because of the syntax error
        printf("ERR: %d\n", (int)err); // print the erorr type (should be JEST_ERROR_SYNTAX (3))
    }

    Jest_destroyJsonVal(&v);
    return 0;
}