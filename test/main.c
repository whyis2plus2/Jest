#include <stdio.h>

#define SUCRE_IMPL 1
#include "../sucre.h"

// TODO: implement parsing and not just lexing

int main(void)
{
    static char strbuf[1024] = {0};
    Sucre_Lexer l = {0};

    char *fbuffer;
    size_t fsize = Sucre_readEntireFile(&fbuffer, "test.json5");
    if (!fsize) return 1;

    if (!Sucre_initLexer(&l, strbuf, sizeof(strbuf), fbuffer, fsize)) return 1;

    Sucre_JsonVal v;
    Sucre_parseJsonLexer(&v, &l);

    // for (size_t i = 0; i < v.v.as_obj.nfields; ++i) {
    //     printf("v[\'%.*s\'].type == %d\n", (int)v.v.as_obj.fn_lens[i], v.v.as_obj.field_names[i], v.v.as_obj.field_values[i].type);
    // }

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
    free(fbuffer);
    return 0;
}