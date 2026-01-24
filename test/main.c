#include <stdio.h>

#define SUCRE_IMPL 1
#include "../sucre.h"

// TODO: implement parsing and not just lexing

int main(void)
{
    Sucre_JsonVal v;
    Sucre_parseJsonFileFromPath(&v, "test.json5");

    Sucre_Error err;
    Sucre_JsonVal *v2 = Sucre_jsonIdx(&v, "['foo 🌿/'][bar][0]", &err);

    printf("v: ");
    Sucre_printJsonVal(stdout, &v, true);
    printf("\nv2: ");
    Sucre_printJsonVal(stdout, v2, true);
    printf("\n\n");

    v2 = Sucre_jsonIdx(&v, "['foo 🌿/'][bar][]", &err); // intentionally bad syntax
    if (!v2) { // v2 will be null because of the syntax error
        printf("ERR: %d\n", (int)err); // print the erorr type (should be SUCRE_ERROR_SYNTAX (3))
    }

    Sucre_destroyJsonVal(&v);
    return 0;
}