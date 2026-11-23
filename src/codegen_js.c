#include <stdlib.h>
#include <string.h>
#include "ast.h"

static FILE *out;
static Node *program;
static int indent = 0;

enum { P_ASSIGN = 2, P_TERNARY = 3, P_OR = 4, P_AND = 5, P_EQ = 9, P_REL = 10,
       P_ADD = 12, P_MUL = 13, P_UNARY = 15, P_POSTFIX = 16, P_CALL = 18, P_PRIMARY = 20 };

static const char *reserved[] = {
    "break", "case", "catch", "class", "const", "continue", "debugger", "default",
    "delete", "do", "else", "enum", "export", "extends", "false", "finally", "for",
    "function", "if", "import", "in", "instanceof", "new", "null", "return", "super",
    "switch", "this", "throw", "true", "try", "typeof", "var", "void", "while", "with",
    "yield", "let", "static", "implements", "interface", "package", "private",
    "protected", "public", "await", "arguments", "eval", "undefined", "NaN",
    "Infinity", "Math", "Number", "Array", "Object", "String", "process", "console",
    "main_result", NULL
};

static const char *runtime =
"// згенеровано транслятором minic\n"
"let output = \"\";\n"
"\n"
"function printf(fmt, ...args) {\n"
"    let k = 0;\n"
"    const start = output.length;\n"
"    output += fmt.replace(/%([-+ 0#]*)(\\d*)(?:\\.(\\d+))?(?:hh|h|ll|l)?([diuxXcf%])/g,\n"
"        (m, flags, width, prec, conv) => {\n"
"            if (conv === \"%\") return \"%\";\n"
"            const v = args[k++];\n"
"            let s;\n"
"            if (conv === \"c\") {\n"
"                s = String.fromCharCode(v);\n"
"            } else if (conv === \"x\" || conv === \"X\") {\n"
"                s = (Math.trunc(v) >>> 0).toString(16);\n"
"                if (conv === \"X\") s = s.toUpperCase();\n"
"            } else {\n"
"                const neg = v < 0 || Object.is(v, -0);\n"
"                const a = Math.abs(v);\n"
"                if (conv === \"f\")\n"
"                    s = isFinite(a) ? a.toFixed(prec === undefined ? 6 : +prec) : (isNaN(a) ? \"nan\" : \"inf\");\n"
"                else {\n"
"                    s = String(Math.trunc(a));\n"
"                    if (prec !== undefined) s = s.padStart(+prec, \"0\");\n"
"                }\n"
"                if (neg && !(conv !== \"f\" && Math.trunc(a) === 0)) s = \"-\" + s;\n"
"                else if (flags.includes(\"+\")) s = \"+\" + s;\n"
"                else if (flags.includes(\" \")) s = \" \" + s;\n"
"            }\n"
"            const w = +width;\n"
"            if (w > s.length) {\n"
"                if (flags.includes(\"-\")) s = s.padEnd(w);\n"
"                else if (flags.includes(\"0\") && conv !== \"c\") {\n"
"                    const sign = /^[-+ ]/.test(s) ? s[0] : \"\";\n"
"                    s = sign + s.slice(sign.length).padStart(w - sign.length, \"0\");\n"
"                } else s = s.padStart(w);\n"
"            }\n"
"            return s;\n"
"        });\n"
"    return output.length - start;\n"
"}\n"
"\n";

static void gen_value(Node *n, int minp);
static void gen_stmt(Node *n);

static void name(const char *s);

static void var_name(Node *n)
{
    if (n->alias) fputs(n->alias, out);
    else name(n->name);
}

static void name(const char *s)
{
    for (int i = 0; reserved[i]; i++)
        if (strcmp(reserved[i], s) == 0) {
            fprintf(out, "%s_", s);
            return;
        }
    fputs(s, out);
}

static void tabs(void)
{
    for (int i = 0; i < indent; i++) fputs("    ", out);
}

static void js_string(const char *s)
{
    fputc('"', out);
    for (; *s; s++) {
        switch (*s) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out); break;
        case '\t': fputs("\\t", out); break;
        case '\r': fputs("\\r", out); break;
        default:
            if ((unsigned char)*s < 0x20) fprintf(out, "\\x%02x", *s);
            else fputc(*s, out);
        }
    }
    fputc('"', out);
}

static int is_bool(Node *n)
{
    if (n->kind == N_UNOP) return n->name[0] == '!';
    if (n->kind != N_BINOP) return 0;
    return !(strchr("+-*/%", n->name[0]) && n->name[1] == '\0');
}

static int binop_prec(const char *op)
{
    if (!strcmp(op, "||")) return P_OR;
    if (!strcmp(op, "&&")) return P_AND;
    if (!strcmp(op, "==") || !strcmp(op, "!=")) return P_EQ;
    if (strchr("<>", op[0])) return P_REL;
    if (strchr("+-", op[0])) return P_ADD;
    return P_MUL;
}

static void open_paren(int p, int minp) { if (p < minp) fputc('(', out); }
static void close_paren(int p, int minp) { if (p < minp) fputc(')', out); }

static void gen_cond(Node *n, int minp)
{
    if (!is_bool(n)) {
        gen_value(n, minp);
        return;
    }
    if (n->kind == N_UNOP) {
        open_paren(P_UNARY, minp);
        fputc('!', out);
        gen_cond(n->kids[0], P_UNARY);
        close_paren(P_UNARY, minp);
        return;
    }
    int p = binop_prec(n->name);
    int logic = p == P_OR || p == P_AND;
    const char *op = n->name;
    if (!strcmp(op, "==")) op = "===";
    if (!strcmp(op, "!=")) op = "!==";
    open_paren(p, minp);
    if (logic) gen_cond(n->kids[0], p);
    else gen_value(n->kids[0], p);
    fprintf(out, " %s ", op);
    if (logic) gen_cond(n->kids[1], p + 1);
    else gen_value(n->kids[1], p + 1);
    close_paren(p, minp);
}

static void gen_conv(Node *n, Type t, int minp)
{
    if (t == T_INT && n->type == T_DOUBLE) {
        fputs("Math.trunc(", out);
        gen_value(n, 0);
        fputc(')', out);
    } else {
        gen_value(n, minp);
    }
}

static Node *find_func(const char *s)
{
    Node *f = NULL;
    for (int i = 0; i < program->nkids; i++) {
        Node *d = program->kids[i];
        if (d->kind == N_FUNC && strcmp(d->name, s) == 0) f = d;
    }
    return f;
}

static void gen_call(Node *n)
{
    const char *fn = n->name;
    Node *f = NULL;
    if (!strcmp(fn, "sqrt")) fputs("Math.sqrt(", out);
    else if (!strcmp(fn, "fabs")) fputs("Math.abs(", out);
    else if (!strcmp(fn, "pow")) fputs("Math.pow(", out);
    else if (!strcmp(fn, "printf")) fputs("printf(", out);
    else {
        f = find_func(fn);
        name(fn);
        fputc('(', out);
    }
    for (int i = 0; i < n->nkids; i++) {
        if (i) fputs(", ", out);
        Node *a = n->kids[i];
        if (a->kind == N_STRING) js_string(a->name);
        else if (f) gen_conv(a, f->kids[0]->kids[i]->type, 0);
        else gen_value(a, 0);
    }
    fputc(')', out);
}

static void gen_assign(Node *n, int minp)
{
    Node *lhs = n->kids[0], *rhs = n->kids[1];
    open_paren(P_ASSIGN, minp);
    gen_value(lhs, P_ASSIGN + 1);
    if (n->name[0] == '=') {
        fputs(" = ", out);
        gen_conv(rhs, lhs->type, P_ASSIGN);
    } else if (lhs->type == T_INT && (n->name[0] == '/' || rhs->type == T_DOUBLE)) {
        fputs(" = Math.trunc(", out);
        gen_value(lhs, P_MUL);
        fprintf(out, " %c ", n->name[0]);
        gen_value(rhs, binop_prec(n->name) + 1);
        fputc(')', out);
    } else {
        fprintf(out, " %s ", n->name);
        gen_value(rhs, P_ASSIGN);
    }
    close_paren(P_ASSIGN, minp);
}

static void gen_value(Node *n, int minp)
{
    switch (n->kind) {
    case N_INT:
        fprintf(out, "%d", n->ival);
        break;
    case N_DOUBLE: {
        char buf[64];
        for (int prec = 15; prec <= 17; prec++) {
            snprintf(buf, sizeof buf, "%.*g", prec, n->dval);
            if (strtod(buf, NULL) == n->dval) break;
        }
        fputs(buf, out);
        break;
    }
    case N_STRING:
        js_string(n->name);
        break;
    case N_IDENT:
        var_name(n);
        break;
    case N_INDEX:
        var_name(n->kids[0]);
        fputc('[', out);
        gen_value(n->kids[1], 0);
        fputc(']', out);
        break;
    case N_CALL:
        gen_call(n);
        break;
    case N_CAST:
        gen_conv(n->kids[0], n->type, minp);
        break;
    case N_ASSIGN:
        gen_assign(n, minp);
        break;
    case N_PREFIX:
        open_paren(P_UNARY, minp);
        fputs(n->name, out);
        gen_value(n->kids[0], P_UNARY);
        close_paren(P_UNARY, minp);
        break;
    case N_POSTFIX:
        open_paren(P_POSTFIX, minp);
        gen_value(n->kids[0], P_POSTFIX);
        fputs(n->name, out);
        close_paren(P_POSTFIX, minp);
        break;
    case N_UNOP:
        if (n->name[0] == '!') {
            open_paren(P_TERNARY, minp);
            gen_cond(n, P_OR);
            fputs(" ? 1 : 0", out);
            close_paren(P_TERNARY, minp);
            break;
        }
        open_paren(P_UNARY, minp);
        fputc('-', out);
        gen_value(n->kids[0], n->kids[0]->kind == N_UNOP || n->kids[0]->kind == N_PREFIX
                              ? P_PRIMARY : P_UNARY);
        close_paren(P_UNARY, minp);
        break;
    case N_TERNARY:
        open_paren(P_TERNARY, minp);
        gen_cond(n->kids[0], P_OR);
        fputs(" ? ", out);
        gen_conv(n->kids[1], n->type, P_ASSIGN);
        fputs(" : ", out);
        gen_conv(n->kids[2], n->type, P_ASSIGN);
        close_paren(P_TERNARY, minp);
        break;
    case N_BINOP:
        if (is_bool(n)) {
            open_paren(P_TERNARY, minp);
            gen_cond(n, P_OR);
            fputs(" ? 1 : 0", out);
            close_paren(P_TERNARY, minp);
        } else if (n->name[0] == '/' && n->type == T_INT) {
            fputs("Math.trunc(", out);
            gen_value(n->kids[0], P_MUL);
            fputs(" / ", out);
            gen_value(n->kids[1], P_MUL + 1);
            fputc(')', out);
        } else {
            int p = binop_prec(n->name);
            open_paren(p, minp);
            gen_value(n->kids[0], p);
            fprintf(out, " %s ", n->name);
            gen_value(n->kids[1], p + 1);
            close_paren(p, minp);
        }
        break;
    default:
        fputs("undefined", out);
    }
}

static void gen_vars(Node *d, int with_let)
{
    if (with_let) fputs("let ", out);
    for (int i = 0; i < d->nkids; i++) {
        Node *v = d->kids[i];
        if (i) fputs(", ", out);
        var_name(v);
        fputs(" = ", out);
        if (!v->is_array) {
            if (v->nkids) gen_conv(v->kids[0], v->type, P_ASSIGN);
            else fputc('0', out);
            continue;
        }
        if (v->nkids == 0) {
            fprintf(out, "new Array(%d).fill(0)", v->ival);
            continue;
        }
        Node *init = v->kids[0];
        fputc('[', out);
        for (int j = 0; j < init->nkids; j++) {
            if (j) fputs(", ", out);
            gen_conv(init->kids[j], elem_type(v->type), P_ASSIGN);
        }
        if (v->ival > init->nkids)
            fprintf(out, ", ...new Array(%d).fill(0)", v->ival - init->nkids);
        fputc(']', out);
    }
}

static void gen_body(Node *n)
{
    fputs(" {\n", out);
    indent++;
    if (n->kind == N_BLOCK) {
        for (int i = 0; i < n->nkids; i++) gen_stmt(n->kids[i]);
    } else {
        gen_stmt(n);
    }
    indent--;
    tabs();
    fputc('}', out);
}

static void gen_if(Node *n)
{
    fputs("if (", out);
    gen_cond(n->kids[0], 0);
    fputc(')', out);
    gen_body(n->kids[1]);
    if (n->nkids == 3) {
        if (n->kids[2]->kind == N_IF) {
            fputs(" else ", out);
            gen_if(n->kids[2]);
            return;
        }
        fputs(" else", out);
        gen_body(n->kids[2]);
    }
}

static void gen_stmt(Node *n)
{
    switch (n->kind) {
    case N_EMPTY:
        return;
    case N_DECL:
        tabs();
        gen_vars(n, 1);
        fputs(";\n", out);
        return;
    case N_BLOCK:
        tabs();
        fputc('{', out);
        fputc('\n', out);
        indent++;
        for (int i = 0; i < n->nkids; i++) gen_stmt(n->kids[i]);
        indent--;
        tabs();
        fputs("}\n", out);
        return;
    case N_EXPR_STMT:
        tabs();
        gen_value(n->kids[0], 0);
        fputs(";\n", out);
        return;
    case N_IF:
        tabs();
        gen_if(n);
        fputc('\n', out);
        return;
    case N_WHILE:
        tabs();
        fputs("while (", out);
        gen_cond(n->kids[0], 0);
        fputc(')', out);
        gen_body(n->kids[1]);
        fputc('\n', out);
        return;
    case N_DO:
        tabs();
        fputs("do", out);
        gen_body(n->kids[0]);
        fputs(" while (", out);
        gen_cond(n->kids[1], 0);
        fputs(");\n", out);
        return;
    case N_FOR:
        tabs();
        fputs("for (", out);
        if (n->kids[0]->kind == N_DECL) gen_vars(n->kids[0], 1);
        else if (n->kids[0]->kind == N_EXPR_STMT) gen_value(n->kids[0]->kids[0], 0);
        fputs("; ", out);
        if (n->kids[1]->kind != N_EMPTY) gen_cond(n->kids[1], 0);
        fputs("; ", out);
        if (n->kids[2]->kind != N_EMPTY) gen_value(n->kids[2], 0);
        fputc(')', out);
        gen_body(n->kids[3]);
        fputc('\n', out);
        return;
    case N_RETURN:
        tabs();
        fputs("return", out);
        if (n->nkids) {
            fputc(' ', out);
            gen_conv(n->kids[0], n->type, 0);
        }
        fputs(";\n", out);
        return;
    case N_BREAK:
        tabs();
        fputs("break;\n", out);
        return;
    case N_CONTINUE:
        tabs();
        fputs("continue;\n", out);
        return;
    default:
        return;
    }
}

static void gen_func(Node *f)
{
    Node *params = f->kids[0];
    fputs("function ", out);
    name(f->name);
    fputc('(', out);
    for (int i = 0; i < params->nkids; i++) {
        if (i) fputs(", ", out);
        var_name(params->kids[i]);
    }
    fputs(") {\n", out);
    indent++;
    Node *body = f->kids[1];
    for (int i = 0; i < body->nkids; i++) gen_stmt(body->kids[i]);
    indent--;
    fputs("}\n\n", out);
}

void gen_js(FILE *f, Node *root)
{
    out = f;
    program = root;
    fputs(runtime, out);
    for (int i = 0; i < root->nkids; i++) {
        Node *d = root->kids[i];
        if (d->kind == N_DECL) gen_stmt(d);
    }
    fputc('\n', out);
    for (int i = 0; i < root->nkids; i++) {
        Node *d = root->kids[i];
        if (d->kind == N_FUNC && d->nkids == 2) gen_func(d);
    }
    fputs("const main_result = main();\n", out);
    fputs("if (typeof process !== \"undefined\") {\n", out);
    fputs("    process.stdout.write(output);\n", out);
    fputs("    process.exitCode = main_result | 0;\n", out);
    fputs("} else {\n", out);
    fputs("    console.log(output);\n", out);
    fputs("}\n", out);
}
