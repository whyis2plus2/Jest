#include <stdio.h>

#define SUCRE_IMPL 1
#define SUCRE_ENFORCE_TODOS
#include "../sucre.h"

int main(void)
{
    static char strbuf[1024] = {0};
    Sucre_Lexer l = {0};

    char *fbuffer;
    size_t fsize = Sucre_readEntireFile(&fbuffer, "test.json5");
    if (!fsize) return 1;

    if (!Sucre_initLexer(&l, strbuf, sizeof(strbuf), fbuffer, fsize)) return 1;

    while (Sucre_lexerStep(&l)) {
        if (l.type < 256) {
            printf("%c\n", l.type);
        } else if (l.type == SUCRE_LEXEME_ERR) {
            printf("ERR: %c\n", l.filebuf[l.filebuf_offset]);
        } else if (l.type == SUCRE_LEXEME_IDENT) {
            printf("Ident: `%.*s`\n", (int)l.ident_len, &l.filebuf[l.ident_start]);
        } else if (l.type == SUCRE_LEXEME_NULL) {
            printf("Null: null\n");
        } else if (l.type == SUCRE_LEXEME_BOOL) {
            printf("Bool: `%s`\n", (l.boolval)? "true" : "false");
        } else if (l.type == SUCRE_LEXEME_NUMBER) {
            printf("Number: `%.16g`\n", l.numval);
        } else if (l.type == SUCRE_LEXEME_STR) {
            printf("String: `%.*s`\n", (int)l.stringval_len, l.strbuf);
        } else {
            printf("TYPE: %d\n", l.type);
        } 
    }

    free(fbuffer);
    return 0;
}