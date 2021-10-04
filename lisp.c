/**
 * To compile on macOS:
 *
 * $ cc -std=c89 -Wall lisp.c -o lisp
 */


#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
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


typedef struct env_t {
  char*         symbol;
  expr_t*       expr;
  struct env_t* next;
} env_t;


jmp_buf restart;


expr_t* ATOM;
expr_t* CAR;
expr_t* CDR;
expr_t* COND;
expr_t* CONS;
expr_t* EQ;
expr_t* LABEL;
expr_t* LAMBDA;
expr_t* NIL;
expr_t* QUOTE;
expr_t* T;


expr_t* eval(expr_t*, env_t*);
expr_t* apply(expr_t*, expr_t*, env_t*);


char* skip_whitespace(char* s) {
  while (*s == ' ' || *s == '\t' || *s == '\n') {
    s++;
  }

  return s;
}


void error(char* fmt, ...) {
  va_list arg;

  fprintf(stderr, "error: ");

  va_start(arg, fmt);
  vfprintf(stderr, fmt, arg);
  va_end(arg);

  fprintf(stderr, "\n");

  longjmp(restart, 1);
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
          free_tokens(tokens);
          error("unterminated string");
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


expr_t* make_symbol(char* symbol) {
  expr_t* expr = malloc(sizeof(expr_t));

  expr->type         = EXPR_TYPE_SYMBOL;
  expr->value.symbol = strdup(symbol);

  upcase(expr->value.symbol);

  return expr;
}


expr_t* make_pair(expr_t* car, expr_t* cdr) {
  expr_t* expr = malloc(sizeof(expr_t));

  expr->type           = EXPR_TYPE_PAIR;
  expr->value.pair.car = car;
  expr->value.pair.cdr = cdr;

  return expr;
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
    return make_symbol(value);
  }

  return expr;
}


expr_t* quote(expr_t* expr) {
  expr_t* quote = make_symbol("QUOTE");
  expr_t* node  = make_pair(expr, NULL);

  return make_pair(quote, node);
}


expr_t* parse(tokens_t* tokens, size_t* index) {
  if (*index == tokens->count) {
    error("incomplete expression");
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
      break;

    case TOKEN_TYPE_LIST_BEGIN:
    {
      expr_t* prev_expr = NULL;
      expr_t* expr      = NULL;

      while (++(*index) < tokens->count && tokens->tokens[*index].type != TOKEN_TYPE_DOT && tokens->tokens[*index].type != TOKEN_TYPE_LIST_END) {
        expr_t* sub_expr  = parse(tokens, index);
        expr_t* this_expr = make_pair(sub_expr, NULL);

        if (prev_expr) {
          prev_expr->value.pair.cdr = this_expr;
        } else {
          expr = this_expr;
        }
        prev_expr = this_expr;
      }

      if (*index == tokens->count) {
        free_expr(expr);
        error("expected close bracket");
      }

      if (tokens->tokens[*index].type == TOKEN_TYPE_DOT) {
        if (!prev_expr) {
          error("unexpected dot");
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
  print_expr(expr);
  printf("\n");
  free_tokens(&tokens);

  return expr;
}


int lookup(char* symbol, env_t* env, expr_t** expr) {
  while (env) {
    if (strcmp(env->symbol, symbol) == 0) {
      *expr = env->expr;
      return 1;
    }

    env = env->next;
  }

  return 0;
}


expr_t* car(expr_t* expr) {
  return expr->value.pair.car;
}


expr_t* cdr(expr_t* expr) {
  return expr->value.pair.cdr;
}


expr_t* caar(expr_t* expr) {
  return car(car(expr));
}


expr_t* cadr(expr_t* expr) {
  return car(cdr(expr));
}


expr_t* cadar(expr_t* expr) {
  return car(cdr(car(expr)));
}


expr_t* cdar(expr_t* expr) {
  return cdr(car(expr));
}


expr_t* evcon(expr_t* c, env_t* env) {
  while (c) {
    expr_t* condition = eval(caar(c), env);
    if (condition) {
      return eval(cadar(c), env);
    }

    c = cdr(c);
  }

  return NULL;
}


expr_t* cons(expr_t* car, expr_t* cdr) {
  return make_pair(car, cdr);
}


expr_t* evlis(expr_t* m, env_t* env) {
  if (m == NULL) {
    return NULL;
  }

  return cons(eval(car(m), env), evlis(cdr(m), env));
}


int atom(expr_t* expr) {
  return expr == NULL || (expr->type != EXPR_TYPE_PAIR);
}


int eq(expr_t* a, expr_t* b) {
  if (a == NULL && b == NULL) {
    return 1;
  }

  if (a == NULL || b == NULL) {
    return 0;
  }

  if (a->type != b->type) {
    return 0;
  }

  if (a->type == EXPR_TYPE_SYMBOL) {
    return (strcmp(a->value.symbol, b->value.symbol) == 0);
  }

  if (a->type == EXPR_TYPE_FIXED) {
    return (a->value.fixed ==  b->value.fixed);
  }

  return 0;
}


expr_t* apply(expr_t* fn, expr_t* x, env_t* env) {
  switch (fn->type) {
    case EXPR_TYPE_FIXED:
      error("cannot apply %d", fn->value.fixed);

    case EXPR_TYPE_FLOATING:
      error("cannot apply %g", fn->value.floating);

    case EXPR_TYPE_STRING:
      error("cannot apply \"%s\"", fn->value.string);

    case EXPR_TYPE_SYMBOL:
      if (eq(fn, CAR )) return caar(x);
      if (eq(fn, CDR )) return cdar(x);
      if (eq(fn, CONS)) return cons(car(x), cadr(x));
      if (eq(fn, ATOM)) return atom(car(x))        ? T : NIL;
      if (eq(fn, EQ  )) return eq(car(x), cadr(x)) ? T : NIL;

      return apply(eval(fn, env), x, env);

    case EXPR_TYPE_PAIR:
      break;
  }

  return NULL;
}


expr_t* eval(expr_t* expr, env_t* env) {
  if (expr == NULL) {
    return NULL;
  }

  switch (expr->type) {
    case EXPR_TYPE_FIXED:
    case EXPR_TYPE_FLOATING:
    case EXPR_TYPE_STRING:
      return expr;

    case EXPR_TYPE_SYMBOL:
    {
      expr_t* value;
      if (!lookup(expr->value.symbol, env, &value)) {
        error("%s undefined", expr->value.symbol);
      }
      return value;
    }

    case EXPR_TYPE_PAIR:
      if (eq(car(expr), QUOTE)) return cadr(expr);
      if (eq(car(expr), COND )) return evcon(cdr(expr), env);

      return apply(car(expr), evlis(cdr(expr), env), env);
  }

  return NULL;
}


void print(expr_t* expr) {
  if (expr == NULL) {
    printf("NIL");
    return;
  }

  switch (expr->type) {
    case EXPR_TYPE_SYMBOL:
      printf("%s", expr->value.symbol);
      break;

    case EXPR_TYPE_STRING:
      printf("%s", expr->value.string);
      break;

    case EXPR_TYPE_FIXED:
      printf("%d", expr->value.fixed);
      break;

    case EXPR_TYPE_FLOATING:
      printf("%f", expr->value.floating);
      break;

    case EXPR_TYPE_PAIR:
      printf("(");
      print(expr->value.pair.car);
      printf(" ");
      print(expr->value.pair.cdr);
      printf(")");
      break;
  }
}


env_t* associate(char* symbol, expr_t* expr, env_t* env) {
  env_t* node = malloc(sizeof(env_t));

  node->symbol = symbol;
  node->expr   = expr;
  node->next   = env;

  return node;
}


env_t* make_initial_env(void) {
  env_t* env = NULL;

  env = associate("NIL", NIL, env);
  env = associate("T",   T,   env);
  
  return env;
}


void init(void) {
  ATOM   = make_symbol("ATOM");
  CAR    = make_symbol("CAR");
  CDR    = make_symbol("CDR");
  COND   = make_symbol("COND");
  CONS   = make_symbol("CONS");
  EQ     = make_symbol("EQ");
  LABEL  = make_symbol("LABEL");
  LAMBDA = make_symbol("LAMBDA");
  NIL    = NULL;
  QUOTE  = make_symbol("QUOTE");
  T      = make_symbol("T");
}


int main(int argc, char* argv[]) {
  init();

  env_t*  env = make_initial_env();
  expr_t* expr;

  (void) setjmp(restart);

  for (;;) {
    expr = read(stdin);
    expr = eval(expr, env);
    print(expr);
    printf("\n");
  }

  return 0;
}
