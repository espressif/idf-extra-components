/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * SPDX-FileContributor: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

/**
 * JSON type identifier. Basic types are:
 *  o Object
 *  o Array
 *  o String
 *  o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1 << 0,
    JSMN_ARRAY = 1 << 1,
    JSMN_STRING = 1 << 2,
    JSMN_PRIMITIVE = 1 << 3
} jsmntype_t;

enum jsmnerr {
    /* Not enough tokens were provided */
    JSMN_ERROR_NOMEM = -1,
    /* Invalid character inside JSON string */
    JSMN_ERROR_INVAL = -2,
    /* The string is not a full JSON packet, more bytes expected */
    JSMN_ERROR_PART = -3
};

/**
 * JSON token description.
 * type     type (object, array, string etc.)
 * start    start position in JSON data string
 * end      end position in JSON data string
 */
typedef struct jsmntok {
    jsmntype_t type;
    int start;
    int end;
    int size;
#ifdef JSMN_PARENT_LINKS
    int parent;
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string.
 */
typedef struct jsmn_parser {
    unsigned int pos;     /* offset in the JSON string */
    unsigned int toknext; /* next token to allocate */
    int toksuper;         /* superior token node, e.g. parent object or array */
    int state;            /* grammar state, used by JSMN_STRICT (see jsmn_state) */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
JSMN_API void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each
 * describing
 * a single JSON object.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens);

#ifndef JSMN_HEADER
/**
 * Allocates a fresh unused token from the token pool.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
                                   const size_t num_tokens)
{
    jsmntok_t *tok;
    if (parser->toknext >= num_tokens) {
        return NULL;
    }
    tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
#ifdef JSMN_PARENT_LINKS
    tok->parent = -1;
#endif
    return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type,
                            const int start, const int end)
{
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

/* Value of a hex digit, or -1 */
static int jsmn_hexval(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

#ifdef JSMN_STRICT
/*
 * Where a strict parse is in the RFC 8259 grammar. Only the pass that
 * receives tokens validates; the counting pass (tokens == NULL) just counts.
 */
enum jsmn_state {
    JSMN_ST_ROOT_VALUE = 0, /* expecting the one top-level value */
    JSMN_ST_ROOT_END,       /* top-level value complete: only whitespace may follow */
    JSMN_ST_OBJ_OPEN,       /* just after '{': a name, or '}' */
    JSMN_ST_OBJ_NAME,       /* just after ',' in an object: a name */
    JSMN_ST_OBJ_COLON,      /* just after a name: ':' */
    JSMN_ST_OBJ_VALUE,      /* just after ':': a value */
    JSMN_ST_OBJ_END,        /* just after a member: ',' or '}' */
    JSMN_ST_ARR_OPEN,       /* just after '[': a value, or ']' */
    JSMN_ST_ARR_VALUE,      /* just after ',' in an array: a value */
    JSMN_ST_ARR_END         /* just after an element: ',' or ']' */
};

static int jsmn_state_allows_value(int st)
{
    return st == JSMN_ST_ROOT_VALUE || st == JSMN_ST_OBJ_VALUE ||
           st == JSMN_ST_ARR_OPEN || st == JSMN_ST_ARR_VALUE;
}

static int jsmn_state_allows_name(int st)
{
    return st == JSMN_ST_OBJ_OPEN || st == JSMN_ST_OBJ_NAME;
}

/* '}' or ']' may not follow ',' (no trailing comma), a name, or ':' */
static int jsmn_state_allows_close(int st, char c)
{
    return c == '}' ? (st == JSMN_ST_OBJ_OPEN || st == JSMN_ST_OBJ_END)
           : (st == JSMN_ST_ARR_OPEN || st == JSMN_ST_ARR_END);
}

static int jsmn_state_after_value(int st)
{
    if (st == JSMN_ST_ROOT_VALUE) {
        return JSMN_ST_ROOT_END;
    }
    return st == JSMN_ST_OBJ_VALUE ? JSMN_ST_OBJ_END : JSMN_ST_ARR_END;
}

/* After a container closes, what encloses it decides what may follow */
static int jsmn_state_after_close(const jsmn_parser *parser, const jsmntok_t *tokens)
{
    if (parser->toksuper == -1) {
        return JSMN_ST_ROOT_END;
    }
    return tokens[parser->toksuper].type == JSMN_ARRAY ? JSMN_ST_ARR_END : JSMN_ST_OBJ_END;
}

static size_t jsmn_skip_digits(const char *s, size_t i, size_t n)
{
    while (i < n && s[i] >= '0' && s[i] <= '9') {
        i++;
    }
    return i;
}

/* true, false, null, or a number as RFC 8259 section 6 spells it */
static int jsmn_primitive_valid(const char *s, size_t n)
{
    size_t i = 0, j;
    if (n == 4 && (!memcmp(s, "true", 4) || !memcmp(s, "null", 4))) {
        return 1;
    }
    if (n == 5 && !memcmp(s, "false", 5)) {
        return 1;
    }
    if (i < n && s[i] == '-') {
        i++;
    }
    /* int: a single zero, or digits without a leading zero */
    if (i < n && s[i] == '0') {
        i++;
    } else if ((j = jsmn_skip_digits(s, i, n)) == i) {
        return 0;
    } else {
        i = j;
    }
    if (i < n && s[i] == '.') {
        if ((j = jsmn_skip_digits(s, i + 1, n)) == i + 1) {
            return 0;
        }
        i = j;
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < n && (s[i] == '+' || s[i] == '-')) {
            i++;
        }
        if ((j = jsmn_skip_digits(s, i, n)) == i) {
            return 0;
        }
        i = j;
    }
    return i == n;
}

/*
 * Length of the well-formed UTF-8 sequence starting at js[pos] (RFC 3629
 * section 4: no overlong forms, no surrogates, nothing above U+10FFFF), or 0.
 */
static size_t jsmn_utf8_seq_len(const char *js, size_t pos, size_t len)
{
    unsigned char c = (unsigned char)js[pos];
    unsigned char lo = 0x80, hi = 0xBF;
    size_t need, i;
    if (c >= 0xC2 && c <= 0xDF) {
        need = 1;
    } else if (c >= 0xE0 && c <= 0xEF) {
        need = 2;
        if (c == 0xE0) {
            lo = 0xA0;
        } else if (c == 0xED) {
            hi = 0x9F;
        }
    } else if (c >= 0xF0 && c <= 0xF4) {
        need = 3;
        if (c == 0xF0) {
            lo = 0x90;
        } else if (c == 0xF4) {
            hi = 0x8F;
        }
    } else {
        return 0;
    }
    if (pos + need >= len) {
        return 0;
    }
    for (i = 1; i <= need; i++) {
        unsigned char cc = (unsigned char)js[pos + i];
        if (cc < lo || cc > hi) {
            return 0;
        }
        lo = 0x80;
        hi = 0xBF;
    }
    return need + 1;
}
#define JSMN_AT_END(js, pos, len) ((pos) >= (len))
#else
#define JSMN_AT_END(js, pos, len) ((pos) >= (len) || (js)[(pos)] == '\0')
#endif

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
                                const size_t len, jsmntok_t *tokens,
                                const size_t num_tokens)
{
    jsmntok_t *token;
    int start;

    start = parser->pos;

    for (; !JSMN_AT_END(js, parser->pos, len); parser->pos++) {
        switch (js[parser->pos]) {
#ifndef JSMN_STRICT
        /* In strict mode primitive must be followed by "," or "}" or "]" */
        case ':':
#endif
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case ',':
        case ']':
        case '}':
            goto found;
        default:
            /* to quiet a warning from gcc*/
            break;
        }
        if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
            parser->pos = start;
            return JSMN_ERROR_INVAL;
        }
    }
    /* End of input terminates a primitive: a lone top-level value is a JSON
     * text (RFC 8259 section 2). An unclosed container is caught by the
     * caller. */

found:
    if (tokens == NULL) {
        parser->pos--;
        return 0;
    }
#ifdef JSMN_STRICT
    if (!jsmn_primitive_valid(js + start, parser->pos - start)) {
        parser->pos = start;
        return JSMN_ERROR_INVAL;
    }
#endif
    token = jsmn_alloc_token(parser, tokens, num_tokens);
    if (token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
    }
    jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
    token->parent = parser->toksuper;
#endif
    parser->pos--;
    return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
                             const size_t len, jsmntok_t *tokens,
                             const size_t num_tokens)
{
    jsmntok_t *token;

    int start = parser->pos;

    /* Skip starting quote */
    parser->pos++;

    for (; !JSMN_AT_END(js, parser->pos, len); parser->pos++) {
        char c = js[parser->pos];

#ifdef JSMN_STRICT
        /* Control characters must be escaped (RFC 8259 section 7) and the
         * text must be UTF-8 (section 8.1) */
        if (tokens != NULL) {
            if ((unsigned char)c < 0x20) {
                parser->pos = start;
                return JSMN_ERROR_INVAL;
            }
            if ((unsigned char)c >= 0x80) {
                size_t seq = jsmn_utf8_seq_len(js, parser->pos, len);
                if (seq == 0) {
                    parser->pos = start;
                    return JSMN_ERROR_INVAL;
                }
                parser->pos += seq - 1;
                continue;
            }
        }
#endif
        /* Quote: end of string */
        if (c == '\"') {
            if (tokens == NULL) {
                return 0;
            }
            token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (token == NULL) {
                parser->pos = start;
                return JSMN_ERROR_NOMEM;
            }
            jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
            token->parent = parser->toksuper;
#endif
            return 0;
        }

        /* Backslash: Quoted symbol expected */
        if (c == '\\' && parser->pos + 1 < len) {
            int i;
            parser->pos++;
            switch (js[parser->pos]) {
            /* Allowed escaped symbols */
            case '\"':
            case '/':
            case '\\':
            case 'b':
            case 'f':
            case 'r':
            case 'n':
            case 't':
                break;
            /* Allows escaped symbol \uXXXX */
            case 'u':
                parser->pos++;
                for (i = 0; i < 4 && !JSMN_AT_END(js, parser->pos, len); i++) {
                    /* If it isn't a hex character we have an error */
                    if (jsmn_hexval(js[parser->pos]) < 0) {
                        parser->pos = start;
                        return JSMN_ERROR_INVAL;
                    }
                    parser->pos++;
                }
                parser->pos--;
                break;
            /* Unexpected symbol */
            default:
                parser->pos = start;
                return JSMN_ERROR_INVAL;
            }
        }
    }
    parser->pos = start;
    return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens)
{
    int r;
    int i;
    jsmntok_t *token;
    int count = parser->toknext;

    for (; !JSMN_AT_END(js, parser->pos, len); parser->pos++) {
        char c;
        jsmntype_t type;

        c = js[parser->pos];
        switch (c) {
        case '{':
        case '[':
            count++;
            if (tokens == NULL) {
                break;
            }
#ifdef JSMN_STRICT
            if (!jsmn_state_allows_value(parser->state)) {
                return JSMN_ERROR_INVAL;
            }
#endif
            token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (token == NULL) {
                return JSMN_ERROR_NOMEM;
            }
            if (parser->toksuper != -1) {
                jsmntok_t *t = &tokens[parser->toksuper];
                t->size++;
#ifdef JSMN_PARENT_LINKS
                token->parent = parser->toksuper;
#endif
            }
            token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
            token->start = parser->pos;
            parser->toksuper = parser->toknext - 1;
#ifdef JSMN_STRICT
            parser->state = (c == '{' ? JSMN_ST_OBJ_OPEN : JSMN_ST_ARR_OPEN);
#endif
            break;
        case '}':
        case ']':
            if (tokens == NULL) {
                break;
            }
            type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_STRICT
            if (!jsmn_state_allows_close(parser->state, c)) {
                return JSMN_ERROR_INVAL;
            }
#endif
#ifdef JSMN_PARENT_LINKS
            if (parser->toknext < 1) {
                return JSMN_ERROR_INVAL;
            }
            token = &tokens[parser->toknext - 1];
            for (;;) {
                if (token->start != -1 && token->end == -1) {
                    if (token->type != type) {
                        return JSMN_ERROR_INVAL;
                    }
                    token->end = parser->pos + 1;
                    parser->toksuper = token->parent;
                    break;
                }
                if (token->parent == -1) {
                    if (token->type != type || parser->toksuper == -1) {
                        return JSMN_ERROR_INVAL;
                    }
                    break;
                }
                token = &tokens[token->parent];
            }
#else
            for (i = parser->toknext - 1; i >= 0; i--) {
                token = &tokens[i];
                if (token->start != -1 && token->end == -1) {
                    if (token->type != type) {
                        return JSMN_ERROR_INVAL;
                    }
                    parser->toksuper = -1;
                    token->end = parser->pos + 1;
                    break;
                }
            }
            /* Error if unmatched closing bracket */
            if (i == -1) {
                return JSMN_ERROR_INVAL;
            }
            for (; i >= 0; i--) {
                token = &tokens[i];
                if (token->start != -1 && token->end == -1) {
                    parser->toksuper = i;
                    break;
                }
            }
#endif
#ifdef JSMN_STRICT
            parser->state = jsmn_state_after_close(parser, tokens);
#endif
            break;
        case '\"':
#ifdef JSMN_STRICT
            if (tokens != NULL && !jsmn_state_allows_value(parser->state) &&
                    !jsmn_state_allows_name(parser->state)) {
                return JSMN_ERROR_INVAL;
            }
#endif
            r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
            if (r < 0) {
                return r;
            }
            count++;
            if (parser->toksuper != -1 && tokens != NULL) {
                tokens[parser->toksuper].size++;
            }
#ifdef JSMN_STRICT
            if (tokens != NULL) {
                parser->state = jsmn_state_allows_name(parser->state) ? JSMN_ST_OBJ_COLON
                                : jsmn_state_after_value(parser->state);
            }
#endif
            break;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
            break;
        case ':':
#ifdef JSMN_STRICT
            if (tokens != NULL) {
                if (parser->state != JSMN_ST_OBJ_COLON) {
                    return JSMN_ERROR_INVAL;
                }
                parser->state = JSMN_ST_OBJ_VALUE;
            }
#endif
            parser->toksuper = parser->toknext - 1;
            break;
        case ',':
#ifdef JSMN_STRICT
            if (tokens != NULL) {
                if (parser->state == JSMN_ST_OBJ_END) {
                    parser->state = JSMN_ST_OBJ_NAME;
                } else if (parser->state == JSMN_ST_ARR_END) {
                    parser->state = JSMN_ST_ARR_VALUE;
                } else {
                    return JSMN_ERROR_INVAL;
                }
            }
#endif
            if (tokens != NULL && parser->toksuper != -1 &&
                    tokens[parser->toksuper].type != JSMN_ARRAY &&
                    tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
                parser->toksuper = tokens[parser->toksuper].parent;
#else
                for (i = parser->toknext - 1; i >= 0; i--) {
                    if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
                        if (tokens[i].start != -1 && tokens[i].end == -1) {
                            parser->toksuper = i;
                            break;
                        }
                    }
                }
#endif
            }
            break;
        /* Anything else is a primitive: in strict mode jsmn_parse_primitive
         * accepts only true, false, null and numbers, and only where a
         * value may appear (never as a name) */
        default:
#ifdef JSMN_STRICT
            if (tokens != NULL && !jsmn_state_allows_value(parser->state)) {
                return JSMN_ERROR_INVAL;
            }
#endif
            r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
            if (r < 0) {
                return r;
            }
            count++;
            if (parser->toksuper != -1 && tokens != NULL) {
                tokens[parser->toksuper].size++;
            }
#ifdef JSMN_STRICT
            if (tokens != NULL) {
                parser->state = jsmn_state_after_value(parser->state);
            }
#endif
            break;
        }
    }

    if (tokens != NULL) {
        for (i = parser->toknext - 1; i >= 0; i--) {
            /* Unmatched opened object or array */
            if (tokens[i].start != -1 && tokens[i].end == -1) {
                return JSMN_ERROR_PART;
            }
        }
#ifdef JSMN_STRICT
        /* Exactly one complete top-level value */
        if (parser->state != JSMN_ST_ROOT_END) {
            return JSMN_ERROR_PART;
        }
#endif
    }

    return count;
}

/**
 * Creates a new parser based over a given buffer with an array of tokens
 * available.
 */
JSMN_API void jsmn_init(jsmn_parser *parser)
{
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
    parser->state = 0;
}

#endif /* JSMN_HEADER */

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */
