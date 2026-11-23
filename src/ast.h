#ifndef AST_H
#define AST_H

#include <stdio.h>

typedef enum {
    T_NONE, T_VOID, T_INT, T_DOUBLE, T_STRING, T_INT_ARR, T_DOUBLE_ARR
} Type;

typedef enum {
    N_PROGRAM, N_FUNC, N_PARAMS, N_PARAM, N_DECL, N_VAR, N_INIT_LIST,
    N_BLOCK, N_IF, N_WHILE, N_DO, N_FOR, N_RETURN, N_BREAK, N_CONTINUE,
    N_EXPR_STMT, N_EMPTY,
    N_ASSIGN, N_BINOP, N_UNOP, N_PREFIX, N_POSTFIX, N_TERNARY, N_CAST,
    N_CALL, N_INDEX, N_IDENT, N_INT, N_DOUBLE, N_STRING
} NodeKind;

typedef struct Node {
    NodeKind kind;
    int line;
    char *name;
    int ival;
    double dval;
    Type type;
    int is_array;
    char *alias;
    struct Node **kids;
    int nkids;
} Node;

Node *node_new(NodeKind kind, int line);
Node *node_add(Node *n, Node *kid);
Node *node_op(NodeKind kind, int line, const char *op, Node *a, Node *b);
Node *node_name(NodeKind kind, int line, const char *name);

const char *kind_name(NodeKind k);
const char *type_name(Type t);
Type elem_type(Type t);
Type arr_type(Type t);

void ast_print(FILE *f, Node *n, int depth);
void ast_json(FILE *f, Node *n);
void ast_dot(FILE *f, Node *root);
void ast_html(FILE *f, Node *root, const char *title);

int sema_check(Node *root);

int interp_run(Node *root);

void gen_js(FILE *f, Node *root);

#endif
