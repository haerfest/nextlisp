#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int is_eos(char s) {
  return s == '\0';
}


int is_whitespace(char s) {
  return (s == ' ' || s == '\t');
}


int is_bracket(char s) {
  return (s == '(' || s == ')');
}


char* skip_whitespace(char* s) {
  while (!is_eos(*s) && is_whitespace(*s)) {
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
  struct token_t* next;
  token_type_t    type;
  char*           value;
} token_t;


token_t* add_token(token_t* tokens, token_type_t type, char* value) {
  token_t* token;

  token = malloc(sizeof(token_t));
  if (token == NULL) {
    perror(NULL);
    exit(1);
  }

  token->next  = NULL;
  token->type  = type;
  token->value = value;

  if (tokens == NULL) {
    return token;
  }

  token_t* last = tokens;
  while (last->next) {
    last = last->next;
  }
  last->next = token;

  return tokens;
}


token_t* tokenize(char* s) {
  token_t* tokens = NULL;

  for (;;) {
    s = skip_whitespace(s);
    if (is_eos(*s)) {
      break;
    }

    if (*s == '(') {
      tokens = add_token(tokens, TOKEN_LIST_BEGIN, NULL);
      s++;
    } else if (*s == ')') {
      add_token(tokens, TOKEN_LIST_END, NULL);
      s++;
    } else {
      char* start = s;
      while (!is_eos(*s) && !is_whitespace(*s) && !is_bracket(*s)) {
        s++;
      }
      size_t n = s - start;
      char* atom = malloc(n + 1);
      if (atom == NULL) {
        perror(NULL);
        exit(1);
      }
      strncpy(atom, start, n);
      atom[n] = '\0';
      tokens = add_token(tokens, TOKEN_ATOM, atom);
    }
  }

  return tokens;
}


void print_tokens(token_t* tokens) {
  while (tokens) {
    switch (tokens->type) {
      case TOKEN_LIST_BEGIN:
        printf("LIST_BEGIN ");
        break;

      case TOKEN_LIST_END:
        printf("LIST_END ");
        break;

      case TOKEN_ATOM:
        printf("ATOM:'%s' ", tokens->value);
        break;
    }

    tokens = tokens->next;
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
  char     buffer[128 + 1];
  token_t* tokens;

  (void) fgets(buffer, sizeof(buffer), in);
  if (feof(in)) {
    exit(0);
  }

  chomp(buffer);
  tokens = tokenize(buffer);
  print_tokens(tokens);

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
