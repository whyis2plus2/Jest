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

#include <math.h> // for signbit, isnan, isinf, INFINITY, NAN

#define SUCRE_TODO(str) do {fprintf(stderr, "[%s:%d: %s] TODO: %s\n", __FILE__, __LINE__, __func__, str); exit(-1);} while (0)

#ifndef SUCRE_H_
#define SUCRE_H_ 1

typedef int Sucre_LexemeType;
enum Sucre_LexemeType {
    SUCRE_LEXEME_ERR = 256,
    SUCRE_LEXEME_IDENT,
    SUCRE_LEXEME_NULL,
    SUCRE_LEXEME_BOOL,
    SUCRE_LEXEME_NUM,
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

typedef enum Sucre_JsonType {
    SUCRE_JSONTYPE_NULL,
    SUCRE_JSONTYPE_BOOL,
    SUCRE_JSONTYPE_NUM,
    SUCRE_JSONTYPE_STR,
    SUCRE_JSONTYPE_ARR,
    SUCRE_JSONTYPE_OBJ
} Sucre_JsonType;

typedef struct Sucre_JsonVal {
    Sucre_JsonType type;

    union {
        bool as_bool;
        double as_num;
        struct { size_t len; char *v; } as_str;
        struct { size_t len, cap; struct Sucre_JsonVal *v; } as_arr;

        struct {
            size_t nfields, nalloced;
            size_t *fn_lens; // field name lengths
            char **field_names; // field names
            struct Sucre_JsonVal *field_values; // field values
        } as_obj;
    } v;
} Sucre_JsonVal;

char *Sucre_strndup(const char *str, size_t len);
size_t Sucre_readEntireFile(char **out, const char *path);
bool Sucre_initLexer(Sucre_Lexer *l, char *strbuf, size_t strbuf_sz, const char *filebuf, size_t filebuf_sz);
bool Sucre_lexerStep(Sucre_Lexer *l);

void Sucre_jsonArrayAppend(Sucre_JsonVal *arr, const Sucre_JsonVal *elem);
void Sucre_jsonObjSet(Sucre_JsonVal *obj, const char *field_name, size_t name_len, const Sucre_JsonVal *value);
void Sucre_parseJsonLexer(Sucre_JsonVal *out, Sucre_Lexer *lexer);

void Sucre_printJsonVal(FILE *file, const Sucre_JsonVal *val, bool escape_unicode);

#endif // !SUCRE_H_

#ifdef SUCRE_IMPL

// helper functions for lexers
static void SucreInternal_lexerSkipCommentAndWhiteSpace(Sucre_Lexer *l);
static void SucreInternal_lexerHandleStr(Sucre_Lexer *l);

// helper functions for parsing individual pieces of data
static void SucreInternal_parseNull(Sucre_JsonVal *out, Sucre_Lexer *lexer);
static void SucreInternal_parseBool(Sucre_JsonVal *out, Sucre_Lexer *lexer);
static void SucreInternal_parseNum(Sucre_JsonVal *out, Sucre_Lexer *lexer);
static void SucreInternal_parseStr(Sucre_JsonVal *out, Sucre_Lexer *lexer);
static void SucreInternal_parseArr(Sucre_JsonVal *out, Sucre_Lexer *lexer);
static void SucreInternal_parseObj(Sucre_JsonVal *out, Sucre_Lexer *lexer);

// Sucre_printJsonVal with prepended tabs
static void SucreInternal_printJsonVal(FILE *file, const Sucre_JsonVal *val, bool escape_unicode, int starttabs, int midtabs);

char *Sucre_strndup(const char *str, size_t len)
{
    if (!str) return NULL;

    char *ret = (char *)calloc(len + 1, 1);
    if (!ret) return NULL;

    memcpy(ret, str, len);
    return ret;
}

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

    return Sucre_lexerStep(l);
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
            l->type = SUCRE_LEXEME_NUM;
            l->numval = INFINITY;
        } else if (!strncmp(&l->filebuf[l->ident_start], "NaN", l->ident_len)) {
            l->type = SUCRE_LEXEME_NUM;
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
        l->type = SUCRE_LEXEME_NUM;
        return true;
    }

    return false;

lbl_str:
    l->type = SUCRE_LEXEME_STR;
    SucreInternal_lexerHandleStr(l);
    return true;
}

void Sucre_jsonArrayAppend(Sucre_JsonVal *arr, const Sucre_JsonVal *elem)
{
    if (!arr || !elem || arr == elem) return;
    if (arr->type != SUCRE_JSONTYPE_ARR) return;

    if (arr->v.as_arr.len + 1 > arr->v.as_arr.cap) {
        arr->v.as_arr.cap = (arr->v.as_arr.cap)? arr->v.as_arr.cap * 2 : 32;
        arr->v.as_arr.v = (Sucre_JsonVal *)realloc(arr->v.as_arr.v, sizeof(*arr->v.as_arr.v) * arr->v.as_arr.cap);
        if (!arr->v.as_arr.v) SUCRE_TODO("handle this");
    }

    memcpy(&arr->v.as_arr.v[arr->v.as_arr.len++], elem, sizeof(*elem));
}

void Sucre_jsonObjSet(Sucre_JsonVal *obj, const char *field_name, size_t name_len, const Sucre_JsonVal *value)
{
    if (!obj || !field_name || !value) return;
    if (obj == value) return;
    if (obj->type != SUCRE_JSONTYPE_OBJ) return;

    for (size_t i = 0; i < obj->v.as_obj.nfields; ++i) {
        if (!strncmp(field_name, obj->v.as_obj.field_names[i], name_len)) {
            memcpy(&obj->v.as_obj.field_values[i], value, sizeof(*value));
            return;
        }
    }

    if (obj->v.as_obj.nfields + 1 > obj->v.as_obj.nalloced) {
        obj->v.as_obj.nalloced = (obj->v.as_obj.nalloced)? 2 * obj->v.as_obj.nalloced : 16;
        obj->v.as_obj.field_names = (char **)realloc(obj->v.as_obj.field_names, sizeof(*obj->v.as_obj.field_names) * obj->v.as_obj.nalloced);
        obj->v.as_obj.fn_lens = (size_t *)realloc(obj->v.as_obj.fn_lens, sizeof(*obj->v.as_obj.fn_lens) * obj->v.as_obj.nalloced);
        obj->v.as_obj.field_values = (Sucre_JsonVal *)realloc(obj->v.as_obj.field_values, sizeof(*obj->v.as_obj.field_values) * obj->v.as_obj.nalloced);
        if (!obj->v.as_obj.field_names || !obj->v.as_obj.fn_lens || !obj->v.as_obj.field_values) SUCRE_TODO("handle this");
    }

    obj->v.as_obj.fn_lens[obj->v.as_obj.nfields] = name_len;
    obj->v.as_obj.field_names[obj->v.as_obj.nfields] = Sucre_strndup(field_name, name_len);
    if (!obj->v.as_obj.field_names[obj->v.as_obj.nfields]) SUCRE_TODO("handle this");
    memcpy(&obj->v.as_obj.field_values[obj->v.as_obj.nfields], value, sizeof(*value));
    ++obj->v.as_obj.nfields;
}

void Sucre_parseJsonLexer(Sucre_JsonVal *out, Sucre_Lexer *lexer)
{
    if (!out || !lexer) return;

    switch (lexer->type) {
        case SUCRE_LEXEME_NULL: SucreInternal_parseNull(out,lexer);  break;
        case SUCRE_LEXEME_BOOL: SucreInternal_parseBool(out, lexer); break;
        case SUCRE_LEXEME_NUM:  SucreInternal_parseNum(out, lexer);  break;
        case SUCRE_LEXEME_STR:  SucreInternal_parseStr(out, lexer);  break;
        case '[':               SucreInternal_parseArr(out, lexer);  break;
        case '{':               SucreInternal_parseObj(out, lexer);  break;
        default: printf("TYPE: %d\n", lexer->type); SUCRE_TODO("handle invalid tokens");
    }
}

void Sucre_printJsonVal(FILE *file, const Sucre_JsonVal *val, bool escape_unicode)
{
    SucreInternal_printJsonVal(file, val, escape_unicode, 0, 0);
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

static void SucreInternal_parseNull(Sucre_JsonVal *out, Sucre_Lexer *lexer)
{
    if (!out || !lexer) return;
    if (lexer->type != SUCRE_LEXEME_NULL) return;

    out->type = SUCRE_JSONTYPE_NULL;
    Sucre_lexerStep(lexer);
}

static void SucreInternal_parseBool(Sucre_JsonVal *out, Sucre_Lexer *lexer)
{
    if (!out || !lexer) return;
    if (lexer->type != SUCRE_LEXEME_BOOL) return;

    out->type = SUCRE_JSONTYPE_BOOL;
    out->v.as_bool = lexer->boolval;
    Sucre_lexerStep(lexer);
}

static void SucreInternal_parseNum(Sucre_JsonVal *out, Sucre_Lexer *lexer)
{
    if (!out || !lexer) return;
    if (lexer->type != SUCRE_LEXEME_NUM) return;

    out->type = SUCRE_JSONTYPE_NUM;
    out->v.as_num = lexer->numval;
    Sucre_lexerStep(lexer);
}

static void SucreInternal_parseStr(Sucre_JsonVal *out, Sucre_Lexer *lexer)
{
    if (!out || !lexer) return;
    if (lexer->type != SUCRE_LEXEME_STR) return;

    out->type = SUCRE_JSONTYPE_STR;
    out->v.as_str.len = lexer->stringval_len;
    out->v.as_str.v   = Sucre_strndup(lexer->strbuf, lexer->stringval_len);
    if (!out->v.as_str.v) SUCRE_TODO("handle this Sucre_strndup failure");
    Sucre_lexerStep(lexer);
}

static void SucreInternal_parseArr(Sucre_JsonVal *out, Sucre_Lexer *lexer)
{
    if (!out || !lexer) return;
    if (lexer->type != '[') return;

    out->type = SUCRE_JSONTYPE_ARR;
    memset(&out->v, 0, sizeof(out->v));

    do {
        Sucre_lexerStep(lexer);
        if (lexer->type == ']') break;
        Sucre_JsonVal elem;
        Sucre_parseJsonLexer(&elem, lexer);
        Sucre_jsonArrayAppend(out, &elem);
    } while (lexer->type == ',');
    if (lexer->type != ']') SUCRE_TODO("handle mismatched closing square brackets");

    Sucre_lexerStep(lexer);
}

static void SucreInternal_parseObj(Sucre_JsonVal *out, Sucre_Lexer *lexer)
{
    if (!out || !lexer) return;
    if (lexer->type != '{') return;

    out->type = SUCRE_JSONTYPE_OBJ;
    memset(&out->v, 0, sizeof(out->v));

    do {
    Sucre_lexerStep(lexer);
        if (lexer->type == '}') break;
        char *name = NULL;
        size_t name_len = 0;
        
        if (lexer->type != SUCRE_LEXEME_STR && lexer->type != SUCRE_LEXEME_IDENT) SUCRE_TODO("handle invalid keys");
        
        name_len = (lexer->type == SUCRE_LEXEME_STR)? lexer->stringval_len : lexer->ident_len;
        name = Sucre_strndup(
            (lexer->type == SUCRE_LEXEME_STR)
                ?lexer->strbuf
                :&lexer->filebuf[lexer->ident_start],
            name_len
        );
        if (!name) SUCRE_TODO("handle name strndup failure");

        Sucre_lexerStep(lexer);
        if (lexer->type != ':') SUCRE_TODO("handle missing : after key value");
        Sucre_lexerStep(lexer);

        Sucre_JsonVal elem;
        Sucre_parseJsonLexer(&elem, lexer);
        Sucre_jsonObjSet(out, name, name_len, &elem);
    } while (lexer->type == ',');
    if (lexer->type != '}') SUCRE_TODO("handle mismatched closing curly brackets");

    Sucre_lexerStep(lexer);
}

static void SucreInternal_printJsonVal(FILE *file, const Sucre_JsonVal *val, bool escape_unicode, int starttabs, int midtabs)
{
    if (!file || !val) return;

    for (int i = 0; i < starttabs; ++i) fputc('\t', file);

    switch (val->type) {
        case SUCRE_JSONTYPE_NULL: fprintf(file, "%s", "null"); break;
        case SUCRE_JSONTYPE_BOOL: fprintf(file, "%s", (val->v.as_bool)? "true" : "false"); break;
        case SUCRE_JSONTYPE_NUM:  goto lbl_print_num;
        case SUCRE_JSONTYPE_STR:  goto lbl_print_str;
        case SUCRE_JSONTYPE_ARR:  goto lbl_print_arr;
        case SUCRE_JSONTYPE_OBJ:  goto lbl_print_obj;
    }

    return;

lbl_print_num:
    if (signbit(val->v.as_num)) fputc('-', file);

    if (isinf(val->v.as_num)) fprintf(file, "%s", "Infinity");
    else if (isnan(val->v.as_num)) fprintf(file, "%s", "NaN");
    else fprintf(file, "%.15g", val->v.as_num);
    return;

lbl_print_str:
    fputc('"', file);
    for (size_t i = 0; i < val->v.as_str.len; ++i) {
        switch (val->v.as_str.v[i]) {
            case '\n': fprintf(file, "%s", "\\n");  continue;
            case '\\': fprintf(file, "%s", "\\\\"); continue;
            case '\'': fprintf(file, "%s", "\\'");  continue;
            case '\"': fprintf(file, "%s", "\\\""); continue;
            case '/':  fprintf(file, "%s", "\\/");  continue;
            case '\b': fprintf(file, "%s", "\\b");  continue;
            case '\f': fprintf(file, "%s", "\\f");  continue;
            case '\r': fprintf(file, "%s", "\\r");  continue;
            case '\t': fprintf(file, "%s", "\\t");  continue;
            case '\v': fprintf(file, "%s", "\\v");  continue;
            default: break;
        }

        uint32_t codepoint = (uint8_t)(val->v.as_str.v[i]);

        if (iscntrl((int)codepoint)) {
            fprintf(file, "\\u%.4" PRIx32, codepoint);
            continue;
        }

        if (escape_unicode && codepoint >= 0x80) {
            if (codepoint & 0xf0) {
                codepoint = (codepoint & 0x07) << 6;
                codepoint |= ((uint8_t)(val->v.as_str.v[++i]) & 0x3f);
                codepoint <<= 6;
                codepoint |= ((uint8_t)(val->v.as_str.v[++i]) & 0x3f);
                codepoint <<= 6;
                codepoint |= ((uint8_t)(val->v.as_str.v[++i]) & 0x3f);

                // break the codepoint into utf-16 surrogate pairs
                codepoint -= 0x10000;
                codepoint &= 0xfffff;
                uint16_t hi = (codepoint >> 10) + 0xd800;
                uint16_t lo = (codepoint & 0x3ff) + 0xdc00;

                fprintf(file, "\\u%.4" PRIx32 "\\u%.4" PRIx32, hi, lo);
                continue;
            }

            if (codepoint & 0xc0) {
                codepoint = (codepoint & 0x1f) << 6;
                codepoint |= ((uint8_t)(val->v.as_str.v[++i]) & 0x3f);
                fprintf(file, "\\u%.4" PRIx32, codepoint);
                continue;
            }

            if (codepoint & 0xe0) {
                codepoint = (codepoint & 0x0f) << 6;
                codepoint |= ((uint8_t)(val->v.as_str.v[++i]) & 0x3f);
                codepoint <<= 6;
                codepoint |= ((uint8_t)(val->v.as_str.v[++i]) & 0x3f);
                fprintf(file, "\\u%.4" PRIx32, codepoint);
                continue;
            }
        } else fputc(val->v.as_str.v[i], file);
    }
    fputc('"', file);

    return;

lbl_print_arr:
    fputc('[', file);

    for (size_t i = 0; i < val->v.as_arr.len; ++i) {
        if (i) fputc(',', file);
        fputc('\n', file);

        SucreInternal_printJsonVal(file, &val->v.as_arr.v[i], escape_unicode, midtabs + 1, midtabs + 1);
    }

    fputc('\n', file);
    for (int i = 0; i < midtabs; ++i) fputc('\t', file);
    fputc(']', file);
    return;

lbl_print_obj:
    fputc('{', file);

    for (size_t i = 0; i < val->v.as_obj.nfields; ++i) {
        if (i) fputc(',', file);
        fputc('\n', file);

        Sucre_JsonVal name_as_val;
        name_as_val.type = SUCRE_JSONTYPE_STR;
        name_as_val.v.as_str.len = val->v.as_obj.fn_lens[i];
        name_as_val.v.as_str.v   = val->v.as_obj.field_names[i];

        SucreInternal_printJsonVal(file, &name_as_val, escape_unicode, midtabs + 1, midtabs + 1);
        fputc(':', file);
        fputc(' ', file);
        SucreInternal_printJsonVal(file, &val->v.as_obj.field_values[i], escape_unicode, 0, midtabs + 1);
    }

    fputc('\n', file);
    for (int i = 0; i < midtabs; ++i) fputc('\t', file);
    fputc('}', file);
    return;
}

#endif // SUCRE_IMPL
