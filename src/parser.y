%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

int yylex(void);
void yyerror(const char *msg);

Node *ast_root = NULL;
int syntax_errors = 0;

static Node *make_decl(Type t, Node *list, int line)
{
    list->kind = N_DECL;
    list->type = t;
    list->line = line;
    for (int i = 0; i < list->nkids; i++) {
        Node *v = list->kids[i];
        v->type = v->is_array ? arr_type(t) : t;
    }
    return list;
}
%}

%define parse.error verbose
%define parse.lac full
%locations
%expect 0

%union {
    Node *node;
    char *str;
    int ival;
    double dval;
    Type type;
}

%token <str> ID STRING_LIT
%token <ival> INT_LIT
%token <dval> FLOAT_LIT
%token INT DOUBLE VOID IF ELSE WHILE DO FOR RETURN BREAK CONTINUE
%token EQ NE LE GE AND OR INC DEC
%token ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%right '?' ':'
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right UNARY INC DEC

%type <type> type
%type <node> program ext_def func_def params param_list param decl var_list var
%type <node> init_list block items stmt for_init opt_expr expr args arg_list

%%

program
    : %empty                    { $$ = ast_root = node_new(N_PROGRAM, 1); }
    | program ext_def           { $$ = node_add($1, $2); }
    ;

ext_def
    : func_def
    | decl
    ;

type
    : INT                       { $$ = T_INT; }
    | DOUBLE                    { $$ = T_DOUBLE; }
    | VOID                      { $$ = T_VOID; }
    ;

func_def
    : type ID '(' params ')' block {
                                  $$ = node_name(N_FUNC, @2.first_line, $2);
                                  $$->type = $1;
                                  node_add($$, $4);
                                  node_add($$, $6);
                                }
    | type ID '(' params ')' ';' {
                                  $$ = node_name(N_FUNC, @2.first_line, $2);
                                  $$->type = $1;
                                  node_add($$, $4);
                                }
    ;

params
    : %empty                    { $$ = node_new(N_PARAMS, yylloc.first_line); }
    | VOID                      { $$ = node_new(N_PARAMS, @1.first_line); }
    | param_list
    ;

param_list
    : param                     { $$ = node_add(node_new(N_PARAMS, @1.first_line), $1); }
    | param_list ',' param      { $$ = node_add($1, $3); }
    ;

param
    : type ID                   { $$ = node_name(N_PARAM, @2.first_line, $2); $$->type = $1; }
    | type ID '[' ']'           {
                                  $$ = node_name(N_PARAM, @2.first_line, $2);
                                  $$->type = arr_type($1);
                                  $$->is_array = 1;
                                }
    ;

decl
    : type var_list ';'         { $$ = make_decl($1, $2, @1.first_line); }
    ;

var_list
    : var                       { $$ = node_add(node_new(N_DECL, @1.first_line), $1); }
    | var_list ',' var          { $$ = node_add($1, $3); }
    ;

var
    : ID                        { $$ = node_name(N_VAR, @1.first_line, $1); }
    | ID '=' expr               { $$ = node_add(node_name(N_VAR, @1.first_line, $1), $3); }
    | ID '[' INT_LIT ']'        {
                                  $$ = node_name(N_VAR, @1.first_line, $1);
                                  $$->is_array = 1;
                                  $$->ival = $3;
                                }
    | ID '[' INT_LIT ']' '=' '{' init_list '}' {
                                  $$ = node_add(node_name(N_VAR, @1.first_line, $1), $7);
                                  $$->is_array = 1;
                                  $$->ival = $3;
                                }
    | ID '[' ']' '=' '{' init_list '}' {
                                  $$ = node_add(node_name(N_VAR, @1.first_line, $1), $6);
                                  $$->is_array = 1;
                                  $$->ival = $6->nkids;
                                }
    ;

init_list
    : expr                      { $$ = node_add(node_new(N_INIT_LIST, @1.first_line), $1); }
    | init_list ',' expr        { $$ = node_add($1, $3); }
    ;

block
    : '{' items '}'             { $$ = $2; $$->line = @1.first_line; }
    ;

items
    : %empty                    { $$ = node_new(N_BLOCK, yylloc.first_line); }
    | items stmt                { $$ = node_add($1, $2); }
    | items decl                { $$ = node_add($1, $2); }
    ;

stmt
    : block
    | expr ';'                  { $$ = node_op(N_EXPR_STMT, @1.first_line, NULL, $1, NULL); }
    | ';'                       { $$ = node_new(N_EMPTY, @1.first_line); }
    | IF '(' expr ')' stmt %prec LOWER_THAN_ELSE
                                { $$ = node_op(N_IF, @1.first_line, NULL, $3, $5); }
    | IF '(' expr ')' stmt ELSE stmt {
                                  $$ = node_op(N_IF, @1.first_line, NULL, $3, $5);
                                  node_add($$, $7);
                                }
    | WHILE '(' expr ')' stmt   { $$ = node_op(N_WHILE, @1.first_line, NULL, $3, $5); }
    | DO stmt WHILE '(' expr ')' ';'
                                { $$ = node_op(N_DO, @1.first_line, NULL, $2, $5); }
    | FOR '(' for_init opt_expr ';' opt_expr ')' stmt {
                                  $$ = node_op(N_FOR, @1.first_line, NULL, $3, $4);
                                  node_add($$, $6);
                                  node_add($$, $8);
                                }
    | RETURN ';'                { $$ = node_new(N_RETURN, @1.first_line); }
    | RETURN expr ';'           { $$ = node_op(N_RETURN, @1.first_line, NULL, $2, NULL); }
    | BREAK ';'                 { $$ = node_new(N_BREAK, @1.first_line); }
    | CONTINUE ';'              { $$ = node_new(N_CONTINUE, @1.first_line); }
    | error ';'                 { $$ = node_new(N_EMPTY, @1.first_line); yyerrok; }
    | error '}'                 { $$ = node_new(N_EMPTY, @1.first_line); yyerrok; }
    ;

for_init
    : ';'                       { $$ = node_new(N_EMPTY, @1.first_line); }
    | expr ';'                  { $$ = node_op(N_EXPR_STMT, @1.first_line, NULL, $1, NULL); }
    | decl
    ;

opt_expr
    : %empty                    { $$ = node_new(N_EMPTY, yylloc.first_line); }
    | expr
    ;

expr
    : expr '=' expr             { $$ = node_op(N_ASSIGN, @2.first_line, "=", $1, $3); }
    | expr ADD_ASSIGN expr      { $$ = node_op(N_ASSIGN, @2.first_line, "+=", $1, $3); }
    | expr SUB_ASSIGN expr      { $$ = node_op(N_ASSIGN, @2.first_line, "-=", $1, $3); }
    | expr MUL_ASSIGN expr      { $$ = node_op(N_ASSIGN, @2.first_line, "*=", $1, $3); }
    | expr DIV_ASSIGN expr      { $$ = node_op(N_ASSIGN, @2.first_line, "/=", $1, $3); }
    | expr MOD_ASSIGN expr      { $$ = node_op(N_ASSIGN, @2.first_line, "%=", $1, $3); }
    | expr '?' expr ':' expr    {
                                  $$ = node_op(N_TERNARY, @2.first_line, NULL, $1, $3);
                                  node_add($$, $5);
                                }
    | expr OR expr              { $$ = node_op(N_BINOP, @2.first_line, "||", $1, $3); }
    | expr AND expr             { $$ = node_op(N_BINOP, @2.first_line, "&&", $1, $3); }
    | expr EQ expr              { $$ = node_op(N_BINOP, @2.first_line, "==", $1, $3); }
    | expr NE expr              { $$ = node_op(N_BINOP, @2.first_line, "!=", $1, $3); }
    | expr '<' expr             { $$ = node_op(N_BINOP, @2.first_line, "<", $1, $3); }
    | expr '>' expr             { $$ = node_op(N_BINOP, @2.first_line, ">", $1, $3); }
    | expr LE expr              { $$ = node_op(N_BINOP, @2.first_line, "<=", $1, $3); }
    | expr GE expr              { $$ = node_op(N_BINOP, @2.first_line, ">=", $1, $3); }
    | expr '+' expr             { $$ = node_op(N_BINOP, @2.first_line, "+", $1, $3); }
    | expr '-' expr             { $$ = node_op(N_BINOP, @2.first_line, "-", $1, $3); }
    | expr '*' expr             { $$ = node_op(N_BINOP, @2.first_line, "*", $1, $3); }
    | expr '/' expr             { $$ = node_op(N_BINOP, @2.first_line, "/", $1, $3); }
    | expr '%' expr             { $$ = node_op(N_BINOP, @2.first_line, "%", $1, $3); }
    | '-' expr %prec UNARY      { $$ = node_op(N_UNOP, @1.first_line, "-", $2, NULL); }
    | '+' expr %prec UNARY      { $$ = $2; }
    | '!' expr %prec UNARY      { $$ = node_op(N_UNOP, @1.first_line, "!", $2, NULL); }
    | INC expr                  { $$ = node_op(N_PREFIX, @1.first_line, "++", $2, NULL); }
    | DEC expr                  { $$ = node_op(N_PREFIX, @1.first_line, "--", $2, NULL); }
    | expr INC                  { $$ = node_op(N_POSTFIX, @2.first_line, "++", $1, NULL); }
    | expr DEC                  { $$ = node_op(N_POSTFIX, @2.first_line, "--", $1, NULL); }
    | '(' type ')' expr %prec UNARY {
                                  $$ = node_op(N_CAST, @1.first_line, NULL, $4, NULL);
                                  $$->type = $2;
                                }
    | '(' expr ')'              { $$ = $2; }
    | ID '(' args ')'           { $$ = $3; $$->name = $1; $$->line = @1.first_line; }
    | ID '[' expr ']'           {
                                  $$ = node_op(N_INDEX, @1.first_line, NULL,
                                               node_name(N_IDENT, @1.first_line, $1), $3);
                                }
    | ID                        { $$ = node_name(N_IDENT, @1.first_line, $1); }
    | INT_LIT                   { $$ = node_new(N_INT, @1.first_line); $$->ival = $1; }
    | FLOAT_LIT                 { $$ = node_new(N_DOUBLE, @1.first_line); $$->dval = $1; }
    | STRING_LIT                { $$ = node_name(N_STRING, @1.first_line, $1); }
    ;

args
    : %empty                    { $$ = node_new(N_CALL, yylloc.first_line); }
    | arg_list
    ;

arg_list
    : expr                      { $$ = node_add(node_new(N_CALL, @1.first_line), $1); }
    | arg_list ',' expr         { $$ = node_add($1, $3); }
    ;

%%

void yyerror(const char *msg)
{
    syntax_errors++;
    fprintf(stderr, "рядок %d:%d: %s\n", yylloc.first_line, yylloc.first_column, msg);
}
