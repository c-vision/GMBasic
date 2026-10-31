// FILE: gmb_connector.c — Connector Runtime F0 (C shared, parità Basic.Connectors.*)
// Implementazione file-based per config/connectors.json + marketplace statico 52 connettori.
// Per F0 il file è JSON semplice; l'accesso è via cJSON se disponibile, altrimenti fallback stringa.

#include "gmb_connector.h"
#include "gmb_commonlib.h"
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

#ifdef __APPLE__
#include <sys/stat.h>
#include <unistd.h>
#endif

static const char* marketplace_json =
"[{\"name\":\"gmail\",\"kind\":\"gmail\",\"displayName\":\"Gmail\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"mail.read\",\"mail.search\",\"mail.send\",\"mail.modify\",\"mail.labels\",\"mail.attachments\"],\"tools\":[\"search\",\"read\",\"send\",\"reply\",\"forward\",\"labels\",\"attachments\"],\"description\":\"Gmail \\u2014 email search/read/send with OAuth\",\"enabled\":true}"
","
"{\"name\":\"google-calendar\",\"kind\":\"calendar\",\"displayName\":\"Google Calendar\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"calendar.read\",\"calendar.write\",\"calendar.events\"],\"tools\":[\"events\",\"create\",\"update\",\"cancel\",\"availability\",\"invite\"],\"description\":\"Google Calendar \\u2014 events and availability\",\"enabled\":true}"
","
"{\"name\":\"google-drive\",\"kind\":\"drive\",\"displayName\":\"Google Drive\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"drive.read\",\"drive.write\",\"drive.search\"],\"tools\":[\"search\",\"read\",\"write\",\"delete\",\"move\",\"copy\",\"presign\"],\"description\":\"Google Drive \\u2014 file storage\",\"enabled\":true}"
","
"{\"name\":\"github\",\"kind\":\"github\",\"displayName\":\"GitHub\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"repo.read\",\"repo.write\",\"issues.read\",\"issues.write\",\"pr.read\",\"pr.write\",\"actions.read\",\"actions.write\"],\"tools\":[\"repository\",\"issues\",\"pull_requests\",\"branches\",\"commits\",\"releases\",\"actions\",\"workflows\"],\"description\":\"GitHub \\u2014 repositories/issues/PR/Actions\",\"enabled\":true}"
","
"{\"name\":\"slack\",\"kind\":\"slack\",\"displayName\":\"Slack\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"chat.read\",\"chat.write\",\"channels.read\",\"channels.write\"],\"tools\":[\"search\",\"read_thread\",\"read_channel\",\"search_messages\",\"list_members\",\"send\",\"reply\",\"react\"],\"description\":\"Slack \\u2014 messaging and channels\",\"enabled\":true}"
","
"{\"name\":\"microsoft-365\",\"kind\":\"microsoft\",\"displayName\":\"Microsoft 365\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"mail.read\",\"mail.send\",\"calendar.read\",\"calendar.write\",\"drive.read\",\"drive.write\",\"teams.read\"],\"tools\":[\"mail.search\",\"mail.send\",\"calendar.events\",\"drive.search\",\"teams.messages\",\"onedrive.read\",\"sharepoint.read\"],\"description\":\"Microsoft 365 \\u2014 Outlook/Calendar/OneDrive/SharePoint/Teams via Graph\",\"enabled\":true}"
","
"{\"name\":\"web\",\"kind\":\"web\",\"displayName\":\"Web\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"web.search\",\"web.fetch\",\"web.extract\"],\"tools\":[\"search\",\"fetch\",\"extract\",\"browser\",\"screenshot\",\"links\"],\"description\":\"Web \\u2014 search/fetch/extract/browser\",\"enabled\":true}"
","
"{\"name\":\"http\",\"kind\":\"http\",\"displayName\":\"HTTP\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"http.get\",\"http.post\",\"http.put\",\"http.patch\",\"http.delete\"],\"tools\":[\"get\",\"post\",\"put\",\"patch\",\"delete\"],\"description\":\"HTTP \\u2014 raw protocol with auth/headers/cookies/JSON/XML/multipart/retry\",\"enabled\":true}"
","
"{\"name\":\"filesystem\",\"kind\":\"filesystem\",\"displayName\":\"Filesystem\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"fs.read\",\"fs.write\",\"fs.list\",\"fs.find\"],\"tools\":[\"list\",\"find\",\"read\",\"write\",\"move\",\"copy\",\"delete\",\"metadata\",\"hash\",\"diff\"],\"description\":\"Filesystem \\u2014 sandboxed workspace I/O\",\"enabled\":true}"
","
"{\"name\":\"postgres\",\"kind\":\"postgres\",\"displayName\":\"PostgreSQL\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"db.connect\",\"db.query\",\"db.execute\",\"db.schema\"],\"tools\":[\"connect\",\"query\",\"execute\",\"transaction\",\"schema\",\"tables\",\"describe\"],\"description\":\"PostgreSQL \\u2014 common DB API\",\"enabled\":true}"
","
"{\"name\":\"sqlite\",\"kind\":\"sqlite\",\"displayName\":\"SQLite\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"db.connect\",\"db.query\",\"db.execute\",\"db.schema\"],\"tools\":[\"connect\",\"query\",\"execute\",\"transaction\",\"schema\",\"tables\",\"describe\"],\"description\":\"SQLite \\u2014 embedded DB\",\"enabled\":true}"
","
"{\"name\":\"google-sheets\",\"kind\":\"sheets\",\"displayName\":\"Google Sheets\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"sheets.read\",\"sheets.write\"],\"tools\":[\"read\",\"write\",\"append\",\"query\",\"insert\"],\"description\":\"Google Sheets\",\"enabled\":true}"
","
"{\"name\":\"google-docs\",\"kind\":\"docs\",\"displayName\":\"Google Docs\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"docs.read\",\"docs.write\"],\"tools\":[\"read\",\"write\",\"create\",\"update\"],\"description\":\"Google Docs\",\"enabled\":true}"
","
"{\"name\":\"onedrive\",\"kind\":\"onedrive\",\"displayName\":\"OneDrive\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"drive.read\",\"drive.write\"],\"tools\":[\"list\",\"read\",\"write\",\"delete\",\"move\"],\"description\":\"OneDrive\",\"enabled\":true}"
","
"{\"name\":\"sharepoint\",\"kind\":\"sharepoint\",\"displayName\":\"SharePoint\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"sharepoint.read\",\"sharepoint.write\"],\"tools\":[\"list\",\"read\",\"write\",\"search\"],\"description\":\"SharePoint\",\"enabled\":true}"
","
"{\"name\":\"notion\",\"kind\":\"notion\",\"displayName\":\"Notion\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"notion.read\",\"notion.write\"],\"tools\":[\"search\",\"page.read\",\"page.create\",\"page.update\",\"database.query\",\"database.insert\"],\"description\":\"Notion\",\"enabled\":true}"
","
"{\"name\":\"linear\",\"kind\":\"linear\",\"displayName\":\"Linear\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"linear.read\",\"linear.write\"],\"tools\":[\"issues\",\"projects\",\"cycles\",\"comments\",\"teams\",\"labels\"],\"description\":\"Linear\",\"enabled\":true}"
","
"{\"name\":\"jira\",\"kind\":\"jira\",\"displayName\":\"Jira\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"jira.read\",\"jira.write\"],\"tools\":[\"issues\",\"projects\",\"comments\",\"transitions\",\"worklogs\",\"sprints\"],\"description\":\"Jira\",\"enabled\":true}"
","
"{\"name\":\"discord\",\"kind\":\"discord\",\"displayName\":\"Discord\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"discord.read\",\"discord.write\"],\"tools\":[\"search\",\"read_channel\",\"send\",\"reply\",\"react\"],\"description\":\"Discord\",\"enabled\":true}"
","
"{\"name\":\"telegram\",\"kind\":\"telegram\",\"displayName\":\"Telegram\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"telegram.read\",\"telegram.write\"],\"tools\":[\"search\",\"send\",\"reply\",\"list_members\"],\"description\":\"Telegram\",\"enabled\":true}"
","
"{\"name\":\"microsoft-teams\",\"kind\":\"teams\",\"displayName\":\"Microsoft Teams\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"teams.read\",\"teams.write\"],\"tools\":[\"messages\",\"channels\",\"send\",\"reply\"],\"description\":\"Microsoft Teams\",\"enabled\":true}"
","
"{\"name\":\"dropbox\",\"kind\":\"dropbox\",\"displayName\":\"Dropbox\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"drive.read\",\"drive.write\"],\"tools\":[\"list\",\"read\",\"write\",\"delete\",\"move\",\"presign\"],\"description\":\"Dropbox\",\"enabled\":true}"
","
"{\"name\":\"box\",\"kind\":\"box\",\"displayName\":\"Box\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"drive.read\",\"drive.write\"],\"tools\":[\"list\",\"read\",\"write\",\"delete\"],\"description\":\"Box\",\"enabled\":true}"
","
"{\"name\":\"stripe\",\"kind\":\"stripe\",\"displayName\":\"Stripe\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"stripe.read\",\"stripe.write\"],\"tools\":[\"customers\",\"invoices\",\"subscriptions\",\"payments\",\"refunds\",\"products\"],\"description\":\"Stripe \\u2014 HIGH_RISK\",\"enabled\":true}"
","
"{\"name\":\"hubspot\",\"kind\":\"hubspot\",\"displayName\":\"HubSpot\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"hubspot.read\",\"hubspot.write\"],\"tools\":[\"contacts\",\"companies\",\"deals\",\"activities\",\"notes\",\"tickets\"],\"description\":\"HubSpot CRM\",\"enabled\":true}"
","
"{\"name\":\"salesforce\",\"kind\":\"salesforce\",\"displayName\":\"Salesforce\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"salesforce.read\",\"salesforce.write\"],\"tools\":[\"contacts\",\"companies\",\"deals\",\"activities\"],\"description\":\"Salesforce CRM\",\"enabled\":true}"
","
"{\"name\":\"shopify\",\"kind\":\"shopify\",\"displayName\":\"Shopify\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"shopify.read\",\"shopify.write\"],\"tools\":[\"products\",\"orders\",\"customers\",\"inventory\"],\"description\":\"Shopify\",\"enabled\":true}"
","
"{\"name\":\"woocommerce\",\"kind\":\"woocommerce\",\"displayName\":\"WooCommerce\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"woo.read\",\"woo.write\"],\"tools\":[\"products\",\"orders\",\"customers\"],\"description\":\"WooCommerce\",\"enabled\":true}"
","
"{\"name\":\"zendesk\",\"kind\":\"zendesk\",\"displayName\":\"Zendesk\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"zendesk.read\",\"zendesk.write\"],\"tools\":[\"tickets\",\"users\",\"groups\",\"search\"],\"description\":\"Zendesk\",\"enabled\":true}"
","
"{\"name\":\"twilio\",\"kind\":\"twilio\",\"displayName\":\"Twilio\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"twilio.read\",\"twilio.write\"],\"tools\":[\"sms.send\",\"call.create\",\"messages\"],\"description\":\"Twilio\",\"enabled\":true}"
","
"{\"name\":\"sendgrid\",\"kind\":\"sendgrid\",\"displayName\":\"SendGrid\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"sendgrid.write\"],\"tools\":[\"send\",\"templates\",\"lists\"],\"description\":\"SendGrid\",\"enabled\":true}"
","
"{\"name\":\"docusign\",\"kind\":\"docusign\",\"displayName\":\"DocuSign\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"docusign.read\",\"docusign.write\"],\"tools\":[\"envelopes\",\"templates\",\"recipients\"],\"description\":\"DocuSign \\u2014 HIGH_RISK\",\"enabled\":true}"
","
"{\"name\":\"calendly\",\"kind\":\"calendly\",\"displayName\":\"Calendly\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"calendly.read\",\"calendly.write\"],\"tools\":[\"events\",\"invitees\",\"scheduling\"],\"description\":\"Calendly\",\"enabled\":true}"
","
"{\"name\":\"docker\",\"kind\":\"docker\",\"displayName\":\"Docker\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"docker.read\",\"docker.write\"],\"tools\":[\"containers\",\"images\",\"networks\",\"volumes\",\"exec\"],\"description\":\"Docker\",\"enabled\":true}"
","
"{\"name\":\"kubernetes\",\"kind\":\"kubernetes\",\"displayName\":\"Kubernetes\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"k8s.read\",\"k8s.write\"],\"tools\":[\"pods\",\"deployments\",\"services\",\"jobs\",\"logs\"],\"description\":\"Kubernetes\",\"enabled\":true}"
","
"{\"name\":\"aws\",\"kind\":\"aws\",\"displayName\":\"AWS\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"aws.read\",\"aws.write\"],\"tools\":[\"s3.list\",\"s3.read\",\"s3.write\",\"lambda.invoke\",\"ec2.list\"],\"description\":\"AWS\",\"enabled\":true}"
","
"{\"name\":\"azure\",\"kind\":\"azure\",\"displayName\":\"Azure\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"azure.read\",\"azure.write\"],\"tools\":[\"storage.list\",\"vm.list\",\"functions.invoke\"],\"description\":\"Azure\",\"enabled\":true}"
","
"{\"name\":\"google-cloud\",\"kind\":\"gcp\",\"displayName\":\"Google Cloud\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"gcp.read\",\"gcp.write\"],\"tools\":[\"storage.list\",\"compute.list\",\"functions.invoke\"],\"description\":\"Google Cloud\",\"enabled\":true}"
","
"{\"name\":\"cloudflare\",\"kind\":\"cloudflare\",\"displayName\":\"Cloudflare\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"cloudflare.read\",\"cloudflare.write\"],\"tools\":[\"zones\",\"dns\",\"workers\",\"r2.list\"],\"description\":\"Cloudflare\",\"enabled\":true}"
","
"{\"name\":\"sentry\",\"kind\":\"sentry\",\"displayName\":\"Sentry\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"sentry.read\"],\"tools\":[\"errors\",\"traces\",\"metrics\",\"logs\",\"alerts\"],\"description\":\"Sentry \\u2014 observability\",\"enabled\":true}"
","
"{\"name\":\"grafana\",\"kind\":\"grafana\",\"displayName\":\"Grafana\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"grafana.read\"],\"tools\":[\"dashboards\",\"metrics\",\"alerts\"],\"description\":\"Grafana\",\"enabled\":true}"
","
"{\"name\":\"prometheus\",\"kind\":\"prometheus\",\"displayName\":\"Prometheus\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"prometheus.read\"],\"tools\":[\"query\",\"metrics\",\"alerts\"],\"description\":\"Prometheus\",\"enabled\":true}"
","
"{\"name\":\"gitlab\",\"kind\":\"gitlab\",\"displayName\":\"GitLab\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"gitlab.read\",\"gitlab.write\"],\"tools\":[\"repositories\",\"issues\",\"merge_requests\",\"pipelines\"],\"description\":\"GitLab\",\"enabled\":true}"
","
"{\"name\":\"bitbucket\",\"kind\":\"bitbucket\",\"displayName\":\"Bitbucket\",\"version\":\"1.0\",\"authKind\":\"oauth2\",\"capabilities\":[\"bitbucket.read\",\"bitbucket.write\"],\"tools\":[\"repositories\",\"issues\",\"pull_requests\"],\"description\":\"Bitbucket\",\"enabled\":true}"
","
"{\"name\":\"openai\",\"kind\":\"openai\",\"displayName\":\"OpenAI\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"ai.chat\",\"ai.embed\"],\"tools\":[\"chat\",\"embed\",\"models\",\"files\"],\"description\":\"OpenAI\",\"enabled\":true}"
","
"{\"name\":\"anthropic\",\"kind\":\"anthropic\",\"displayName\":\"Anthropic\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"ai.chat\"],\"tools\":[\"chat\",\"models\"],\"description\":\"Anthropic\",\"enabled\":true}"
","
"{\"name\":\"openrouter\",\"kind\":\"openrouter\",\"displayName\":\"OpenRouter\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"ai.chat\"],\"tools\":[\"chat\",\"models\"],\"description\":\"OpenRouter\",\"enabled\":true}"
","
"{\"name\":\"huggingface\",\"kind\":\"huggingface\",\"displayName\":\"Hugging Face\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"hf.read\"],\"tools\":[\"models\",\"datasets\",\"inference\"],\"description\":\"Hugging Face\",\"enabled\":true}"
","
"{\"name\":\"weights-biases\",\"kind\":\"wandb\",\"displayName\":\"Weights \\u0026 Biases\",\"version\":\"1.0\",\"authKind\":\"apikey\",\"capabilities\":[\"wandb.read\",\"wandb.write\"],\"tools\":[\"runs\",\"artifacts\",\"sweeps\"],\"description\":\"Weights \\u0026 Biases\",\"enabled\":true}"
","
"{\"name\":\"qdrant\",\"kind\":\"qdrant\",\"displayName\":\"Qdrant\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"vector.read\",\"vector.write\"],\"tools\":[\"insert\",\"search\",\"delete\",\"collections\",\"metadata\"],\"description\":\"Qdrant\",\"enabled\":true}"
","
"{\"name\":\"chroma\",\"kind\":\"chroma\",\"displayName\":\"Chroma\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"vector.read\",\"vector.write\"],\"tools\":[\"insert\",\"search\",\"delete\",\"collections\"],\"description\":\"Chroma\",\"enabled\":true}"
","
"{\"name\":\"elasticsearch\",\"kind\":\"elasticsearch\",\"displayName\":\"Elasticsearch\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"search.read\",\"search.write\"],\"tools\":[\"search\",\"index\",\"delete\",\"indices\"],\"description\":\"Elasticsearch\",\"enabled\":true}"
","
"{\"name\":\"neo4j\",\"kind\":\"neo4j\",\"displayName\":\"Neo4j\",\"version\":\"1.0\",\"authKind\":\"none\",\"capabilities\":[\"graph.read\",\"graph.write\"],\"tools\":[\"query\",\"insert\",\"traverse\",\"collections\"],\"description\":\"Neo4j\",\"enabled\":true}"
"]";

static char* dup_str(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* r = (char*)malloc(n+1);
    if (r) memcpy(r, s, n+1);
    return r;
}

/* =====================================================================
 * F1 — Registry in-memory dei connettori (parità Basic.Connectors.*).
 * Inizializzato dal catalogo statico (marketplace_json) via gmb_json_parse.
 * Le mutazioni (register/unregister/enable/policy/secret/auth) restano in
 * memoria per la durata del processo; lo stato di partenza è il catalogo.
 * ===================================================================== */

typedef struct {
    char* name; char* kind; char* displayName; char* version; char* authKind;
    char** caps; int ncaps;
    char** tools; int ntools;
    char* description;
    int enabled;
    char** pol_k; char** pol_v; int npol;
    char** sec_k; char** sec_v; int nsec;
} conn_t;

static conn_t* g_conns = NULL;
static int g_nconns = 0;
static int g_reg_inited = 0;

static const char* val_str(gmb_val_t v) {
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return v.v_string.data;
    return NULL;
}

static char** val_list_strs(gmb_val_t v, int* nout) {
    *nout = 0;
    gmb_list_t l = gmb_val_as_list(v);
    gmb_int_t n = gmb_list_count(l);
    if (n <= 0) return NULL;
    char** out = (char**)calloc((size_t)n, sizeof(char*));
    if (!out) return NULL;
    for (gmb_int_t i = 0; i < n; i++) out[i] = dup_str(val_str(gmb_list_get(l, i)));
    *nout = (int)n;
    return out;
}

static void registry_init(void) {
    if (g_reg_inited) return;
    g_reg_inited = 1;
    gmb_val_t root = gmb_json_parse(marketplace_json);
    gmb_list_t lst = gmb_val_as_list(root);
    gmb_int_t n = gmb_list_count(lst);
    if (n <= 0) return;
    g_conns = (conn_t*)calloc((size_t)n, sizeof(conn_t));
    if (!g_conns) return;
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_dict_t d = gmb_val_as_dict(gmb_list_get(lst, i));
        conn_t* c = &g_conns[g_nconns];
        c->name = dup_str(val_str(gmb_dict_get(d, "name")));
        c->kind = dup_str(val_str(gmb_dict_get(d, "kind")));
        c->displayName = dup_str(val_str(gmb_dict_get(d, "displayName")));
        c->version = dup_str(val_str(gmb_dict_get(d, "version")));
        c->authKind = dup_str(val_str(gmb_dict_get(d, "authKind")));
        c->description = dup_str(val_str(gmb_dict_get(d, "description")));
        c->caps = val_list_strs(gmb_dict_get(d, "capabilities"), &c->ncaps);
        c->tools = val_list_strs(gmb_dict_get(d, "tools"), &c->ntools);
        c->enabled = 1;
        g_nconns++;
    }
}

static int ci_contains(const char* hay, const char* needle) {
    if (!hay || !needle) return 0;
    size_t nl = strlen(needle);
    if (!nl) return 1;
    for (const char* p = hay; *p; p++) {
        size_t i = 0;
        while (i < nl && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == nl) return 1;
    }
    return 0;
}

static int ci_equal(const char* a, const char* b) {
    if (!a || !b) return a == b;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

static conn_t* registry_find(const char* name) {
    registry_init();
    if (!name) return NULL;
    for (int i = 0; i < g_nconns; i++)
        if (g_conns[i].name && ci_equal(g_conns[i].name, name)) return &g_conns[i];
    return NULL;
}

static conn_t* registry_add(const char* name) {
    conn_t* arr = (conn_t*)realloc(g_conns, (size_t)(g_nconns + 1) * sizeof(conn_t));
    if (!arr) return NULL;
    g_conns = arr;
    conn_t* c = &g_conns[g_nconns++];
    memset(c, 0, sizeof(conn_t));
    c->name = dup_str(name);
    c->enabled = 1;
    return c;
}

static void registry_remove(conn_t* c) {
    if (!c) return;
    int idx = (int)(c - g_conns);
    if (idx < 0 || idx >= g_nconns) return;
    for (int i = idx; i + 1 < g_nconns; i++) g_conns[i] = g_conns[i + 1];
    g_nconns--;
}

static const char* conn_pol_get(conn_t* c, const char* tool) {
    if (!c || !tool) return NULL;
    for (int i = 0; i < c->npol; i++) if (ci_equal(c->pol_k[i], tool)) return c->pol_v[i];
    return NULL;
}
static void conn_pol_set(conn_t* c, const char* tool, const char* pol) {
    if (!c || !tool) return;
    for (int i = 0; i < c->npol; i++) if (ci_equal(c->pol_k[i], tool)) { free(c->pol_v[i]); c->pol_v[i] = dup_str(pol); return; }
    c->pol_k = (char**)realloc(c->pol_k, (size_t)(c->npol + 1) * sizeof(char*));
    c->pol_v = (char**)realloc(c->pol_v, (size_t)(c->npol + 1) * sizeof(char*));
    c->pol_k[c->npol] = dup_str(tool); c->pol_v[c->npol] = dup_str(pol); c->npol++;
}
static const char* conn_sec_get(conn_t* c, const char* key) {
    if (!c || !key) return NULL;
    for (int i = 0; i < c->nsec; i++) if (ci_equal(c->sec_k[i], key)) return c->sec_v[i];
    return NULL;
}
static void conn_sec_set(conn_t* c, const char* key, const char* val) {
    if (!c || !key) return;
    for (int i = 0; i < c->nsec; i++) if (ci_equal(c->sec_k[i], key)) { free(c->sec_v[i]); c->sec_v[i] = dup_str(val); return; }
    c->sec_k = (char**)realloc(c->sec_k, (size_t)(c->nsec + 1) * sizeof(char*));
    c->sec_v = (char**)realloc(c->sec_v, (size_t)(c->nsec + 1) * sizeof(char*));
    c->sec_k[c->nsec] = dup_str(key); c->sec_v[c->nsec] = dup_str(val); c->nsec++;
}

/* ---- string builder + escaping System.Text.Json (default encoder) ---- */
typedef struct { char* p; size_t n, cap; } sb_t;
static void sb_init(sb_t* b) { b->cap = 256; b->n = 0; b->p = (char*)malloc(b->cap); if (b->p) b->p[0] = 0; }
static void sb_reserve(sb_t* b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return;
    while (b->n + extra + 1 > b->cap) b->cap *= 2;
    b->p = (char*)realloc(b->p, b->cap);
}
static void sb_addn(sb_t* b, const char* s, size_t n) { if (!b->p) return; sb_reserve(b, n); memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = 0; }
static void sb_adds(sb_t* b, const char* s) { if (s) sb_addn(b, s, strlen(s)); }
static void sb_addc(sb_t* b, char c) { if (!b->p) return; sb_reserve(b, 1); b->p[b->n++] = c; b->p[b->n] = 0; }
static char* sb_take(sb_t* b) { char* r = b->p; b->p = NULL; return r; }

static void sb_json_string(sb_t* b, const char* s) {
    sb_addc(b, '"');
    if (s) {
        const unsigned char* p = (const unsigned char*)s;
        while (*p) {
            unsigned char c = *p;
            if (c == '"') { sb_adds(b, "\\\""); p++; }
            else if (c == '\\') { sb_adds(b, "\\\\"); p++; }
            else if (c == '\n') { sb_adds(b, "\\n"); p++; }
            else if (c == '\r') { sb_adds(b, "\\r"); p++; }
            else if (c == '\t') { sb_adds(b, "\\t"); p++; }
            else if (c == '\b') { sb_adds(b, "\\b"); p++; }
            else if (c == '\f') { sb_adds(b, "\\f"); p++; }
            else if (c < 0x20) { char t[8]; snprintf(t, sizeof t, "\\u%04x", c); sb_adds(b, t); p++; }
            else if (c == '&' || c == '<' || c == '>' || c == '\'' || c == '+') {
                char t[8]; snprintf(t, sizeof t, "\\u%04x", c); sb_adds(b, t); p++;
            }
            else if (c >= 0x80) {
                int cp = 0, len = 1;
                if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
                else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
                else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
                else { cp = c; len = 1; }
                for (int i = 1; i < len && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3F);
                if (cp <= 0xFFFF) { char t[8]; snprintf(t, sizeof t, "\\u%04x", cp); sb_adds(b, t); }
                else { cp -= 0x10000; char t[16]; snprintf(t, sizeof t, "\\u%04x\\u%04x", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF)); sb_adds(b, t); }
                p += len;
            }
            else { sb_addc(b, (char)c); p++; }
        }
    }
    sb_addc(b, '"');
}

static void sb_str_array_compact(sb_t* b, char** arr, int n) {
    sb_addc(b, '[');
    for (int i = 0; i < n; i++) { if (i) sb_addc(b, ','); sb_json_string(b, arr[i]); }
    sb_addc(b, ']');
}

/* def completa (17 campi) — usata da SEARCH (System.Text.Json WriteIndented: 2 spazi/livello) */
static void sb_ind(sb_t* b, int n) { for (int i = 0; i < n; i++) sb_adds(b, "  "); }

static void sb_def_full(sb_t* b, conn_t* c, int base) {
    sb_ind(b, base); sb_adds(b, "{\n");
    sb_ind(b, base + 1); sb_adds(b, "\"name\": "); sb_json_string(b, c->name); sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"kind\": "); sb_json_string(b, c->kind); sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"displayName\": "); sb_json_string(b, c->displayName); sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"version\": "); sb_json_string(b, c->version); sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"authKind\": "); sb_json_string(b, c->authKind); sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"capabilities\": ");
    if (c->ncaps == 0) sb_adds(b, "[]");
    else { sb_adds(b, "[\n"); for (int i = 0; i < c->ncaps; i++) { sb_ind(b, base + 2); sb_json_string(b, c->caps[i]); sb_adds(b, i + 1 < c->ncaps ? ",\n" : "\n"); } sb_ind(b, base + 1); sb_adds(b, "]"); }
    sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"tools\": ");
    if (c->ntools == 0) sb_adds(b, "[]");
    else { sb_adds(b, "[\n"); for (int i = 0; i < c->ntools; i++) { sb_ind(b, base + 2); sb_json_string(b, c->tools[i]); sb_adds(b, i + 1 < c->ntools ? ",\n" : "\n"); } sb_ind(b, base + 1); sb_adds(b, "]"); }
    sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"description\": "); sb_json_string(b, c->description); sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"url\": null,\n");
    sb_ind(b, base + 1); sb_adds(b, "\"command\": null,\n");
    sb_ind(b, base + 1); sb_adds(b, "\"args\": null,\n");
    sb_ind(b, base + 1); sb_adds(b, "\"env\": null,\n");
    sb_ind(b, base + 1);
    if (c->nsec == 0) sb_adds(b, "\"secrets\": null,\n");
    else { sb_adds(b, "\"secrets\": {\n"); for (int i = 0; i < c->nsec; i++) { sb_ind(b, base + 2); sb_json_string(b, c->sec_k[i]); sb_adds(b, ": "); sb_json_string(b, c->sec_v[i]); sb_adds(b, i + 1 < c->nsec ? ",\n" : "\n"); } sb_ind(b, base + 1); sb_adds(b, "},\n"); }
    sb_ind(b, base + 1);
    if (c->npol == 0) sb_adds(b, "\"toolPolicies\": null,\n");
    else { sb_adds(b, "\"toolPolicies\": {\n"); for (int i = 0; i < c->npol; i++) { sb_ind(b, base + 2); sb_json_string(b, c->pol_k[i]); sb_adds(b, ": "); sb_json_string(b, c->pol_v[i]); sb_adds(b, i + 1 < c->npol ? ",\n" : "\n"); } sb_ind(b, base + 1); sb_adds(b, "},\n"); }
    sb_ind(b, base + 1); sb_adds(b, "\"enabled\": "); sb_adds(b, c->enabled ? "true" : "false"); sb_adds(b, ",\n");
    sb_ind(b, base + 1); sb_adds(b, "\"createdAt\": null,\n");
    sb_ind(b, base + 1); sb_adds(b, "\"updatedAt\": null\n");
    sb_ind(b, base); sb_adds(b, "}");
}

static char* ser_list(void) {
    registry_init();
    sb_t b; sb_init(&b);
    sb_addc(&b, '[');
    for (int i = 0; i < g_nconns; i++) {
        conn_t* c = &g_conns[i];
        if (i) sb_addc(&b, ',');
        sb_adds(&b, "{\"name\":"); sb_json_string(&b, c->name);
        sb_adds(&b, ",\"kind\":"); sb_json_string(&b, c->kind);
        sb_adds(&b, ",\"displayName\":"); sb_json_string(&b, c->displayName);
        sb_adds(&b, ",\"version\":"); sb_json_string(&b, c->version);
        sb_adds(&b, ",\"authKind\":"); sb_json_string(&b, c->authKind);
        sb_adds(&b, ",\"capabilities\":"); sb_str_array_compact(&b, c->caps, c->ncaps);
        sb_adds(&b, ",\"tools\":"); sb_str_array_compact(&b, c->tools, c->ntools);
        sb_adds(&b, ",\"description\":"); sb_json_string(&b, c->description);
        sb_adds(&b, ",\"enabled\":"); sb_adds(&b, c->enabled ? "true" : "false");
        sb_addc(&b, '}');
    }
    sb_addc(&b, ']');
    return sb_take(&b);
}

static char* ser_status(const char* name) {
    conn_t* c = registry_find(name);
    if (!c) {
        char buf[512];
        snprintf(buf, sizeof buf, "{\"error\":\"connector '%s' not found\"}", name ? name : "");
        return dup_str(buf);
    }
    sb_t b; sb_init(&b);
    sb_adds(&b, "{\n");
    sb_ind(&b, 1); sb_adds(&b, "\"name\": "); sb_json_string(&b, c->name); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"kind\": "); sb_json_string(&b, c->kind); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"displayName\": "); sb_json_string(&b, c->displayName); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"version\": "); sb_json_string(&b, c->version); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"authKind\": "); sb_json_string(&b, c->authKind); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"enabled\": "); sb_adds(&b, c->enabled ? "true" : "false"); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"description\": "); sb_json_string(&b, c->description); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"capabilities\": ");
    if (c->ncaps == 0) sb_adds(&b, "[]");
    else { sb_adds(&b, "[\n"); for (int i = 0; i < c->ncaps; i++) { sb_ind(&b, 2); sb_json_string(&b, c->caps[i]); sb_adds(&b, i + 1 < c->ncaps ? ",\n" : "\n"); } sb_ind(&b, 1); sb_adds(&b, "]"); }
    sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"tools\": ");
    if (c->ntools == 0) sb_adds(&b, "[]");
    else { sb_adds(&b, "[\n"); for (int i = 0; i < c->ntools; i++) { sb_ind(&b, 2); sb_json_string(&b, c->tools[i]); sb_adds(&b, i + 1 < c->ntools ? ",\n" : "\n"); } sb_ind(&b, 1); sb_adds(&b, "]"); }
    sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"session\": null\n");
    sb_adds(&b, "}");
    return sb_take(&b);
}

static char* ser_tools(const char* name) {
    conn_t* c = registry_find(name);
    if (!c || c->ntools == 0) return dup_str("[]");
    sb_t b; sb_init(&b);
    sb_adds(&b, "[\n");
    for (int i = 0; i < c->ntools; i++) {
        char desc[512];
        snprintf(desc, sizeof desc, "%s tool %s", c->displayName ? c->displayName : c->name, c->tools[i]);
        sb_ind(&b, 1); sb_adds(&b, "{\n");
        sb_ind(&b, 2); sb_adds(&b, "\"name\": "); sb_json_string(&b, c->tools[i]); sb_adds(&b, ",\n");
        sb_ind(&b, 2); sb_adds(&b, "\"description\": "); sb_json_string(&b, desc); sb_adds(&b, ",\n");
        sb_ind(&b, 2); sb_adds(&b, "\"inputSchemaJson\": \"{}\",\n");
        sb_ind(&b, 2); sb_adds(&b, "\"outputSchemaJson\": \"{}\",\n");
        sb_ind(&b, 2); sb_adds(&b, "\"permission\": \"read\",\n");
        sb_ind(&b, 2); sb_adds(&b, "\"riskLevel\": \"LOW\",\n");
        sb_ind(&b, 2); sb_adds(&b, "\"requiresApproval\": false,\n");
        sb_ind(&b, 2); sb_adds(&b, "\"isHighRisk\": false\n");
        sb_ind(&b, 1); sb_adds(&b, "}");
        sb_adds(&b, i + 1 < c->ntools ? ",\n" : "\n");
    }
    sb_adds(&b, "]");
    return sb_take(&b);
}

static char* ser_search(const char* cap) {
    registry_init();
    if (!cap) return dup_str("[]");
    sb_t b; sb_init(&b);
    sb_adds(&b, "[\n");
    int first = 1;
    for (int i = 0; i < g_nconns; i++) {
        conn_t* c = &g_conns[i];
        int match = 0;
        for (int k = 0; k < c->ncaps && !match; k++) if (ci_contains(c->caps[k], cap)) match = 1;
        for (int k = 0; k < c->ntools && !match; k++) if (ci_contains(c->tools[k], cap)) match = 1;
        if (!match && ci_contains(c->name, cap)) match = 1;
        if (!match) continue;
        if (!first) sb_adds(&b, ",\n");
        first = 0;
        sb_def_full(&b, c, 1);
    }
    if (first) { free(b.p); return dup_str("[]"); }
    sb_adds(&b, "\n]");
    return sb_take(&b);
}

static char* ser_resources(const char* name) {
    conn_t* c = registry_find(name);
    if (!c || c->ntools == 0) return dup_str("[]");
    sb_t b; sb_init(&b);
    sb_adds(&b, "[\n");
    for (int i = 0; i < c->ntools; i++) {
        char desc[512];
        snprintf(desc, sizeof desc, "%s tool %s", c->displayName ? c->displayName : c->name, c->tools[i]);
        sb_ind(&b, 1); sb_adds(&b, "{\n");
        sb_ind(&b, 2); sb_adds(&b, "\"uri\": \"connector://"); sb_adds(&b, name); sb_adds(&b, "/"); sb_adds(&b, c->tools[i]); sb_adds(&b, "\",\n");
        sb_ind(&b, 2); sb_adds(&b, "\"name\": "); sb_json_string(&b, c->tools[i]); sb_adds(&b, ",\n");
        sb_ind(&b, 2); sb_adds(&b, "\"description\": "); sb_json_string(&b, desc); sb_adds(&b, "\n");
        sb_ind(&b, 1); sb_adds(&b, "}");
        sb_adds(&b, i + 1 < c->ntools ? ",\n" : "\n");
    }
    sb_adds(&b, "]");
    return sb_take(&b);
}

char* gmb_connector_marketplace_list(void) { return dup_str(marketplace_json); }

char* gmb_connector_marketplace_get(const char* name) {
    if (!name) return dup_str("{}");
    // Estrae l'entry dal catalogo statico (parità Basic.Connectors.ConnectorRegistry.MarketplaceGet
    // → JSON della ConnectorDefinition). Ogni entry è un oggetto piatto (nessun '{' annidato),
    // quindi si isola dal '{' che apre l'oggetto al primo '}' successivo.
    char needle[512];
    snprintf(needle, sizeof(needle), "\"name\":\"%s\"", name);
    const char* p = strstr(marketplace_json, needle);
    if (!p) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"connector '%s' not in marketplace\"}", name);
        return dup_str(buf);
    }
    const char* start = p;
    while (start > marketplace_json && *start != '{') start--;
    const char* end = p;
    while (*end && *end != '}') end++;
    size_t len = (size_t)(end - start) + (*end == '}' ? 1u : 0u);
    char* out = (char*)malloc(len + 1);
    if (!out) return dup_str("{}");
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

char* gmb_connector_list(void) { return ser_list(); }
char* gmb_connector_status(const char* name) { return ser_status(name); }
char* gmb_connector_tools(const char* name) { return ser_tools(name); }
char* gmb_connector_search(const char* cap) { return ser_search(cap); }

int gmb_connector_register(const char* name, const char* kind, const char* cfg) {
    (void)cfg;
    if (!name) return 0;
    conn_t* c = registry_find(name);
    if (c) {
        if (kind) { free(c->kind); c->kind = dup_str(kind); }
        return 1;
    }
    c = registry_add(name);
    if (!c) return 0;
    c->kind = dup_str(kind ? kind : name);
    c->displayName = dup_str(name);
    c->version = dup_str("1.0");
    c->authKind = dup_str("none");
    c->caps = (char**)calloc(1, sizeof(char*)); c->caps[0] = dup_str(c->kind); c->ncaps = 1;
    c->tools = (char**)calloc(1, sizeof(char*)); c->tools[0] = dup_str(c->kind); c->ntools = 1;
    { char d[512]; snprintf(d, sizeof d, "Connector %s (%s)", name, c->kind); c->description = dup_str(d); }
    c->enabled = 1;
    return 1;
}

int gmb_connector_unregister(const char* name) {
    conn_t* c = registry_find(name);
    if (c) registry_remove(c);
    return 1;
}

int gmb_connector_load(const char* name) {
    if (!name) return 0;
    conn_t* c = registry_find(name);
    if (!c) { gmb_connector_register(name, name, NULL); c = registry_find(name); }
    if (c) c->enabled = 1;
    return 1;
}

int gmb_connector_enable(const char* name, int en) {
    conn_t* c = registry_find(name);
    if (c) c->enabled = en ? 1 : 0;
    return 1;
}

char* gmb_connector_call(const char* c, const char* tool, const char* args) {
    (void)args;
    conn_t* ent = registry_find(c);
    const char* kind = ent && ent->kind ? ent->kind : "";
    char note[640];
    snprintf(note, sizeof note,
        "native connector '%s' stub \xe2\x80\x94 F1 pending (%s), use CONNECTOR MARKETPLACE GET to load full impl",
        c ? c : "", kind);
    sb_t b; sb_init(&b);
    sb_adds(&b, "{\n");
    sb_ind(&b, 1); sb_adds(&b, "\"connector\": "); sb_json_string(&b, c); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"tool\": "); sb_json_string(&b, tool); sb_adds(&b, ",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"args\": {},\n");
    sb_ind(&b, 1); sb_adds(&b, "\"status\": \"ok\",\n");
    sb_ind(&b, 1); sb_adds(&b, "\"note\": "); sb_json_string(&b, note); sb_adds(&b, "\n");
    sb_adds(&b, "}");
    return sb_take(&b);
}

int gmb_connector_refresh(const char* name) { (void)name; return 1; }
char* gmb_connector_policy_get(const char* c, const char* t) { const char* p = conn_pol_get(registry_find(c), t); return dup_str(p ? p : "allow"); }
int gmb_connector_policy_set(const char* c, const char* t, const char* p) { conn_pol_set(registry_find(c), t, p); return 1; }
char* gmb_connector_secret_get(const char* c, const char* k) { const char* v = conn_sec_get(registry_find(c), k); return dup_str(v ? v : ""); }
int gmb_connector_secret_set(const char* c, const char* k, const char* v) { conn_sec_set(registry_find(c), k, v); return 1; }
int gmb_connector_auth(const char* c, const char* j) {
    conn_t* ent = registry_find(c);
    if (!ent || !j) return 0;
    gmb_dict_t d = gmb_dict_new();
    gmb_json_parse_to_dict(j, &d);
    /* api_key (da "api_key" o "apiKey") */
    const char* ak = val_str(gmb_dict_get(d, "api_key"));
    if (!ak) ak = val_str(gmb_dict_get(d, "apiKey"));
    if (ak) conn_sec_set(ent, "api_key", ak);
    const char* at = val_str(gmb_dict_get(d, "access_token"));
    if (at) conn_sec_set(ent, "access_token", at);
    const char* rt = val_str(gmb_dict_get(d, "refresh_token"));
    if (rt) conn_sec_set(ent, "refresh_token", rt);
    const char* ci = val_str(gmb_dict_get(d, "client_id"));
    if (ci) conn_sec_set(ent, "client_id", ci);
    if (at) conn_sec_set(ent, "oauth_token", j);
    return 1;
}
char* gmb_connector_resources(const char* n) { return ser_resources(n); }
char* gmb_connector_prompts(const char* n) { (void)n; return dup_str("[]"); }
char* gmb_connector_oauth_create_verifier(int len) {
    if (len<43||len>128) len=64;
    const char* cs="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    char* out=(char*)malloc(len+1);
    if(!out) return NULL;
    for(int i=0;i<len;i++) out[i]=cs[rand()%64];
    out[len]='\0';
    return out;
}
char* gmb_connector_oauth_create_challenge(const char* v) {
    if(!v) return dup_str("");
    // Semplificato: ritorna verifier (SHA256+base64url richiede OpenSSL — stub per F0)
    char buf[256]; snprintf(buf,sizeof(buf),"challenge_%s", v);
    return dup_str(buf);
}
char* gmb_connector_oauth_build_auth_url(const char* ep, const char* cid, const char* ru, const char* sc, const char* st, const char* ver) {
    char buf[2048];
    snprintf(buf,sizeof(buf),"%s?response_type=code&client_id=%s&redirect_uri=%s&scope=%s&state=%s&code_challenge=%s&code_challenge_method=S256",
        ep?ep:"", cid?cid:"", ru?ru:"", sc?sc:"", st?st:"", ver?ver:"");
    return dup_str(buf);
}
char* gmb_connector_oauth_get_auth_header(const char* c) { (void)c; return dup_str(""); }
