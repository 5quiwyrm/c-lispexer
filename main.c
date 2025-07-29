#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "libdyn.h"

#define throw(str) do { \
    fprintf(stderr, "[ERROR]: "str); \
    exit(1); \
} while (0) \

bool is_whitespace(char ch) {
    return (ch == ' ' || ch == '\n' || ch == '\r');
}
bool is_numeric(char ch) {
    return (ch > 47 && ch < 58);
}

typedef struct {
    char *contents;
    DString ds;
    size_t count;
    size_t head;
    size_t pos_line;
    size_t pos_idx;
    size_t old_head;
    size_t old_pos_line;
    size_t old_pos_idx;
} Lispexer;

// The `Lispexer` lives for as long as ds.items lives for.
#define lispexer_from_dstring(dstr) { \
    .contents = dstr.items, \
    .ds = {0}, \
    .count = dstr.count, \
    .head = 0, \
    .pos_line = 0, \
    .pos_idx = 0, \
    .old_head = 0, \
    .old_pos_line = 0, \
    .old_pos_idx = 0, \
}

#define free_lispexer(le) do { \
    free((le)->contents); \
    free((le)->ds.items); \
} while (0)

void lispexer_next(Lispexer *l) {
    dstring_clear(&l->ds);
    l->old_head = l->head;
    l->old_pos_idx = l->pos_idx;
    l->old_pos_line = l->pos_line;
    bool in_string = false;
    bool escaping = false;
    for (; l->head < l->count; l->head++) {
        char ch = l->contents[l->head];
        l->pos_idx++;
        if (ch == '\n') {
            l->pos_line++;
            l->pos_idx = 0;
        }
        else if (escaping) {
            escaping = false;
            dstring_append(&l->ds, ch);
            continue;
        }
        //   NOTE: the folllowing `&& !escaping`s are redundant. `escaping` is
        //      already guaranteed to be false at this point.
        //      These checks are added for readability, and may be removed.
        else if (in_string && ch == '\"' && !escaping) {
            in_string = false;
            dstring_append(&l->ds, ch);
            l->head++;
            goto next;
        }
        else if (in_string && ch == '\\' && !escaping) {
            escaping = true;
            dstring_append(&l->ds, ch);
            continue;
        }
        else if (!in_string && ch == '\"') {
            if (l->ds.count != 0) {
                goto next;
            } else {
                in_string = true;
                dstring_append(&l->ds, ch);
                continue;
            }
        }
        else if (
            !in_string
            && is_whitespace(ch)
        ) {
            if (l->ds.count == 0) {
                continue;
            } else {
                l->head++;
                goto next;
            }
        }
        else if (!in_string && ch == '(') {
            if (l->ds.count != 0) {
                l->pos_idx--;
                goto next;
            } else {
                dstring_append(&l->ds, ch);
                l->head++;
                goto next;
            }
        }
        else if (!in_string && ch == ')') {
            if (l->ds.count != 0) {
                l->pos_idx--;
                goto next;
            } else {
                dstring_append(&l->ds, ch);
                l->head++;
                goto next;
            }
        }
        else { dstring_append(&l->ds, ch); }
    }
    next:
    return;
}

typedef enum {
    TOKEN_INT,
    TOKEN_IDENT,
    TOKEN_STRING,
    TOKEN_SYNTAX,
    TOKEN_MALFORMED,
} TokenKind;

const char *display_kind(TokenKind k) {
    switch (k) {
        case TOKEN_INT:
            return "int";
        case TOKEN_IDENT:
            return "ident";
        case TOKEN_STRING:
            return "string";
        case TOKEN_SYNTAX:
            return "syntax";
        case TOKEN_MALFORMED:
            return "\x1b[31mMALFORMED\x1b[0m";
    }
}

typedef enum {
    SYNTAX_OPAREN,
    SYNTAX_CPAREN,
} SyntaxKind;

const char *display_syntax(SyntaxKind s) {
    switch (s) {
        case SYNTAX_OPAREN: return "(";
        case SYNTAX_CPAREN: return ")";
    }
}

typedef union {
    int as_int;
    char *as_ident;
    char *as_string;
    SyntaxKind as_syntax;
} TokenData;

typedef struct {
    TokenKind kind;
    TokenData data;
    size_t line;
    size_t idx;
} LToken;

const LToken LToken_malformed = {
    .kind = TOKEN_MALFORMED,
    .data = 0,
    .line = 0,
    .idx = 0,
};

LToken to_ltoken(DString *ds, size_t line, size_t idx) {
    if (ds->count == 0) {
        return LToken_malformed;
    }
    if (ds->items[0] == '\"') {
        if (ds->count < 2) {
            fprintf(stderr, "string literal should be at least 2 characters long\n");
            return LToken_malformed;
        }
        TokenData data;
        data.as_string = strdup(ds->items + 1);
        data.as_string[ds->count - 2] = '\0';
        LToken ret = {
            .kind = TOKEN_STRING,
            .data = data,
            .line = line,
            .idx = idx,
        };
        return ret;
    } else if (is_numeric(ds->items[0])) {
        LToken ret = {
            .kind = TOKEN_INT,
            .data = atoi(ds->items),
            .line = line,
            .idx = idx,
        };
        return ret;
    } else if (strcmp(ds->items, "(") == 0) {
        LToken ret = {
            .kind = TOKEN_SYNTAX,
            .data = SYNTAX_OPAREN,
            .line = line,
            .idx = idx,
        };
        return ret;
    } else if (strcmp(ds->items, ")") == 0) {
        LToken ret = {
            .kind = TOKEN_SYNTAX,
            .data = SYNTAX_CPAREN,
            .line = line,
            .idx = idx,
        };
        return ret;
    } else {
        TokenData data;
        data.as_ident = strdup(ds->items);
        LToken ret = {
            .kind = TOKEN_IDENT,
            .data = data,
            .line = line,
            .idx = idx,
        };
        return ret;
    }
}

void dump_ltoken(LToken *tk) {
    switch (tk->kind) {
        case TOKEN_INT:
            printf("%s: %d\n", display_kind(tk->kind), tk->data.as_int);
            break;
        case TOKEN_IDENT:
            printf("%s: %s\n", display_kind(tk->kind), tk->data.as_ident);
            break;
        case TOKEN_STRING:
            printf("%s: %s\n", display_kind(tk->kind), tk->data.as_string);
            break;
        case TOKEN_SYNTAX:
            printf("%s: %s\n",
                display_kind(tk->kind), display_syntax(tk->data.as_syntax));
            break;
        case TOKEN_MALFORMED:
            printf("%s\n", display_kind(tk->kind));
            break;
    }
}

void free_ltoken(LToken *tk) {
    switch (tk->kind) {
        case TOKEN_IDENT:
            free(tk->data.as_ident);
            break;
        case TOKEN_STRING:
            free(tk->data.as_string);
            break;
        case TOKEN_SYNTAX:
        case TOKEN_MALFORMED:
            break;
    }
}

typedef enum {
    AAst,
    ALToken,
    AEnd,
    ANull,
} AKind;

typedef struct AST AST;

typedef union {
    AST *ast;
    LToken *ltoken;
    int *end;
} AData;

typedef struct {
    AKind kind;
    AData as;
} AObj;

const AObj AObjNull = {
    .kind = ANull,
    .as = NULL,
};

struct AST {
    AObj *items;
    size_t count;
    size_t capacity;
};

void dump_ast_rec(AST* ast, size_t indent_level) {
    if (ast->items == NULL) return;
    vec_foreach(AObj, o, ast) {
        switch (o->kind) {
            case AAst:
                dump_ast_rec(o->as.ast, indent_level + 1);
                break;
            case ALToken:
                for (int i = 0; i < indent_level; i++) printf("| ");
                dump_ltoken(o->as.ltoken);
                break;
            case AEnd:
                for (int i = 1; i < indent_level; i++) printf("| ");
                printf("\n");
        }
    }
}

#define dump_ast(ast) dump_ast_rec(ast, 0);

void free_ast(AST* ast) {
    if (ast->items == NULL) return;
    vec_foreach(AObj, o, ast) {
        switch (o->kind) {
            case AAst:
                free_ast(o->as.ast);
                break;
            case ALToken:
                free_ltoken(o->as.ltoken);
                free(o->as.ltoken);
                break;
            case AEnd:
                free(o->as.end);
                break;
        }
    }
    free(ast);
}

void ast_append_ltoken(AST* ast, LToken *lt) {
    // dump_ltoken(lt);
    AST *prev = ast;
    for (;;) {
        if (prev->count == 0) break;
        AObj tast = prev->items[prev->count - 1];
        if (tast.kind == AAst) {
            if (tast.as.ast->count == 0) {
                prev = tast.as.ast;
                break;
            } else if (tast.as.ast->items[tast.as.ast->count - 1].kind != AEnd) {
                prev = tast.as.ast;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    // printf("prev->items: %u\n", prev->items);
    vec_reserve(prev, prev->count + 1);
    switch (lt->kind) {
        case TOKEN_INT:
            LToken *new_lt_int = (LToken *)malloc(sizeof(LToken));
            new_lt_int->kind = lt->kind;
            new_lt_int->line = lt->line;
            new_lt_int->idx = lt->idx;
            new_lt_int->data.as_int = lt->data.as_int;
            prev->items[prev->count].as.ltoken = new_lt_int;
            prev->items[prev->count].kind = ALToken;
            prev->count++;
            break;
        case TOKEN_IDENT:
            LToken *new_lt_ident = (LToken *)malloc(sizeof(LToken));
            new_lt_ident->kind = lt->kind;
            new_lt_ident->line = lt->line;
            new_lt_ident->idx = lt->idx;
            new_lt_ident->data.as_ident =
                strdup(lt->data.as_ident);
            prev->items[prev->count].as.ltoken = new_lt_ident;
            prev->items[prev->count].kind = ALToken;
            prev->count++;
            break;
        case TOKEN_STRING:
            LToken *new_lt_str = (LToken *)malloc(sizeof(LToken));
            new_lt_str->kind = lt->kind;
            new_lt_str->line = lt->line;
            new_lt_str->idx = lt->idx;
            new_lt_str->data.as_string =
                strdup(lt->data.as_string);
            prev->items[prev->count].kind = ALToken;
            prev->items[prev->count].as.ltoken = new_lt_str;
            prev->count++;
            break;
        case TOKEN_SYNTAX:
            switch (lt->data.as_syntax) {
                case SYNTAX_OPAREN:
                    AST *new_ast = (AST *)malloc(sizeof(AST));
                    prev->items[prev->count].as.ast = new_ast;
                    prev->items[prev->count].kind = AAst;
                    prev->count++;
                    vec_ptr_init(new_ast);
                    // printf("new_ast: %u\n", new_ast);
                    break;
                case SYNTAX_CPAREN:
                    prev->items[prev->count].kind = AEnd;
                    prev->count++;
                    break;
                default:
                    LToken *new_lt_syntax = (LToken *)malloc(sizeof(LToken));
                    new_lt_syntax->kind = lt->kind;
                    new_lt_syntax->line = lt->line;
                    new_lt_syntax->idx = lt->idx;
                    new_lt_syntax->data.as_syntax = lt->data.as_syntax;
                    prev->items[prev->count].as.ltoken = new_lt_syntax;
                    prev->items[prev->count].kind = ALToken;
                    prev->count++;
                    break;
            }
            break;
        case TOKEN_MALFORMED:
            break;
    }
}

#define ast_append_from_lexer(ast, lexer) do { \
    LToken __l = to_ltoken(&(lexer)->ds, (lexer)->pos_line, (lexer)->pos_idx); \
    ast_append_ltoken((ast), &__l); \
    free_ltoken(&__l); \
} while (0)

// For now this will only support addition and subtraction.
// No fancy partial application or anything.
AObj traverse_ast_rec(AST *ast) {
    if (ast->count < 3) return AObjNull;
    AObj fn = ast->items[0];
    if (
        fn.kind == ALToken
        && fn.as.ltoken->kind == TOKEN_IDENT
        && (
            strcmp(fn.as.ltoken->data.as_ident, "+") == 0
            || strcmp(fn.as.ltoken->data.as_ident, "-") == 0
        )
    ) {
        if (strcmp(fn.as.ltoken->data.as_ident, "+") == 0) {
            size_t total = 0;
            int ctr = 0;
            vec_foreach(AObj, ob, ast) {
                ctr++;
                if (ctr == 1) goto skip_iter;
                if (
                    ob->kind == ALToken
                    && ob->as.ltoken->kind == TOKEN_INT
                ) {
                    total += ob->as.ltoken->data.as_int;
                } else if (ob->kind == AAst) {
                    AObj evaled = traverse_ast_rec(ob->as.ast);
                    if (
                        evaled.kind == ALToken
                        && evaled.as.ltoken->kind == TOKEN_INT
                    ) {
                        total += evaled.as.ltoken->data.as_int;
                    }
                } else if (ob->kind != AEnd) {
                    return AObjNull;
                }
                skip_iter:
            }
            LToken *ret_ltoken = (LToken *)malloc(sizeof(LToken));
            ret_ltoken->line = 0;
            ret_ltoken->idx = 0;
            ret_ltoken->data.as_int = total;
            ret_ltoken->kind = TOKEN_INT;
            AObj ret = {
                .kind = ALToken,
                .as.ltoken = ret_ltoken,
            };
            return ret;
        } else if (strcmp(fn.as.ltoken->data.as_ident, "-") == 0) {
            if (ast->count != 4) {
                return AObjNull;
            }
            int a;
            AObj araw = ast->items[1];
            if (
                araw.kind == ALToken
                && araw.as.ltoken->kind == TOKEN_INT
            ) {
                a = araw.as.ltoken->data.as_int;
            } else if (araw.kind == AAst) {
                AObj evaled = traverse_ast_rec(araw.as.ast);
                if (
                    evaled.kind == ALToken
                    && evaled.as.ltoken->kind == TOKEN_INT
                ) {
                    a = araw.as.ltoken->data.as_int;
                } else {
                    return AObjNull;
                }
            } else {
                return AObjNull;
            }
            int b;
            AObj braw = ast->items[2];
            if (
                braw.kind == ALToken
                && braw.as.ltoken->kind == TOKEN_INT
            ) {
                b = braw.as.ltoken->data.as_int;
            } else if (braw.kind == AAst) {
                AObj evaled = traverse_ast_rec(braw.as.ast);
                if (
                    evaled.kind == ALToken
                    && evaled.as.ltoken->kind == TOKEN_INT
                ) {
                    b = braw.as.ltoken->data.as_int;
                } else {
                    return AObjNull;
                }
            } else {
                return AObjNull;
            }
            LToken *ret_ltoken = (LToken *)malloc(sizeof(LToken));
            ret_ltoken->line = 0;
            ret_ltoken->idx = 0;
            ret_ltoken->data.as_int = a - b;
            ret_ltoken->kind = TOKEN_INT;
            AObj ret = {
                .kind = ALToken,
                .as.ltoken = ret_ltoken,
            };
            return ret;
        }
    }
}

int main() {
    FILE *f;
    f = fopen("arithmetic.txt", "r");
    if (f == NULL) {
        throw("unable to open file\n");
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);
    DString contents = {0};
    vec_reserve(&contents, fsize + 1);
    fread(contents.items, sizeof(char), fsize, f);
    contents.items[fsize] = '\0';
    contents.count = fsize;
    fclose(f);
    printf("fsize: %d\n", fsize);
    Lispexer l = lispexer_from_dstring(contents);
    vec_ptr_init(&l.ds);
    AST ast = {0};
    for (;;) {
        lispexer_next(&l);
        if (l.ds.count != 0) {
            ast_append_from_lexer(&ast, &l);
        } else {
            break;
        }
    }
    free_lispexer(&l);

    dump_ast(&ast);

    AObj result = traverse_ast_rec(ast.items[0].as.ast);
    if (result.kind == ALToken) {
        dump_ltoken(result.as.ltoken);
    } else {
        printf("Expected ALToken, got %d\n", result.kind);
    }

    return 0;
}
