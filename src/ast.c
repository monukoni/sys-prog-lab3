#include <stdlib.h>
#include <string.h>
#include "ast.h"

static const char *kind_names[] = {
    "Program", "Func", "Params", "Param", "Decl", "Var", "InitList",
    "Block", "If", "While", "DoWhile", "For", "Return", "Break", "Continue",
    "ExprStmt", "Empty",
    "Assign", "BinOp", "UnOp", "Prefix", "Postfix", "Ternary", "Cast",
    "Call", "Index", "Ident", "Int", "Double", "String"
};

static const char *type_names[] = {
    "", "void", "int", "double", "string", "int[]", "double[]"
};

const char *kind_name(NodeKind k) { return kind_names[k]; }
const char *type_name(Type t) { return type_names[t]; }

Type elem_type(Type t)
{
    if (t == T_INT_ARR) return T_INT;
    if (t == T_DOUBLE_ARR) return T_DOUBLE;
    return t;
}

Type arr_type(Type t)
{
    return t == T_DOUBLE ? T_DOUBLE_ARR : T_INT_ARR;
}

Node *node_new(NodeKind kind, int line)
{
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->line = line;
    return n;
}

Node *node_add(Node *n, Node *kid)
{
    n->kids = realloc(n->kids, (n->nkids + 1) * sizeof(Node *));
    n->kids[n->nkids++] = kid;
    return n;
}

Node *node_op(NodeKind kind, int line, const char *op, Node *a, Node *b)
{
    Node *n = node_new(kind, line);
    n->name = op ? strdup(op) : NULL;
    if (a) node_add(n, a);
    if (b) node_add(n, b);
    return n;
}

Node *node_name(NodeKind kind, int line, const char *name)
{
    Node *n = node_new(kind, line);
    n->name = strdup(name);
    return n;
}

static void label(Node *n, char *buf, size_t size)
{
    switch (n->kind) {
    case N_INT:
        snprintf(buf, size, "%d", n->ival);
        break;
    case N_DOUBLE:
        snprintf(buf, size, "%g", n->dval);
        break;
    case N_STRING:
        snprintf(buf, size, "\"%s\"", n->name);
        break;
    case N_VAR:
    case N_PARAM:
        if (n->is_array && n->kind == N_PARAM)
            snprintf(buf, size, "Param %s[]", n->name);
        else if (n->is_array)
            snprintf(buf, size, "%s %s[%d]", kind_name(n->kind), n->name, n->ival);
        else
            snprintf(buf, size, "%s %s", kind_name(n->kind), n->name);
        break;
    case N_FUNC:
        snprintf(buf, size, "Func %s %s", type_name(n->type), n->name);
        break;
    case N_DECL:
        snprintf(buf, size, "Decl %s", type_name(n->type));
        break;
    case N_CAST:
        snprintf(buf, size, "Cast (%s)", type_name(n->type));
        break;
    default:
        if (n->name)
            snprintf(buf, size, "%s %s", kind_name(n->kind), n->name);
        else
            snprintf(buf, size, "%s", kind_name(n->kind));
    }
}

void ast_print(FILE *f, Node *n, int depth)
{
    char buf[256];
    if (!n) return;
    label(n, buf, sizeof buf);
    fprintf(f, "%*s%s", depth * 2, "", buf);
    if (n->type != T_NONE && n->kind != N_FUNC && n->kind != N_DECL && n->kind != N_CAST)
        fprintf(f, " : %s", type_name(n->type));
    fprintf(f, "  (line %d)\n", n->line);
    for (int i = 0; i < n->nkids; i++)
        ast_print(f, n->kids[i], depth + 1);
}

static void json_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        switch (*s) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f); break;
        case '\t': fputs("\\t", f); break;
        case '<':  fputs("\\u003c", f); break;
        default:
            if ((unsigned char)*s < 0x20) fprintf(f, "\\u%04x", *s);
            else fputc(*s, f);
        }
    }
    fputc('"', f);
}

void ast_json(FILE *f, Node *n)
{
    char buf[256];
    label(n, buf, sizeof buf);
    fprintf(f, "{\"name\":");
    json_str(f, buf);
    fprintf(f, ",\"kind\":\"%s\",\"line\":%d,\"type\":\"%s\"",
            kind_name(n->kind), n->line, type_name(n->type));
    if (n->nkids > 0) {
        fprintf(f, ",\"children\":[");
        for (int i = 0; i < n->nkids; i++) {
            if (i) fputc(',', f);
            ast_json(f, n->kids[i]);
        }
        fputc(']', f);
    }
    fputc('}', f);
}

static int dot_node(FILE *f, Node *n, int *counter)
{
    char buf[256];
    int id = (*counter)++;
    label(n, buf, sizeof buf);
    fprintf(f, "  n%d [label=", id);
    json_str(f, buf);
    fprintf(f, "];\n");
    for (int i = 0; i < n->nkids; i++) {
        int kid = dot_node(f, n->kids[i], counter);
        fprintf(f, "  n%d -> n%d;\n", id, kid);
    }
    return id;
}

void ast_dot(FILE *f, Node *root)
{
    int counter = 0;
    fprintf(f, "digraph AST {\n  node [shape=box, fontname=\"Consolas\"];\n");
    dot_node(f, root, &counter);
    fprintf(f, "}\n");
}

static const char *html_head =
"<!DOCTYPE html>\n<html lang=\"uk\">\n<head>\n<meta charset=\"utf-8\">\n"
"<title>AST</title>\n"
"<script src=\"https://cdnjs.cloudflare.com/ajax/libs/d3/7.8.5/d3.min.js\"></script>\n"
"<style>\n"
"body { margin: 0; font-family: Consolas, monospace; background: #fafafa; }\n"
"header { padding: 8px 16px; background: #263238; color: #fff; }\n"
"header span { color: #b0bec5; margin-left: 16px; font-size: 13px; }\n"
"#tree { width: 100vw; height: calc(100vh - 40px); }\n"
".node circle { stroke-width: 2px; cursor: pointer; }\n"
".node text { font-size: 12px; paint-order: stroke; stroke: #fafafa; stroke-width: 3px; }\n"
".link { fill: none; stroke: #b0bec5; stroke-width: 1.5px; }\n"
"#tip { position: absolute; background: #fff; border: 1px solid #999; padding: 4px 8px;\n"
"       font-size: 12px; pointer-events: none; display: none; }\n"
"</style>\n</head>\n<body>\n";

static const char *html_script =
"<div id=\"tree\"></div>\n<div id=\"tip\"></div>\n<script>\n"
"const colors = { stmt: '#1e88e5', expr: '#43a047', leaf: '#fb8c00', decl: '#8e24aa' };\n"
"function group(k) {\n"
"  if (['Ident','Int','Double','String'].includes(k)) return 'leaf';\n"
"  if (['Program','Func','Params','Param','Decl','Var','InitList'].includes(k)) return 'decl';\n"
"  if (['Assign','BinOp','UnOp','Prefix','Postfix','Ternary','Cast','Call','Index'].includes(k)) return 'expr';\n"
"  return 'stmt';\n"
"}\n"
"const root = d3.hierarchy(data);\n"
"const dx = 22, dy = 170;\n"
"const tree = d3.tree().nodeSize([dx, dy]);\n"
"const svg = d3.select('#tree').append('svg').attr('width', '100%').attr('height', '100%');\n"
"const g = svg.append('g');\n"
"const zoom = d3.zoom().scaleExtent([0.1, 3]).on('zoom', e => g.attr('transform', e.transform));\n"
"svg.call(zoom).call(zoom.transform, d3.zoomIdentity.translate(40, window.innerHeight / 2));\n"
"const gLink = g.append('g'), gNode = g.append('g');\n"
"const tip = d3.select('#tip');\n"
"root.descendants().forEach((d, i) => {\n"
"  d.id = i;\n"
"  d._children = d.children;\n"
"  if (d.depth >= 4) d.children = null;\n"
"});\n"
"function update(src) {\n"
"  tree(root);\n"
"  const nodes = root.descendants(), links = root.links();\n"
"  const t = svg.transition().duration(300);\n"
"  const node = gNode.selectAll('g').data(nodes, d => d.id);\n"
"  const enter = node.enter().append('g').attr('class', 'node')\n"
"    .attr('transform', `translate(${src.y0 || 0},${src.x0 || 0})`)\n"
"    .on('click', (e, d) => { d.children = d.children ? null : d._children; update(d); })\n"
"    .on('mousemove', (e, d) => tip.style('display', 'block')\n"
"        .style('left', (e.pageX + 12) + 'px').style('top', (e.pageY + 12) + 'px')\n"
"        .html(`${d.data.kind}<br>рядок ${d.data.line}` + (d.data.type ? `<br>тип ${d.data.type}` : '')))\n"
"    .on('mouseout', () => tip.style('display', 'none'));\n"
"  enter.append('circle').attr('r', 5)\n"
"    .attr('stroke', d => colors[group(d.data.kind)]);\n"
"  enter.append('text').attr('dy', '0.32em').attr('x', 9).text(d => d.data.name);\n"
"  node.merge(enter).transition(t).attr('transform', d => `translate(${d.y},${d.x})`)\n"
"    .select('circle').attr('fill', d => d._children && !d.children ? colors[group(d.data.kind)] : '#fff');\n"
"  node.exit().transition(t).remove().attr('transform', `translate(${src.y},${src.x})`);\n"
"  const link = gLink.selectAll('path').data(links, d => d.target.id);\n"
"  const diag = d3.linkHorizontal().x(d => d.y).y(d => d.x);\n"
"  link.enter().append('path').attr('class', 'link')\n"
"    .attr('d', () => { const o = { x: src.x0 || 0, y: src.y0 || 0 }; return diag({ source: o, target: o }); })\n"
"    .merge(link).transition(t).attr('d', diag);\n"
"  link.exit().transition(t).remove();\n"
"  root.eachBefore(d => { d.x0 = d.x; d.y0 = d.y; });\n"
"}\n"
"update(root);\n"
"</script>\n</body>\n</html>\n";

void ast_html(FILE *f, Node *root, const char *title)
{
    fputs(html_head, f);
    fprintf(f, "<header>AST: ");
    for (const char *p = title; *p; p++) {
        if (*p == '<') fputs("&lt;", f);
        else if (*p == '&') fputs("&amp;", f);
        else fputc(*p, f);
    }
    fprintf(f, "<span>клік по вузлу згортає/розгортає, колесо миші масштабує</span></header>\n");
    fprintf(f, "<script>\nconst data = ");
    ast_json(f, root);
    fprintf(f, ";\n</script>\n");
    fputs(html_script, f);
}
