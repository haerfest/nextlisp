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
  EXPR_TYPE_PROC
} expr_type_t;


typedef struct expr_t {
  expr_type_t type;
  union {
    char*  symbol;
    char*  string;
    int    number;
    struct {
      struct expr_t* car;
      struct expr_t* cdr;
    } pair;
    struct {
      char* name;
      struct expr_t* (*fn)(struct expr_t*);
    } proc;
  } value;
} expr_t;


typedef expr_t* (proc_t)(expr_t*);


#define caar(expr)  car(car(expr))
#define cadr(expr)  car(cdr(expr))
#define caddr(expr) car(cdr(cdr(expr)))
#define cadar(expr) car(cdr(car(expr)))
#define cdar(expr)  cdr(car(expr))


#define MAKE_SYMBOL(name) \
  expr_t  SYMBOL_ ## name = { .type = EXPR_TYPE_SYMBOL, .value.symbol = #name }; \
  expr_t* name = &SYMBOL_ ## name;

MAKE_SYMBOL(COND);
MAKE_SYMBOL(LABEL);
MAKE_SYMBOL(LAMBDA);
MAKE_SYMBOL(NIL);
MAKE_SYMBOL(QUOTE);
MAKE_SYMBOL(T);


jmp_buf restart;


expr_t* eval(expr_t*, expr_t*);
expr_t* apply(expr_t*, expr_t*, expr_t*);
void    print(expr_t*);


void error(void) {
  fprintf(stderr, "error\n");
  fflush(stderr);

  longjmp(restart, 1);
}


char* skip_whitespace(char* s) {
  while (*s == ' ' || *s == '\t' || *s == '\n') {
    s++;
  }

  return s;
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
      case '(' : add_token(tokens, TOKEN_TYPE_LIST_BEGIN, NULL); s++; break;
      case ')' : add_token(tokens, TOKEN_TYPE_LIST_END,   NULL); s++; break;
      case '\'': add_token(tokens, TOKEN_TYPE_QUOTE,      NULL); s++; break;
      case '.' : add_token(tokens, TOKEN_TYPE_DOT,        NULL); s++; break;
        
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
          error();
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
      case TOKEN_TYPE_LIST_BEGIN: printf("[(] ");                break;
      case TOKEN_TYPE_LIST_END  : printf("[)] ");                break;
      case TOKEN_TYPE_QUOTE     : printf("['] ");                break;
      case TOKEN_TYPE_DOT       : printf("[.] ");                break;
      case TOKEN_TYPE_ATOM      : printf("[%s] ", token->value); break;
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
    case EXPR_TYPE_PROC:
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


expr_t* make_proc(char* name, proc_t* proc) {
  expr_t* expr = malloc(sizeof(expr_t));

  expr->type            = EXPR_TYPE_PROC;
  expr->value.proc.name = name;
  expr->value.proc.fn   = proc;

  return expr;
}


expr_t* make_atom(char* value) {
  if (*value == '"') {
    expr_t* expr       = malloc(sizeof(expr_t));
    expr->type         = EXPR_TYPE_STRING;
    expr->value.string = strndup(value + 1, strlen(value) - 2);
    return expr;
  }

  if ((*value >= '0' && *value <= '9') ||
      ((*value == '-' || *value == '+') && (*(value + 1) >= '0' && *(value + 1) <= '9'))) {
    expr_t* expr       = malloc(sizeof(expr_t));
    expr->type         = EXPR_TYPE_NUMBER;
    expr->value.number = strtol(value, NULL, 10);
    return expr;
  }

  return intern(value);
}


expr_t* parse(tokens_t* tokens, size_t* index) {
  if (*index == tokens->count) {
    error();
  }

  switch (tokens->tokens[*index].type) {
    case TOKEN_TYPE_ATOM:
      return make_atom(tokens->tokens[*index].value);

    case TOKEN_TYPE_QUOTE:
      (*index)++;
      return cons(QUOTE, cons(parse(tokens, index), NULL));

    case TOKEN_TYPE_DOT:
      error();

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
        error();
      }

      if (tokens->tokens[*index].type == TOKEN_TYPE_DOT) {
        if (!prev_expr) {
          error();
        }

        (*index)++;
        prev_expr->value.pair.cdr = parse(tokens, index);
      }

      return expr;
    }
    
    case TOKEN_TYPE_LIST_END:
      error();
  }

  return NULL;
}


expr_t* car(expr_t* expr) {
  return expr->value.pair.car;
}


expr_t* cdr(expr_t* expr) {
  return expr->value.pair.cdr;
}


int atom(expr_t* expr) {
  return expr == NULL || (expr->type != EXPR_TYPE_PAIR);
}


int eq(expr_t* a, expr_t* b) {
  if (a == NULL || b == NULL) {
    return (a == NULL && b == NULL);
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
    case EXPR_TYPE_SYMBOL: printf("%s", expr->value.symbol);           break;
    case EXPR_TYPE_STRING: printf("%s", expr->value.string);           break;
    case EXPR_TYPE_NUMBER: printf("%d", expr->value.number);           break;
    case EXPR_TYPE_PROC  : printf("#<PROC %s>", expr->value.proc.name); break;

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

  size_t  start = 0;
  expr_t* expr  = parse(&tokens, &start);
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


expr_t* apply(expr_t* fn, expr_t* args, expr_t* env) {
  printf("apply: fn=");
  print(fn);
  printf(", args=");
  print(args);
  printf(", env=");
  print(env);
  printf("\n");

  switch (fn->type) {
    case EXPR_TYPE_SYMBOL:
      return apply(eval(fn, env), args, env);

    case EXPR_TYPE_PAIR:
      if (eq(car(fn), LAMBDA)) return eval(caddr(fn), pairlis(cadr(fn), args, env));
      if (eq(car(fn), LABEL )) return apply(caddr(fn), args, cons(cons(cadr(fn), caddr(fn)), env));

      error();

    case EXPR_TYPE_PROC:
      return fn->value.proc.fn(args);

    default:
      error();
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

  error();
  return 0;
}


expr_t* eval(expr_t* expr, expr_t* env) {
  printf("eval: expr=");
  print(expr);
  printf(", env=");
  print(env);
  printf("\n");

  if (expr == NULL) {
    return NULL;
  }

  switch (expr->type) {
    case EXPR_TYPE_NUMBER:
    case EXPR_TYPE_STRING:
    case EXPR_TYPE_PROC:
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


expr_t* proc_add(expr_t* args) {
  int sum = 0;

  while (args) {
    sum += car(args)->value.number;
    args = cdr(args);
  }

  return make_number(sum);
}


expr_t* proc_subtract(expr_t* args) {
  int sum = car(args)->value.number;
  args = cdr(args);

  while (args) {
    sum -= car(args)->value.number;
    args = cdr(args);
  }

  return make_number(sum);
}


expr_t* proc_car(expr_t* args) {
  return caar(args);
}


expr_t* proc_cdr(expr_t* args) {
  return cdar(args);
}


expr_t* proc_cons(expr_t* args) {
  return cons(car(args), cadr(args));
}


expr_t* proc_atom(expr_t* args) {
  return atom(car(args)) ? T : NULL;
}


expr_t* proc_eq(expr_t* args) {
  return eq(car(args), cadr(args)) ? T : NULL;
}


expr_t* make_initial_env(void) {
  expr_t* env = NULL;

  env = cons(cons(NIL, NULL), env);
  env = cons(cons(T,   T   ), env);

  env = cons(cons(intern("+"   ), make_proc("+",    proc_add     )), env);
  env = cons(cons(intern("-"   ), make_proc("-",    proc_subtract)), env);
  env = cons(cons(intern("CAR" ), make_proc("CAR",  proc_car     )), env);
  env = cons(cons(intern("CDR" ), make_proc("CDR",  proc_cdr     )), env);
  env = cons(cons(intern("CONS"), make_proc("CONS", proc_cons    )), env);
  env = cons(cons(intern("ATOM"), make_proc("ATOM", proc_atom    )), env);
  env = cons(cons(intern("EQ"  ), make_proc("EQ",   proc_eq      )), env);

  return env;
}


int main(int argc, char* argv[]) {
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
