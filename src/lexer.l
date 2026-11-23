%{
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "parser.tab.h"

int show_tokens = 0;
int lex_errors = 0;
static int column = 1;

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = column; \
    column += yyleng; \
    yylloc.last_column = column - 1;

#define TOK(t) do { if (show_tokens) printf("%4d  %-12s %s\n", yylineno, #t, yytext); return t; } while (0)

static char escape(char c)
{
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '0': return '\0';
    default:  return c;
    }
}

static char *unquote(const char *s, int len)
{
    char *res = malloc(len);
    int j = 0;
    for (int i = 1; i < len - 1; i++) {
        if (s[i] == '\\' && i + 1 < len - 1)
            res[j++] = escape(s[++i]);
        else
            res[j++] = s[i];
    }
    res[j] = '\0';
    return res;
}
%}

%option noyywrap nounput noinput yylineno
%x COMMENT

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*
EXP     [eE][+-]?{DIGIT}+

%%

"/*"                    { BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>\n             { column = 1; }
<COMMENT>.              { }
"//".*                  { }
^[ \t]*#.*              { }

"int"                   TOK(INT);
"double"                TOK(DOUBLE);
"void"                  TOK(VOID);
"if"                    TOK(IF);
"else"                  TOK(ELSE);
"while"                 TOK(WHILE);
"do"                    TOK(DO);
"for"                   TOK(FOR);
"return"                TOK(RETURN);
"break"                 TOK(BREAK);
"continue"              TOK(CONTINUE);

{DIGIT}+"."{DIGIT}*{EXP}? |
"."{DIGIT}+{EXP}?       |
{DIGIT}+{EXP}           { yylval.dval = atof(yytext); TOK(FLOAT_LIT); }
{DIGIT}+                { yylval.ival = atoi(yytext); TOK(INT_LIT); }
'(\\.|[^\\'\n])'        { yylval.ival = yytext[1] == '\\' ? escape(yytext[2]) : yytext[1]; TOK(INT_LIT); }
\"(\\.|[^\\"\n])*\"     { yylval.str = unquote(yytext, yyleng); TOK(STRING_LIT); }
{ID}                    { yylval.str = strdup(yytext); TOK(ID); }

"=="                    TOK(EQ);
"!="                    TOK(NE);
"<="                    TOK(LE);
">="                    TOK(GE);
"&&"                    TOK(AND);
"||"                    TOK(OR);
"++"                    TOK(INC);
"--"                    TOK(DEC);
"+="                    TOK(ADD_ASSIGN);
"-="                    TOK(SUB_ASSIGN);
"*="                    TOK(MUL_ASSIGN);
"/="                    TOK(DIV_ASSIGN);
"%="                    TOK(MOD_ASSIGN);

[-+*/%<>=!?:;,(){}\[\]] {
                          if (show_tokens) printf("%4d  '%c'\n", yylineno, yytext[0]);
                          return yytext[0];
                        }

\n                      { column = 1; }
[ \t\r]+                { }
.                       {
                          fprintf(stderr, "рядок %d:%d: невідомий символ '%s'\n",
                                  yylineno, column - 1, yytext);
                          lex_errors++;
                        }

%%
