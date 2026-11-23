#include <stdlib.h>
#include <string.h>
#include "ast.h"

typedef struct {
    char *name;
    Type type;
    Node *func;
    char *alias;
} Symbol;

#define MAX_SYMS 1024
#define MAX_SCOPES 128

static Symbol syms[MAX_SYMS];
static int nsyms = 0;
static int scope_start[MAX_SCOPES];
static int nscopes = 0;

static int errors = 0;
static int loop_depth = 0;
static int alias_count = 0;
static Node *cur_func = NULL;

static void error(Node *n, const char *fmt, const char *arg)
{
    fprintf(stderr, "рядок %d: помилка: ", n->line);
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n");
    errors++;
}

static void push_scope(void) { scope_start[nscopes++] = nsyms; }
static void pop_scope(void) { nsyms = scope_start[--nscopes]; }

static Symbol *lookup(const char *name)
{
    for (int i = nsyms - 1; i >= 0; i--)
        if (strcmp(syms[i].name, name) == 0)
            return &syms[i];
    return NULL;
}

static Symbol *declare(Node *n, const char *name, Type type, Node *func)
{
    for (int i = scope_start[nscopes - 1]; i < nsyms; i++) {
        if (strcmp(syms[i].name, name) == 0) {
            if (func && syms[i].func && (syms[i].func->nkids < 2 || func->nkids < 2)) {
                if (syms[i].func->nkids < 2)
                    syms[i].func = func;
                return &syms[i];
            }
            error(n, "повторне оголошення '%s'", name);
            return &syms[i];
        }
    }
    if (nsyms == MAX_SYMS) {
        error(n, "забагато ідентифікаторів%s", "");
        return NULL;
    }
    syms[nsyms].name = (char *)name;
    syms[nsyms].type = type;
    syms[nsyms].func = func;
    syms[nsyms].alias = NULL;
    if (!func && lookup(name)) {
        char buf[300];
        snprintf(buf, sizeof buf, "%s$%d", name, ++alias_count);
        syms[nsyms].alias = n->alias = strdup(buf);
    }
    return &syms[nsyms++];
}

static int is_scalar(Type t) { return t == T_INT || t == T_DOUBLE; }
static int is_array(Type t) { return t == T_INT_ARR || t == T_DOUBLE_ARR; }

static Type expr(Node *n);

static void check_lvalue(Node *n)
{
    if (n->kind == N_INDEX) return;
    if (n->kind == N_IDENT && is_scalar(n->type)) return;
    error(n, "ліворуч від присвоєння має бути змінна або елемент масиву%s", "");
}

static int builtin(Node *n)
{
    const char *f = n->name;
    if (strcmp(f, "printf") == 0) {
        if (n->nkids == 0 || n->kids[0]->kind != N_STRING)
            error(n, "перший аргумент printf має бути рядком%s", "");
        for (int i = 1; i < n->nkids; i++)
            if (!is_scalar(expr(n->kids[i])))
                error(n->kids[i], "аргумент printf має бути int або double%s", "");
        if (n->nkids > 0 && n->kids[0]->kind == N_STRING)
            n->kids[0]->type = T_STRING;
        n->type = T_INT;
        return 1;
    }
    if (strcmp(f, "sqrt") == 0 || strcmp(f, "fabs") == 0 || strcmp(f, "pow") == 0) {
        int need = strcmp(f, "pow") == 0 ? 2 : 1;
        if (n->nkids != need)
            error(n, "неправильна кількість аргументів %s", f);
        for (int i = 0; i < n->nkids; i++)
            if (!is_scalar(expr(n->kids[i])))
                error(n->kids[i], "аргумент %s має бути числом", f);
        n->type = T_DOUBLE;
        return 1;
    }
    return 0;
}

static Type expr(Node *n)
{
    Type a, b;
    Symbol *s;

    switch (n->kind) {
    case N_INT:
        return n->type = T_INT;
    case N_DOUBLE:
        return n->type = T_DOUBLE;
    case N_STRING:
        error(n, "рядкові літерали дозволені лише в printf%s", "");
        return n->type = T_STRING;
    case N_IDENT:
        s = lookup(n->name);
        if (!s || s->func) {
            error(n, "'%s' не оголошено як змінну", n->name);
            return n->type = T_INT;
        }
        n->alias = s->alias;
        return n->type = s->type;
    case N_INDEX:
        a = expr(n->kids[0]);
        b = expr(n->kids[1]);
        if (!is_array(a))
            error(n, "'%s' не є масивом", n->kids[0]->name);
        if (b != T_INT)
            error(n, "індекс масиву має бути int%s", "");
        return n->type = is_array(a) ? elem_type(a) : T_INT;
    case N_CALL:
        if (builtin(n))
            return n->type;
        s = lookup(n->name);
        if (!s || !s->func) {
            error(n, "функцію '%s' не оголошено", n->name);
            for (int i = 0; i < n->nkids; i++) expr(n->kids[i]);
            return n->type = T_INT;
        }
        {
            Node *params = s->func->kids[0];
            if (params->nkids != n->nkids)
                error(n, "неправильна кількість аргументів у виклику '%s'", n->name);
            for (int i = 0; i < n->nkids; i++) {
                Type t = expr(n->kids[i]);
                if (i >= params->nkids) continue;
                Type p = params->kids[i]->type;
                if (is_array(p) && t != p)
                    error(n->kids[i], "аргумент має бути масивом %s", type_name(p));
                if (is_scalar(p) && !is_scalar(t))
                    error(n->kids[i], "аргумент має бути числом%s", "");
            }
        }
        return n->type = s->func->type;
    case N_ASSIGN:
        a = expr(n->kids[0]);
        b = expr(n->kids[1]);
        check_lvalue(n->kids[0]);
        if (!is_scalar(b))
            error(n, "праворуч від '%s' має бути число", n->name);
        if (strcmp(n->name, "%=") == 0 && (a != T_INT || b != T_INT))
            error(n, "операція %s лише для int", n->name);
        return n->type = a;
    case N_PREFIX:
    case N_POSTFIX:
        a = expr(n->kids[0]);
        check_lvalue(n->kids[0]);
        return n->type = a;
    case N_UNOP:
        a = expr(n->kids[0]);
        if (!is_scalar(a))
            error(n, "операнд '%s' має бути числом", n->name);
        return n->type = strcmp(n->name, "!") == 0 ? T_INT : a;
    case N_BINOP:
        a = expr(n->kids[0]);
        b = expr(n->kids[1]);
        if (!is_scalar(a) || !is_scalar(b)) {
            error(n, "операнди '%s' мають бути числами", n->name);
            return n->type = T_INT;
        }
        if (strcmp(n->name, "%") == 0 && (a != T_INT || b != T_INT))
            error(n, "операція %% лише для int%s", "");
        if (strchr("+-*/%", n->name[0]) && n->name[1] == '\0')
            return n->type = (a == T_DOUBLE || b == T_DOUBLE) ? T_DOUBLE : T_INT;
        return n->type = T_INT;
    case N_TERNARY:
        if (!is_scalar(expr(n->kids[0])))
            error(n, "умова має бути числом%s", "");
        a = expr(n->kids[1]);
        b = expr(n->kids[2]);
        if (!is_scalar(a) || !is_scalar(b))
            error(n, "гілки ?: мають бути числами%s", "");
        return n->type = (a == T_DOUBLE || b == T_DOUBLE) ? T_DOUBLE : T_INT;
    case N_CAST:
        if (!is_scalar(expr(n->kids[0])) || !is_scalar(n->type))
            error(n, "неприпустиме зведення типу%s", "");
        return n->type;
    default:
        error(n, "очікувався вираз%s", "");
        return T_INT;
    }
}

static void cond(Node *n)
{
    if (n->kind != N_EMPTY && !is_scalar(expr(n)))
        error(n, "умова має бути числом%s", "");
}

static void decl(Node *d)
{
    if (d->type == T_VOID) {
        error(d, "змінна не може мати тип void%s", "");
        return;
    }
    for (int i = 0; i < d->nkids; i++) {
        Node *v = d->kids[i];
        if (v->is_array && v->ival <= 0)
            error(v, "розмір масиву '%s' має бути додатним", v->name);
        if (v->nkids > 0) {
            Node *init = v->kids[0];
            if (v->is_array) {
                if (init->nkids > v->ival)
                    error(v, "забагато ініціалізаторів для '%s'", v->name);
                for (int j = 0; j < init->nkids; j++)
                    if (!is_scalar(expr(init->kids[j])))
                        error(init->kids[j], "елемент ініціалізатора має бути числом%s", "");
            } else if (!is_scalar(expr(init))) {
                error(v, "ініціалізатор '%s' має бути числом", v->name);
            }
        }
        declare(v, v->name, v->type, NULL);
    }
}

static void stmt(Node *n);

static void block(Node *n, int new_scope)
{
    if (new_scope) push_scope();
    for (int i = 0; i < n->nkids; i++) {
        if (n->kids[i]->kind == N_DECL) decl(n->kids[i]);
        else stmt(n->kids[i]);
    }
    if (new_scope) pop_scope();
}

static void stmt(Node *n)
{
    switch (n->kind) {
    case N_BLOCK:
        block(n, 1);
        break;
    case N_EXPR_STMT:
        expr(n->kids[0]);
        break;
    case N_EMPTY:
        break;
    case N_IF:
        cond(n->kids[0]);
        stmt(n->kids[1]);
        if (n->nkids == 3) stmt(n->kids[2]);
        break;
    case N_WHILE:
        cond(n->kids[0]);
        loop_depth++;
        stmt(n->kids[1]);
        loop_depth--;
        break;
    case N_DO:
        loop_depth++;
        stmt(n->kids[0]);
        loop_depth--;
        cond(n->kids[1]);
        break;
    case N_FOR:
        push_scope();
        if (n->kids[0]->kind == N_DECL) decl(n->kids[0]);
        else stmt(n->kids[0]);
        cond(n->kids[1]);
        if (n->kids[2]->kind != N_EMPTY) expr(n->kids[2]);
        loop_depth++;
        stmt(n->kids[3]);
        loop_depth--;
        pop_scope();
        break;
    case N_RETURN:
        if (n->nkids == 0 && cur_func->type != T_VOID)
            error(n, "функція '%s' має повертати значення", cur_func->name);
        if (n->nkids == 1) {
            Type t = expr(n->kids[0]);
            if (cur_func->type == T_VOID)
                error(n, "void-функція '%s' не може повертати значення", cur_func->name);
            else if (!is_scalar(t))
                error(n, "return має повертати число%s", "");
        }
        n->type = cur_func->type;
        break;
    case N_BREAK:
    case N_CONTINUE:
        if (loop_depth == 0)
            error(n, "'%s' поза циклом", n->kind == N_BREAK ? "break" : "continue");
        break;
    default:
        error(n, "невідомий оператор%s", "");
    }
}

static void func(Node *f)
{
    declare(f, f->name, f->type, f);
    if (f->nkids < 2) return;
    cur_func = f;
    push_scope();
    Node *params = f->kids[0];
    for (int i = 0; i < params->nkids; i++) {
        Node *p = params->kids[i];
        if (p->type == T_VOID)
            error(p, "параметр '%s' не може мати тип void", p->name);
        declare(p, p->name, p->type, NULL);
    }
    block(f->kids[1], 0);
    pop_scope();
    cur_func = NULL;
}

int sema_check(Node *root)
{
    int has_main = 0;
    errors = 0;
    nsyms = nscopes = 0;
    push_scope();
    for (int i = 0; i < root->nkids; i++) {
        Node *d = root->kids[i];
        if (d->kind == N_FUNC) {
            func(d);
            if (strcmp(d->name, "main") == 0 && d->nkids == 2) has_main = 1;
        } else {
            decl(d);
        }
    }
    if (!has_main)
        error(root, "не знайдено функцію main%s", "");
    pop_scope();
    return errors;
}
