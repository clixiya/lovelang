#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "love.h"

static const char *g_source = NULL;
static size_t g_pos = 0;
static int g_line = 1;

static int g_has_peek = 0;
static Token g_peek_token;

static char current_char(void) {
    return g_source[g_pos];
}

static char next_char(void) {
    return g_source[g_pos + 1];
}

static void advance_char(void) {
    if (g_source[g_pos] == '\n') {
        g_line++;
    }
    g_pos++;
}

static void skip_ws_and_comments(void) {
    for (;;) {
        while (isspace((unsigned char)current_char())) {
            advance_char();
        }

        if (current_char() == '/' && next_char() == '/') {
            while (current_char() != '\n' && current_char() != '\0') {
                advance_char();
            }
            continue;
        }

        if (current_char() == '#') {
            while (current_char() != '\n' && current_char() != '\0') {
                advance_char();
            }
            continue;
        }

        if (current_char() == '~' && next_char() == '~') {
            while (current_char() != '\n' && current_char() != '\0') {
                advance_char();
            }
            continue;
        }

        if (current_char() == '/' && next_char() == '*') {
            advance_char();
            advance_char();
            while (current_char() != '\0') {
                if (current_char() == '*' && next_char() == '/') {
                    advance_char();
                    advance_char();
                    break;
                }
                advance_char();
            }
            continue;
        }

        break;
    }
}

static Token make_token(TokenType type, const char *lexeme) {
    Token token;
    token.type = type;
    token.line = g_line;
    strncpy(token.lexeme, lexeme, sizeof(token.lexeme) - 1);
    token.lexeme[sizeof(token.lexeme) - 1] = '\0';
    return token;
}

static Token read_string(char delimiter) {
    char out[256];
    int idx = 0;

    advance_char();
    while (current_char() != '\0' && current_char() != delimiter) {
        if (current_char() == '\\') {
            advance_char();
            switch (current_char()) {
                case 'n': out[idx++] = '\n'; break;
                case 't': out[idx++] = '\t'; break;
                case 'r': out[idx++] = '\r'; break;
                case '\\': out[idx++] = '\\'; break;
                case '"': out[idx++] = '"'; break;
                case '\'': out[idx++] = '\''; break;
                default: out[idx++] = current_char(); break;
            }
        } else {
            out[idx++] = current_char();
        }

        if (idx >= (int)sizeof(out) - 1) {
            break;
        }

        advance_char();
    }

    out[idx] = '\0';

    if (current_char() == delimiter) {
        advance_char();
    }

    return make_token(TOKEN_STRING, out);
}

static Token read_number(void) {
    char out[256];
    int idx = 0;

    while (isdigit((unsigned char)current_char())) {
        out[idx++] = current_char();
        if (idx >= (int)sizeof(out) - 1) {
            break;
        }
        advance_char();
    }

    out[idx] = '\0';
    return make_token(TOKEN_INT, out);
}

static Token read_identifier(void) {
    char out[256];
    int idx = 0;

    while (isalnum((unsigned char)current_char()) || current_char() == '_') {
        out[idx++] = current_char();
        if (idx >= (int)sizeof(out) - 1) {
            break;
        }
        advance_char();
    }

    out[idx] = '\0';

    struct {
        const char *kw;
        TokenType type;
    } keywords[] = {
        {"yaad", TOKEN_YAAD},
        {"yaad_karo", TOKEN_YAAD_KARO},
        {"vada", TOKEN_VADA},
        {"bolo", TOKEN_BOLO},
        {"typing", TOKEN_TYPING},
        {"agar", TOKEN_AGAR},
        {"bewafa", TOKEN_BEWAFA},
        {"ye_karo", TOKEN_YE_KARO},
        {"vo_karo", TOKEN_VO_KARO},
        {"jabtak", TOKEN_JABTAK},
        {"intezaar", TOKEN_INTEZAAR},
        {"dhadkan", TOKEN_DHADKAN},
        {"ehsaas", TOKEN_EHSAAS},
        {"festival", TOKEN_FESTIVAL},
        {"sach", TOKEN_TRUE},
        {"jhooth", TOKEN_FALSE},
        {"aur", TOKEN_AND},
        {"ya", TOKEN_OR},
        {"nahi", TOKEN_NOT},
        {NULL, TOKEN_UNKNOWN}
    };

    for (int i = 0; keywords[i].kw != NULL; i++) {
        if (strcmp(out, keywords[i].kw) == 0) {
            return make_token(keywords[i].type, out);
        }
    }

    return make_token(TOKEN_IDENT, out);
}

static Token next_raw_token(void) {
    skip_ws_and_comments();

    if (current_char() == '\0') {
        return make_token(TOKEN_EOF, "");
    }

    if (current_char() == '"' || current_char() == '\'') {
        return read_string(current_char());
    }

    if (isdigit((unsigned char)current_char())) {
        return read_number();
    }

    if (isalpha((unsigned char)current_char()) || current_char() == '_') {
        return read_identifier();
    }

    {
        char one[2] = { current_char(), '\0' };
        char c = current_char();
        advance_char();

        switch (c) {
            case '(': return make_token(TOKEN_LPAREN, "(");
            case ')': return make_token(TOKEN_RPAREN, ")");
            case '{': return make_token(TOKEN_LBRACE, "{");
            case '}': return make_token(TOKEN_RBRACE, "}");
            case ';': return make_token(TOKEN_SEMI, ";");
            case ',': return make_token(TOKEN_COMMA, ",");
            case '+': return make_token(TOKEN_PLUS, "+");
            case '-': return make_token(TOKEN_MINUS, "-");
            case '*': return make_token(TOKEN_STAR, "*");
            case '/': return make_token(TOKEN_SLASH, "/");
            case '%': return make_token(TOKEN_PERCENT, "%");
            case '=':
                if (current_char() == '=') {
                    advance_char();
                    return make_token(TOKEN_EQ, "==");
                }
                return make_token(TOKEN_ASSIGN, "=");
            case '!':
                if (current_char() == '=') {
                    advance_char();
                    return make_token(TOKEN_NEQ, "!=");
                }
                return make_token(TOKEN_NOT, "!");
            case '<':
                if (current_char() == '=') {
                    advance_char();
                    return make_token(TOKEN_LTE, "<=");
                }
                return make_token(TOKEN_LT, "<");
            case '>':
                if (current_char() == '=') {
                    advance_char();
                    return make_token(TOKEN_GTE, ">=");
                }
                return make_token(TOKEN_GT, ">");
            case '&':
                if (current_char() == '&') {
                    advance_char();
                    return make_token(TOKEN_AND, "&&");
                }
                break;
            case '|':
                if (current_char() == '|') {
                    advance_char();
                    return make_token(TOKEN_OR, "||");
                }
                break;
            default:
                break;
        }

        return make_token(TOKEN_UNKNOWN, one);
    }
}

void lexer_init(const char *source) {
    g_source = source;
    g_pos = 0;
    g_line = 1;
    g_has_peek = 0;
}

Token lexer_next(void) {
    if (g_has_peek) {
        g_has_peek = 0;
        return g_peek_token;
    }
    return next_raw_token();
}

Token lexer_peek(void) {
    if (!g_has_peek) {
        g_peek_token = next_raw_token();
        g_has_peek = 1;
    }
    return g_peek_token;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_UNKNOWN: return "TOKEN_UNKNOWN";
        case TOKEN_IDENT: return "TOKEN_IDENT";
        case TOKEN_INT: return "TOKEN_INT";
        case TOKEN_STRING: return "TOKEN_STRING";
        case TOKEN_TRUE: return "TOKEN_TRUE";
        case TOKEN_FALSE: return "TOKEN_FALSE";
        case TOKEN_YAAD: return "TOKEN_YAAD";
        case TOKEN_YAAD_KARO: return "TOKEN_YAAD_KARO";
        case TOKEN_VADA: return "TOKEN_VADA";
        case TOKEN_BOLO: return "TOKEN_BOLO";
        case TOKEN_TYPING: return "TOKEN_TYPING";
        case TOKEN_AGAR: return "TOKEN_AGAR";
        case TOKEN_BEWAFA: return "TOKEN_BEWAFA";
        case TOKEN_YE_KARO: return "TOKEN_YE_KARO";
        case TOKEN_VO_KARO: return "TOKEN_VO_KARO";
        case TOKEN_JABTAK: return "TOKEN_JABTAK";
        case TOKEN_INTEZAAR: return "TOKEN_INTEZAAR";
        case TOKEN_DHADKAN: return "TOKEN_DHADKAN";
        case TOKEN_EHSAAS: return "TOKEN_EHSAAS";
        case TOKEN_FESTIVAL: return "TOKEN_FESTIVAL";
        case TOKEN_AND: return "TOKEN_AND";
        case TOKEN_OR: return "TOKEN_OR";
        case TOKEN_NOT: return "TOKEN_NOT";
        case TOKEN_ASSIGN: return "TOKEN_ASSIGN";
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_STAR: return "TOKEN_STAR";
        case TOKEN_SLASH: return "TOKEN_SLASH";
        case TOKEN_PERCENT: return "TOKEN_PERCENT";
        case TOKEN_EQ: return "TOKEN_EQ";
        case TOKEN_NEQ: return "TOKEN_NEQ";
        case TOKEN_LT: return "TOKEN_LT";
        case TOKEN_LTE: return "TOKEN_LTE";
        case TOKEN_GT: return "TOKEN_GT";
        case TOKEN_GTE: return "TOKEN_GTE";
        case TOKEN_LPAREN: return "TOKEN_LPAREN";
        case TOKEN_RPAREN: return "TOKEN_RPAREN";
        case TOKEN_LBRACE: return "TOKEN_LBRACE";
        case TOKEN_RBRACE: return "TOKEN_RBRACE";
        case TOKEN_SEMI: return "TOKEN_SEMI";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        default: return "TOKEN_INVALID";
    }
}
