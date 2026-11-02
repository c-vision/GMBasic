/* gmb_ai_decide.c — decision layer (JEV/SystemOne) per le gambe C: contratto + Mock/Rules.
 * Mirror di GMBasicSharp/common/AI/Decision. Il core di inferenza è in LLMEnv (ABI v8
 * CAP_DECIDE); qui non si tocca il motore. */
#include "gmb_ai_decide.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 24 slot canonici: le etichette sono STABILI (cambiarle invalida bias/T allenati). */
const gmb_decision_option_t gmb_decision_slots_noul[2] = {
    { "false", "false", -1, 0.0f, 1.0f },
    { "true",  "true",  -1, 0.0f, 1.0f },
};
const gmb_decision_option_t gmb_decision_slots_score[6] = {
    { "0", "0", -1, 0.0f, 1.0f }, { "1", "1", -1, 0.0f, 1.0f },
    { "2", "2", -1, 0.0f, 1.0f }, { "3", "3", -1, 0.0f, 1.0f },
    { "4", "4", -1, 0.0f, 1.0f }, { "5", "5", -1, 0.0f, 1.0f },
};
const gmb_decision_option_t gmb_decision_slots_choice[16] = {
    { "A", "A", -1, 0.0f, 1.0f }, { "B", "B", -1, 0.0f, 1.0f },
    { "C", "C", -1, 0.0f, 1.0f }, { "D", "D", -1, 0.0f, 1.0f },
    { "E", "E", -1, 0.0f, 1.0f }, { "F", "F", -1, 0.0f, 1.0f },
    { "G", "G", -1, 0.0f, 1.0f }, { "H", "H", -1, 0.0f, 1.0f },
    { "I", "I", -1, 0.0f, 1.0f }, { "J", "J", -1, 0.0f, 1.0f },
    { "K", "K", -1, 0.0f, 1.0f }, { "L", "L", -1, 0.0f, 1.0f },
    { "M", "M", -1, 0.0f, 1.0f }, { "N", "N", -1, 0.0f, 1.0f },
    { "O", "O", -1, 0.0f, 1.0f }, { "P", "P", -1, 0.0f, 1.0f },
};

struct gmb_decision_provider {
    int is_rules;              /* 0 = mock, 1 = rules */
    char* mock_option_id;      /* mock: NULL = prima opzione */
    double mock_probability;
};

gmb_decision_provider_t* gmb_decision_provider_mock(const char* option_id, double probability) {
    if (probability < 0.0 || probability > 1.0) return NULL;
    gmb_decision_provider_t* p = (gmb_decision_provider_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->is_rules = 0;
    p->mock_probability = probability;
    if (option_id && option_id[0]) {
        size_t n = strlen(option_id);
        p->mock_option_id = (char*)malloc(n + 1);
        if (!p->mock_option_id) { free(p); return NULL; }
        memcpy(p->mock_option_id, option_id, n + 1);
    }
    return p;
}

gmb_decision_provider_t* gmb_decision_provider_rules(void) {
    gmb_decision_provider_t* p = (gmb_decision_provider_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->is_rules = 1;
    return p;
}

void gmb_decision_provider_free(gmb_decision_provider_t* provider) {
    if (!provider) return;
    free(provider->mock_option_id);
    free(provider);
}

static int ieq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int icontains(const char* haystack, const char* needle) {
    if (!haystack || !needle || !*needle) return 0;
    size_t nl = strlen(needle);
    for (const char* p = haystack; *p; p++) {
        size_t i = 0;
        while (i < nl && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == nl) return 1;
    }
    return 0;
}

static const gmb_decision_option_t* find_option(const gmb_decision_option_t* options,
                                                int n_options, const char* id) {
    if (!id) return NULL;
    for (int i = 0; i < n_options; i++)
        if (ieq(options[i].id, id)) return &options[i];
    return NULL;
}

static int validate_domain(gmb_decision_kind_t kind, const gmb_decision_option_t* options,
                           int n_options) {
    if (!options || n_options < 1 || n_options > GMB_DECISION_MAX_OPTIONS) return -1;
    if (kind == GMB_DECISION_NOUL && n_options != 2) return -1;
    for (int i = 0; i < n_options; i++) {
        if (!options[i].id || !options[i].id[0]) return -1;
        for (int j = i + 1; j < n_options; j++)
            if (ieq(options[i].id, options[j].id)) return -1;
    }
    return 0;
}

static void fill_result(gmb_decision_result_t* out, gmb_decision_kind_t kind,
                        const gmb_decision_option_t* option, double probability) {
    memset(out, 0, sizeof(*out));
    out->kind = kind;
    out->option_id = option->id;
    out->value = option->label ? option->label : option->id;
    out->probability = probability;
    out->has_confidence = 0;
    out->has_calibrated = 0;
}

int gmb_decision_evaluate(const gmb_decision_provider_t* provider, const char* state,
                          gmb_decision_kind_t kind,
                          const gmb_decision_option_t* options, int n_options,
                          gmb_decision_result_t* out) {
    if (!provider || !out) return -1;
    if (validate_domain(kind, options, n_options) != 0) return -1;

    if (!provider->is_rules) {
        const gmb_decision_option_t* option =
            provider->mock_option_id ? find_option(options, n_options, provider->mock_option_id)
                                     : &options[0];
        if (!option) return -1;
        fill_result(out, kind, option, provider->mock_probability);
        return 1;
    }

    /* Rules: NOUL riconosce yes/true/sì vs no/false; per gli altri tipi, prima opzione 0.5. */
    if (kind == GMB_DECISION_NOUL) {
        const char* s = state ? state : "";
        /* UTF-8: tolower non fa case-folding sulle lettere accentate, quindi "sì" va
         * confrontato anche nelle varianti maiuscole/miste (SÌ/Sì/sÌ). */
        if (icontains(s, "sì") || icontains(s, "Sì") || icontains(s, "sÌ") || icontains(s, "SÌ") ||
            icontains(s, "si") || icontains(s, "yes") || icontains(s, "true")) {
            const gmb_decision_option_t* option = find_option(options, n_options, "true");
            if (option) { fill_result(out, kind, option, 1.0); return 1; }
        }
        if (icontains(s, "no") || icontains(s, "false")) {
            const gmb_decision_option_t* option = find_option(options, n_options, "false");
            if (option) { fill_result(out, kind, option, 1.0); return 1; }
        }
    }
    fill_result(out, kind, &options[0], 0.5);
    return 1;
}

static void json_escape(const char* src, char* dst, size_t cap) {
    size_t o = 0;
    if (cap == 0) return;
    for (const char* p = src ? src : ""; *p && o + 2 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { dst[o++] = '\\'; dst[o++] = (char)c; }
        else if (c == '\n') { dst[o++] = '\\'; dst[o++] = 'n'; }
        else if (c == '\r') { dst[o++] = '\\'; dst[o++] = 'r'; }
        else if (c == '\t') { dst[o++] = '\\'; dst[o++] = 't'; }
        else if (c < 0x20) { continue; }
        else dst[o++] = (char)c;
    }
    dst[o] = 0;
}

int gmb_decision_result_json(const gmb_decision_result_t* result, char* out, size_t cap) {
    if (!result || !out || cap == 0) return -1;
    const char* kind = result->kind == GMB_DECISION_NOUL ? "NOUL"
                     : result->kind == GMB_DECISION_SCORE ? "SCORE" : "CHOICE";
    char option[256], value[512], conf[48], cal[48];
    json_escape(result->option_id, option, sizeof(option));
    json_escape(result->value, value, sizeof(value));
    if (result->has_confidence)
        snprintf(conf, sizeof(conf), "%.6f", result->confidence);
    else
        snprintf(conf, sizeof(conf), "null");
    if (result->has_calibrated)
        snprintf(cal, sizeof(cal), "%.6f", result->calibrated);
    else
        snprintf(cal, sizeof(cal), "null");
    int n = snprintf(out, cap,
        "{\"kind\":\"%s\",\"option\":\"%s\",\"value\":\"%s\",\"probability\":%.6f,"
        "\"confidence\":%s,\"calibrated\":%s}",
        kind, option, value, result->probability, conf, cal);
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}
