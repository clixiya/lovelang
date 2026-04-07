#ifndef LOVELANG_H
#define LOVELANG_H

#include <stddef.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_UNKNOWN,

    TOKEN_IDENT,
    TOKEN_INT,
    TOKEN_STRING,
    TOKEN_TRUE,
    TOKEN_FALSE,

    TOKEN_YAAD,
    TOKEN_YAAD_KARO,
    TOKEN_VADA,
    TOKEN_BOLO,
    TOKEN_TYPING,
    TOKEN_AGAR,
    TOKEN_BEWAFA,
    TOKEN_YE_KARO,
    TOKEN_VO_KARO,
    TOKEN_JABTAK,
    TOKEN_INTEZAAR,
    TOKEN_DHADKAN,
    TOKEN_EHSAAS,
    TOKEN_FESTIVAL,

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,

    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,

    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_LTE,
    TOKEN_GT,
    TOKEN_GTE,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMI,
    TOKEN_COMMA
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[256];
    int line;
} Token;

void lexer_init(const char *source);
Token lexer_next(void);
Token lexer_peek(void);
const char *token_type_name(TokenType type);

typedef enum {
    NODE_BLOCK,
    NODE_VAR_DECL,
    NODE_CONST_DECL,
    NODE_ASSIGN,
    NODE_PRINT,
    NODE_TYPING,
    NODE_IF,
    NODE_WHILE,
    NODE_FUNC_DECL,
    NODE_RETURN,
    NODE_CALL,
    NODE_FESTIVAL,

    NODE_INT,
    NODE_STRING,
    NODE_BOOL,
    NODE_IDENT,
    NODE_BINARY,
    NODE_UNARY
} NodeType;

typedef struct Node {
    NodeType type;
    int line;

    char *text;
    long int_value;
    int bool_value;

    struct Node *left;
    struct Node *right;

    struct Node *cond;
    struct Node *then_branch;
    struct Node *else_branch;

    struct Node *body;
    struct Node *params;
    struct Node *args;
    struct Node *next;
} Node;

Node *parse_program(void);
void free_node(Node *node);

typedef struct {
    char mode[16];
    int debug_love;
} RuntimeConfig;

int runtime_execute(Node *program, const RuntimeConfig *config);

#endif
