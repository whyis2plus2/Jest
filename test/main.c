#include <stdio.h>

#define JEST_IMPL 1
#include "../jest.h"

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
