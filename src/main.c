#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"

extern FILE *yyin;
extern int yydebug;
extern int show_tokens;
extern int lex_errors;
extern int syntax_errors;
extern Node *ast_root;
int yyparse(void);

static void usage(const char *prog)
{
    fprintf(stderr,
        "використання: %s [опції] файл.c\n"
        "  -t          вивести токени (лексичний аналіз)\n"
        "  -y          трасування роботи LALR-парсера\n"
        "  -a          вивести AST у текстовому вигляді\n"
        "  -j файл     зберегти AST у JSON\n"
        "  -d файл     зберегти AST у форматі Graphviz DOT\n"
        "  -w файл     зберегти HTML-сторінку з деревом (d3.js)\n"
        "  -o файл     транслювати в JavaScript\n"
        "  -r          виконати програму інтерпретатором\n"
        "без -a/-j/-d/-w/-o програма виконується (-r)\n", prog);
    exit(2);
}

static FILE *open_out(const char *path)
{
    FILE *f = strcmp(path, "-") == 0 ? stdout : fopen(path, "w");
    if (!f) {
        perror(path);
        exit(2);
    }
    return f;
}

static void close_out(FILE *f)
{
    if (f != stdout) fclose(f);
}

int main(int argc, char **argv)
{
    int opt, print_ast = 0, run = 0;
    char *json = NULL, *dot = NULL, *html = NULL, *js = NULL;

    while ((opt = getopt(argc, argv, "tyaj:d:w:o:r")) != -1) {
        switch (opt) {
        case 't': show_tokens = 1; break;
        case 'y': yydebug = 1; break;
        case 'a': print_ast = 1; break;
        case 'j': json = optarg; break;
        case 'd': dot = optarg; break;
        case 'w': html = optarg; break;
        case 'o': js = optarg; break;
        case 'r': run = 1; break;
        default: usage(argv[0]);
        }
    }
    if (optind != argc - 1) usage(argv[0]);
    if (!print_ast && !json && !dot && !html && !js && !show_tokens) run = 1;

    yyin = fopen(argv[optind], "r");
    if (!yyin) {
        perror(argv[optind]);
        return 2;
    }

    yyparse();
    fclose(yyin);
    if (lex_errors || syntax_errors || !ast_root) {
        fprintf(stderr, "помилок розбору: %d\n", lex_errors + syntax_errors);
        return 1;
    }

    int sema_errors = sema_check(ast_root);

    if (print_ast) ast_print(stdout, ast_root, 0);
    if (json) {
        FILE *f = open_out(json);
        ast_json(f, ast_root);
        fputc('\n', f);
        close_out(f);
    }
    if (dot) {
        FILE *f = open_out(dot);
        ast_dot(f, ast_root);
        close_out(f);
    }
    if (html) {
        FILE *f = open_out(html);
        ast_html(f, ast_root, argv[optind]);
        close_out(f);
    }

    if (sema_errors) {
        fprintf(stderr, "семантичних помилок: %d\n", sema_errors);
        return 1;
    }

    if (js) {
        FILE *f = open_out(js);
        gen_js(f, ast_root);
        close_out(f);
    }
    if (run) return interp_run(ast_root);
    return 0;
}
