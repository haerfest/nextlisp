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
  EXPR_TYPE_NUMBER,
  EXPR_TYPE_PRIMITIVE
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
    int    number;
    struct expr_t* (*primitive)(struct expr_t*);
  } value;
} expr_t;


typedef expr_t* (primitive_t)(expr_t*);


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


expr_t* eval(expr_t*, expr_t*);
expr_t* apply(expr_t*, expr_t*, expr_t*);


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

    case EXPR_TYPE_NUMBER:
    case EXPR_TYPE_PRIMITIVE:
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


expr_t* intern(char* symbol) {
  expr_t* expr = malloc(sizeof(expr_t));

  expr->type         = EXPR_TYPE_SYMBOL;
  expr->value.symbol = strdup(symbol);

  upcase(expr->value.symbol);

  return expr;
}


expr_t* cons(expr_t* car, expr_t* cdr) {
  expr_t* expr = malloc(sizeof(expr_t));

  expr->type           = EXPR_TYPE_PAIR;
  expr->value.pair.car = car;
  expr->value.pair.cdr = cdr;

  return expr;
}


expr_t* make_primitive(primitive_t* primitive) {
  expr_t* expr = malloc(sizeof(expr_t));

  expr->type            = EXPR_TYPE_PRIMITIVE;
  expr->value.primitive = primitive;

  return expr;
}


expr_t* make_atom(char* value) {
  if (*value == '"') {
    expr_t* expr       = malloc(sizeof(expr_t));
    expr->type         = EXPR_TYPE_STRING;
    expr->value.string = strndup(value + 1, strlen(value) - 2);
    return expr;
  }

  if (*value >= '0' && *value <= '9') {
    expr_t* expr       = malloc(sizeof(expr_t));
    expr->type         = EXPR_TYPE_NUMBER;
    expr->value.number = strtol(value, NULL, 10);
    return expr;
  }

  return intern(value);
}


expr_t* quote(expr_t* expr) {
  return cons(QUOTE, cons(expr, NULL));
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
      return cons(QUOTE, cons(parse(tokens, index), NULL));
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
        expr_t* this_expr = cons(sub_expr, NULL);

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


expr_t* caddr(expr_t* expr) {
  return car(cdr(cdr(expr)));
}


expr_t* cadar(expr_t* expr) {
  return car(cdr(car(expr)));
}


expr_t* cdar(expr_t* expr) {
  return cdr(car(expr));
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

  if (a->type == EXPR_TYPE_NUMBER) {
    return (a->value.number ==  b->value.number);
  }

  return 0;
}


void print_helper(expr_t* expr, int do_print_brackets) {
  if (expr == NULL) {
    printf("NIL");
    return;
  }

  switch (expr->type) {
    case EXPR_TYPE_SYMBOL   : printf("%s", expr->value.symbol); break;
    case EXPR_TYPE_STRING   : printf("%s", expr->value.string); break;
    case EXPR_TYPE_NUMBER   : printf("%d", expr->value.number); break;
    case EXPR_TYPE_PRIMITIVE: printf("<PRIMITIVE>");            break;

    case EXPR_TYPE_PAIR:
      if (do_print_brackets) {
        printf("(");
      }
      print_helper(car(expr), 1);
      if (!atom(cdr(expr))) {
        printf(" ");
        print_helper(cdr(expr), 0);
      } else if (cdr(expr) != NULL) {
        printf(" . ");
        print_helper(cdr(expr), 1);
      }
      if (do_print_brackets) {
        printf(")");
      }
      break;

  }
}


void print(expr_t* expr) {
  print_helper(expr, 1);
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
  print(expr);
  printf("\n");
  free_tokens(&tokens);

  return expr;
}


expr_t* evcon(expr_t* c, expr_t* env) {
  while (c) {
    expr_t* condition = eval(caar(c), env);
    if (condition) {
      return eval(cadar(c), env);
    }

    c = cdr(c);
  }

  return NULL;
}


expr_t* evlis(expr_t* m, expr_t* env) {
  if (m == NULL) {
    return NULL;
  }

  return cons(eval(car(m), env), evlis(cdr(m), env));
}


expr_t* pairlis(expr_t* x, expr_t* y, expr_t* env) {
  if (x == NULL) return env;

  return cons(cons(car(x), car(y)), pairlis(cdr(x), cdr(y), env));
}


expr_t* apply(expr_t* fn, expr_t* x, expr_t* env) {
  switch (fn->type) {
    case EXPR_TYPE_NUMBER: error("cannot apply %d",     fn->value.number);
    case EXPR_TYPE_STRING: error("cannot apply \"%s\"", fn->value.string);

    case EXPR_TYPE_SYMBOL:
      if (eq(fn, CAR )) return caar(x);
      if (eq(fn, CDR )) return cdar(x);
      if (eq(fn, CONS)) return cons(car(x), cadr(x));
      if (eq(fn, ATOM)) return atom(car(x))        ? T : NULL;
      if (eq(fn, EQ  )) return eq(car(x), cadr(x)) ? T : NULL;

      return apply(eval(fn, env), x, env);

    case EXPR_TYPE_PAIR:
      if (eq(car(fn), LAMBDA)) return eval(caddr(fn), pairlis(cadr(fn), x, env));
      if (eq(car(fn), LABEL )) return apply(caddr(fn), x, cons(cons(cadr(fn), caddr(fn)), env));

      error("cannot apply");

    case EXPR_TYPE_PRIMITIVE:
      return fn->value.primitive(x);
  }

  return NULL;
}


int equal(expr_t* x, expr_t* y) {
  for (; x && y; x = cdr(x), y = cdr(y)) {
    if (atom(x)) {
      return atom(y) ? eq(x, y) : 0;
    }

    if (!equal(car(x), car(y))) {
      return 0;
    }
  }

  return 0;
}


expr_t* assoc(expr_t* x, expr_t* env) {
  for (; env; env = cdr(env)) {
    if (equal(caar(env), x)) {
      return car(env);
    }
  }

  error("%s undefined", x->value.symbol);
  return 0;
}


expr_t* eval(expr_t* expr, expr_t* env) {
  if (expr == NULL) {
    return NULL;
  }

  switch (expr->type) {
    case EXPR_TYPE_NUMBER:
    case EXPR_TYPE_STRING:
    case EXPR_TYPE_PRIMITIVE:
      return expr;

    case EXPR_TYPE_SYMBOL:
      return cdr(assoc(expr, env));

    case EXPR_TYPE_PAIR:
      if (eq(car(expr), QUOTE)) return cadr(expr);
      if (eq(car(expr), COND )) return evcon(cdr(expr), env);

      return apply(car(expr), evlis(cdr(expr), env), env);
  }

  return NULL;
}


expr_t* make_number(int number) {
  expr_t* expr = malloc(sizeof(expr_t));

  expr->type         = EXPR_TYPE_NUMBER;
  expr->value.number = number;

  return expr;
}


expr_t* primitive_add(expr_t* args) {
  int sum = 0;

  while (args) {
    if (car(args)->type != EXPR_TYPE_NUMBER) {
      error("argument not a number");
    }
    sum += car(args)->value.number;
    args = cdr(args);
  }

  return make_number(sum);
}


expr_t* primitive_subtract(expr_t* args) {
  int sum = 0;

  while (args) {
    if (car(args)->type != EXPR_TYPE_NUMBER) {
      error("argument not a number");
    }
    sum -= car(args)->value.number;
    args = cdr(args);
  }

  return make_number(sum);
}


expr_t* make_initial_env(void) {
  expr_t* env = NULL;

  env = cons(cons(NIL, NULL), env);
  env = cons(cons(T,   T   ), env);
  env = cons(cons(intern("+"), make_primitive(primitive_add     )), env);
  env = cons(cons(intern("-"), make_primitive(primitive_subtract)), env);
  
  return env;
}


void init(void) {
  ATOM   = intern("ATOM");
  CAR    = intern("CAR");
  CDR    = intern("CDR");
  COND   = intern("COND");
  CONS   = intern("CONS");
  EQ     = intern("EQ");
  LABEL  = intern("LABEL");
  LAMBDA = intern("LAMBDA");
  NIL    = intern("NIL");
  QUOTE  = intern("QUOTE");
  T      = intern("T");
}


int main(int argc, char* argv[]) {
  init();

  expr_t* env = make_initial_env();
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
