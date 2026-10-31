/* ============================================================================
 * gmb_tui_pretty.c — gmb_tui_pretty. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Pretty ===================== */

gmb_doc_t* gmb_doc_nil(void)
{
    gmb_doc_t* d = (gmb_doc_t*)calloc(1, sizeof(gmb_doc_t));
    d->kind = GMB_DOC_NIL;
    return d;
}
gmb_doc_t* gmb_doc_text(const char* s)
{
    gmb_doc_t* d = (gmb_doc_t*)calloc(1, sizeof(gmb_doc_t));
    d->kind = GMB_DOC_TEXT;
    snprintf(d->text, sizeof(d->text), "%s", s ? s : "");
    return d;
}
gmb_doc_t* gmb_doc_line(void)
{
    gmb_doc_t* d = (gmb_doc_t*)calloc(1, sizeof(gmb_doc_t));
    d->kind = GMB_DOC_LINE;
    snprintf(d->text, sizeof(d->text), " ");
    return d;
}
gmb_doc_t* gmb_doc_line_empty(void)
{
    gmb_doc_t* d = (gmb_doc_t*)calloc(1, sizeof(gmb_doc_t));
    d->kind = GMB_DOC_LINE;
    return d;
}
gmb_doc_t* gmb_doc_hard_line(void)
{
    gmb_doc_t* d = (gmb_doc_t*)calloc(1, sizeof(gmb_doc_t));
    d->kind = GMB_DOC_HARDLINE;
    return d;
}
gmb_doc_t* gmb_doc_nest(int indent, gmb_doc_t* doc)
{
    gmb_doc_t* d = (gmb_doc_t*)calloc(1, sizeof(gmb_doc_t));
    d->kind = GMB_DOC_NEST;
    d->indent = indent;
    d->left = doc;
    return d;
}
gmb_doc_t* gmb_doc_concat(gmb_doc_t* a, gmb_doc_t* b)
{
    gmb_doc_t* d = (gmb_doc_t*)calloc(1, sizeof(gmb_doc_t));
    d->kind = GMB_DOC_CONCAT;
    d->left = a;
    d->right = b;
    return d;
}
gmb_doc_t* gmb_doc_group(gmb_doc_t* doc)
{
    gmb_doc_t* d = (gmb_doc_t*)calloc(1, sizeof(gmb_doc_t));
    d->kind = GMB_DOC_GROUP;
    d->left = doc;
    return d;
}
gmb_doc_t* gmb_doc_append(gmb_doc_t* a, gmb_doc_t* b)
{
    if (a->kind == GMB_DOC_NIL) return b;
    if (b->kind == GMB_DOC_NIL) return a;
    return gmb_doc_concat(a, b);
}
gmb_doc_t* gmb_doc_space(void) { return gmb_doc_text(" "); }
gmb_doc_t* gmb_doc_soft_line(void) { return gmb_doc_group(gmb_doc_line()); }

enum { DOC_MODE_FLAT = 0, DOC_MODE_BREAK };

typedef struct doc_frame { int indent; int mode; gmb_doc_t* doc; } doc_frame_t;

#define DOC_STACK_MAX 256

static bool doc_fits(int w, int indent, gmb_doc_t* first, doc_frame_t* stack, int* si)
{
    doc_frame_t work[DOC_STACK_MAX];
    int wn = 0;
    work[wn].indent = indent;
    work[wn].mode = DOC_MODE_FLAT;
    work[wn].doc = first;
    wn++;
    int si2 = *si;
    while (true)
    {
        if (w < 0) return false;
        doc_frame_t item;
        if (wn > 0)
        {
            wn--;
            item = work[wn];
        }
        else if (si2 > 0)
        {
            si2--;
            item = stack[si2];
        }
        else
        {
            return true;
        }
        gmb_doc_t* doc = item.doc;
        if (!doc) return true;
        switch (doc->kind)
        {
            case GMB_DOC_NIL: break;
            case GMB_DOC_TEXT: w -= (int)strlen(doc->text); break;
            case GMB_DOC_CONCAT:
                if (wn + 2 <= DOC_STACK_MAX) { work[wn++].doc = doc->right; work[wn++].doc = doc->left; }
                break;
            case GMB_DOC_NEST:
                if (wn + 1 <= DOC_STACK_MAX) { work[wn].indent = item.indent + doc->indent; work[wn].mode = item.mode; work[wn].doc = doc->left; wn++; }
                break;
            case GMB_DOC_LINE:
                if (item.mode == DOC_MODE_FLAT) w -= (int)strlen(doc->text);
                else return true;
                break;
            case GMB_DOC_HARDLINE: return true;
            case GMB_DOC_GROUP:
                if (wn + 1 <= DOC_STACK_MAX) { work[wn].indent = item.indent; work[wn].mode = DOC_MODE_FLAT; work[wn].doc = doc->left; wn++; }
                break;
        }
    }
}

int gmb_doc_render_string(gmb_doc_t* doc, int width, char* out, int cap)
{
    doc_frame_t stack[DOC_STACK_MAX];
    int sn = 0;
    stack[sn].indent = 0;
    stack[sn].mode = DOC_MODE_BREAK;
    stack[sn].doc = doc;
    sn++;
    int col = 0;
    int o = 0;
    while (sn > 0)
    {
        sn--;
        doc_frame_t frame = stack[sn];
        gmb_doc_t* d = frame.doc;
        if (!d) continue;
        switch (d->kind)
        {
            case GMB_DOC_NIL: break;
            case GMB_DOC_TEXT:
                for (char* p = d->text; *p && o < cap - 1; p++) out[o++] = *p;
                col += (int)strlen(d->text);
                break;
            case GMB_DOC_CONCAT:
                if (sn + 2 <= DOC_STACK_MAX)
                {
                    stack[sn].indent = frame.indent; stack[sn].mode = frame.mode; stack[sn].doc = d->right; sn++;
                    stack[sn].indent = frame.indent; stack[sn].mode = frame.mode; stack[sn].doc = d->left; sn++;
                }
                break;
            case GMB_DOC_NEST:
                if (sn + 1 <= DOC_STACK_MAX) { stack[sn].indent = frame.indent + d->indent; stack[sn].mode = frame.mode; stack[sn].doc = d->left; sn++; }
                break;
            case GMB_DOC_LINE:
                if (frame.mode == DOC_MODE_FLAT)
                {
                    for (char* p = d->text; *p && o < cap - 1; p++) out[o++] = *p;
                    col += (int)strlen(d->text);
                }
                else
                {
                    if (o < cap - 1) out[o++] = '\n';
                    int ind = frame.indent > 0 ? frame.indent : 0;
                    for (int i = 0; i < ind && o < cap - 1; i++) out[o++] = ' ';
                    col = ind;
                }
                break;
            case GMB_DOC_HARDLINE:
                if (o < cap - 1) out[o++] = '\n';
                {
                    int ind = frame.indent > 0 ? frame.indent : 0;
                    for (int i = 0; i < ind && o < cap - 1; i++) out[o++] = ' ';
                    col = ind;
                }
                break;
            case GMB_DOC_GROUP:
            {
                bool flat_ok = frame.mode == DOC_MODE_FLAT || doc_fits(width - col, frame.indent, d->left, stack, &sn);
                int m = flat_ok ? DOC_MODE_FLAT : DOC_MODE_BREAK;
                if (sn + 1 <= DOC_STACK_MAX) { stack[sn].indent = frame.indent; stack[sn].mode = m; stack[sn].doc = d->left; sn++; }
                break;
            }
        }
    }
    out[o] = '\0';
    return o;
}

void gmb_doc_free(gmb_doc_t* doc)
{
    if (!doc) return;
    if (doc->left) gmb_doc_free(doc->left);
    if (doc->right) gmb_doc_free(doc->right);
    free(doc);
}
