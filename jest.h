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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JEST_TODO(str) do {fprintf(stderr, "[%s:%d: %s] TODO: %s\n", __FILE__, __LINE__, __func__, str); exit(-1);} while (0)

#ifndef JEST_H_
#define JEST_H_ 1

#ifndef JEST_NAN
#   ifdef NAN
#       define JEST_NAN NAN
#   elif defined(__GNUC__)
#       define JEST_NAN __builtin_nanf("")
#   else
#       define JEST_NAN (0.0/0.0)
#   endif
#endif // !JEST_NAN

#ifndef JEST_INF
#   ifdef INF
#       define JEST_INF INF
#   elif defined(__GNUC__)
#       define JEST_INF __builtin_inff()
#   else
#       define JEST_INF (1.0/0.0)
#   endif
#endif // !JEST_INF

typedef int Jest_LexemeType;
enum Jest_LexemeType {
    JEST_LEXEME_ERR = 256,
    JEST_LEXEME_IDENT,
    JEST_LEXEME_NULL,
    JEST_LEXEME_BOOL,
    JEST_LEXEME_NUM,
    JEST_LEXEME_STR,
    JEST_LEXEME_EOF
};

typedef struct Jest_Lexer {
    bool boolval;
    Jest_LexemeType type;

    char *strbuf;
    size_t strbuf_sz;
    size_t strval_len;

    size_t filebuf_offset;
    size_t filebuf_sz;
    const char *filebuf;

    size_t ident_start;
    size_t ident_len;
    double numval;
} Jest_Lexer;

typedef enum Jest_JsonType {
    JEST_JSONTYPE_NULL,
    JEST_JSONTYPE_BOOL,
    JEST_JSONTYPE_NUM,
    JEST_JSONTYPE_STR,
    JEST_JSONTYPE_ARR,
    JEST_JSONTYPE_OBJ,
    JEST_JSONTYPE_ERR
} Jest_JsonType;

typedef enum Jest_Error {
    JEST_ERROR_NONE, // no error, everything is fine :)
    JEST_ERROR_NOMEM, // malloc/calloc/realloc failed
    JEST_ERROR_BADPARAM, // invalid parameter
    JEST_ERROR_SYNTAX, // syntax error
    JEST_ERROR_BADLEXER, // lexer creation failed at some point
    JEST_ERROR_BADCHAR, // malformed unicode/hex char
    JEST_ERROR_IO // i/o error
} Jest_Error;

typedef struct Jest_JsonVal {
    Jest_JsonType type;

    union {
        bool as_bool;
        double as_num;
        struct { size_t len; char *data; } as_str;
        struct { size_t len, cap; struct Jest_JsonVal *elems; } as_arr;

        struct {
            size_t nfields, nalloced;
            size_t *fn_lens; // field name lengths
            char **field_names; // field names
            struct Jest_JsonVal *field_values; // field values
        } as_obj;

        Jest_Error as_err;
    } v;
} Jest_JsonVal;

bool Jest_signbit(double x);
bool Jest_isnan(double x);
bool Jest_isinf(double x);

char *Jest_strndup(const char *str, size_t len);
char *Jest_dblToStr(char *buffer, size_t bufsz, double x);
char *Jest_boolToStr(char *buffer, size_t bufsz, bool b);
size_t Jest_readEntireFileFromPath(char **out, const char *path);
size_t Jest_readEntireFile(char **out, FILE *file);
bool Jest_initLexer(Jest_Lexer *l, char *strbuf, size_t strbuf_sz, const char *filebuf, size_t filebuf_sz);
bool Jest_lexerStep(Jest_Lexer *l);

Jest_Error Jest_jsonArrayAppend(Jest_JsonVal *arr, const Jest_JsonVal *elem);
Jest_Error Jest_jsonObjAdd(Jest_JsonVal *obj, const char *field_name, Jest_JsonVal value);
Jest_Error Jest_parseJsonLexer(Jest_JsonVal *out, Jest_Lexer *lexer);
Jest_Error Jest_parseJsonFile(Jest_JsonVal *out, FILE *file);
Jest_Error Jest_parseJsonFileFromPath(Jest_JsonVal *out, const char *path);
Jest_Error Jest_parseJsonFromStr(Jest_JsonVal *out, const char *str);

void Jest_printJsonVal(FILE *file, const Jest_JsonVal *val, bool escape_unicode);
void Jest_destroyJsonVal(Jest_JsonVal *val);

Jest_JsonVal *Jest_jsonIdx(Jest_JsonVal *parent, const char *accessor, Jest_Error *opt_err_out);

static inline Jest_JsonVal Jest_jsonNull(void)
{
    Jest_JsonVal out;
    out.type = JEST_JSONTYPE_NULL;
    return out;
}

static inline Jest_JsonVal Jest_jsonBool(bool val)
{
    Jest_JsonVal out;
    out.type = JEST_JSONTYPE_BOOL;
    out.v.as_bool = val;
    return out;
}

static inline Jest_JsonVal Jest_jsonNumber(double val)
{
    Jest_JsonVal out;
    out.type = JEST_JSONTYPE_NUM;
    out.v.as_num = val;
    return out;
}

static inline Jest_JsonVal Jest_jsonString(const char *val)
{
    Jest_JsonVal out;
    out.type = JEST_JSONTYPE_STR;
    out.v.as_str.len = strlen(val);
    out.v.as_str.data = Jest_strndup(val, out.v.as_str.len);
    return out;
}

static inline Jest_JsonVal Jest_jsonArray(void)
{
    Jest_JsonVal out;
    out.type = JEST_JSONTYPE_ARR;
    memset(&out.v, 0, sizeof(out.v));
    return out;
}

static inline Jest_JsonVal Jest_jsonObj(void)
{
    Jest_JsonVal out;
    out.type = JEST_JSONTYPE_OBJ;
    memset(&out.v, 0, sizeof(out.v));
    return out;
}

#endif // !JEST_H_

#ifdef JEST_IMPL

// helper functions for lexers
static void Jest__lexerSkipCommentAndWhiteSpace(Jest_Lexer *l);
static Jest_Error Jest__lexerHandleStr(Jest_Lexer *l);

// helper functions for parsing individual pieces of data
static Jest_Error Jest__parseNull(Jest_JsonVal *out, Jest_Lexer *lexer);
static Jest_Error Jest__parseBool(Jest_JsonVal *out, Jest_Lexer *lexer);
static Jest_Error Jest__parseNum(Jest_JsonVal *out, Jest_Lexer *lexer);
static Jest_Error Jest__parseStr(Jest_JsonVal *out, Jest_Lexer *lexer);
static Jest_Error Jest__parseArr(Jest_JsonVal *out, Jest_Lexer *lexer);
static Jest_Error Jest__parseObj(Jest_JsonVal *out, Jest_Lexer *lexer);

// Jest_printJsonVal with prepended tabs
static void Jest__printJsonVal(FILE *file, const Jest_JsonVal *val, bool escape_unicode, int starttabs, int midtabs);

bool Jest_signbit(double x)
{
    return *(uint64_t *)(void *)&x >> 63;
}

bool Jest_isnan(double x)
{
    const uint64_t bits = *(uint64_t *)(void *)&x;
    // 0xffe0000000000000 (infinity's bits shifted by one so that the sign bit can be ignored)
    return bits << 1 > UINT64_C(0xffe0000000000000);
}

bool Jest_isinf(double x)
{
    const uint64_t bits = *(uint64_t *)(void *)&x;
    // 0xffe0000000000000 (infinity's bits shifted by one so that the sign bit can be ignored)
    return bits << 1 == UINT64_C(0xffe0000000000000);
}

char *Jest_strndup(const char *str, size_t len)
{
    if (!str) return NULL;

    char *ret = (char *)calloc(len + 1, 1);
    if (!ret) return NULL;

    memcpy(ret, str, len);
    return ret;
}

char *Jest_dblToStr(char *buffer, size_t bufsz, double x)
{
    if (!buffer || bufsz < 32) return NULL;

    size_t start = 0;
    if (Jest_signbit(x)) buffer[start++] = '-';

    if (Jest_isinf(x)) {
        strcpy(&buffer[start], "Infinity");
        return buffer;
    }

    if (Jest_isnan(x)) {
        strcpy(&buffer[start], "NaN");
        return buffer;
    }

    sprintf(buffer, "%.15g", x);
    return buffer;
}

char *Jest_boolToStr(char *buffer, size_t bufsz, bool b)
{
    if (!buffer || bufsz < 6) return NULL;
    strcpy(buffer, (b)? "true" : "false");
    return buffer;
}

size_t Jest_readEntireFile(char **out, FILE *file)
{
    if (!out || !file) return 0; 

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    *out = calloc(fsize + 1, 1);
    if (!*out) return 0;

    fread(*out, 1, fsize, file);
    return (size_t)fsize;
}

size_t Jest_readEntireFileFromPath(char **out, const char *path)
{
    if (!out || !path) return 0; 

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    const size_t ret = Jest_readEntireFile(out, f);
    fclose(f);

    return ret;
}

bool Jest_initLexer(Jest_Lexer *l, char *strbuf, size_t strbuf_sz, const char *filebuf, size_t filebuf_sz)
{
    if (!l || !strbuf || !strbuf_sz || !filebuf || !filebuf_sz) return false;

    memset(l, 0, sizeof(*l));
    l->strbuf = strbuf;
    l->strbuf_sz = strbuf_sz;

    l->filebuf = filebuf;
    l->filebuf_sz = filebuf_sz;
    l->filebuf_offset = 0;

    return Jest_lexerStep(l);
}

bool Jest_lexerStep(Jest_Lexer *l)
{
    if (!l) return false;
    l->type = JEST_LEXEME_ERR;

    Jest__lexerSkipCommentAndWhiteSpace(l);
    if (l->filebuf_offset >= l->filebuf_sz) {
        l->type = JEST_LEXEME_EOF;
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
        l->type = JEST_LEXEME_IDENT;
        l->ident_start = l->filebuf_offset;

        while (isalnum(l->filebuf[l->filebuf_offset]) || l->filebuf[l->filebuf_offset] == '_' || l->filebuf[l->filebuf_offset] == '$') {
            ++l->filebuf_offset;
        }

        l->ident_len = l->filebuf_offset - l->ident_start;
        if (!strncmp(&l->filebuf[l->ident_start], "Infinity", l->ident_len)) {
            l->type = JEST_LEXEME_NUM;
            l->numval = JEST_INF;
        } else if (!strncmp(&l->filebuf[l->ident_start], "NaN", l->ident_len)) {
            l->type = JEST_LEXEME_NUM;
            l->numval = JEST_NAN;
        } else if (!strncmp(&l->filebuf[l->ident_start], "true", l->ident_len)) {
            l->type = JEST_LEXEME_BOOL;
            l->boolval = true;
        } else if (!strncmp(&l->filebuf[l->ident_start], "false", l->ident_len)) {
            l->type = JEST_LEXEME_BOOL;
            l->boolval = false;
        } else if (!strncmp(&l->filebuf[l->ident_start], "null", l->ident_len)) {
            l->type = JEST_LEXEME_NULL;
        }

        return true;
    }

    if (isdigit(l->filebuf[l->filebuf_offset]) || l->filebuf[l->filebuf_offset] == '.' || l->filebuf[l->filebuf_offset] == '+' || l->filebuf[l->filebuf_offset] == '-') {
        int tmp = 0;
        if (sscanf(&l->filebuf[l->filebuf_offset], "%lf%n", &l->numval, &tmp) != 1) return false;
        l->filebuf_offset += tmp;
        l->type = JEST_LEXEME_NUM;
        return true;
    }

    return false;

lbl_str:
    l->type = JEST_LEXEME_STR;
    if (Jest__lexerHandleStr(l)) return false;
    return true;
}

Jest_Error Jest_jsonArrayAppend(Jest_JsonVal *arr, const Jest_JsonVal *elem)
{
    if (!arr || !elem || arr == elem) return JEST_ERROR_BADPARAM;
    if (arr->type != JEST_JSONTYPE_ARR) return JEST_ERROR_BADPARAM;

    if (arr->v.as_arr.len + 1 > arr->v.as_arr.cap) {
        arr->v.as_arr.cap = (arr->v.as_arr.cap)? arr->v.as_arr.cap * 2 : 32;
        arr->v.as_arr.elems = (Jest_JsonVal *)realloc(arr->v.as_arr.elems, sizeof(*arr->v.as_arr.elems) * arr->v.as_arr.cap);
        if (!arr->v.as_arr.elems) return JEST_ERROR_NOMEM;
    }

    memcpy(&arr->v.as_arr.elems[arr->v.as_arr.len++], elem, sizeof(*elem));
    return JEST_ERROR_NONE;
}

Jest_Error Jest_jsonObjAdd(Jest_JsonVal *obj, const char *field_name, Jest_JsonVal value)
{
    if (!obj || !field_name) return JEST_ERROR_BADPARAM;
    if (obj->type != JEST_JSONTYPE_OBJ) return JEST_ERROR_BADPARAM;

    const size_t name_len = strlen(field_name);

    for (size_t i = 0; i < obj->v.as_obj.nfields; ++i) {
        if (!strncmp(field_name, obj->v.as_obj.field_names[i], name_len)) {
            memcpy(&obj->v.as_obj.field_values[i], &value, sizeof(value));
            return JEST_ERROR_NONE;
        }
    }

    if (obj->v.as_obj.nfields + 1 > obj->v.as_obj.nalloced) {
        obj->v.as_obj.nalloced = (obj->v.as_obj.nalloced)? 2 * obj->v.as_obj.nalloced : 16;
        obj->v.as_obj.field_names = (char **)realloc(obj->v.as_obj.field_names, sizeof(*obj->v.as_obj.field_names) * obj->v.as_obj.nalloced);
        obj->v.as_obj.fn_lens = (size_t *)realloc(obj->v.as_obj.fn_lens, sizeof(*obj->v.as_obj.fn_lens) * obj->v.as_obj.nalloced);
        obj->v.as_obj.field_values = (Jest_JsonVal *)realloc(obj->v.as_obj.field_values, sizeof(*obj->v.as_obj.field_values) * obj->v.as_obj.nalloced);
        if (!obj->v.as_obj.field_names || !obj->v.as_obj.fn_lens || !obj->v.as_obj.field_values) return JEST_ERROR_NOMEM;
    }

    obj->v.as_obj.fn_lens[obj->v.as_obj.nfields] = name_len;
    obj->v.as_obj.field_names[obj->v.as_obj.nfields] = Jest_strndup(field_name, name_len);
    if (!obj->v.as_obj.field_names[obj->v.as_obj.nfields]) return JEST_ERROR_NOMEM;
    memcpy(&obj->v.as_obj.field_values[obj->v.as_obj.nfields], &value, sizeof(value));
    ++obj->v.as_obj.nfields;

    return JEST_ERROR_NONE;
}

Jest_Error Jest_parseJsonLexer(Jest_JsonVal *out, Jest_Lexer *lexer)
{
    if (!out || !lexer) return JEST_ERROR_BADPARAM;

    switch (lexer->type) {
        case JEST_LEXEME_NULL: Jest__parseNull(out,lexer);  break;
        case JEST_LEXEME_BOOL: Jest__parseBool(out, lexer); break;
        case JEST_LEXEME_NUM:  Jest__parseNum(out, lexer);  break;
        case JEST_LEXEME_STR:  Jest__parseStr(out, lexer);  break;
        case '[':               Jest__parseArr(out, lexer);  break;
        case '{':               Jest__parseObj(out, lexer);  break;
        default: return JEST_ERROR_SYNTAX;
    }

    return JEST_ERROR_NONE;
}

Jest_Error Jest_parseJsonFile(Jest_JsonVal *out, FILE *file)
{
    if (!out || !file) return JEST_ERROR_BADPARAM;

    char *strbuf = calloc(1024, 1);
    if (!strbuf) return JEST_ERROR_NOMEM;

    char *filebuf = NULL;
    size_t filebuf_sz = Jest_readEntireFile(&filebuf, file);

    if (!filebuf || !filebuf_sz) {
        free(strbuf);
        return JEST_ERROR_IO;
    }

    Jest_Lexer l;
    if (!Jest_initLexer(&l, strbuf, 1024, filebuf, filebuf_sz)) {
        free(strbuf);
        free(filebuf);
        return JEST_ERROR_BADLEXER;
    }

    Jest_Error ret = Jest_parseJsonLexer(out, &l);

    free(strbuf);
    free(filebuf);
    return ret;
}

Jest_Error Jest_parseJsonFileFromPath(Jest_JsonVal *out, const char *path)
{
    if (!out || !path) return 0; 

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    const Jest_Error ret = Jest_parseJsonFile(out, f);
    fclose(f);

    return ret;
}

Jest_Error Jest_parseJsonFromStr(Jest_JsonVal *out, const char *str);

void Jest_printJsonVal(FILE *file, const Jest_JsonVal *val, bool escape_unicode)
{
    Jest__printJsonVal(file, val, escape_unicode, 0, 0);
}

void Jest_destroyJsonVal(Jest_JsonVal *val)
{
    if (!val) return;

    switch (val->type) {
        case JEST_JSONTYPE_NULL:
        case JEST_JSONTYPE_BOOL:
        case JEST_JSONTYPE_NUM:
        case JEST_JSONTYPE_ERR:
            break;

        case JEST_JSONTYPE_STR: goto lbl_destroy_str;
        case JEST_JSONTYPE_ARR: goto lbl_destroy_arr;
        case JEST_JSONTYPE_OBJ: goto lbl_destroy_obj;
    }

    memset(val, 0, sizeof(*val));
    return;

lbl_destroy_str:
    free(val->v.as_str.data);
    memset(val, 0, sizeof(*val));
    return;

lbl_destroy_arr:
    for (size_t i = 0; i < val->v.as_arr.len; ++i) {
        Jest_destroyJsonVal(&val->v.as_arr.elems[i]);
    }

    free(val->v.as_arr.elems);
    memset(val, 0, sizeof(*val));
    return;

lbl_destroy_obj:
    for (size_t i = 0; i < val->v.as_obj.nfields; ++i) {
        Jest_destroyJsonVal(&val->v.as_obj.field_values[i]);
        free(val->v.as_obj.field_names[i]);
    }

    free(val->v.as_obj.field_names);
    free(val->v.as_obj.fn_lens);
    free(val->v.as_obj.field_values);
    memset(val, 0, sizeof(*val));
    return;
}

Jest_JsonVal *Jest_jsonIdx(Jest_JsonVal *parent, const char *accessor, Jest_Error *opt_err_out)
{
    if (!parent || !accessor) {
        if (opt_err_out) *opt_err_out = JEST_ERROR_BADPARAM;
        return NULL;
    }

    if (parent->type != JEST_JSONTYPE_OBJ && parent->type != JEST_JSONTYPE_ARR) {
        if (opt_err_out) *opt_err_out = JEST_ERROR_BADPARAM;
        return NULL;
    }

    Jest_Lexer lexer;
    char *strbuf = malloc(1024);
    if (!strbuf) {
        if (opt_err_out) *opt_err_out = JEST_ERROR_NOMEM;
        return NULL;
    }

    if (!Jest_initLexer(&lexer, strbuf, 1024, accessor, strlen(accessor) + 1)) {
        if (opt_err_out) *opt_err_out = JEST_ERROR_BADLEXER;
        free(strbuf);
        return NULL;
    }

    bool is_field_start = false;
    Jest_JsonVal *current = parent;

    const char *field_name = NULL;
    size_t field_name_len = 0;
    size_t elem_idx = (size_t)-1;

    do {
        if (is_field_start && current->type == JEST_JSONTYPE_OBJ) {
            if (lexer.type != JEST_LEXEME_STR && lexer.type != JEST_LEXEME_IDENT) {
                if (opt_err_out) *opt_err_out = JEST_ERROR_BADPARAM;
                return NULL;
            }

            field_name = (lexer.type == JEST_LEXEME_STR)
                ? lexer.strbuf
                : &lexer.filebuf[lexer.ident_start];

            field_name_len = (lexer.type == JEST_LEXEME_STR)
                ? lexer.strval_len
                : lexer.ident_len;

            size_t i; // forward-declare 'i' for error-handling purposes
            for (i = 0; i < current->v.as_obj.nfields; ++i) {
                if (!strncmp(current->v.as_obj.field_names[i], field_name, field_name_len)) {
                    break;
                }
            }

            if (i >= current->v.as_obj.nfields) {
                if (opt_err_out) *opt_err_out = JEST_ERROR_BADPARAM;
                free(strbuf);
                return NULL;
            }

            current = &current->v.as_obj.field_values[i];

            is_field_start = false;

            Jest_lexerStep(&lexer);
            if (lexer.type != ']') {
                if (opt_err_out) *opt_err_out = JEST_ERROR_SYNTAX;
                free(strbuf);
                return NULL;
            }
            continue;
        }

        if (is_field_start && current->type == JEST_JSONTYPE_ARR) {
            if (lexer.type != JEST_LEXEME_NUM) {
                if (opt_err_out) *opt_err_out = JEST_ERROR_SYNTAX;
                free(strbuf);
                return NULL;
            }

            elem_idx = (size_t)lexer.numval;
            if (elem_idx >= current->v.as_arr.len) {
                if (opt_err_out) *opt_err_out = JEST_ERROR_BADPARAM;
                free(strbuf);
                return NULL;
            }

            current = &current->v.as_arr.elems[elem_idx];

            is_field_start = false;
            
            Jest_lexerStep(&lexer);
            if (lexer.type != ']') {
                if (opt_err_out) *opt_err_out = JEST_ERROR_SYNTAX;
                free(strbuf);
                return NULL;
            }
            continue;
        }

        if (lexer.type == '[') {
            if (is_field_start) {
                if (opt_err_out) *opt_err_out = JEST_ERROR_SYNTAX;
                free(strbuf);
                return NULL;
            }

            is_field_start = true;
            continue;
        }

        if (opt_err_out) *opt_err_out = JEST_ERROR_SYNTAX;
        free(strbuf);
        return NULL;
    } while (Jest_lexerStep(&lexer));

    free(strbuf);

    if (opt_err_out) *opt_err_out = JEST_ERROR_NONE;
    return current;
}

static void Jest__lexerSkipCommentAndWhiteSpace(Jest_Lexer *l)
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
    Jest__lexerSkipCommentAndWhiteSpace(l); // skip any extra comments and such
}

static Jest_Error Jest__lexerHandleStr(Jest_Lexer *l)
{
    if (!l) return JEST_ERROR_BADPARAM;

    Jest__lexerSkipCommentAndWhiteSpace(l);
    if (l->filebuf[l->filebuf_offset] != '\'' && l->filebuf[l->filebuf_offset] != '\"') {
        return JEST_ERROR_BADPARAM;
    }

    l->type = JEST_LEXEME_STR;
    const char quot = l->filebuf[l->filebuf_offset++];

    l->strval_len = 0;
    while (l->filebuf[l->filebuf_offset] != quot) {
        if (l->filebuf[l->filebuf_offset] == '\\') {
            l->filebuf_offset++;
            switch (l->filebuf[l->filebuf_offset]) {
                case '\n':
                case '\r':
                    while (isspace(l->filebuf[l->filebuf_offset + 1])) ++l->filebuf_offset;
                    break;

                case '\\': l->strbuf[l->strval_len++] = '\\'; break;
                case '\'': l->strbuf[l->strval_len++] = '\''; break;
                case '\"': l->strbuf[l->strval_len++] = '\"'; break;
                case '/':  l->strbuf[l->strval_len++] =  '/'; break;
                case 'b':  l->strbuf[l->strval_len++] = '\b'; break;
                case 'f':  l->strbuf[l->strval_len++] = '\f'; break;
                case 'n':  l->strbuf[l->strval_len++] = '\n'; break;
                case 'r':  l->strbuf[l->strval_len++] = '\r'; break;
                case 't':  l->strbuf[l->strval_len++] = '\t'; break;
                case 'v':  l->strbuf[l->strval_len++] = '\v'; break;
                case '0':  l->strbuf[l->strval_len++] = '\0'; break;
                case 'u': {
                    --l->filebuf_offset;
                    int offset = 0;
                    uint32_t codepoint = 0;
                    sscanf(&l->filebuf[l->filebuf_offset], "\\u%4" SCNx32 "%n", &codepoint, &offset);
                    if (offset < 6) return JEST_ERROR_BADCHAR;

                    // if codepoint is a hi surrogate
                    if (0xd800 <= codepoint && codepoint <= 0xdfbb) {
                        uint32_t lo = 0;
                        const int sscanf_result = sscanf(&l->filebuf[l->filebuf_offset + 6], "\\u%4" SCNx32 "%n", &lo, &offset);

                        if (sscanf_result == 1) { // if there is a second unicode char
                            if (offset < 6) return JEST_ERROR_BADCHAR;

                            if (0xdc00 <= lo && lo <= 0xdfff) { // check if it's a lo surrogate to match the hi surrogate
                                offset = 12;
                                codepoint = ((codepoint - 0xd800) << 10) + ((lo - 0xdc00)) + 0x10000;
                            } else { // if not set the offset to 6 so that it isn't skipped on the next iteration of the loop
                                offset =  6;
                            }
                        }
                    }

                    if (codepoint <= 0x7F) {
                        l->strbuf[l->strval_len++] = (char)codepoint;
                    } else if (codepoint <= 0x7FF) {
                        l->strbuf[l->strval_len++] = (char)(((codepoint >> 6) & 0x1f)  | 0xc0);
                        l->strbuf[l->strval_len++] = (char)(((codepoint)      & 0x3f)  | 0x80);
                    } else if (codepoint <= 0xFFFF) {
                        l->strbuf[l->strval_len++] = (char)(((codepoint >> 12) & 0x0f) | 0xe0);
                        l->strbuf[l->strval_len++] = (char)(((codepoint >> 6)  & 0x3f) | 0x80);
                        l->strbuf[l->strval_len++] = (char)(((codepoint)       & 0x3f) | 0x80);
                    } else if (codepoint <= 0x10FFFF) {
                        l->strbuf[l->strval_len++] = (char)(((codepoint >> 18) & 0x07) | 0xf0);
                        l->strbuf[l->strval_len++] = (char)(((codepoint >> 12) & 0x3f) | 0x80);
                        l->strbuf[l->strval_len++] = (char)(((codepoint >> 6)  & 0x3f) | 0x80);
                        l->strbuf[l->strval_len++] = (char)(((codepoint)       & 0x3f) | 0x80);
                    } else {
                        return JEST_ERROR_BADCHAR;
                    }

                    l->filebuf_offset += offset - 1;
                } break;

                case 'x': {
                    --l->filebuf_offset;
                    int offset = 0;
                    uint32_t codepoint = 0;
                    sscanf(&l->filebuf[l->filebuf_offset], "\\x%2" SCNx32 "%n", &codepoint, &offset);
                    if (offset < 4) return JEST_ERROR_BADCHAR;

                    if (codepoint <= 0x7F) {
                        l->strbuf[l->strval_len++] = (char)codepoint;
                    } else {
                        l->strbuf[l->strval_len++] = (char)(((codepoint >> 6) & 0x1f)  | 0xc0);
                        l->strbuf[l->strval_len++] = (char)(((codepoint)      & 0x3f)  | 0x80);
                    }

                    l->filebuf_offset += offset - 1;
                } break;
                default: l->strbuf[l->strval_len++] = l->filebuf[l->filebuf_offset]; break;
            }

            ++l->filebuf_offset;
            continue;
        }

        l->strbuf[l->strval_len++] = l->filebuf[l->filebuf_offset];
        ++l->filebuf_offset;
    }

    ++l->filebuf_offset;
    return JEST_ERROR_NONE;
}

static Jest_Error Jest__parseNull(Jest_JsonVal *out, Jest_Lexer *lexer)
{
    if (!out || !lexer) return JEST_ERROR_BADPARAM;
    if (lexer->type != JEST_LEXEME_NULL) return JEST_ERROR_BADPARAM;

    out->type = JEST_JSONTYPE_NULL;
    Jest_lexerStep(lexer);

    return JEST_ERROR_NONE;
}

static Jest_Error Jest__parseBool(Jest_JsonVal *out, Jest_Lexer *lexer)
{
    if (!out || !lexer) return JEST_ERROR_BADPARAM;
    if (lexer->type != JEST_LEXEME_BOOL) return JEST_ERROR_BADPARAM;

    out->type = JEST_JSONTYPE_BOOL;
    out->v.as_bool = lexer->boolval;
    Jest_lexerStep(lexer);

    return JEST_ERROR_NONE;
}

static Jest_Error Jest__parseNum(Jest_JsonVal *out, Jest_Lexer *lexer)
{
    if (!out || !lexer) return JEST_ERROR_BADPARAM;
    if (lexer->type != JEST_LEXEME_NUM) return JEST_ERROR_BADPARAM;

    out->type = JEST_JSONTYPE_NUM;
    out->v.as_num = lexer->numval;
    Jest_lexerStep(lexer);

    return JEST_ERROR_NONE;
}

static Jest_Error Jest__parseStr(Jest_JsonVal *out, Jest_Lexer *lexer)
{
    if (!out || !lexer) return JEST_ERROR_BADPARAM;
    if (lexer->type != JEST_LEXEME_STR) return JEST_ERROR_BADPARAM;

    out->type = JEST_JSONTYPE_STR;
    out->v.as_str.len = lexer->strval_len;
    out->v.as_str.data   = Jest_strndup(lexer->strbuf, lexer->strval_len);
    if (!out->v.as_str.data) return JEST_ERROR_NOMEM;

    Jest_lexerStep(lexer);
    return JEST_ERROR_NONE;
}

static Jest_Error Jest__parseArr(Jest_JsonVal *out, Jest_Lexer *lexer)
{
    if (!out || !lexer) return JEST_ERROR_BADPARAM;
    if (lexer->type != '[') return JEST_ERROR_BADPARAM;

    out->type = JEST_JSONTYPE_ARR;
    memset(&out->v, 0, sizeof(out->v));

    do {
        Jest_lexerStep(lexer);
        if (lexer->type == ']') break;
        Jest_JsonVal elem;
        Jest_parseJsonLexer(&elem, lexer);
        Jest_jsonArrayAppend(out, &elem);
    } while (lexer->type == ',');
    if (lexer->type != ']') return JEST_ERROR_SYNTAX;

    Jest_lexerStep(lexer);
    return JEST_ERROR_NONE;
}

static Jest_Error Jest__parseObj(Jest_JsonVal *out, Jest_Lexer *lexer)
{
    if (!out || !lexer) return JEST_ERROR_BADPARAM;
    if (lexer->type != '{') return JEST_ERROR_BADPARAM;

    out->type = JEST_JSONTYPE_OBJ;
    memset(&out->v, 0, sizeof(out->v));

    do {
    Jest_lexerStep(lexer);
        if (lexer->type == '}') break;
        char *name = NULL;
        size_t name_len = 0;
        
        if (lexer->type != JEST_LEXEME_STR && lexer->type != JEST_LEXEME_IDENT) return JEST_ERROR_BADPARAM;
        
        name_len = (lexer->type == JEST_LEXEME_STR)? lexer->strval_len : lexer->ident_len;
        name = Jest_strndup(
            (lexer->type == JEST_LEXEME_STR)
                ?lexer->strbuf
                :&lexer->filebuf[lexer->ident_start],
            name_len
        );

        if (!name) return JEST_ERROR_NOMEM;

        Jest_lexerStep(lexer);
        if (lexer->type != ':') return JEST_ERROR_SYNTAX;
        Jest_lexerStep(lexer);

        Jest_JsonVal elem;
        Jest_parseJsonLexer(&elem, lexer);
        Jest_jsonObjAdd(out, name, elem);
        free(name);
    } while (lexer->type == ',');
    if (lexer->type != '}') return JEST_ERROR_SYNTAX;

    Jest_lexerStep(lexer);
    return JEST_ERROR_NONE;
}

static void Jest__printJsonVal(FILE *file, const Jest_JsonVal *val, bool escape_unicode, int starttabs, int midtabs)
{
    if (!file || !val) return;

    for (int i = 0; i < starttabs; ++i) fputc('\t', file);

    switch (val->type) {
        case JEST_JSONTYPE_NULL: fprintf(file, "%s", "null"); break;
        case JEST_JSONTYPE_BOOL: fprintf(file, "%s", (val->v.as_bool)? "true" : "false"); break;
        case JEST_JSONTYPE_NUM:  goto lbl_print_num;
        case JEST_JSONTYPE_STR:  goto lbl_print_str;
        case JEST_JSONTYPE_ARR:  goto lbl_print_arr;
        case JEST_JSONTYPE_OBJ:  goto lbl_print_obj;
        case JEST_JSONTYPE_ERR:  break;
    }

    return;

lbl_print_num:
    if (Jest_isinf(val->v.as_num)) {
        if (Jest_signbit(val->v.as_num)) fputc('-', file);
        fprintf(file, "%s", "Infinity");
        return;
    }

    if (Jest_isnan(val->v.as_num)) {
        if (Jest_signbit(val->v.as_num)) fputc('-', file);
        fprintf(file, "%s", "NaN");
        return;
    }

    fprintf(file, "%.15g", val->v.as_num);
    return;

lbl_print_str:
    fputc('"', file);
    for (size_t i = 0; i < val->v.as_str.len; ++i) {
        switch (val->v.as_str.data[i]) {
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

        uint32_t codepoint = (uint8_t)(val->v.as_str.data[i]);

        if (iscntrl((int)codepoint)) {
            fprintf(file, "\\u%.4" PRIx32, codepoint);
            continue;
        }

        if (escape_unicode && codepoint >= 0x80) {
            if (codepoint & 0xf0) {
                codepoint = (codepoint & 0x07) << 6;
                codepoint |= ((uint8_t)(val->v.as_str.data[++i]) & 0x3f);
                codepoint <<= 6;
                codepoint |= ((uint8_t)(val->v.as_str.data[++i]) & 0x3f);
                codepoint <<= 6;
                codepoint |= ((uint8_t)(val->v.as_str.data[++i]) & 0x3f);

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
                codepoint |= ((uint8_t)(val->v.as_str.data[++i]) & 0x3f);
                fprintf(file, "\\u%.4" PRIx32, codepoint);
                continue;
            }

            if (codepoint & 0xe0) {
                codepoint = (codepoint & 0x0f) << 6;
                codepoint |= ((uint8_t)(val->v.as_str.data[++i]) & 0x3f);
                codepoint <<= 6;
                codepoint |= ((uint8_t)(val->v.as_str.data[++i]) & 0x3f);
                fprintf(file, "\\u%.4" PRIx32, codepoint);
                continue;
            }
        } else fputc(val->v.as_str.data[i], file);
    }
    fputc('"', file);

    return;

lbl_print_arr:
    fputc('[', file);

    for (size_t i = 0; i < val->v.as_arr.len; ++i) {
        if (i) fputc(',', file);
        fputc('\n', file);

        Jest__printJsonVal(file, &val->v.as_arr.elems[i], escape_unicode, midtabs + 1, midtabs + 1);
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

        Jest_JsonVal name_as_val;
        name_as_val.type = JEST_JSONTYPE_STR;
        name_as_val.v.as_str.len = val->v.as_obj.fn_lens[i];
        name_as_val.v.as_str.data   = val->v.as_obj.field_names[i];

        Jest__printJsonVal(file, &name_as_val, escape_unicode, midtabs + 1, midtabs + 1);
        fputc(':', file);
        fputc(' ', file);
        Jest__printJsonVal(file, &val->v.as_obj.field_values[i], escape_unicode, 0, midtabs + 1);
    }

    fputc('\n', file);
    for (int i = 0; i < midtabs; ++i) fputc('\t', file);
    fputc('}', file);
    return;
}

#endif // JEST_IMPL
