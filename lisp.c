#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char* skip_whitespace(char* s) {
  while (*s == ' ' || *s == '\t' || *s == '\n') {
    s++;
  }

  return s;
}


typedef enum {
  TOKEN_LIST_BEGIN,
  TOKEN_LIST_END,
  TOKEN_ATOM
} token_type_t;


typedef struct token_t {
  token_type_t type;
  char*        value;
} token_t;


typedef struct {
  size_t   count;
  token_t* tokens;
} tokens_t;


void error(char* message) {
  fprintf(stderr, "error: %s\n", message);
}


void fatal(char* message) {
  error(message);
  exit(1);
}


void add_token(tokens_t* tokens, token_type_t type, char* value) {
  token_t* reallocated = realloc(tokens->tokens, sizeof(token_t) * (tokens->count + 1));
  if (reallocated == NULL) {
    fatal("out of memory");
  }

  tokens->tokens                      = reallocated;
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
        add_token(tokens, TOKEN_LIST_BEGIN, NULL);
        s++;
        break;

      case ')':
        add_token(tokens, TOKEN_LIST_END, NULL);
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
        if (value == NULL) {
          fatal("out of memory");
        }
        add_token(tokens, TOKEN_ATOM, value);
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
        if (value == NULL) {
          fatal("out of memory");
        }
        add_token(tokens, TOKEN_ATOM, value);
        break;
      }
    }
  }
}


void print_tokens(tokens_t* tokens) {
  size_t i;

  printf("%lu tokens\n", tokens->count);

  for (i = 0; i < tokens->count; i++) {
    token_t* token = &tokens->tokens[i];

    switch (token->type) {
      case TOKEN_LIST_BEGIN:
        printf("LIST( ");
        break;

      case TOKEN_LIST_END:
        printf(")LIST ");
        break;

      case TOKEN_ATOM:
        printf("ATOM(%s) ", token->value);
        break;
    }
  }

  printf("\n");
}


void chomp(char* s) {
  size_t i = strlen(s);
  if (i > 0 && s[i - 1] == '\n') {
    s[i - 1] = '\0';
  }
}


typedef int expr_t;


expr_t read(FILE* in) {
  char buffer[128 + 1];

  (void) fgets(buffer, sizeof(buffer), in);
  if (feof(in)) {
    exit(0);
  }

  tokens_t tokens = {0, NULL};
  tokenize(buffer, &tokens);
  print_tokens(&tokens);
  free_tokens(&tokens);

  return 0;
}


typedef int env_t;


expr_t eval(expr_t expr, env_t env) {
  return 0;
}


void print(expr_t expr) {
}


int main(int argc, char* argv[]) {
  env_t  env;
  expr_t expr;

  for (;;) {
    expr = read(stdin);
    expr = eval(expr, env);
    print(expr);
  }

  return 0;
}
