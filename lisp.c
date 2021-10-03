/**
 * To compile on macOS:
 *
 * $ cc -std=c89 -Wall lisp.c -o lisp
 */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef enum {
  TOKEN_TYPE_LIST_BEGIN,
  TOKEN_TYPE_LIST_END,
  TOKEN_TYPE_QUOTE,
  TOKEN_TYPE_DOT,
  TOKEN_TYPE_ATOM
} token_type_t;


typedef struct token_t {
  token_type_t type;
  char*        value;
} token_t;


typedef struct {
  size_t   count;
  token_t* tokens;
} tokens_t;




typedef enum {
  EXPR_TYPE_PAIR,
  EXPR_TYPE_SYMBOL,
  EXPR_TYPE_STRING,
  EXPR_TYPE_FLOATING,
  EXPR_TYPE_FIXED
} expr_type_t;


typedef struct expr_t {
  expr_type_t type;
  union {
    struct {
      struct expr_t* car;
      struct expr_t* cdr;
    } pair;
    char*  symbol;
    char*  string;
    double floating;
    int    fixed;
  } value;
} expr_t;


typedef int env_t;


char* skip_whitespace(char* s) {
  while (*s == ' ' || *s == '\t' || *s == '\n') {
    s++;
  }

  return s;
}


void error(char* message) {
  fprintf(stderr, "error: %s\n", message);
}


void fatal(char* message) {
  error(message);
  exit(1);
}


void add_token(tokens_t* tokens, token_type_t type, char* value) {
  tokens->tokens                      = realloc(tokens->tokens, sizeof(token_t) * (tokens->count + 1));
  tokens->tokens[tokens->count].type  = type;
  tokens->tokens[tokens->count].value = value;
  tokens->count++;
}


void free_tokens(tokens_t* tokens) {
  free(tokens->tokens);
  tokens->tokens = NULL;
  tokens->count  = 0;
}


void tokenize(char* s, tokens_t* tokens) {
  for (;;) {
    s = skip_whitespace(s);
    if (*s == '\0') {
      return;
    }

    switch (*s) {
      case '(':
        add_token(tokens, TOKEN_TYPE_LIST_BEGIN, NULL);
        s++;
        break;

      case ')':
        add_token(tokens, TOKEN_TYPE_LIST_END, NULL);
        s++;
        break;

      case '\'':
        add_token(tokens, TOKEN_TYPE_QUOTE, NULL);
        s++;
        break;

      case '.':
        add_token(tokens, TOKEN_TYPE_DOT, NULL);
        s++;
        break;
        
      case '"':
      {
        char* start      = s;
        int   is_escaped = 0;

        s++;
        while (*s != '\0' && (*s != '"' || is_escaped)) {
          if (is_escaped) {
            is_escaped = 0;
          } else if (*s == '\\') {
            is_escaped = 1;
          }
          s++;
        }

        if (*s == '\0') {
          error("unterminated string");
          free_tokens(tokens);
          return;
        }

        s++;

        size_t n     = s - start;
        char*  value = strndup(start, n);
        add_token(tokens, TOKEN_TYPE_ATOM, value);
        break;
      }

      default:
      {
        char* start = s;
        while (*s != '\0' && *s != '(' && *s != ')' && *s != ' ' && *s != '\t' && *s != '\n') {
          s++;
        }

        size_t n     = s - start;
        char*  value = strndup(start, n);
        add_token(tokens, TOKEN_TYPE_ATOM, value);
        break;
      }
    }
  }
}


void print_tokens(tokens_t* tokens) {
  size_t i;

  for (i = 0; i < tokens->count; i++) {
    token_t* token = &tokens->tokens[i];

    switch (token->type) {
      case TOKEN_TYPE_LIST_BEGIN:
        printf("[(] ");
        break;

      case TOKEN_TYPE_LIST_END:
        printf("[)] ");
        break;

      case TOKEN_TYPE_QUOTE:
        printf("['] ");
        break;

      case TOKEN_TYPE_DOT:
        printf("[.] ");
        break;

      case TOKEN_TYPE_ATOM:
        printf("[%s] ", token->value);
        break;
    }
  }

  printf("\n");
}


void free_expr(expr_t* expr) {
  if (expr == NULL) {
    return;
  }
  
  switch (expr->type) {
    case EXPR_TYPE_PAIR:
      free_expr(expr->value.pair.car);
      free_expr(expr->value.pair.cdr);
      break;

    case EXPR_TYPE_SYMBOL:
      free(expr->value.symbol);
      break;

    case EXPR_TYPE_STRING:
      free(expr->value.string);
      break;

    case EXPR_TYPE_FLOATING:
      break;

    case EXPR_TYPE_FIXED:
      break;
  }

  free(expr);
}


void upcase(char* s) {
  size_t i;

  for (i = 0; i < strlen(s); i++) {
    s[i] = toupper(s[i]);
  }
}


expr_t* make_atom(char* value) {
  expr_t* expr = malloc(sizeof(expr_t));

  if (*value == '"') {
    expr->type         = EXPR_TYPE_STRING;
    expr->value.string = strndup(value + 1, strlen(value) - 2);
  } else if (*value >= '0' && *value <= '9') {
    upcase(value);
    if (strchr(value, '.') || strchr(value, 'E')) {
      expr->type           = EXPR_TYPE_FLOATING;
      expr->value.floating = strtod(value, NULL);
    } else {
      expr->type        = EXPR_TYPE_FIXED;
      expr->value.fixed = strtol(value, NULL, 10);
    }
  } else {
    expr->type         = EXPR_TYPE_SYMBOL;
    expr->value.symbol = strdup(value);
    upcase(expr->value.symbol);
  }

  return expr;
}


expr_t* quote(expr_t* expr) {
  expr_t* quote       = malloc(sizeof(expr_t));
  quote->type         = EXPR_TYPE_SYMBOL;
  quote->value.symbol = "QUOTE";
  
  expr_t* pair = malloc(sizeof(expr_t));
  pair->type           = EXPR_TYPE_PAIR;
  pair->value.pair.car = quote;
  pair->value.pair.cdr = expr;

  return pair;
}


expr_t* parse(tokens_t* tokens, size_t* index) {
  if (*index == tokens->count) {
    perror("incomplete expression");
    return NULL;
  }

  switch (tokens->tokens[*index].type) {
    case TOKEN_TYPE_ATOM:
      return make_atom(tokens->tokens[*index].value);

    case TOKEN_TYPE_QUOTE:
    {
      (*index)++;
      expr_t* expr = parse(tokens, index);
      return quote(expr);
    }

    case TOKEN_TYPE_DOT:
      error("unexpected dot");
      return NULL;

    case TOKEN_TYPE_LIST_BEGIN:
    {
      expr_t* prev_expr = NULL;
      expr_t* expr      = NULL;

      while (++(*index) < tokens->count && tokens->tokens[*index].type != TOKEN_TYPE_DOT && tokens->tokens[*index].type != TOKEN_TYPE_LIST_END) {
        expr_t* this_expr         = malloc(sizeof(expr_t));
        this_expr->type           = EXPR_TYPE_PAIR;
        this_expr->value.pair.car = parse(tokens, index);
        this_expr->value.pair.cdr = NULL;

        if (prev_expr) {
          prev_expr->value.pair.cdr = this_expr;
        } else {
          expr = this_expr;
        }
        prev_expr = this_expr;
      }

      if (*index == tokens->count) {
        error("expected close bracket");
        free_expr(expr);
        break;
      }

      if (tokens->tokens[*index].type == TOKEN_TYPE_DOT) {
        if (!prev_expr) {
          error("unexpected dot");
          return NULL;
        }

        (*index)++;
        prev_expr->value.pair.cdr = parse(tokens, index);
      }

      return expr;
    }
    
    case TOKEN_TYPE_LIST_END:
      error("unexpected close bracket");
      break;
  }

  return NULL;
}


void print_expr(expr_t* expr) {
  if (expr == NULL) {
    printf("NIL");
    return;
  }

  switch (expr->type) {
    case EXPR_TYPE_FLOATING:
      printf("%g", expr->value.floating);
      break;

    case EXPR_TYPE_FIXED:
      printf("%d", expr->value.fixed);
      break;

    case EXPR_TYPE_STRING:
      printf("\"%s\"", expr->value.string);
      break;

    case EXPR_TYPE_SYMBOL:
      printf("%s", expr->value.symbol);
      break;

    case EXPR_TYPE_PAIR:
      printf("(");
      print_expr(expr->value.pair.car);
      printf(" . ");
      print_expr(expr->value.pair.cdr);
      printf(")");
      break;
  }
}


expr_t* read(FILE* in) {
  char buffer[128 + 1];

  printf("> ");
  fflush(stdout);

  (void) fgets(buffer, sizeof(buffer), in);
  if (feof(in)) {
    exit(0);
  }

  tokens_t tokens = {0, NULL};
  tokenize(buffer, &tokens);
  print_tokens(&tokens);

  size_t  start = 0;
  expr_t* expr  = parse(&tokens, &start);
  free_tokens(&tokens);
  print_expr(expr);
  printf("\n");
  return expr;
}


expr_t* eval(expr_t* expr, env_t* env) {
  return NULL;
}


void print(expr_t* expr) {
}


int main(int argc, char* argv[]) {
  env_t*  env = NULL;
  expr_t* expr;

  for (;;) {
    expr = read(stdin);
    expr = eval(expr, env);
    print(expr);
  }

  return 0;
}
