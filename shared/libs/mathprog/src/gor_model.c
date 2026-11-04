/* gor_model.c — modellazione solver-agnostica + JSON. */
#include "gor_model.h"
#include "gor_lp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOR_MODEL_MAXNAME 48

typedef struct {
    gor_var_kind kind;
    double lo, hi;
    char name[GOR_MODEL_MAXNAME];
    double value;
} gor_mvar;

typedef struct {
    int ncoef;
    int *vars;
    double *coef;
    gor_cmp cmp;
    double rhs;
} gor_mcon;

struct gor_model {
    int nvar, cvar;
    gor_mvar *vars;
    int ncon, ccon;
    gor_mcon *cons;
    int nobj;
    int *obj_vars;
    double *obj_coef;
    gor_obj_sense sense;
};

gor_model *gor_model_new(void)
{
    gor_model *m = (gor_model *)calloc(1, sizeof(gor_model));
    return m;
}

void gor_model_free(gor_model *m)
{
    int i;
    if (!m) return;
    for (i = 0; i < m->ncon; i++) { free(m->cons[i].vars); free(m->cons[i].coef); }
    free(m->cons); free(m->vars); free(m->obj_vars); free(m->obj_coef);
    free(m);
}

int gor_model_add_var(gor_model *m, gor_var_kind kind, double lo, double hi, const char *name)
{
    if (!m) return -1;
    if (m->nvar >= m->cvar) {
        int nc = m->cvar ? m->cvar * 2 : 16;
        gor_mvar *nv = (gor_mvar *)realloc(m->vars, (size_t)nc * sizeof(gor_mvar));
        if (!nv) return -1;
        m->vars = nv; m->cvar = nc;
    }
    m->vars[m->nvar].kind = kind;
    m->vars[m->nvar].lo = (kind == GOR_VAR_BIN) ? 0.0 : lo;
    m->vars[m->nvar].hi = (kind == GOR_VAR_BIN) ? 1.0 : hi;
    m->vars[m->nvar].value = 0.0;
    m->vars[m->nvar].name[0] = 0;
    if (name) { strncpy(m->vars[m->nvar].name, name, GOR_MODEL_MAXNAME - 1); }
    return m->nvar++;
}

int gor_model_add_constr(gor_model *m, int ncoef, const int *vars, const double *coef,
                         gor_cmp cmp, double rhs)
{
    gor_mcon *c;
    if (!m || ncoef <= 0 || vars == NULL || coef == NULL) return -1;
    if (m->ncon >= m->ccon) {
        int nc = m->ccon ? m->ccon * 2 : 16;
        gor_mcon *n2 = (gor_mcon *)realloc(m->cons, (size_t)nc * sizeof(gor_mcon));
        if (!n2) return -1;
        m->cons = n2; m->ccon = nc;
    }
    c = &m->cons[m->ncon];
    c->ncoef = ncoef;
    c->vars = (int *)malloc((size_t)ncoef * sizeof(int));
    c->coef = (double *)malloc((size_t)ncoef * sizeof(double));
    if (!c->vars || !c->coef) { free(c->vars); free(c->coef); return -1; }
    memcpy(c->vars, vars, (size_t)ncoef * sizeof(int));
    memcpy(c->coef, coef, (size_t)ncoef * sizeof(double));
    c->cmp = cmp; c->rhs = rhs;
    m->ncon++;
    return 0;
}

void gor_model_set_obj(gor_model *m, int ncoef, const int *vars, const double *coef,
                       gor_obj_sense sense)
{
    if (!m) return;
    free(m->obj_vars); free(m->obj_coef);
    m->obj_vars = (int *)malloc((size_t)ncoef * sizeof(int));
    m->obj_coef = (double *)malloc((size_t)ncoef * sizeof(double));
    if (m->obj_vars && m->obj_coef) {
        memcpy(m->obj_vars, vars, (size_t)ncoef * sizeof(int));
        memcpy(m->obj_coef, coef, (size_t)ncoef * sizeof(double));
        m->nobj = ncoef;
    } else { m->nobj = 0; }
    m->sense = sense;
}

/* ============================== solve (conversione a forma standard) ============================== */

int gor_model_solve(gor_model *m, double *obj)
{
    int n = m ? m->nvar : 0;
    int mrows, ncols, i, k, r;
    double *b, *x, *offset;
    int nint = 0;
    int nrows_ub = 0, status;
    if (!m || n <= 0) return 1;
    offset = (double *)malloc((size_t)n * sizeof(double));
    if (!offset) return 4;
    for (i = 0; i < n; i++) offset[i] = (m->vars[i].lo > -1e299) ? m->vars[i].lo : 0.0;
    /* conta righe: vincoli + upper bound finiti */
    for (i = 0; i < m->ncon; i++) nrows_ub++;
    for (i = 0; i < n; i++) if (m->vars[i].hi < 1e299) nrows_ub++;
    mrows = nrows_ub;
    ncols = n; /* una variabile per colonna (x' >= 0) */
    if (mrows < 1) mrows = 1;
    b = (double *)calloc((size_t)mrows, sizeof(double));
    if (!b) { free(offset); return 4; }
    r = 0;
    /* vincoli del modello (slack/surplus inline: aggiungo colonne slack dopo) */
    {
        int nslack = 0;
        for (i = 0; i < m->ncon; i++) if (m->cons[i].cmp != GOR_CMP_EQ) nslack++;
        for (i = 0; i < n; i++) if (m->vars[i].hi < 1e299) nslack++;
        x = (double *)calloc((size_t)(ncols + nslack), sizeof(double));
        if (!x) { free(b); free(offset); return 4; }
        double *A2 = (double *)calloc((size_t)mrows * (size_t)(ncols + nslack), sizeof(double));
        double *c2 = (double *)calloc((size_t)(ncols + nslack), sizeof(double));
        int *int2 = (int *)malloc((size_t)(ncols + nslack) * sizeof(int));
        int ns = 0;
        if (!A2 || !c2 || !int2) { free(b); free(x); free(offset); free(A2); free(c2); free(int2); return 4; }
        for (i = 0; i < m->ncon; i++) {
            gor_mcon *con = &m->cons[i];
            double shift = 0.0;
            for (k = 0; k < con->ncoef; k++) {
                A2[(size_t)r * (size_t)(ncols + nslack) + (size_t)con->vars[k]] += con->coef[k];
                shift += con->coef[k] * offset[con->vars[k]];
            }
            b[r] = con->rhs - shift;
            if (con->cmp == GOR_CMP_LE) { A2[(size_t)r * (size_t)(ncols + nslack) + (size_t)(ncols + ns)] = 1.0; ns++; }
            else if (con->cmp == GOR_CMP_GE) { A2[(size_t)r * (size_t)(ncols + nslack) + (size_t)(ncols + ns)] = -1.0; ns++; }
            r++;
        }
        /* upper bound x_i <= hi_i -> x'_i + s = hi_i - lo_i */
        for (i = 0; i < n; i++) {
            if (m->vars[i].hi < 1e299) {
                A2[(size_t)r * (size_t)(ncols + nslack) + (size_t)i] = 1.0;
                A2[(size_t)r * (size_t)(ncols + nslack) + (size_t)(ncols + ns)] = 1.0; ns++;
                b[r] = m->vars[i].hi - offset[i];
                r++;
            }
        }
        /* obiettivo */
        for (k = 0; k < m->nobj; k++) c2[m->obj_vars[k]] = (m->sense == GOR_OBJ_MIN) ? m->obj_coef[k] : -m->obj_coef[k];
        /* intere */
        for (i = 0; i < n; i++) if (m->vars[i].kind != GOR_VAR_CONT) int2[nint++] = i;
        {
            int ncols2 = ncols + nslack;
            int st = gor_milp_bb(mrows, ncols2, A2, b, c2, nint, int2, x, obj, 20000, 1e-6);
            if (st == 0) {
                for (i = 0; i < n; i++) m->vars[i].value = x[i] + offset[i];
                if (m->sense == GOR_OBJ_MAX && obj) *obj = -*obj;
            }
            status = st;
        }
        free(A2); free(c2); free(int2);
    }
    free(b); free(x); free(offset);
    return status;
}

double gor_model_var_value(const gor_model *m, int i)
{
    if (!m || i < 0 || i >= m->nvar) return 0.0;
    return m->vars[i].value;
}

int gor_model_num_vars(const gor_model *m) { return m ? m->nvar : 0; }

/* ============================== JSON ============================== */

char *gor_model_to_json(const gor_model *m)
{
    size_t cap = 65536, len = 0;
    char *buf;
    int i, k;
    if (!m) return NULL;
    buf = (char *)malloc(cap);
    if (!buf) return NULL;
    len += (size_t)snprintf(buf + len, cap - len, "{\"vars\":[");
    for (i = 0; i < m->nvar; i++) {
        const char *k2 = m->vars[i].kind == GOR_VAR_BIN ? "bin" : (m->vars[i].kind == GOR_VAR_INT ? "int" : "cont");
        len += (size_t)snprintf(buf + len, cap - len, "%s{\"name\":\"%s\",\"kind\":\"%s\",\"lo\":%g,\"hi\":%g}",
               i ? "," : "", m->vars[i].name, k2, m->vars[i].lo, m->vars[i].hi);
    }
    len += (size_t)snprintf(buf + len, cap - len, "],\"constraints\":[");
    for (i = 0; i < m->ncon; i++) {
        const char *cmp = m->cons[i].cmp == GOR_CMP_LE ? "le" : (m->cons[i].cmp == GOR_CMP_GE ? "ge" : "eq");
        len += (size_t)snprintf(buf + len, cap - len, "%s{\"cmp\":\"%s\",\"rhs\":%g,\"terms\":[", i ? "," : "", cmp, m->cons[i].rhs);
        for (k = 0; k < m->cons[i].ncoef; k++)
            len += (size_t)snprintf(buf + len, cap - len, "%s[%d,%g]", k ? "," : "", m->cons[i].vars[k], m->cons[i].coef[k]);
        len += (size_t)snprintf(buf + len, cap - len, "]}");
    }
    len += (size_t)snprintf(buf + len, cap - len, "],\"objective\":{\"sense\":\"%s\",\"terms\":[", m->sense == GOR_OBJ_MIN ? "min" : "max");
    for (k = 0; k < m->nobj; k++)
        len += (size_t)snprintf(buf + len, cap - len, "%s[%d,%g]", k ? "," : "", m->obj_vars[k], m->obj_coef[k]);
    snprintf(buf + len, cap - len, "]}}");
    return buf;
}
