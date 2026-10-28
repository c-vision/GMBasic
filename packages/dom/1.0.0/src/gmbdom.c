/* gmbdom.c — shim FFI del pacchetto `dom` (namespace DOM) su libxml2.
 *
 * API (dom.inc): AVAILABLE / PARSE / TEXT / SELECT / NUMNODES / NODEATTR / FREENODESET / FREE.
 * Deterministico: parse di HTML in memoria, nessuna rete.
 */
#include "gmbdom.h"

#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <stdlib.h>
#include <string.h>

#define DOM_MAX_DOCS 64
#define DOM_MAX_SETS 64

static htmlDocPtr g_docs[DOM_MAX_DOCS];
static xmlXPathObjectPtr g_sets[DOM_MAX_SETS];

int gmbdom_available(void)
{
    return 1;   /* libxml2 linkata nello shim */
}

int gmbdom_parse(const char* html)
{
    if (!html) return 0;
    htmlDocPtr d = htmlReadMemory(html, (int)strlen(html), "inline", NULL,
                                  HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!d) return 0;
    for (int i = 0; i < DOM_MAX_DOCS; i++) {
        if (!g_docs[i]) { g_docs[i] = d; return i + 1; }
    }
    xmlFreeDoc(d);
    return 0;
}

static xmlDocPtr doc_of(int h)
{
    if (h <= 0 || h > DOM_MAX_DOCS) return NULL;
    return g_docs[h - 1];
}

static xmlXPathObjectPtr xpath_eval(xmlDocPtr d, const char* expr)
{
    if (!d || !expr) return NULL;
    xmlXPathContextPtr ctx = xmlXPathNewContext(d);
    if (!ctx) return NULL;
    xmlXPathObjectPtr r = xmlXPathEvalExpression((const xmlChar*)expr, ctx);
    xmlXPathFreeContext(ctx);
    return r;
}

/* PRIMO nodo del risultato XPath come stringa (contenuto testuale o valore). */
char* gmbdom_text(int h, const char* expr)
{
    xmlDocPtr d = doc_of(h);
    xmlXPathObjectPtr o = xpath_eval(d, expr);
    if (!o) return strdup("");
    char* out = NULL;
    if (o->type == XPATH_NODESET && o->nodesetval && o->nodesetval->nodeNr > 0) {
        xmlChar* t = xmlNodeGetContent(o->nodesetval->nodeTab[0]);
        out = strdup(t ? (const char*)t : "");
        if (t) xmlFree(t);
    } else if (o->type == XPATH_STRING) {
        out = strdup(o->stringval ? (const char*)o->stringval : "");
    } else {
        out = strdup("");
    }
    xmlXPathFreeObject(o);
    return out;
}

int gmbdom_select(int h, const char* expr)
{
    xmlDocPtr d = doc_of(h);
    xmlXPathObjectPtr o = xpath_eval(d, expr);
    if (!o || o->type != XPATH_NODESET) { if (o) xmlXPathFreeObject(o); return 0; }
    for (int i = 0; i < DOM_MAX_SETS; i++) {
        if (!g_sets[i]) { g_sets[i] = o; return i + 1; }
    }
    xmlXPathFreeObject(o);
    return 0;
}

int gmbdom_numnodes(int ns)
{
    if (ns <= 0 || ns > DOM_MAX_SETS || !g_sets[ns - 1]) return 0;
    xmlNodeSetPtr n = g_sets[ns - 1]->nodesetval;
    return n ? n->nodeNr : 0;
}

char* gmbdom_nodeattr(int ns, int i, const char* attr)
{
    if (ns <= 0 || ns > DOM_MAX_SETS || !g_sets[ns - 1]) return strdup("");
    xmlNodeSetPtr n = g_sets[ns - 1]->nodesetval;
    if (!n || i < 0 || i >= n->nodeNr || !attr) return strdup("");
    xmlChar* v = xmlGetProp(n->nodeTab[i], (const xmlChar*)attr);
    char* out = strdup(v ? (const char*)v : "");
    if (v) xmlFree(v);
    return out;
}

int gmbdom_freenodeset(int ns)
{
    if (ns > 0 && ns <= DOM_MAX_SETS && g_sets[ns - 1]) {
        xmlXPathFreeObject(g_sets[ns - 1]);
        g_sets[ns - 1] = NULL;
    }
    return 0;
}

int gmbdom_free(int h)
{
    if (h > 0 && h <= DOM_MAX_DOCS && g_docs[h - 1]) {
        xmlFreeDoc(g_docs[h - 1]);
        g_docs[h - 1] = NULL;
    }
    return 0;
}
