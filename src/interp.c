#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ast.h"

typedef struct {
    Type elem;
    int n;
    int *iv;
    double *dv;
} Array;

typedef struct {
    Type t;
    int i;
    double d;
    Array *a;
} Value;

typedef struct {
    const char *name;
    Value v;
    int owner;
} Var;

typedef struct {
    Type t;
    int *ip;
    double *dp;
} Ref;

typedef enum { EX_NORMAL, EX_BREAK, EX_CONTINUE, EX_RETURN } Exec;

#define MAX_VARS 100000
#define MAX_DEPTH 10000

static Var vars[MAX_VARS];
static int nvars = 0;
static int nglobals = 0;
static int frame_base = 0;
static int depth = 0;
static Value ret_val;
static Node *program;

static void runtime_error(Node *n, const char *msg)
{
    fflush(stdout);
    fprintf(stderr, "рядок %d: помилка виконання: %s\n", n->line, msg);
    exit(1);
}

static Value int_val(int i) { Value v = { T_INT, i, 0, NULL }; return v; }
static Value dbl_val(double d) { Value v = { T_DOUBLE, 0, d, NULL }; return v; }

static double as_double(Value v) { return v.t == T_DOUBLE ? v.d : v.i; }
static int as_int(Value v) { return v.t == T_DOUBLE ? (int)v.d : v.i; }
static int truth(Value v) { return v.t == T_DOUBLE ? v.d != 0 : v.i != 0; }

static Value convert(Value v, Type t)
{
    if (t == T_INT) return int_val(as_int(v));
    if (t == T_DOUBLE) return dbl_val(as_double(v));
    return v;
}

static Var *find(Node *n, const char *name)
{
    for (int i = nvars - 1; i >= frame_base; i--)
        if (strcmp(vars[i].name, name) == 0) return &vars[i];
    for (int i = nglobals - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) return &vars[i];
    runtime_error(n, "невідома змінна");
    return NULL;
}

static void push_var(Node *n, const char *name, Value v, int owner)
{
    if (nvars == MAX_VARS) runtime_error(n, "переповнення стеку змінних");
    vars[nvars].name = name;
    vars[nvars].v = v;
    vars[nvars].owner = owner;
    nvars++;
}

static void pop_vars(int to)
{
    while (nvars > to) {
        nvars--;
        if (vars[nvars].owner) {
            Array *a = vars[nvars].v.a;
            free(a->iv);
            free(a->dv);
            free(a);
        }
    }
}

static Array *new_array(Type elem, int n)
{
    Array *a = calloc(1, sizeof(Array));
    a->elem = elem;
    a->n = n;
    if (elem == T_INT) a->iv = calloc(n, sizeof(int));
    else a->dv = calloc(n, sizeof(double));
    return a;
}

static Value eval(Node *n);
static Exec exec(Node *n);

static Ref ref(Node *n)
{
    Ref r = { T_NONE, NULL, NULL };
    if (n->kind == N_IDENT) {
        Var *v = find(n, n->name);
        r.t = v->v.t;
        if (r.t == T_INT) r.ip = &v->v.i;
        else r.dp = &v->v.d;
    } else {
        Var *v = find(n->kids[0], n->kids[0]->name);
        int i = as_int(eval(n->kids[1]));
        Array *a = v->v.a;
        if (i < 0 || i >= a->n) runtime_error(n, "індекс за межами масиву");
        r.t = a->elem;
        if (r.t == T_INT) r.ip = &a->iv[i];
        else r.dp = &a->dv[i];
    }
    return r;
}

static Value load(Ref r) { return r.t == T_INT ? int_val(*r.ip) : dbl_val(*r.dp); }

static void store(Ref r, Value v)
{
    if (r.t == T_INT) *r.ip = as_int(v);
    else *r.dp = as_double(v);
}

static Value arith(Node *n, const char *op, Value a, Value b)
{
    if (a.t == T_INT && b.t == T_INT) {
        int x = a.i, y = b.i;
        switch (op[0]) {
        case '+': return int_val(x + y);
        case '-': return int_val(x - y);
        case '*': return int_val(x * y);
        case '/':
            if (y == 0) runtime_error(n, "ділення на нуль");
            return int_val(x / y);
        case '%':
            if (y == 0) runtime_error(n, "ділення на нуль");
            return int_val(x % y);
        }
    }
    double x = as_double(a), y = as_double(b);
    switch (op[0]) {
    case '+': return dbl_val(x + y);
    case '-': return dbl_val(x - y);
    case '*': return dbl_val(x * y);
    case '/': return dbl_val(x / y);
    }
    runtime_error(n, "невідома операція");
    return int_val(0);
}

static int compare(const char *op, Value a, Value b)
{
    double x = as_double(a), y = as_double(b);
    if (a.t == T_INT && b.t == T_INT) { x = a.i; y = b.i; }
    if (strcmp(op, "==") == 0) return x == y;
    if (strcmp(op, "!=") == 0) return x != y;
    if (strcmp(op, "<") == 0) return x < y;
    if (strcmp(op, ">") == 0) return x > y;
    if (strcmp(op, "<=") == 0) return x <= y;
    return x >= y;
}

static int do_printf(Node *n)
{
    const char *f = n->kids[0]->name;
    int arg = 1, count = 0;
    char spec[32];

    while (*f) {
        if (*f != '%') {
            putchar(*f++);
            count++;
            continue;
        }
        int len = 0;
        spec[len++] = *f++;
        while (*f && !strchr("diouxXcsfFeEgGaA%", *f) && len < 30)
            spec[len++] = *f++;
        if (!*f) break;
        char conv = *f++;
        spec[len++] = conv;
        spec[len] = '\0';
        if (conv == '%') {
            putchar('%');
            count++;
            continue;
        }
        if (arg >= n->nkids) runtime_error(n, "printf: замало аргументів");
        Value v = eval(n->kids[arg++]);
        if (strchr("fFeEgGaA", conv))
            count += printf(spec, as_double(v));
        else if (conv == 's')
            runtime_error(n, "printf: %s не підтримується");
        else
            count += printf(spec, as_int(v));
    }
    return count;
}

static Value call(Node *n)
{
    if (strcmp(n->name, "printf") == 0) return int_val(do_printf(n));
    if (strcmp(n->name, "sqrt") == 0) return dbl_val(sqrt(as_double(eval(n->kids[0]))));
    if (strcmp(n->name, "fabs") == 0) return dbl_val(fabs(as_double(eval(n->kids[0]))));
    if (strcmp(n->name, "pow") == 0) {
        double x = as_double(eval(n->kids[0]));
        double y = as_double(eval(n->kids[1]));
        return dbl_val(pow(x, y));
    }

    Node *f = NULL;
    for (int i = 0; i < program->nkids; i++) {
        Node *d = program->kids[i];
        if (d->kind == N_FUNC && d->nkids == 2 && strcmp(d->name, n->name) == 0) f = d;
    }
    if (!f) runtime_error(n, "функція без тіла");
    if (depth >= MAX_DEPTH) runtime_error(n, "занадто глибока рекурсія");

    Node *params = f->kids[0];
    Value args[64];
    if (n->nkids > 64) runtime_error(n, "забагато аргументів");
    for (int i = 0; i < n->nkids; i++)
        args[i] = convert(eval(n->kids[i]), params->kids[i]->type);

    int old_base = frame_base, old_top = nvars;
    frame_base = nvars;
    depth++;
    for (int i = 0; i < n->nkids; i++)
        push_var(n, params->kids[i]->name, args[i], 0);

    ret_val = int_val(0);
    exec(f->kids[1]);
    Value res = convert(ret_val, f->type);

    pop_vars(old_top);
    frame_base = old_base;
    depth--;
    return res;
}

static Value eval(Node *n)
{
    Value a, b;
    Ref r;

    switch (n->kind) {
    case N_INT:
        return int_val(n->ival);
    case N_DOUBLE:
        return dbl_val(n->dval);
    case N_IDENT:
        return find(n, n->name)->v;
    case N_INDEX:
        return load(ref(n));
    case N_CALL:
        return call(n);
    case N_CAST:
        return convert(eval(n->kids[0]), n->type);
    case N_ASSIGN:
        r = ref(n->kids[0]);
        b = eval(n->kids[1]);
        if (n->name[0] != '=')
            b = arith(n, n->name, load(r), b);
        store(r, b);
        return load(r);
    case N_PREFIX:
    case N_POSTFIX:
        r = ref(n->kids[0]);
        a = load(r);
        store(r, arith(n, n->name[0] == '+' ? "+" : "-", a, int_val(1)));
        return n->kind == N_PREFIX ? load(r) : a;
    case N_UNOP:
        a = eval(n->kids[0]);
        if (n->name[0] == '!') return int_val(!truth(a));
        return a.t == T_INT ? int_val(-a.i) : dbl_val(-a.d);
    case N_TERNARY:
        a = truth(eval(n->kids[0])) ? eval(n->kids[1]) : eval(n->kids[2]);
        return convert(a, n->type);
    case N_BINOP:
        if (strcmp(n->name, "&&") == 0)
            return int_val(truth(eval(n->kids[0])) && truth(eval(n->kids[1])));
        if (strcmp(n->name, "||") == 0)
            return int_val(truth(eval(n->kids[0])) || truth(eval(n->kids[1])));
        a = eval(n->kids[0]);
        b = eval(n->kids[1]);
        if (strchr("+-*/%", n->name[0]) && n->name[1] == '\0')
            return arith(n, n->name, a, b);
        return int_val(compare(n->name, a, b));
    default:
        runtime_error(n, "очікувався вираз");
        return int_val(0);
    }
}

static void decl(Node *d)
{
    for (int i = 0; i < d->nkids; i++) {
        Node *v = d->kids[i];
        if (v->is_array) {
            Array *a = new_array(elem_type(v->type), v->ival);
            if (v->nkids > 0) {
                Node *init = v->kids[0];
                for (int j = 0; j < init->nkids; j++) {
                    Value x = eval(init->kids[j]);
                    if (a->elem == T_INT) a->iv[j] = as_int(x);
                    else a->dv[j] = as_double(x);
                }
            }
            Value val = { v->type, 0, 0, a };
            push_var(v, v->name, val, 1);
        } else {
            Value val = v->nkids > 0 ? eval(v->kids[0]) : int_val(0);
            push_var(v, v->name, convert(val, v->type), 0);
        }
    }
}

static Exec exec(Node *n)
{
    Exec e;
    int top;

    switch (n->kind) {
    case N_DECL:
        decl(n);
        return EX_NORMAL;
    case N_BLOCK:
        top = nvars;
        for (int i = 0; i < n->nkids; i++) {
            e = exec(n->kids[i]);
            if (e != EX_NORMAL) {
                pop_vars(top);
                return e;
            }
        }
        pop_vars(top);
        return EX_NORMAL;
    case N_EXPR_STMT:
        eval(n->kids[0]);
        return EX_NORMAL;
    case N_EMPTY:
        return EX_NORMAL;
    case N_IF:
        if (truth(eval(n->kids[0])))
            return exec(n->kids[1]);
        if (n->nkids == 3)
            return exec(n->kids[2]);
        return EX_NORMAL;
    case N_WHILE:
        while (truth(eval(n->kids[0]))) {
            e = exec(n->kids[1]);
            if (e == EX_BREAK) break;
            if (e == EX_RETURN) return e;
        }
        return EX_NORMAL;
    case N_DO:
        do {
            e = exec(n->kids[0]);
            if (e == EX_BREAK) break;
            if (e == EX_RETURN) return e;
        } while (truth(eval(n->kids[1])));
        return EX_NORMAL;
    case N_FOR:
        top = nvars;
        exec(n->kids[0]);
        while (n->kids[1]->kind == N_EMPTY || truth(eval(n->kids[1]))) {
            e = exec(n->kids[3]);
            if (e == EX_BREAK) break;
            if (e == EX_RETURN) {
                pop_vars(top);
                return e;
            }
            if (n->kids[2]->kind != N_EMPTY) eval(n->kids[2]);
        }
        pop_vars(top);
        return EX_NORMAL;
    case N_RETURN:
        ret_val = n->nkids ? eval(n->kids[0]) : int_val(0);
        return EX_RETURN;
    case N_BREAK:
        return EX_BREAK;
    case N_CONTINUE:
        return EX_CONTINUE;
    default:
        runtime_error(n, "невідомий оператор");
        return EX_NORMAL;
    }
}

int interp_run(Node *root)
{
    program = root;
    for (int i = 0; i < root->nkids; i++)
        if (root->kids[i]->kind == N_DECL) decl(root->kids[i]);
    nglobals = nvars;
    frame_base = nvars;

    Node main_call = { 0 };
    main_call.kind = N_CALL;
    main_call.name = "main";
    Value res = call(&main_call);
    fflush(stdout);
    return res.i;
}
