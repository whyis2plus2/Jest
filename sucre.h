/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include <ctype.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h> // for NAN, INFINITY

#define SUCRE_TODO(str) do {fprintf(stderr, "[%s:%d: %s] TODO: %s\n", __FILE__, __LINE__, __func__, str); exit(-1);} while (0)

#ifndef SUCRE_H_
#define SUCRE_H_ 1

typedef int Sucre_LexemeType;
enum Sucre_LexemeType {
    SUCRE_LEXEME_ERR = 256,
    SUCRE_LEXEME_IDENT,
    SUCRE_LEXEME_NULL,
    SUCRE_LEXEME_BOOL,
    SUCRE_LEXEME_NUMBER,
    SUCRE_LEXEME_STR,
    SUCRE_LEXEME_EOF
};

typedef struct Sucre_Lexer {
    bool boolval;
    Sucre_LexemeType type;

    char *strbuf;
    size_t strbuf_sz;
    size_t stringval_len;

    size_t filebuf_offset;
    size_t filebuf_sz;
    const char *filebuf;

    size_t ident_start;
    size_t ident_len;
    double numval;
} Sucre_Lexer;

size_t Sucre_readEntireFile(char **out, const char *path);
bool Sucre_initLexer(Sucre_Lexer *l, char *strbuf, size_t strbuf_sz, const char *filebuf, size_t filebuf_sz);
bool Sucre_lexerStep(Sucre_Lexer *l);

#endif // !SUCRE_H_

#ifdef SUCRE_IMPL

static void SucreInternal_lexerSkipCommentAndWhiteSpace(Sucre_Lexer *l);
static void SucreInternal_lexerHandleStr(Sucre_Lexer *l);

size_t Sucre_readEntireFile(char **out, const char *path)
{
    if (!out || !path) return 0; 

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    *out = calloc(fsize + 1, 1);
    if (!*out) return 0;

    fread(*out, 1, fsize, f);
    fclose(f);

    return (size_t)fsize;
}

bool Sucre_initLexer(Sucre_Lexer *l, char *strbuf, size_t strbuf_sz, const char *filebuf, size_t filebuf_sz)
{
    if (!l || !strbuf || !strbuf_sz || !filebuf || !filebuf_sz) return false;

    memset(l, 0, sizeof(*l));
    l->strbuf = strbuf;
    l->strbuf_sz = strbuf_sz;

    l->filebuf = filebuf;
    l->filebuf_sz = filebuf_sz;
    l->filebuf_offset = 0;

    return true;
}

bool Sucre_lexerStep(Sucre_Lexer *l)
{
    if (!l) return false;
    l->type = SUCRE_LEXEME_ERR;

    SucreInternal_lexerSkipCommentAndWhiteSpace(l);
    if (l->filebuf_offset >= l->filebuf_sz) {
        l->type = SUCRE_LEXEME_EOF;
        return false;
    }

    switch (l->filebuf[l->filebuf_offset]) {
        case ':': l->type = ':'; break;
        case ',': l->type = ','; break;
        case '{': l->type = '{'; break;
        case '}': l->type = '}'; break;
        case '[': l->type = '['; break;
        case ']': l->type = ']'; break;
        case '"':  
        case '\'': goto lbl_str;
        default: break;
    }

    if (l->type < 256) {
        ++l->filebuf_offset;
        return true;
    }

    if (isalpha(l->filebuf[l->filebuf_offset]) || l->filebuf[l->filebuf_offset] == '_' || l->filebuf[l->filebuf_offset] == '$') {
        l->type = SUCRE_LEXEME_IDENT;
        l->ident_start = l->filebuf_offset;

        while (isalnum(l->filebuf[l->filebuf_offset]) || l->filebuf[l->filebuf_offset] == '_' || l->filebuf[l->filebuf_offset] == '$') {
            ++l->filebuf_offset;
        }

        l->ident_len = l->filebuf_offset - l->ident_start;
        if (!strncmp(&l->filebuf[l->ident_start], "Infinity", l->ident_len)) {
            l->type = SUCRE_LEXEME_NUMBER;
            l->numval = INFINITY;
        } else if (!strncmp(&l->filebuf[l->ident_start], "NaN", l->ident_len)) {
            l->type = SUCRE_LEXEME_NUMBER;
            l->numval = NAN;
        } else if (!strncmp(&l->filebuf[l->ident_start], "true", l->ident_len)) {
            l->type = SUCRE_LEXEME_BOOL;
            l->boolval = true;
        } else if (!strncmp(&l->filebuf[l->ident_start], "false", l->ident_len)) {
            l->type = SUCRE_LEXEME_BOOL;
            l->boolval = false;
        } else if (!strncmp(&l->filebuf[l->ident_start], "null", l->ident_len)) {
            l->type = SUCRE_LEXEME_NULL;
        }

        return true;
    }

    if (isdigit(l->filebuf[l->filebuf_offset]) || l->filebuf[l->filebuf_offset] == '.' || l->filebuf[l->filebuf_offset] == '+' || l->filebuf[l->filebuf_offset] == '-') {
        int tmp = 0;
        if (sscanf(&l->filebuf[l->filebuf_offset], "%lf%n", &l->numval, &tmp) != 1) return false;
        l->filebuf_offset += tmp;
        l->type = SUCRE_LEXEME_NUMBER;
        return true;
    }

    return false;

lbl_str:
    l->type = SUCRE_LEXEME_STR;
    SucreInternal_lexerHandleStr(l);
    return true;
}

static void SucreInternal_lexerSkipCommentAndWhiteSpace(Sucre_Lexer *l)
{
    if (!l) return;

    // save initial offset
    const size_t start_offset = l->filebuf_offset;

    // skip initial whitespace
    while (isspace(l->filebuf[l->filebuf_offset])) ++l->filebuf_offset;
    if (l->filebuf[l->filebuf_offset] != '/') return;

    ++l->filebuf_offset;
    if (l->filebuf[l->filebuf_offset] == '/') { // single line comments
        l->filebuf_offset = strchr(&l->filebuf[l->filebuf_offset], '\n') - l->filebuf;
        if (l->filebuf_offset > l->filebuf_sz) l->filebuf_offset = start_offset;
    } else if (l->filebuf[l->filebuf_offset] == '*') { // multiline comments
        ++l->filebuf_offset;
        do {
            l->filebuf_offset = strchr(&l->filebuf[l->filebuf_offset], '*') - l->filebuf;
            if (l->filebuf_offset >= l->filebuf_sz) {
                l->filebuf_offset = start_offset;
                goto lbl_end;
            }
        } while (l->filebuf[l->filebuf_offset + 1] != '/');
        l->filebuf_offset += 2;
    } else l->filebuf_offset = start_offset;

lbl_end:
    // skip terminal whitespace
    while (isspace(l->filebuf[l->filebuf_offset])) ++l->filebuf_offset;
}

static void SucreInternal_lexerHandleStr(Sucre_Lexer *l)
{
    if (!l) return;

    const size_t start_offset = l->filebuf_offset;

    SucreInternal_lexerSkipCommentAndWhiteSpace(l);
    if (l->filebuf[l->filebuf_offset] != '\'' && l->filebuf[l->filebuf_offset] != '\"') {
        return;
    }

    l->type = SUCRE_LEXEME_STR;
    const char quot = l->filebuf[l->filebuf_offset++];

    l->stringval_len = 0;
    while (l->filebuf[l->filebuf_offset] != quot) {
        if (l->filebuf[l->filebuf_offset] == '\\') {
            l->filebuf_offset++;
            switch (l->filebuf[l->filebuf_offset]) {
                case '\n': l->strbuf[l->stringval_len++] = '\n'; break;
                case '\\': l->strbuf[l->stringval_len++] = '\\'; break;
                case '\'': l->strbuf[l->stringval_len++] = '\''; break;
                case '\"': l->strbuf[l->stringval_len++] = '\"'; break;
                case '/':  l->strbuf[l->stringval_len++] =  '/'; break;
                case 'b':  l->strbuf[l->stringval_len++] = '\b'; break;
                case 'f':  l->strbuf[l->stringval_len++] = '\f'; break;
                case 'n':  l->strbuf[l->stringval_len++] = '\n'; break;
                case 'r':  l->strbuf[l->stringval_len++] = '\r'; break;
                case 't':  l->strbuf[l->stringval_len++] = '\t'; break;
                case 'v':  l->strbuf[l->stringval_len++] = '\v'; break;
                case '0':  l->strbuf[l->stringval_len++] = '\0'; break;
                case 'u': {
                    --l->filebuf_offset;
                    int offset = 0;
                    uint32_t codepoint = 0;
                    sscanf(&l->filebuf[l->filebuf_offset], "\\u%4" SCNx32 "%n", &codepoint, &offset);
                    if (offset < 6) SUCRE_TODO("give an error/warning for unicode chars that don't have all 4 digits");

                    // if codepoint is a hi surrogate
                    if (0xd800 <= codepoint && codepoint <= 0xdfbb) {
                        uint32_t lo = 0;
                        const int sscanf_result = sscanf(&l->filebuf[l->filebuf_offset + 6], "\\u%4" SCNx32 "%n", &lo, &offset);

                        if (sscanf_result == 1) { // if there is a second unicode char
                            if (offset < 6) SUCRE_TODO("give an error/warning for unicode lo surrogates that don't have all 4 digits");

                            if (0xdc00 <= lo && lo <= 0xdfff) { // check if it's a lo surrogate to match the hi surrogate
                                offset = 12;
                                codepoint = ((codepoint - 0xd800) << 10) + ((lo - 0xdc00)) + 0x10000;
                            } else { // if not set the offset to 6 so that it isn't skipped on the next iteration of the loop
                                offset =  6;
                            }
                        }
                    }

                    if (codepoint <= 0x7F) {
                        l->strbuf[l->stringval_len++] = (char)codepoint;
                    } else if (codepoint <= 0x7FF) {
                        l->strbuf[l->stringval_len++] = (char)(((codepoint >> 6) & 0x1f)  | 0xc0);
                        l->strbuf[l->stringval_len++] = (char)(((codepoint)      & 0x3f)  | 0x80);
                    } else if (codepoint <= 0xFFFF) {
                        l->strbuf[l->stringval_len++] = (char)(((codepoint >> 12) & 0x0f) | 0xe0);
                        l->strbuf[l->stringval_len++] = (char)(((codepoint >> 6)  & 0x3f) | 0x80);
                        l->strbuf[l->stringval_len++] = (char)(((codepoint)       & 0x3f) | 0x80);
                    } else if (codepoint <= 0x10FFFF) {
                        l->strbuf[l->stringval_len++] = (char)(((codepoint >> 18) & 0x07) | 0xf0);
                        l->strbuf[l->stringval_len++] = (char)(((codepoint >> 12) & 0x3f) | 0x80);
                        l->strbuf[l->stringval_len++] = (char)(((codepoint >> 6)  & 0x3f) | 0x80);
                        l->strbuf[l->stringval_len++] = (char)(((codepoint)       & 0x3f) | 0x80);
                    } else {
                        SUCRE_TODO("handle the case where the codepoint is > 0x10FFFF (maybe unreachable?)");
                    }

                    l->filebuf_offset += offset - 1;
                } break;

                case 'x': {
                    --l->filebuf_offset;
                    int offset = 0;
                    uint32_t codepoint = 0;
                    sscanf(&l->filebuf[l->filebuf_offset], "\\x%2" SCNx32 "%n", &codepoint, &offset);
                    if (offset < 4) SUCRE_TODO("give an error/warning for hex chars that don't have both digits");

                    if (codepoint <= 0x7F) {
                        l->strbuf[l->stringval_len++] = (char)codepoint;
                    } else {
                        l->strbuf[l->stringval_len++] = (char)(((codepoint >> 6) & 0x1f)  | 0xc0);
                        l->strbuf[l->stringval_len++] = (char)(((codepoint)      & 0x3f)  | 0x80);
                    }

                    l->filebuf_offset += offset - 1;
                } break;
                default: l->strbuf[l->stringval_len++] = l->filebuf[l->filebuf_offset]; break;
            }

            ++l->filebuf_offset;
            continue;
        }

        l->strbuf[l->stringval_len++] = l->filebuf[l->filebuf_offset];
        ++l->filebuf_offset;
    }

    ++l->filebuf_offset;
}

#endif // SUCRE_IMPL
