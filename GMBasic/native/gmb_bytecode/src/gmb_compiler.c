/* native/gmb_runtime_c/src/compiler/gmb_compiler.c — compilatore P-Code C (speculare a
 * common/Compiler/Compiler.cs): AST → bytecode VM → chunk serializzato A1/GMBC.
 * Copre il nucleo del linguaggio (espressioni, assign, print, dim, if/while/do/for,
 * goto/gosub/label, return, sub/function, call, data/read/restore, end, select-case
 * base). I costrutti non ancora compilabili producono un errore esplicito (divergenza). */
#include "gmb_compiler.h"
#include "gmb_inc_resolve.h"   /* gmb_exe_dir: ancora di <exe>/packages (installazione locale) */
#include "gmb_semver.h"
#include "gmb_linker.h"
#include "gmb_lexer.h"
#include "gmb_macro_expand.h"
#include "gmb_members.h"
#include "gmb_mcp_schema.h"
#include "gmb_parser.h"
#include "gen/gmb_registry.gen.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

/* Tipo di ritorno numerico (default 0): NULL = numerico implicito. */
static int cc_ret_numeric(const char* t)
{
    if (!t || !*t) return 1;
    return strcasecmp(t, "INTEGER") == 0 || strcasecmp(t, "LONG") == 0
        || strcasecmp(t, "DOUBLE") == 0 || strcasecmp(t, "SINGLE") == 0
        || strcasecmp(t, "CURRENCY") == 0 || strcasecmp(t, "BYTE") == 0
        || strcasecmp(t, "SBYTE") == 0 || strcasecmp(t, "BIT") == 0
        || strcasecmp(t, "QUAD") == 0 || strcasecmp(t, "ULONG") == 0
        || strcasecmp(t, "UINTEGER") == 0 || strcasecmp(t, "UBYTE") == 0;
}

/* Tuple nominati: nome variabile -> CSV dei campi (solo per l'accesso t.campo).
 * Scoped alla procedura in compilazione (cc_tuple_clear all'ingresso di ogni proc). */
typedef struct { char* var; char* fields; } cc_tuple_map_t;
static cc_tuple_map_t g_tuple_maps[64];
static int g_ntuple_maps;

static void cc_tuple_clear(void)
{
    for (int i = 0; i < g_ntuple_maps; i++) { free(g_tuple_maps[i].var); free(g_tuple_maps[i].fields); }
    g_ntuple_maps = 0;
}
static void cc_tuple_set(const char* var, const char* fields)
{
    if (!var || !fields || g_ntuple_maps >= 64) return;
    for (int i = 0; i < g_ntuple_maps; i++) {
        if (strcmp(g_tuple_maps[i].var, var) == 0) {
            free(g_tuple_maps[i].fields);
            g_tuple_maps[i].fields = strdup(fields);
            return;
        }
    }
    g_tuple_maps[g_ntuple_maps].var = strdup(var);
    g_tuple_maps[g_ntuple_maps].fields = strdup(fields);
    g_ntuple_maps++;
}
static int cc_tuple_field_index(const char* var, const char* field)
{
    if (!var || !field) return -1;
    for (int i = 0; i < g_ntuple_maps; i++) {
        if (strcmp(g_tuple_maps[i].var, var) != 0) continue;
        const char* p = g_tuple_maps[i].fields;
        int idx = 0;
        while (p) {
            const char* comma = strchr(p, ',');
            size_t n = comma ? (size_t)(comma - p) : strlen(p);
            if (n == strlen(field) && strncmp(p, field, n) == 0) return idx;
            if (!comma) break;
            p = comma + 1;
            idx++;
        }
    }
    return -1;
}

/* builtin comuni: nome -> BuiltInFunction id (OP_CALL_BUILTIN) */
typedef struct { const char* name; int code; } cc_builtin_t;
static const cc_builtin_t g_cc_builtins[] = {
    {"LEN",25},{"ABS",19},{"INT",10},{"FIX",9},{"SQR",30},{"SIN",29},{"COS",23},{"TAN",32},
    {"ATN",21},{"EXP",24},{"LOG",26},{"RND",27},{"ASC",20},{"CHR",22},{"VAL",1},{"STR",31},
    {"MID",301},{"LEFT",299},{"RIGHT",300},{"TRIM",314},{"LTRIM",315},{"RTRIM",316},
    {"REPLACE",317},{"REPLACE$",317},
    {"INSTR",302},{"JOIN",322},{"SPLIT",321},{"UPPER",307},{"LOWER",306},{"ROUND",12},{"FLOOR",11},
    {"CEIL",0},{"LEN$",25},
    {"RDD.FLOCK",471},{"RDD.CURVAL",745},{"RDD.BUFFER",724},{"PROVIDER.SETPRIORITY",420},
    {"SQL.LASTINSERTROWID",443},{"SYSTEM.NUMCPU",1209},{"TIMESERIAL",277},{"DATE",293},
    {"IO.FileExists",567},{"IO.FILEEXISTS",567},{"MATRIX.CREATE",608},{"POOL.PUT",1202},
    {"AI.AGENTCODE",1439},{"AI.AGENTFIX",1440},{"AI.AGENTRUN",1441},
    /* AI.AGENTLIST/AGENTEVENTS: il VM/C li implementa (496/499, tabella ext) ma il
     * compilatore A1 non ne mappava il nome → "AGENTLIST non definita" sulle pagine BSP. */
    {"AI.AGENTLIST",496},{"AI.AGENTEVENTS",499},
    {"AI.AGENTGET",490},{"AI.MEMORYASK",739},
    {"AI.WORKFLOWLIST",483},{"AI.WORKFLOWSTATUS",484},{"AI.WORKFLOWSTEPS",485},
    {"AI.AGENTASKSYNC",999},{"WINDOWS.X",600},{"WINDOWS.THEME",601},{"WINDOWS.DESKTOP",602},
    {"MATH.FORMAT",295},{"AI.OCR",684},{"AI.ACTIVE",414},
    {"AI.PROVIDERS",686},{"AI.PLUGINSLIST",1217},{"AI.TRANSCRIBE",685},{"CONVERT.FORMATS",999},

    {"AI.COUNT",328},
    /* MCP (funzioni). La tabella aveva SOLO 3 voci e una aveva il codice SBAGLIATO:
     * MCPSERVERMARKETPLACELIST=723 è in realtà BuiltInFunction.MCPSERVERDISABLE (la lista
     * marketplace è 730) → su VM/C quel comando disabilitava un server invece di elencare
     * il marketplace. Aggiunte anche le altre funzioni MCP: senza voce il compilatore C
     * rifiutava AI.MCPPROMPTS()/AI.MCPCALL()/… con "simbolo non trovato", mentre gli altri
     * 4 backend le risolvevano (parità rotta). Numerazione = BuiltInFunction / tabella
     * gmb_bf_impl di gmb_builtin.c. */
    {"AI.MCPLISTTOOLS",434},
    {"AI.MCPRESOURCES",437},
    {"AI.MCPPROMPTS",438},
    {"AI.MCPLISTTOOLSSERVER",732},
    {"AI.MCPCALL",727},
    {"AI.MCPSERVERLIST",725},
    {"AI.MCPSERVERSTATUS",726},
    {"AI.MCPSERVERMARKETPLACELIST",730},
    {"AI.MCPSERVERMARKETPLACEGET",731},
    {"AI.SEARCH",507},
    {"AI.RAGCONTEXT",508},
    {"AI.ASK",509},
    {"AI.TEAMAGENTS",482},
    {"LLM.DISTILLSTATUS",2029},{"LLM.DISTILLSAMPLEN",2031},{"LLM.DISTILLSAMPLEFAIL",2032},
    {"LLM.DISTILLSAMPLEDONE",2215},{"LLM.DISTILLEVALTOK",2206},{"LLM.DISTILLEVALPASS",2207},
    {"LLM.DISTILLLOSS",2211},{"LLM.DISTILLHOLD",2212},{"LLM.DISTILLCKPT",2213},{"LLM.DISTILLDONE",2214},
    {"AI.TEAMLIST",481},
    {"AI.TEAMRECURSION",1262},
    {"AI.TEAMTRAINSTATUS",1260},
    {"AI.TEAMLINKINFO",1261},
    {"AI.TEAMLATENT",1263},
    {"AI.TEAMRUNTIMESTATS",1264},
    {"AI.CONTEXTUSED",3552},{"AI.CONTEXTBUDGET",3553},{"AI.CONTEXTOVERFLOW",3554},
    {"AI.CONTEXTVIEW",3555},{"AI.CONTEXTVIEWDEBUG",3556},
    {"AI.CONTEXTSET",3557},{"AI.CONTEXTVIEWCMD",3558},
    {"AI.CONTEXTCREATE",3735},{"AI.CONTEXTADD",3736},{"AI.CONTEXTMAP",3737},
    {"AI.CONTEXTTRIM",3738},{"AI.CONTEXTGC",3739},{"AI.CONTEXTCOMMIT",3740},
    {"AI.CONTEXTHASH",3741},{"AI.CONTEXTVALID",3742},{"AI.TASKMETRICS",3743},
    {"AI.WORKERNEW",3559},{"AI.WORKERRUN",3560},{"AI.STATESAVE",3561},{"AI.STATELOAD",3562},
    {"AI.WORKERROUNDS",3563},{"AI.WORKERRESCUED",3564},{"AI.WORKERSPEND",3565},
    {"ALERT",699},
    {"ANALYSIS.PCA",95},
    {"ANALYSIS.REGRESS",91},
    {"ANALYSIS.TRAPEZOID",851},
    {"ANALYSIS.FFT",105},{"ANALYSIS.IFFT",106},{"ANALYSIS.KMEANS",92},
    /* Bug corretto (2026-09-02, seguito task AEVAL/ClipperCodeBlocks): la VM/C ha già
     * l'handler funzionante per queste tre (gmb_bf_103/104/251 in gmb_builtin.c, che
     * chiamano gmb_quad_gk/differenza-centrale/gmb_opt_newton_ctx — motori numerici già
     * usati altrove) ma NESSUNA delle tre era mai stata registrata qui con nessun nome:
     * ANALYSIS.INTEGRATE/DIFFERENTIATE e OPTIMIZE.NEWTON restituivano sempre vuoto,
     * indistinguibile da un bug nella logica numerica invece che da un semplice buco di
     * registrazione — stesso pattern di g_cc_builtins già visto più volte. */
    {"ANALYSIS.INTEGRATE",103},{"ANALYSIS.DIFFERENTIATE",104},
    {"OPTIMIZE.NEWTON",251},
    {"OPTIMIZE.CURVEFIT",888},{"CURVEFIT",888},   /* gmb_bf_888: closure(model) + x,y,p0 liste */
    {"SIGNAL.CONVOLVE",923},{"SIGNAL.CORRELATE",924},
    {"SIGNAL.HANN",925},{"SIGNAL.HAMMING",926},{"SIGNAL.BLACKMAN",927},
    {"ARRAYS.AJOIN",523},
    {"ARRAYS.ASORT",535},{"ASORT",535},   /* funzione ARRAYS.ASORT(list): ordina in place e ritorna la lista (535 = builtin sort; a differenza dello statement, NON fa POP) */
    /* Bug corretto (2026-08-31, audit lambda in builtin): AFILTER/AMAP/AREDUCE/AUNIQUE/
     * ASCAN/ACOUNT/AEVAL non erano MAI registrati qui (assenti dalla tabella) pur avendo
     * già un handler funzionante lato VM (gmb_builtin.c, gmb_bf_514/515/517/518/532) — ogni
     * chiamata cadeva nel fallback generico "proc sconosciuta" (no-op per compile-parity:
     * consuma gli argomenti, restituisce NIL), dando sempre una lista vuota / 0 invece del
     * risultato reale (bug: FunctionsLambda.bas, AFILTER/AMAP/CountIf danno sempre
     * vuoto/0). I codici DEVONO combaciare esattamente con l'enum C# BuiltInFunction
     * (common/Compiler/BuiltInFunction.cs) e con la tabella di dispatch VM sopra citata.
     * ACOUNT(519)/AMAP(515)/AFILTER(516) condividevano inoltre un bug gemello lato VM
     * (gmb_bf_515: due controlli su func_code mai veri, AMAP/ACOUNT eseguivano sempre la
     * sola logica di AFILTER) — corretto contestualmente in gmb_builtin.c. */
    {"ASCAN",514},{"AMAP",515},{"AFILTER",516},{"AREDUCE",517},{"AUNIQUE",518},
    {"ACOUNT",519},{"AEVAL",532},
    {"BT.RUN",624},
    {"CHR",22},
    {"CINT",0},
    {"COLLIDEBATCH",461},
    {"COMMAND",657},
    {"COMMANDCOUNT",656},
    {"CONFIRM",700},
    {"CONN",556},
    {"CONSOLE.GETCURSORPOS",674},
    {"CONVERT.PARSE",758},
    {"CSNG",2},
    {"DATEVALUE",275},
    {"DETERMINANT",266},
    {"DT.DATEVALUE",275},
    {"DUP",303},
    {"FINANCE.PMTEXT",140},
    {"FINANCE.PV",143},
    {"FETCH",563},
    {"FORMAT",295},
    {"IIF",537},
    {"IO.FOPEN",546},
    {"IO.LOF",553},
    {"RANGE",3432},
    {"CSTR",3431},
    {"CStr",3431},
    {"DIRECTCAST",3420},
    {"Directcast",3420},
    {"JSON.EXTRACT",3407},
    {"JSON.Extract",3407},
    {"JSON.Load",3405},
    {"JSON.Loadfile",3405},
    {"JSON.ToJson",3406},
    {"JSON.Tojson",3406},
    {"JSON.ToObject",3408},
    {"JSON.Toobject",3408},
    {"JSON.Keys",3404},
    {"JSON.KEYS",3404},
    {"JSON.ReadValue",3402},
    {"JSON.Readvalue",3402},
    {"JSON.ReadSection",3403},
    {"JSON.Readsection",3403},
    {"JSON.Keys",3404},
    {"JSON.Parse",758},
    {"JSON.Stringify",760},
    {"LBOUND",33},
    {"LOCALPORT",560},
    {"MATH.AVG",620},
    {"MATH.FACT",54},
    {"MATH.LOG",26},
    {"MATH.MIN",44},
    {"MATH.RND",27},
    {"MATH.ROUND",12},
    {"MATH.SIN",29},
    {"MATRIX.DETERMINANT",266},
    {"MUTEX.TRYLOCK",1207},
    {"SYNCMAP.LOAD",1204},{"SYNCMAP.DELETE",1205},{"SYNCMAP.LOADORSTORE",1206},
    {"POOL.GET",1201},
    {"ATOMIC.INCREMENT",1156},{"ATOMIC.FETCHVALUE",1157},{"ATOMIC.STORE",1158},
    {"ATOMIC.COMPARESWAP",1159},{"ATOMIC.SWAP",1165},
    {"CONTEXT.BACKGROUND",1160},{"CONTEXT.WITHCANCEL",1161},{"CONTEXT.CANCEL",1162},
    {"CONTEXT.DONE",1163},{"CONTEXT.CANCELLED",1164},
    {"NEXT",655},
    {"NOW",285},
    {"OPT.MINVOLATILITY",183},
    {"PETSCII",324},
    {"PI",43},
    {"PIXELBUFFER",598},
    {"PROVIDER.ACTIVE",414},
    {"PROVIDER.REGISTER",409},
    {"PROVIDER.SETACTIVE",412},
    {"RDD.GETFIELD",726},
    {"RDD.RLOCK",470},
    {"RDD.TODAY",286},
    {"RGB",597},
    {"SCREENS",588},
    {"SIZEOF",594},
    /* BUG REALE trovato 2026-09-07 (FINANCE.IRR su VM/C sempre 0): il runtime
     * (gmb_fin_irr, gmb_runtime_c/src/gmb_finance.c) e l'handler VM (gmb_bf_148 in
     * gmb_vm_c/gmb_builtin.c, gia' registrato al codice 148 e gia' funzionante --
     * estrae l'array FLUSSI, chiama gmb_fin_irr) esistevano ENTRAMBI e funzionavano
     * -- mancava SOLO questa entry: senza di essa il compilatore non riconosce "IRR"
     * come builtin (nessun errore, IRR(...) veniva risolta altrove/silenziosamente
     * come 0, mai un GMB_OP_CALL_BUILTIN col codice giusto). Stesso schema di SLN
     * sotto (bare name, IMPORT FINANCE la rende visibile senza qualifica). */
    {"IRR",148},
    {"SLN",153},
    /* Stesso gap di IRR per le altre metriche FINANCE: handler VM gia' scritti e
     * registrati (gmb_bf_147=147 NPV, 150=MNPV, 149=MIRR, 151=XNPV, 152=XIRR) —
     * mancavano solo le entry in g_cc_builtins, quindi le chiamate FINANCE.NPV/... 
     * venivano risolte a 0 silenziosamente. Aggiunte come bare name (specchio IRR/SLN). */
    {"NPV",147},
    {"MNPV",150},
    {"MIRR",149},
    {"XNPV",151},
    {"XIRR",152},
    {"SPECIAL.GAMMA",172},
    {"SQL.FETCH",440},
    {"STATS.NORMALCDF",58},
    {"STATS.NORMALPPF",864},
    {"STATS.TTEST",731},
    {"STR",31},
    {"SYNCMAP.STORE",1203},
    {"SYS",593},
    {"SYSTEM.GOMAXPROCS",1155},
    {"SYSTEM.NUMGOROUTINE",1208},
    {"SYSTEM.SIZEOF",594},
    {"SYSTEM.VALTYPE",649},
    {"TEXTWIDTH",583},
    {"TM.DILUTEDEPS",225},
    {"TM.TotalReturn",202},
    {"TOHEX",369},
    {"TP.MAXRETURN",193},
    {"TP.PORTFOLIOMAD",196},
    {"TRADING.TOTALSTOCKRETURN",238},
    {"TYPEINFO",650},
    {"TYPENAME",648},
    {"UBOUND",34},
    {"VECTORDB.COUNT",328},
    {"VECTORDB.PRECISIONAT",3530},
    {"VECTORDB.RECALLAT",3531},
    {"VECTORDB.MRR",3532},
    {"VECTORDB.NDCGAT",3533},
    {"VECTORDB.FAITHFULNESS",3534},
    {"VECTORDB.RELEVANCE",3535},
    {"VECTORDB.HALLUCINATION",3536},
    {"VECTORDB.COMPLETENESS",3537},
    {"VECTORDB.GENTEST",3541},
    {"VECTORDB.DIAGNOSE",3543},
    {"VECTORDB.DIAGNOSEREPORT",3544},
    {"VECTORDB.CALIBRATE",3545},
    {"VECTORDB.EVALGATE",3546},
    {"VECTORDB.EVALDIFF",3547},
    {"VECTORDB.BASELINE",3548},
    {"VECTORDB.DRIFT",3549},
    {"VECTORDB.ABTEST",3550},
    {"VECTORDB.COMPARE",3551},
    {"OBSERVEFILE",3512},{"OBSERVELOG",3513},{"OBSERVESCREEN",3514},
    {"OBSERVATIONLATEST",3515},{"OBSERVATION",3516},{"OBSERVATIONHASH",3517},
    {"OBSERVATIONCOUNT",3518},{"OBSERVATIONLIST",3519},{"OBSERVATIONFILE",3520},
    {"AI.OBSERVEFILE",3512},{"AI.OBSERVELOG",3513},{"AI.OBSERVESCREEN",3514},
    {"AI.OBSERVATIONLATEST",3515},{"AI.OBSERVATION",3516},{"AI.OBSERVATIONHASH",3517},
    {"AI.OBSERVATIONCOUNT",3518},{"AI.OBSERVATIONLIST",3519},{"AI.OBSERVATIONFILE",3520},
    {"WRAP",320},
    {"nums.Count",328},
    {"CWD",566},
    {"IO.CWD",566},
    {"FILEEXISTS",567},
    {"IO.FILEEXISTS",567},
    {"DIREXISTS",568},
    {"IO.DIREXISTS",568},
    {"GETFOLDERPATH",569},
    {"IO.GETFOLDERPATH",569},
    {"NORMALIZEPATH",570},
    {"IO.NORMALIZEPATH",570},
    {"FOLDERSIZE",571},
    {"IO.FOLDERSIZE",571},
    {"DIR",572},
    {"IO.DIR",572},
    {"GETTEMPFILE",573},
    {"IO.GETTEMPFILE",573},
    {"GETTEMPDIR",574},
    {"IO.GETTEMPDIR",574},
    {"APPPATH",575},
    {"IO.APPPATH",575},
    {"BINPATH",576},
    {"IO.BINPATH",576},
    {"PROJECTFOLDER",577},
    {"IO.PROJECTFOLDER",577},
    {"PARENTDIR",578},
    {"IO.PARENTDIR",578},
    {"READBYTES",579},
    {"IO.READBYTES",579},
    {"READLINES",580},
    {"IO.READLINES",580},
    {"READFILE",3806},
    {"IO.READFILE",3806},
    {"COUNTLINES",581},
    {"IO.COUNTLINES",581},
    {"RESOLVEFILEPATH",582},
    {"IO.RESOLVEFILEPATH",582},
    /* ── Second Brain (AI/GRAPH): codici BuiltInFunction C# 1272-1348 ── */
    {"AI.VAULTCREATE",1272},{"AI.VAULTINDEX",1273},{"AI.VAULTINFO",1274},{"AI.VAULTFOLDERS",1275},
    {"AI.VAULTCOUNT",1276},{"AI.VAULTLIST",1277},{"AI.BRAINUSE",1278},{"AI.BRAINDETACH",1279},
    {"LLM.RUNSJSON",2300},{"LLM.RUNSCOUNT",2301},{"LLM.RUNSCLEAR",2302},{"LLM.TRAINCONFIG",2303},{"LLM.TRAINCONFIGSET",2304},{"LLM.RUNSITEM",2305},{"LLM.DATASETSTATS",2306},{"LLM.LORALIST",2307},{"LLM.TRAINEXPORT",2309},{"LLM.RUNSTOP",2310},{"LLM.EVALGATE",2311},{"LLM.LORASAVE",2312},{"LLM.LORALOAD",2313},
    {"AI.BRAININSTALL",1280},{"AI.BRAINUNINSTALL",1281},{"AI.BRAINLIST",1282},{"AI.BRAININFO",1283},
    {"AI.BRAINUSED",1284},{"AI.SEARCHMULTI",1285},{"AI.RAGCONTEXTMULTI",1286},{"AI.ASKMULTI",1287},
    {"GRAPH.QUERYMULTI",1288},{"GRAPH.NEIGHBORS",1289},{"GRAPH.PATH",1290},{"GRAPH.PAGERANK",1291},
    {"GRAPH.CONNECTED",1292},{"GRAPH.STATS",1293},{"GRAPH.STRONGLYCONNECTED",1294},{"GRAPH.TOPOSORT",1295},
    {"GRAPH.BIPARTITE",1296},{"GRAPH.BETWEENNESS",1297},{"GRAPH.CLUSTERING",1298},{"GRAPH.DIAMETER",1299},
    {"GRAPH.MST",1300},{"GRAPH.LOUVAIN",3790},{"GRAPH.MAXFLOW",3791},{"NOTE.CREATE",1301},{"NOTE.UPDATE",1302},{"NOTE.APPEND",1303},
    {"NOTE.DELETE",1304},{"NOTE.MOVE",1305},{"NOTE.GET",1306},{"NOTE.TITLE",1307},{"NOTE.TAGS",1308},
    {"NOTE.WORDCOUNT",1309},{"NOTE.EXISTS",1310},{"NOTE.FOLDER",1311},{"LINKSOUT",1312},{"BACKLINKS",1313},
    {"LINKDANGLING",1314},{"LINKJSON",1315},{"INGEST.FILE",1316},{"INGEST.FOLDER",1317},
    {"AI.MEMORYRANK",1318},{"AI.MEMORYCONSOLIDATE",1319},{"GRAPH.BUILD",1320},{"GRAPH.UPDATE",1321},
    {"GRAPH.FINDPATH",1322},{"GRAPH.RDD",1323},{"AI.ENRICH",1324},{"AI.QAANSWER",1325},{"AI.SUGGESTTAGS",1326},
    {"AI.SIGNNOTE",1327},{"AI.VERIFYNOTE",1328},{"AI.BRAINVAULTBUILD",1329},{"AI.BRAINVAULTQUERY",1330},
    {"AI.BRAINVAULTWRITE",1331},{"GRAPH.DOT",1332},{"GRAPH.SUBGRAPH",1333},{"GRAPH.BYTAG",1334},
    {"GRAPH.BYFOLDER",1335},{"GRAPH.ISVALID",1336},{"AI.SHAREENCRYPT",1337},{"AI.SHAREDECRYPT",1338},
    {"AI.VAULTWATCH",1339},{"AI.VAULTPOLICY",1340},{"AI.VAULTADDFOLDER",1341},{"AI.VAULTREMOVEFOLDER",1342},
    {"AI.VAULTSETFOLDERPROPS",1343},{"AI.NOTELINK",1344},{"AI.NOTEUNLINK",1345},{"AI.NOTESETALIAS",1346},
    {"AI.SYNCFOLDER",1347},{"AI.INGESTWEB",1348},
    /* BROWSER.* (1375-1380) — CDP nativo C (gmb_browser_cdp / gmb_cua_browser_*): naviga via
     * Chromium/Obscura su GMBASIC_CDP_PORT (default 9222). Il VM/C ha gia' gli handler
     * gmb_bf_cua_browser_act(1375-1379)/gmb_bf_cua_browser_start(1380); mancavano le entry qui
     * -> la compilazione nativa di BROWSER.* falliva silenziosamente (rc=1). */
    {"BROWSER.NAVIGATE",1375},{"BROWSER.SNAPSHOT",1376},{"BROWSER.CLICK",1377},
    {"BROWSER.FILL",1378},{"BROWSER.EVALUATE",1379},{"BROWSER.START",1380},
    {"BROWSER.ACCESSTREE",1511},{"WEB",683},{"WEB2MD",683},{"BROWSER.GETCOOKIES",1513},
    {"BROWSER.SETCOOKIE",1514},{"BROWSER.CLEARCOOKIES",1515},
    {"BROWSER.SETUSERAGENT",1517},{"BROWSER.HUMANCLICK",1518},
    {"BROWSER.HUMANFILL",1519},
};

/* costanti builtin %NOME — GENERATE da canonical.registry.json (P0, gmb_pconst_gen.inc):
 * tutte le 277 costanti della superficie canonica (inclusi capability/K_*). */
typedef struct { const char* name; double v; } cc_pconst_t;
static const cc_pconst_t g_cc_pconsts[] = {
#include "gen/gmb_pconst_gen.inc"
};
static int cc_pconst_lookup(const char* name, double* out)
{
    for (size_t i = 0; i < sizeof(g_cc_pconsts) / sizeof(g_cc_pconsts[0]); i++)
        if (strcasecmp(g_cc_pconsts[i].name, name) == 0) { *out = g_cc_pconsts[i].v; return 1; }
    return 0;
}

static int cc_builtin_code(const char* name)
{
    /* normalizza i suffissi di tipo BASIC: STR$ → STR, TI% → TI, A! → A */
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", name ? name : "");
    size_t bl = strlen(buf);
    if (bl > 1) {
        char last = buf[bl - 1];
        if (last == '$' || last == '%' || last == '!' || last == '#') buf[bl - 1] = 0;
    }
    for (size_t i = 0; i < sizeof(g_cc_builtins) / sizeof(g_cc_builtins[0]); i++)
        if (strcasecmp(g_cc_builtins[i].name, buf) == 0) return g_cc_builtins[i].code;
    return -1;
}

/* ── emitter ── */
/* argc: numero di argomenti scritti nel sorgente al call site, -1 se non applicabile
 * (etichette, GOTO/GOSUB, code-ref, o qualunque patch "kind" per cui l'arità non e'
 * nota/rilevante) — usato SOLO da cc_patch_all per scegliere fra overload omonimi con
 * arità diversa (vedi il fix cross-file overload, 2026-09-07). */
typedef struct { int pos; int kind; char* name; int argc; } cc_patch_t;
typedef struct { char* name; int addr; } cc_label_t;
typedef struct { char* name; int slot; } cc_local_t;
typedef struct { int pos; int target; int backward; int cond; } cc_reljump_t;
typedef struct { int try_ip; int finally_ip; int end_ip; int has_catch; } cc_tryregion_t;

typedef struct {
    uint8_t* code; int code_len, code_cap;
    uint32_t* lines;
    gmb_compiled_chunk_t* chunk;
    char* error;
    /* Modalità SNIPPET (EVAL/& a runtime, vedi gmb_compile_snippet): il codice
     * viene emesso con indirizzi assoluti già BASATI su code_base (spazio del
     * chunk merged dell'ospite) e le prime n_ext_procs procedure sono quelle
     * dell'ospite (indirizzi già definitivi, mai ri-basati). */
    int code_base;
    int n_ext_procs;
    int snippet_mode;
    /* globals: nome -> slot */
    char** gnames; int* gslots; int gcount, gcap;
    char* dyn_names[64]; int n_dyn;
    /* patch: call/goto/gosub a etichetta o procedura (indirizzo assoluto u16) */
    cc_patch_t* patches;
    int npatch, ncap;
    int exit_pos[16][64]; int exit_count[16]; int loop_depth;
    int comp_uid;              /* comprehension: id univoco per i nomi dei temporanei __comp_* */
    char** procset; int nprocset, cap_procset;
    /* registry BYREF: nome procedura -> maschera bit dei parametri BYREF */
    char** byref_names; uint64_t* byref_masks; int nbyref, byref_cap;
    /* registry firma procedura (OPTIONAL/PARAMARRAY, 2026-08-31): nome -> nodo AST
     * completo (per leggere param_defaults/param_variadic/n_params dal CALL SITE,
     * dove il chiamante deve sapere quanti argomenti mancano e se compilare i default
     * dichiarati). Il puntatore vive quanto il programma sorgente (mai liberato prima
     * della fine di gmb_compile), safe da conservare senza copia. */
    char** procdef_names; const gmb_stmt_t** procdef_ptrs; int nprocdef, procdef_cap;
    /* patch code-ref (OP_PUSH_CODE_REF): indice costante -> nome procedura */
    int* cr_consts; char** cr_names; int ncr, crcap;
    /* labels della procedura corrente: nome -> addr */
    cc_label_t* labels;
    int nlabels, lcap;
    /* locals della procedura/scope corrente: nome -> slot */
    cc_local_t* locals;
    int nlocals, nloc_cap;
    /* snapshot nomi-locali per procedura (S_eval EVAL/macro): addr + nomi per slot.
     * Il compilatore C non rimuove i block-local (slot monotonici, mai riusati):
     * lo snapshot a fine procedura è esatto per costruzione. */
    uint32_t* ploc_addrs; char*** ploc_names; uint32_t* ploc_counts;
    int nploc, capploc;
    /* 1 durante la compilazione del body di una procedura: le DIM dichiarano
       locali (parità C# AddLocal), non globali */
    int in_proc;
    /* patching dei jump relativi (if/while/for): pos dell'operando u16, target_addr, backward */
    cc_reljump_t* reljumps;
    int nrel, nrel_cap;
    /* side-table TryRegions (parità CompiledChunk.TryRegions): tryIp → finally/end/hasCatch */
    cc_tryregion_t* tryregs;
    int ntryregs, tryreg_cap;
    int current_line;
    /* UDT registry (TYPE...END TYPE, campi nudi): nome tipo -> lista campi
     * (nome + "è stringa?"), popolata in pre-pass da gmb_compile prima di
     * compilare qualunque DIM che vi faccia riferimento. */
    struct cc_udt { char name[64]; char field_names[32][64]; char field_types[32][64]; int field_is_str[32]; int n_fields; int desc_const_idx; } *udts;
    int nudt, udt_cap;
    /* variabile (locale o globale, per nome) -> nome del TYPE/CLASS dichiarato con essa
     * (DIM v AS <Tipo>, o inferito da DIM v = NEW Tipo(...)): per gli UDT (cc_udt_find)
     * abilita la semantica "copia per valore" su riassegnazione; per le CLASS
     * (cc_class_find) abilita il dispatch dei metodi utente via OP_CALL_MEMBER. */
    char** vt_names; char** vt_types; int nvt, vt_cap;
    char** ar_names; int* ar_ranks; int nar, ar_cap;   /* rank array dichiarato (per FOR EACH su matrici) */
    char** ucn; double* ucv; char** ucs; int nuc, ucap; /* costanti utente (CONST) */


    /* nome procedura -> nome CLASS del suo tipo di ritorno dichiarato (FUNCTION ... AS
     * ClassName): popolato in pre-pass (stesso giro di cc_procset_add), usato per
     * tracciare il tipo di `x = FuncCheRestituisceUnaClasse(...)` — altrimenti
     * un'istanza restituita da una FUNCTION ordinaria (non un `NEW ClassName(...)`
     * diretto) perdeva il tipo tracciato e x.Metodo() cadeva nel fallback generico. */
    char** ret_class_procs; char** ret_class_names; int n_ret_class, ret_class_cap;
    /* CLASS registry (CLASS...END CLASS, o TYPE...END TYPE con metodi propri; INHERITS
     * supportato — vedi cc_class_register/cc_class_merge_inherited. Ancora fuori scope:
     * IMPLEMENTS/PROPERTY/campi STATIC/OPERATOR, vedi il case GMB_STMT_CLASS in cc_stmt).
     * Il descrittore (FieldNames/FieldTypes/VTable/Constructors) è costruito SUBITO nel
     * pre-pass con indirizzi placeholder, poi PATCHATO in-place con gli indirizzi reali
     * dopo la compilazione dei metodi/costruttore in passata 4 (vedi
     * cc_class_patch_method/ctor); i metodi/costruttore EREDITATI (non sovrascritti)
     * vengono aggiunti al VTable/Constructors della classe derivata in un passo
     * successivo (cc_class_merge_inherited), quando gli indirizzi della base sono
     * finalmente noti. */
    struct cc_class {
        char name[64];
        int base_idx;   /* indice in classes[] della classe base (INHERITS), -1 se nessuna */
        int is_interface;   /* INTERFACE (2.9): solo method_names (firme), niente VTable/
                             * campi/costruttore/costante descrittore — mai istanziata,
                             * serve solo al controllo compile-time "obj.Metodo()" quando
                             * la variabile è dichiarata AS <Interfaccia> (il dispatch
                             * reale avviene comunque sul VTable dell'istanza concreta). */
        char field_names[32][64]; int field_is_str[32]; int n_fields;
        char method_names[32][64]; int n_methods;
        gmb_cconst_node_t* vtable_node;      /* chiave = nome metodo (propri; ereditati aggiunti dopo) */
        gmb_cconst_node_t* ctor_list_node;   /* 0 o 1 entry (indirizzo costruttore) */
        gmb_cconst_node_t* propget_node;     /* chiave = nome NUDO proprietà (senza "PropGet_") */
        gmb_cconst_node_t* propset_node;     /* chiave = nome NUDO proprietà (senza "PropSet_") */
        /* campi STATIC (2.9): condivisi da TUTTE le istanze — un GLOBALE dedicato per
         * campo (nome compilatore "ClassName::Campo", mai collidibile con un nome BASIC
         * vero: "::" non è un carattere valido in un identificatore) invece di uno slot
         * nel dict-istanza. Vedi cc_class_static_slot. */
        char static_names[8][64]; int static_slots[8]; int n_statics;
        /* proprietà PROPERTY...END PROPERTY (2.9): il parser le sintetizza già come
         * metodi "PropGet_<Nome>"/"PropSet_<Nome>" in cls_methods (compilati come
         * qualunque altro metodo) — qui si traccia solo il nome NUDO della proprietà per
         * riconoscere `obj.Nome` (senza parentesi, quindi mai una CALL) come lettura
         * tramite getter invece di un accesso a campo. La SCRITTURA `obj.Nome = v` non
         * richiede nulla di nuovo: OP_STORE_FIELD verifica GIÀ PropertySetters lato VM
         * prima del gmb_dict_set generico — vedi cc_class_register/vm_desc_table_addr. */
        char prop_names[8][64]; int n_props;
        /* EVENT Nome(...) (2.9): il registro handler è una LISTA per-istanza sotto la
         * chiave dict "__event_<Nome>", inizializzata a VUOTA da un prologo emesso
         * all'inizio di OGNI costruttore (compreso uno sintetizzato se la classe ha
         * eventi ma nessun SUB NEW proprio — vedi cc_class_register/cc_class_member).
         * Necessario perché la VM non ha uno zero-init automatico per "lista vuota"
         * (solo NUMBER/STRING via FieldTypes) e i salti condizionali di questa VM non
         * distinguono un campo mai inizializzato da uno che contiene già una lista
         * (gmb_task_pop_num collassa QUALUNQUE riferimento a 0.0) — niente
         * inizializzazione lazy possibile con gli opcode esistenti. */
        char event_names[8][64]; int n_events;
        int desc_const_idx;
        /* DESTRUCTOR (2026-09-07): la VM (OP_FINALIZE_SCOPE/GLOBAL) legge
         * descriptor["Destructor"] (valore scalare diretto, NON nel VTable) per eseguire
         * il destructor dell'istanza. Il compilatore nativo NON lo registrava mai: un
         * `SUB DESTRUCTOR` finiva solo nel VTable come metodo normale, quindi
         * vm_desc_destructor_addr restituiva -1 e nessun DESTRUCTOR girava. */
        int has_destructor;
        gmb_cconst_entry_t* dtor_entry;   /* entry "Destructor" nel descrittore (val_d patchato) */
    } *classes;
    int nclass, class_cap;
    /* Indice in c->classes[] della CLASS il cui metodo/costruttore si sta compilando
     * ORA (-1 fuori da un corpo di classe): abilita la risoluzione implicita SELF.Campo
     * per un nome nudo che combacia con un campo della classe corrente e non è un
     * local/parametro — vedi cc_class_field_is in GMB_EXPR_VAR/GMB_STMT_ASSIGN. */
    int cur_class;
    /* Inizializzatori dei campi STATIC (2.9): accodati durante cc_class_register (pre-pass,
     * prima che il body __main sia compilato), emessi UNA SOLA VOLTA all'inizio del
     * programma (subito dopo il padding iniziale in gmb_compile) — un campo STATIC è
     * condiviso da tutte le istanze, va inizializzato una volta sola, mai per-istanza
     * (a differenza dei campi normali, zero-inizializzati da OP_NEW_CLASS_INSTANCE ad
     * ogni NEW). */
    struct { int slot; const gmb_expr_t* init; }* static_inits;
    int n_static_inits, static_inits_cap;
    /* Contesto temporaneo durante l'emissione di una lambda. I locali esterni non
     * diventano locals della lambda: il primo riferimento li registra come upvalue. */
    cc_local_t* enclosing_locals; int n_enclosing_locals;
    char** lambda_up_names; uint8_t* lambda_up_is_local; uint8_t* lambda_up_index;
    int n_lambda_up, lambda_up_cap;
    int compiling_lambda;
    /* Variabili note contenenti closure: distingue f(args) da array indexing. */
    char** closure_vars; int n_closure_vars, closure_vars_cap;
    char closure_return_procs[64][64]; int n_closure_return_procs;
    /* Function/SUB annidate emesse come closure (passata 4 le salta). */
    char** nested_closure_names; int n_nested_closure, nested_closure_cap;
    const char* current_proc_name;
    char** gen_names;          /* nomi delle GENERATOR PROCEDURE */
    int n_gen_names;
} gmb_cc_t;

/* Colezioni dichiarate (DIM AS LIST/QUEUE/...): ritorna 1=seq (lista), 2=map (dict),
 * 0 = non-collezione. Il base-name si prende fino a '<' o a spazio ("LIST<STRING>",
 * "QUEUE OF STRING" → "QUEUE"). */
static int cc_container_kind(const char* t)
{
    if (!t) return 0;
    char base[64]; int bi = 0;
    for (const char* q = t; *q && bi < 63; q++) {
        if (*q == '<' || *q == ' ') break;
        base[bi++] = *q;
    }
    base[bi] = 0;
    if (!*base) return 0;
    if (!strcasecmp(base, "LIST") || !strcasecmp(base, "QUEUE") || !strcasecmp(base, "STACK")
        || !strcasecmp(base, "HASHSET") || !strcasecmp(base, "LINKEDLIST")
        || !strcasecmp(base, "DOUBLYLINKEDLIST") || !strcasecmp(base, "CIRCULARLINKEDLIST")
        || !strcasecmp(base, "BAG") || !strcasecmp(base, "HEAP") || !strcasecmp(base, "MAXHEAP")
        || !strcasecmp(base, "MINHEAP") || !strcasecmp(base, "RINGBUFFER") || !strcasecmp(base, "DEQUE")
        || !strcasecmp(base, "PRIORITYQUEUE") || !strcasecmp(base, "HASHRING")
        || !strcasecmp(base, "SPARSESET") || !strcasecmp(base, "SORTEDLIST"))
        return 1;
    if (!strcasecmp(base, "DICT") || !strcasecmp(base, "SORTED") || !strcasecmp(base, "TREE")
        || !strcasecmp(base, "AVLTREE") || !strcasecmp(base, "REDBLACK")
        || !strcasecmp(base, "BPLUSTREE") || !strcasecmp(base, "CACHE")
        || !strcasecmp(base, "TIMELINE")
        || !strcasecmp(base, "SORTEDDICT") || !strcasecmp(base, "SYNCEDDICT")
        || !strcasecmp(base, "ORDEREDDICT"))
        return 2;
    return 0;
}

static int cc_container_is_graph(const char* t)
{
    if (!t) return 0;
    char base[64]; int bi = 0;
    for (const char* q = t; *q && bi < 63; q++) {
        if (*q == '<' || *q == ' ') break;
        base[bi++] = *q;
    }
    base[bi] = 0;
    return *base && !strcasecmp(base, "GRAPH");
}

static int cc_container_is_sorted(const char* t)
{
    if (!t) return 0;
    char base[64]; int bi = 0;
    for (const char* q = t; *q && bi < 63; q++) {
        if (*q == '<' || *q == ' ') break;
        base[bi++] = *q;
    }
    base[bi] = 0;
    return *base && !strcasecmp(base, "SORTED");
}

/* True se il tipo dichiarato è un insieme con semantica di unicità (HASHSET/SET). */
static int cc_container_is_set(const char* t)
{
    if (!t) return 0;
    char base[64]; int bi = 0;
    for (const char* q = t; *q && bi < 63; q++) {
        if (*q == '<' || *q == ' ') break;
        base[bi++] = *q;
    }
    base[bi] = 0;
    return *base && (!strcasecmp(base, "HASHSET") || !strcasecmp(base, "SET"));
}
static void cc_arr_rank_set(gmb_cc_t* c, const char* var, int rank)
{
    if (!c || !var) return;
    for (int i = 0; i < c->nar; i++)
        if (strcasecmp(c->ar_names[i], var) == 0) { c->ar_ranks[i] = rank; return; }
    if (c->nar >= c->ar_cap) { c->ar_cap = c->ar_cap ? c->ar_cap * 2 : 16; c->ar_names = (char**)realloc(c->ar_names, (size_t)c->ar_cap * sizeof(char*)); c->ar_ranks = (int*)realloc(c->ar_ranks, (size_t)c->ar_cap * sizeof(int)); }
    c->ar_names[c->nar] = strdup(var);
    c->ar_ranks[c->nar] = rank;
    c->nar++;
}

static int cc_arr_rank_get(const gmb_cc_t* c, const char* var)
{
    if (!c || !var) return 0;
    for (int i = 0; i < c->nar; i++)
        if (strcasecmp(c->ar_names[i], var) == 0) return c->ar_ranks[i];
    return 0;
}
static bool cc_is_numeric(const char* s)
{
    if (!s || !*s) return false;
    for (const char* p = s; *p; p++) if (!isdigit((unsigned char)*p)) return false;
    return true;
}

static bool cc_proc_is_gen(gmb_cc_t* c, const char* name)
{
    if (!c || !name) return false;
    for (int i = 0; i < c->n_gen_names; i++)
        if (c->gen_names[i] && strcasecmp(c->gen_names[i], name) == 0) return true;
    return false;
}
static bool cc_proc_known(gmb_cc_t* c, const char* name)
{
    if (!name) return false;
    for (int i = 0; i < c->nprocset; i++)
        if (strcasecmp(c->procset[i], name) == 0) return true;
    return false;
}
static void cc_procset_add(gmb_cc_t* c, const char* name)
{
    if (!name) return;
    for (int i = 0; i < c->nprocset; i++) if (strcasecmp(c->procset[i], name) == 0) return;
    if (c->nprocset >= c->cap_procset) { c->cap_procset = c->cap_procset ? c->cap_procset * 2 : 32; c->procset = (char**)realloc(c->procset, (size_t)c->cap_procset * sizeof(char*)); }
    c->procset[c->nprocset++] = strdup(name);
}

static void cc_byref_register(gmb_cc_t* c, const char* name, uint64_t mask)
{
    if (!name) return;
    for (int i = 0; i < c->nbyref; i++)
        if (strcasecmp(c->byref_names[i], name) == 0) { c->byref_masks[i] = mask; return; }
    if (c->nbyref >= c->byref_cap) {
        c->byref_cap = c->byref_cap ? c->byref_cap * 2 : 32;
        c->byref_names = (char**)realloc(c->byref_names, (size_t)c->byref_cap * sizeof(char*));
        c->byref_masks = (uint64_t*)realloc(c->byref_masks, (size_t)c->byref_cap * sizeof(uint64_t));
    }
    c->byref_names[c->nbyref] = strdup(name);
    c->byref_masks[c->nbyref] = mask;
    c->nbyref++;
}

static uint64_t cc_byref_mask(gmb_cc_t* c, const char* name)
{
    if (!name) return 0;
    for (int i = 0; i < c->nbyref; i++)
        if (strcasecmp(c->byref_names[i], name) == 0) return c->byref_masks[i];
    return 0;
}

/* BUG REALE trovato 2026-09-07 (overload per arità tra file di namespace fallisce, es.
 * Somma(a,b) in un file + Somma(a,b,c) in un altro dello stesso namespace, aggregati dal
 * linker con lo STESSO func_name "Somma"): questo registro era UN SOLO slot per NOME
 * (sovrascritto dall'ultima registrazione, "if found: overwrite; return"). cc_procdef è
 * usato dal call site per sapere quanti parametri "fissi" ha il callee e riempire gli
 * OPTIONAL/PARAMARRAY mancanti con i default dichiarati (vedi commento su
 * cc_procdef_lookup_arity sotto) — con due overload omonimi, QUALUNQUE call site (anche a
 * 2 argomenti) leggeva la firma dell'ULTIMO overload registrato (es. quello a 3
 * parametri) e imbottiva la chiamata con un argomento extra (NIL) MAI scritto dal
 * programma: il numero di argomenti realmente pushati sullo stack non corrispondeva più
 * a quello scritto nel sorgente, confondendo il dispatch a runtime (che su altri path
 * distingue gli overload proprio dal numero di argomenti pushati). Fix: non sovrascrivere
 * più — accetta PIÙ definizioni per lo stesso nome, il lookup (cc_procdef_lookup_arity)
 * sceglie quella con l'arità giusta per QUEL call site. Nessuna regressione sul caso
 * comune (un solo def per nome): la lista ha un solo elemento, comportamento identico a
 * prima. */
static void cc_procdef_register(gmb_cc_t* c, const char* name, const gmb_stmt_t* def)
{
    if (!name) return;
    for (int i = 0; i < c->nprocdef; i++)
        if (strcasecmp(c->procdef_names[i], name) == 0 && c->procdef_ptrs[i] == def) return; /* stessa (nome,def): già registrata */
    if (c->nprocdef >= c->procdef_cap) {
        c->procdef_cap = c->procdef_cap ? c->procdef_cap * 2 : 32;
        c->procdef_names = (char**)realloc(c->procdef_names, (size_t)c->procdef_cap * sizeof(char*));
        c->procdef_ptrs = (const gmb_stmt_t**)realloc(c->procdef_ptrs, (size_t)c->procdef_cap * sizeof(gmb_stmt_t*));
    }
    c->procdef_names[c->nprocdef] = strdup(name);
    c->procdef_ptrs[c->nprocdef] = def;
    c->nprocdef++;
}

/* Firma completa di una procedura nota (OPTIONAL/PARAMARRAY, 2026-08-31): usata dal
 * CALL SITE per sapere quanti parametri sono dichiarati, quali hanno un default e quale
 * (l'ultimo) è variadico — un normale GMB_OP_CALL non porta un argc a runtime, quindi gli
 * argomenti mancanti vanno "riempiti" qui, in fase di compilazione, con i default
 * dichiarati (o un array vuoto per un PARAMARRAY senza argomenti).
 * Arity-aware (2026-09-07, fix overload cross-file): se `name` ha PIÙ definizioni
 * registrate (overload per arità), sceglie in ordine: (1) match ESATTO n_params==argc;
 * (2) una definizione che può accettare argc argomenti (argc <= n_params con i parametri
 * oltre argc tutti OPTIONAL/variadici, oppure variadica capace di assorbire argc >
 * n_params); (3) fallback l'ULTIMA registrata (comportamento pre-fix, per compatibilità
 * quando nessuna firma nota combacia esattamente — non dovrebbe capitare per codice
 * valido, ma non regredire un caso limite piuttosto che rompere la build). */
/* 2026-09-24 (parità C#/C99Native): "esistente e ridefinita" — errore forte per
 * definizioni top-level in namespace builtin con firma contestata da un builtin
 * omonimo (prima il builtin vinceva in silenzio: il dispatch builtin-first è prima
 * delle user), e per omonime top-level con stessa arità (prima vinceva la prima in
 * silenzio). Overload veri (arità disgiunte) ok — già dispatchati per arità
 * (cc_procdef_lookup_arity, fix 2026-09-07). Mai per generatori/metodi/nidificate. */
static void cc_err(gmb_cc_t* c, const char* m);
static void cc_check_shadow(gmb_cc_t* c, const gmb_stmt_t* s) {
    if (!s || !s->func_name || s->is_generator) return;
    /* Duplicato solo DENTRO lo stesso namespace (due omonime in namespace diversi non
     * sono un duplicato: il loro collapsing è una limitazione preesistente del chunk,
     * non un errore da introdurre qui). */
    const char* sns = s->func_ns ? s->func_ns : "";
    for (int i = 0; i < c->nprocdef; i++) {
        const gmb_stmt_t* o = c->procdef_ptrs[i];
        if (!o || o == s || !o->func_name || o->is_generator) continue;
        if (strcasecmp(o->func_name, s->func_name) != 0) continue;
        const char* ons = o->func_ns ? o->func_ns : "";
        if (strcasecmp(ons, sns) != 0) continue;
        if (o->n_params == s->n_params) {
            char m[256];
            snprintf(m, sizeof(m), "la procedura '%s' con %d parametri è già definita "
                     "(stessa firma): overload ammessi solo con arità diversa",
                     s->func_name, s->n_params);
            cc_err(c, m);
            return;
        }
    }
    if (!s->func_ns || !s->func_ns[0]) return;
    int req = 0;
    for (int k = 0; k < s->n_params; k++) {
        if ((s->param_optional && s->param_optional[k]) ||
            (s->param_variadic && s->param_variadic[k])) break;
        req++;
    }
    int variadic = s->n_params > 0 && s->param_variadic && s->param_variadic[s->n_params - 1];
    for (size_t i = 0; i < sizeof(gmb_reg_procs) / sizeof(gmb_reg_procs[0]); i++) {
        if (strcasecmp(gmb_reg_procs[i].ns, s->func_ns) != 0) continue;
        if (strcasecmp(gmb_reg_procs[i].name, s->func_name) != 0) continue;
        long lo = req > gmb_reg_procs[i].arity_min ? (long)req : (long)gmb_reg_procs[i].arity_min;
        long hiU = variadic ? LONG_MAX : (long)s->n_params;
        long hiB = (long)gmb_reg_procs[i].arity_max;
        long hi = hiU < hiB ? hiU : hiB;
        if (lo <= hi) {
            char m[320];
            snprintf(m, sizeof(m), "la procedura '%s' nel namespace '%s' ridefinisce il builtin "
                     "esistente con firma contestata (stessa arità effettiva): usare un nome "
                     "diverso oppure una firma diversa (overload con arità non coperta dal builtin)",
                     s->func_name, s->func_ns);
            cc_err(c, m);
            return;
        }
    }
}

static const gmb_stmt_t* cc_procdef_lookup_arity(gmb_cc_t* c, const char* name, int argc)
{
    if (!name) return NULL;
    const gmb_stmt_t* exact = NULL;
    const gmb_stmt_t* compatible = NULL;
    const gmb_stmt_t* last = NULL;
    for (int i = 0; i < c->nprocdef; i++) {
        if (strcasecmp(c->procdef_names[i], name) != 0) continue;
        const gmb_stmt_t* pd = c->procdef_ptrs[i];
        if (!pd) continue;
        last = pd;
        if (pd->n_params == argc) { exact = pd; continue; }
        if (!compatible && argc >= 0 && argc <= pd->n_params) {
            int ok = 1;
            for (int k = argc; k < pd->n_params; k++) {
                int is_last_variadic = (k == pd->n_params - 1) && pd->param_variadic && pd->param_variadic[k];
                int has_default = pd->param_defaults && pd->param_defaults[k];
                if (!is_last_variadic && !has_default) { ok = 0; break; }
            }
            if (ok) compatible = pd;
        }
        if (!compatible && argc > pd->n_params && pd->n_params > 0
            && pd->param_variadic && pd->param_variadic[pd->n_params - 1]) {
            compatible = pd; /* variadico: puo' assorbire argc > n_params dichiarati */
        }
    }
    if (exact) return exact;
    if (compatible) return compatible;
    return last;
}

/* 2026-09-24 (overload vs builtin, parità C#): un overload UTENTE accetta questa arità?
 * Stessa regola di cc_procdef_lookup_arity (esatto, poi default/variadic) senza fallback. */
static int cc_user_accepts_arity(gmb_cc_t* c, const char* name, int argc) {
    if (!name || argc < 0) return 0;
    for (int i = 0; i < c->nprocdef; i++) {
        if (strcasecmp(c->procdef_names[i], name) != 0) continue;
        const gmb_stmt_t* pd = c->procdef_ptrs[i];
        if (!pd) continue;
        if (pd->n_params == argc) return 1;
        if (argc < pd->n_params) {
            int ok = 1;
            for (int k = argc; k < pd->n_params; k++) {
                int is_last_variadic = (k == pd->n_params - 1) && pd->param_variadic && pd->param_variadic[k];
                int has_default = pd->param_defaults && pd->param_defaults[k];
                int is_opt = pd->param_optional && pd->param_optional[k];
                if (!is_last_variadic && !has_default && !is_opt) { ok = 0; break; }
            }
            if (ok) return 1;
        }
        if (argc > pd->n_params && pd->n_params > 0
            && pd->param_variadic && pd->param_variadic[pd->n_params - 1]) return 1;
    }
    return 0;
}

/* 2026-09-24: un builtin (registry canonico) accetta questa arità? Serve a preferire
 * l'overload utente SOLO quando il builtin non può gestire la chiamata (parità C#:
 * lì il fallback utente scatta solo se nessun nativo è compatibile). ns==NULL
 * (chiamata non qualificata) = match per nome su qualunque namespace. */
static int cc_builtin_accepts_arity(const char* ns, const char* name, int argc) {
    if (!name || argc < 0) return 0;
    for (size_t i = 0; i < sizeof(gmb_reg_procs) / sizeof(gmb_reg_procs[0]); i++) {
        if (strcasecmp(gmb_reg_procs[i].name, name) != 0) continue;
        if (ns && ns[0] && strcasecmp(gmb_reg_procs[i].ns, ns) != 0) continue;
        if (argc >= gmb_reg_procs[i].arity_min && argc <= gmb_reg_procs[i].arity_max) return 1;
    }
    return 0;
}

/* Il body contiene una SUB/FUNCTION annidata (a qualunque profondità)? Le
   DIM della procedura contenente restano GLOBALI in quel caso: le procedure
   annidate sono compilate flat e risolvono le variabili catturate come
   globali (cattura by-reference, parità Go). */
static int cc_body_has_nested_func(gmb_stmt_t** stmts, int n)
{
    for (int i = 0; i < n; i++) {
        const gmb_stmt_t* s = stmts[i];
        if (!s) continue;
        if (s->kind == GMB_STMT_FUNCTION) return 1;
        if (s->n_then > 0 && cc_body_has_nested_func(s->then_body, s->n_then)) return 1;
        if (s->n_else > 0 && cc_body_has_nested_func(s->else_body, s->n_else)) return 1;
        if (s->n_for > 0 && cc_body_has_nested_func(s->for_body, s->n_for)) return 1;
        if (s->n_while > 0 && cc_body_has_nested_func(s->while_body, s->n_while)) return 1;
        if (s->n_do > 0 && cc_body_has_nested_func(s->do_body, s->n_do)) return 1;
    }
    return 0;
}

/* SUB/FUNCTION annidate dentro blocchi (es. SUB Main con SUB Interno): le
   raccogliamo come procedure separate (parità C#: il linker le compila come
   procedure proprie; qui senza capture, argomenti passati normalmente). */
static void cc_collect_nested_procs(gmb_cc_t* c, gmb_stmt_t** stmts, int n,
                                     const gmb_stmt_t*** out, int* outn, int* outcap)
{
    for (int i = 0; i < n; i++) {
        const gmb_stmt_t* s = stmts[i];
        if (!s) continue;
        if (s->kind == GMB_STMT_FUNCTION) {
            cc_procset_add(c, s->func_name);
            uint64_t bm = 0;
            for (int k = 0; k < s->n_params && k < 64; k++)
                if (s->param_byref && s->param_byref[k]) bm |= (uint64_t)1 << k;
            cc_byref_register(c, s->func_name, bm);
            if (*outn >= *outcap) {
                *outcap = *outcap ? *outcap * 2 : 16;
                *out = (const gmb_stmt_t**)realloc((void*)*out, (size_t)*outcap * sizeof(**out));
            }
            (*out)[(*outn)++] = s;
            cc_collect_nested_procs(c, s->func_body, s->n_funcbody, out, outn, outcap);
            continue;
        }
        if (s->n_then > 0) cc_collect_nested_procs(c, s->then_body, s->n_then, out, outn, outcap);
        if (s->n_else > 0) cc_collect_nested_procs(c, s->else_body, s->n_else, out, outn, outcap);
        if (s->n_for > 0) cc_collect_nested_procs(c, s->for_body, s->n_for, out, outn, outcap);
        if (s->n_while > 0) cc_collect_nested_procs(c, s->while_body, s->n_while, out, outn, outcap);
        if (s->n_do > 0) cc_collect_nested_procs(c, s->do_body, s->n_do, out, outn, outcap);
    }
}


static void cc_err(gmb_cc_t* c, const char* m)
{
    if (!c->error) c->error = strdup(m);
}

/* ── UDT registry (TYPE...END TYPE, campi nudi — 2.9: pipeline C pura) ── */
static int cc_udt_find(gmb_cc_t* c, const char* name)
{
    if (!name) return -1;
    for (int i = 0; i < c->nudt; i++)
        if (strcasecmp(c->udts[i].name, name) == 0) return i;
    return -1;
}


static void cc_byte(gmb_cc_t* c, uint8_t b)
{
    if (c->code_len >= c->code_cap) {
        int nc = c->code_cap ? c->code_cap * 2 : 256;
        c->code = (uint8_t*)realloc(c->code, (size_t)nc);
        c->lines = (uint32_t*)realloc(c->lines, (size_t)nc * sizeof(uint32_t));
        c->code_cap = nc;
    }
    c->code[c->code_len] = b;
    c->lines[c->code_len] = (uint32_t)c->current_line;
    c->code_len++;
}

static void cc_u16(gmb_cc_t* c, int v)
{
    cc_byte(c, (uint8_t)((v >> 8) & 0xFF));
    cc_byte(c, (uint8_t)(v & 0xFF));
}

/* ── constant pool ── */
static int cc_add_const(gmb_cc_t* c, int type, double d, const char* s)
{
    gmb_compiled_chunk_t* ch = c->chunk;
    for (uint32_t i = 0; i < ch->consts_len; i++) {
        if (ch->consts[i].type == type) {
            if (type == GMB_CCONST_DOUBLE || type == GMB_CCONST_INT) {
                if (ch->consts[i].d == d) {
                    /* Bug corretto (2026-09-06): mai riusare uno slot code-ref
                     * (cc_add_const_unique lo crea con placeholder 0.0, patchato più
                     * tardi all'indirizzo della procedura — vedi cr_consts/gmb_compile).
                     * Un letterale che all'atto della compilazione vale ANCH'ESSO 0.0
                     * (es. il terzo argomento di AREDUCE(arr, ProcNominata, 0), o un
                     * confronto "= 0" ovunque più avanti nello stesso chunk) veniva
                     * silenziosamente deduplicato sullo STESSO slot: quando il patch
                     * pass sovrascriveva il placeholder con l'indirizzo reale della
                     * procedura, il letterale "0" cambiava valore con lui (osservato:
                     * args[2] arrivava a runtime come l'indirizzo di funzione, es. 7.0,
                     * invece di 0.0 — e un filtro "(n MOD 2) = 0" non trovava mai nulla). */
                    int is_coderef_slot = 0;
                    for (int k = 0; k < c->ncr; k++) if (c->cr_consts[k] == (int)i) { is_coderef_slot = 1; break; }
                    if (!is_coderef_slot) return (int)i;
                }
            } else if (type == GMB_CCONST_STRING) {
                if (s && ch->consts[i].s && strcmp(ch->consts[i].s, s) == 0) return (int)i;
            }
        }
    }
    if (ch->consts_len >= 65535) { cc_err(c, "constant pool piena"); return 0; }
    ch->consts = (gmb_compiled_chunk_t*)realloc(ch->consts, (size_t)(ch->consts_len + 1) * sizeof(*ch->consts));
    int idx = (int)ch->consts_len;
    ch->consts[idx].type = type;
    ch->consts[idx].d = d;
    ch->consts[idx].s = s ? strdup(s) : NULL;
    ch->consts[idx].tree = NULL;
    ch->consts_len++;
    return idx;
}

/* code-ref: costante numerica dedicata (senza dedup) il cui valore viene patchato
   all'indirizzo della procedura (OP_PUSH_CODE_REF, parità C# CompileProcedureToCodeRef). */
static int cc_add_const_unique(gmb_cc_t* c)
{
    gmb_compiled_chunk_t* ch = c->chunk;
    if (ch->consts_len >= 65535) { cc_err(c, "constant pool piena"); return 0; }
    ch->consts = (gmb_cconst_t*)realloc(ch->consts, (size_t)(ch->consts_len + 1) * sizeof(gmb_cconst_t));
    int idx = (int)ch->consts_len;
    ch->consts[idx].type = GMB_CCONST_DOUBLE;
    ch->consts[idx].d = 0.0;
    ch->consts[idx].s = NULL;
    ch->consts[idx].tree = NULL;
    ch->consts_len++;
    return idx;
}
static void cc_coderef_register(gmb_cc_t* c, int constIdx, const char* proc)
{
    if (c->ncr >= c->crcap) {
        c->crcap = c->crcap ? c->crcap * 2 : 8;
        c->cr_consts = (int*)realloc(c->cr_consts, (size_t)c->crcap * sizeof(int));
        c->cr_names = (char**)realloc(c->cr_names, (size_t)c->crcap * sizeof(char*));
    }
    c->cr_consts[c->ncr] = constIdx;
    c->cr_names[c->ncr] = strdup(proc);
    c->ncr++;
}

/* ── Modalità SNIPPET a runtime (EVAL/&) ─────────────────────────────────────
 * gmb_compile_snippet compila un FRAMMENTO contro le tabelle di simboli del chunk
 * OSPITE: globali/locali/procedure si risolvono su slot e indirizzi dell'ospite,
 * così il codice gira NELLA task corrente. Stato in file-static (protetto dal
 * mutex) perché gmb_compile è una pipeline unica: niente seconde implementazioni.
 * Fuori da gmb_compile_snippet i puntatori sono NULL → comportamento invariato. */
static const gmb_snippet_ext_t* g_snip_ext = NULL;
static gmb_compiled_chunk_t* g_snip_out = NULL;
static pthread_mutex_t g_snip_mu = PTHREAD_MUTEX_INITIALIZER;

/* Applica i simboli esterni al contesto di compilazione (prima di compilare). */
static void snip_seed(gmb_cc_t* c, const gmb_snippet_ext_t* ext)
{
    gmb_compiled_chunk_t* ch = c->chunk;
    c->snippet_mode = 1;
    c->code_base = ext->code_base;
    c->n_ext_procs = ext->proc_count;

    /* globali: slot dell'ospite, nessuna creazione */
    c->gcap = ext->global_count > 0 ? ext->global_count : 1;
    c->gnames = (char**)calloc((size_t)c->gcap, sizeof(char*));
    c->gslots = (int*)calloc((size_t)c->gcap, sizeof(int));
    for (int i = 0; i < ext->global_count; i++) {
        const char* n = ext->global_names ? ext->global_names[i] : NULL;
        c->gnames[i] = strdup(n && n[0] ? n : "__slot_vuoto__");
        c->gslots[i] = i;
    }
    c->gcount = ext->global_count;

    /* procedure dell'ospite: indirizzi già ASSOLUTI (mai ri-basati) */
    if (ext->proc_count > 0) {
        ch->proc_count = (uint32_t)ext->proc_count;
        ch->proc_names = (char**)calloc((size_t)ext->proc_count, sizeof(char*));
        ch->proc_addrs = (uint32_t*)calloc((size_t)ext->proc_count, sizeof(uint32_t));
        ch->arity_count = (uint32_t)ext->proc_count;
        ch->arity_addrs = (uint32_t*)calloc((size_t)ext->proc_count, sizeof(uint32_t));
        ch->arity_vals = (uint32_t*)calloc((size_t)ext->proc_count, sizeof(uint32_t));
        for (int i = 0; i < ext->proc_count; i++) {
            ch->proc_names[i] = strdup(ext->proc_names && ext->proc_names[i] ? ext->proc_names[i] : "");
            ch->proc_addrs[i] = ext->proc_addrs ? ext->proc_addrs[i] : 0;
            ch->arity_addrs[i] = ch->proc_addrs[i];
            ch->arity_vals[i] = ext->proc_arities ? ext->proc_arities[i] : 0;
        }
    }

    /* constant pool: prenota il prefisso dell'ospite con valori che non possono
     * mai essere deduplicati (NaN ≠ NaN) → le costanti del frammento partono da
     * ext->consts_seed e gli indici coincidono col pool del chunk merged. */
    if (ext->consts_seed > 0) {
        ch->consts_len = ext->consts_seed;
        ch->consts = (gmb_cconst_t*)calloc(ext->consts_seed, sizeof(gmb_cconst_t));
        for (uint32_t i = 0; i < ext->consts_seed; i++) {
            ch->consts[i].type = GMB_CCONST_DOUBLE;
            ch->consts[i].d = NAN;
            ch->consts[i].s = NULL;
            ch->consts[i].tree = NULL;
        }
    }
}

/* ── globals ── */
static int cc_global(gmb_cc_t* c, const char* name, int create)
{
    for (int i = 0; i < c->gcount; i++)
        if (strcasecmp(c->gnames[i], name) == 0) return c->gslots[i];
    if (!create) return -1;
    if (c->snippet_mode) {
        /* Snippet a runtime: i globali sono quelli dell'OSPITE. Un nome ignoto non
         * può creare slot (gli array globali della task sono già dimensionati): il
         * driver lo trasforma in un locale del wrapper (campo DBF / variabile nuova)
         * e ricompila. Il marcatore è parsato da gmb_compile_snippet. */
        char msg[320];
        snprintf(msg, sizeof msg, "SNIP_GLOBAL:%s", name ? name : "");
        cc_err(c, msg);
        return 0;
    }
    if (c->gcount >= c->gcap) { c->gcap = c->gcap ? c->gcap * 2 : 16; c->gnames = (char**)realloc(c->gnames, (size_t)c->gcap * sizeof(char*)); c->gslots = (int*)realloc(c->gslots, (size_t)c->gcap * sizeof(int)); }
    int slot = c->gcount++;
    c->gnames[slot] = strdup(name);
    c->gslots[slot] = slot;
    return slot;
}

static int cc_dyn_is(gmb_cc_t* c, const char* name)
{ if (!c||!name) return 0; for (int i=0;i<c->n_dyn;i++) if (strcasecmp(c->dyn_names[i],name)==0) return 1; return 0; }

/* ── locals (scope corrente) ── */
static int cc_local(gmb_cc_t* c, const char* name)
{
    for (int i = 0; i < c->nlocals; i++)
        if (strcasecmp(c->locals[i].name, name) == 0) return c->locals[i].slot;
    return -1;
}

static int cc_local_add(gmb_cc_t* c, const char* name)
{
    int slot = cc_local(c, name);
    if (slot >= 0) return slot;
    if (c->nlocals >= c->nloc_cap) { c->nloc_cap = c->nloc_cap ? c->nloc_cap * 2 : 8; c->locals = (cc_local_t*)realloc(c->locals, (size_t)c->nloc_cap * sizeof(*c->locals)); }
    int s = c->nlocals++;
    c->locals[s].name = strdup(name);
    c->locals[s].slot = s;
    return s;
}

/* Normalizza un nome locale per lo snapshot (via suffisso tipo, parità C#
 * CompiledChunk.NormalizeLocalName — i nomi viaggiano già normalizzati). */
static void cc_norm_local_name(const char* src, char* dst, size_t dstn)
{
    if (!src || !dst || dstn == 0) return;
    size_t n = strlen(src);
    if (n > 1) {
        char last = src[n - 1];
        if (last == '$' || last == '%' || last == '#' || last == '!' || last == '&' || last == '@')
            n--;
    }
    if (n >= dstn) n = dstn - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* Scatta i nomi dei locali correnti sotto addr (S_eval). Chiamato a fine procedura/
 * lambda/funzione-annidata PRIMA di liberare c->locals. */
static void cc_snapshot_locals(gmb_cc_t* c, int addr)
{
    if (!c || addr < 0 || c->nlocals <= 0) return;
    if (c->nploc >= c->capploc) {
        c->capploc = c->capploc ? c->capploc * 2 : 8;
        c->ploc_addrs = (uint32_t*)realloc(c->ploc_addrs, (size_t)c->capploc * sizeof(uint32_t));
        c->ploc_names = (char***)realloc(c->ploc_names, (size_t)c->capploc * sizeof(char**));
        c->ploc_counts = (uint32_t*)realloc(c->ploc_counts, (size_t)c->capploc * sizeof(uint32_t));
        if (!c->ploc_addrs || !c->ploc_names || !c->ploc_counts) { cc_err(c, "snapshot locali: memoria esaurita"); return; }
    }
    char** names = (char**)calloc((size_t)c->nlocals, sizeof(char*));
    if (!names) { cc_err(c, "snapshot locali: memoria esaurita"); return; }
    for (int i = 0; i < c->nlocals; i++) {
        char nb[72];
        cc_norm_local_name(c->locals[i].name ? c->locals[i].name : "", nb, sizeof(nb));
        names[i] = (nb[0] ? strdup(nb) : NULL);
    }
    c->ploc_addrs[c->nploc] = (uint32_t)addr;
    c->ploc_names[c->nploc] = names;
    c->ploc_counts[c->nploc] = (uint32_t)c->nlocals;
    c->nploc++;
}

static void cc_ploc_free(gmb_cc_t* c)
{
    if (!c) return;
    for (int i = 0; i < c->nploc; i++) {
        if (c->ploc_names && c->ploc_names[i]) {
            for (uint32_t j = 0; j < c->ploc_counts[i]; j++) free(c->ploc_names[i][j]);
            free(c->ploc_names[i]);
        }
    }
    free(c->ploc_addrs); free(c->ploc_names); free(c->ploc_counts);
    c->ploc_addrs = NULL; c->ploc_names = NULL; c->ploc_counts = NULL;
    c->nploc = c->capploc = 0;
}

static int cc_lambda_upvalue(gmb_cc_t* c, const char* name, int create)
{
    if (!c->compiling_lambda || !name) return -1;
    for (int i = 0; i < c->n_lambda_up; i++)
        if (strcasecmp(c->lambda_up_names[i], name) == 0) return i;
    if (!create) return -1;
    int is_local = 0, index = -1;
    for (int i = 0; i < c->n_enclosing_locals; i++)
        if (strcasecmp(c->enclosing_locals[i].name, name) == 0) {
            is_local = 1; index = c->enclosing_locals[i].slot; break;
        }
    if (index < 0) index = cc_global(c, name, 0);
    if (index < 0 || index > 255) return -1;
    if (c->n_lambda_up >= 16) { cc_err(c, "lambda: massimo 16 variabili catturate"); return -1; }
    if (c->n_lambda_up >= c->lambda_up_cap) {
        c->lambda_up_cap = c->lambda_up_cap ? c->lambda_up_cap * 2 : 8;
        c->lambda_up_names = (char**)realloc(c->lambda_up_names, (size_t)c->lambda_up_cap * sizeof(char*));
        c->lambda_up_is_local = (uint8_t*)realloc(c->lambda_up_is_local, (size_t)c->lambda_up_cap);
        c->lambda_up_index = (uint8_t*)realloc(c->lambda_up_index, (size_t)c->lambda_up_cap);
    }
    int u = c->n_lambda_up++;
    c->lambda_up_names[u] = strdup(name);
    c->lambda_up_is_local[u] = (uint8_t)is_local;
    c->lambda_up_index[u] = (uint8_t)index;
    return u;
}

/* Frammenti a runtime: le closure dichiarate nel frammento (DIM f = { … }) devono
 * essere viste anche quando il call-site viene compilato in una passata che non
 * porta con sé la lista per-proc. Set per-compilazione (azzerato all'inizio di
 * gmb_compile_snippet). */
static char* g_snip_closures[64];
static int g_snip_nclosures = 0;

static void snip_closure_clear(void)
{
    for (int i = 0; i < g_snip_nclosures; i++) free(g_snip_closures[i]);
    g_snip_nclosures = 0;
}

static void snip_closure_add(const char* name)
{
    if (!name || !name[0] || g_snip_nclosures >= 64) return;
    for (int i = 0; i < g_snip_nclosures; i++)
        if (strcasecmp(g_snip_closures[i], name) == 0) return;
    g_snip_closures[g_snip_nclosures++] = strdup(name);
}

static void cc_closure_var_add(gmb_cc_t* c, const char* name)
{
    if (!name) return;
    for (int i = 0; i < c->n_closure_vars; i++)
        if (strcasecmp(c->closure_vars[i], name) == 0) return;
    if (c->n_closure_vars >= c->closure_vars_cap) {
        c->closure_vars_cap = c->closure_vars_cap ? c->closure_vars_cap * 2 : 8;
        c->closure_vars = (char**)realloc(c->closure_vars, (size_t)c->closure_vars_cap * sizeof(char*));
    }
    c->closure_vars[c->n_closure_vars++] = strdup(name);
    if (c->snippet_mode) snip_closure_add(name);
}
static int cc_is_closure_var(gmb_cc_t* c, const char* name)
{
    for (int i = 0; i < c->n_closure_vars; i++)
        if (strcasecmp(c->closure_vars[i], name) == 0) return 1;
    if (c->snippet_mode) {
        for (int i = 0; i < g_snip_nclosures; i++)
            if (strcasecmp(g_snip_closures[i], name) == 0) return 1;
    }
    return 0;
}
static int cc_proc_returns_closure(gmb_cc_t* c, const char* name)
{
    for (int i = 0; i < c->n_closure_return_procs; i++)
        if (strcasecmp(c->closure_return_procs[i], name) == 0) return 1;
    return 0;
}

/* ── PRE-PASS closure-as-arg (2026-09-07) ───────────────────────────────────
 * Una FUNCTION/SUB che riceve una CLOSURE come parametro deve invocarla nel corpo via
 * CALL_INDIRECT (`m(x)`), ma il compilatore non sa che il param `m` è una closure (a
 * differenza di `DIM f = FN...`, registrata da cc_closure_var_add). Pre-pass: scansiona i
 * corpi PRIMA della pass 4 (che compila le procedure in ordine di dichiarazione) e, per ogni
 * chiamata a una proc nota con un argomento lambda/closure, registra il param corrispondente
 * del callee come closure var — così il corpo (pass 4) vede `m(x)` → CALL_INDIRECT. */
static void cc_scan_closure_expr(gmb_cc_t* c, const gmb_expr_t* e)
{
    if (!e) return;
    switch (e->kind) {
    case GMB_EXPR_CALL:
        if (e->var && cc_proc_known(c, e->var) && e->args) {
            const gmb_stmt_t* pd = cc_procdef_lookup_arity(c, e->var, e->nargs);
            for (int i = 0; i < e->nargs; i++) {
                const gmb_expr_t* a = e->args[i];
                int iscl = (a && a->kind == GMB_EXPR_LAMBDA)
                    || (a && a->kind == GMB_EXPR_VAR && a->var && cc_is_closure_var(c, a->var));
                if (iscl && pd && pd->func_params && i < pd->n_params && pd->func_params[i])
                    cc_closure_var_add(c, pd->func_params[i]);
            }
        }
        if (e->args) for (int i = 0; i < e->nargs; i++) cc_scan_closure_expr(c, e->args[i]);
        break;
    case GMB_EXPR_BINARY: cc_scan_closure_expr(c, e->left); cc_scan_closure_expr(c, e->right); break;
    case GMB_EXPR_UNARY: cc_scan_closure_expr(c, e->left); break;
    case GMB_EXPR_LAMBDA: cc_scan_closure_expr(c, e->lambda_expr); break;
    case GMB_EXPR_COMPREHENSION:
        cc_scan_closure_expr(c, e->comp_src); cc_scan_closure_expr(c, e->comp_where);
        cc_scan_closure_expr(c, e->comp_order); cc_scan_closure_expr(c, e->comp_sel);
        break;
    default: break;
    }
}
static void cc_scan_closure_body(gmb_cc_t* c, const gmb_stmt_t* const* body, int n)
{
    for (int i = 0; i < n; i++) {
        const gmb_stmt_t* s = body[i]; if (!s) continue;
        cc_scan_closure_expr(c, s->value);
        cc_scan_closure_expr(c, s->expr);
        cc_scan_closure_expr(c, s->ret_value);
        cc_scan_closure_expr(c, s->dim_init);
        if (s->print_args) for (int k = 0; k < s->nprint; k++) cc_scan_closure_expr(c, s->print_args[k]);
        cc_scan_closure_body(c, s->then_body, s->n_then);
        cc_scan_closure_body(c, s->else_body, s->n_else);
        cc_scan_closure_body(c, s->for_body, s->n_for);
        cc_scan_closure_body(c, s->while_body, s->n_while);
        cc_scan_closure_body(c, s->do_body, s->n_do);
        cc_scan_closure_body(c, s->try_body, s->n_try);
        cc_scan_closure_body(c, s->catch_body, s->n_catch);
        cc_scan_closure_body(c, s->finally_body, s->n_finally);
        cc_scan_closure_body(c, s->func_body, s->n_funcbody);
    }
}
static int cc_body_assigns_lambda(const gmb_stmt_t* const* body, int n, const char* fn)
{
    for (int i = 0; i < n; i++) {
        const gmb_stmt_t* s = body[i]; if (!s) continue;
        if ((s->kind == GMB_STMT_ASSIGN || s->kind == GMB_STMT_LET) && s->target && fn
            && (strcasecmp(s->target, fn) == 0 || strcasecmp(s->target, "FUNCTION") == 0)
            && s->value && s->value->kind == GMB_EXPR_LAMBDA) return 1;
        if (cc_body_assigns_lambda((const gmb_stmt_t* const*)s->then_body, s->n_then, fn)
            || cc_body_assigns_lambda((const gmb_stmt_t* const*)s->else_body, s->n_else, fn)
            || cc_body_assigns_lambda((const gmb_stmt_t* const*)s->for_body, s->n_for, fn)
            || cc_body_assigns_lambda((const gmb_stmt_t* const*)s->while_body, s->n_while, fn)
            || cc_body_assigns_lambda((const gmb_stmt_t* const*)s->do_body, s->n_do, fn)
            || cc_body_assigns_lambda((const gmb_stmt_t* const*)s->try_body, s->n_try, fn)
            || cc_body_assigns_lambda((const gmb_stmt_t* const*)s->catch_body, s->n_catch, fn)
            || cc_body_assigns_lambda((const gmb_stmt_t* const*)s->finally_body, s->n_finally, fn)) return 1;
    }
    return 0;
}

/* Un RETURN che restituisce una FUNCTION/SUB ANNIDATA (referenza al nome, es. `RETURN
 * Incrementa`) fa sì che la procedura restituisca una CLOSURE — anche senza assegnare mai
 * una lambda al proprio nome. Il nome restituito deve essere una funzione annidata
 * dichiarata nello stesso corpo (sarà emessa come closure da cc_emit_nested_function). */
static int cc_body_returns_nested_func(const gmb_stmt_t* const* body, int n)
{
    for (int i = 0; i < n; i++) {
        const gmb_stmt_t* s = body[i]; if (!s) continue;
        if (s->kind == GMB_STMT_RETURN && s->ret_value
            && s->ret_value->kind == GMB_EXPR_VAR && s->ret_value->var) {
            for (int j = 0; j < n; j++) {
                const gmb_stmt_t* t = body[j]; if (!t || t->kind != GMB_STMT_FUNCTION || !t->func_name) continue;
                if (strcasecmp(t->func_name, s->ret_value->var) == 0) return 1;
            }
        }
        if (cc_body_returns_nested_func((const gmb_stmt_t* const*)s->then_body, s->n_then)
            || cc_body_returns_nested_func((const gmb_stmt_t* const*)s->else_body, s->n_else)
            || cc_body_returns_nested_func((const gmb_stmt_t* const*)s->for_body, s->n_for)
            || cc_body_returns_nested_func((const gmb_stmt_t* const*)s->while_body, s->n_while)
            || cc_body_returns_nested_func((const gmb_stmt_t* const*)s->do_body, s->n_do)
            || cc_body_returns_nested_func((const gmb_stmt_t* const*)s->try_body, s->n_try)
            || cc_body_returns_nested_func((const gmb_stmt_t* const*)s->catch_body, s->n_catch)
            || cc_body_returns_nested_func((const gmb_stmt_t* const*)s->finally_body, s->n_finally)) return 1;
    }
    return 0;
}

/* ── label / patch ── */
static void cc_label(gmb_cc_t* c, const char* name)
{
    for (int i = 0; i < c->nlabels; i++)
        if (strcasecmp(c->labels[i].name, name) == 0) { c->labels[i].addr = c->code_len; return; }
    if (c->nlabels >= c->lcap) { c->lcap = c->lcap ? c->lcap * 2 : 8; c->labels = (cc_label_t*)realloc(c->labels, (size_t)c->lcap * sizeof(*c->labels)); }
    c->labels[c->nlabels].name = strdup(name);
    c->labels[c->nlabels].addr = c->code_len;
    c->nlabels++;
}

/* argc = -1: comportamento invariato (cc_patch_all risolve col primo nome che combacia,
 * come sempre ha fatto). Usata da tutti i call site tranne la CALL espressione diretta
 * (GMB_EXPR_CALL, unico punto dove l'arità del call site serve a scegliere l'overload
 * giusto — vedi cc_patch_add_argc sotto). */
static void cc_patch_add(gmb_cc_t* c, int pos, int kind, const char* name)
{
    if (c->npatch >= c->ncap) { c->ncap = c->ncap ? c->ncap * 2 : 16; c->patches = (cc_patch_t*)realloc(c->patches, (size_t)c->ncap * sizeof(*c->patches)); }
    c->patches[c->npatch].pos = pos;
    c->patches[c->npatch].kind = kind;   /* 0=etichetta, 1=procedura */
    c->patches[c->npatch].name = strdup(name);
    c->patches[c->npatch].argc = -1;
    c->npatch++;
}

/* BUG REALE trovato 2026-09-07 (overload per arità tra file di namespace fallisce nei
 * file reali, es. Somma(a,b) in un file + Somma(a,b,c) in un altro dello stesso
 * namespace): cc_proc (sotto) registra OGNI FUNCTION/SUB in ch->proc_names/proc_addrs
 * SENZA deduplicare per nome (append incondizionato, quindi due overload omonimi
 * producono DUE entry) — ma cc_patch_all (la risoluzione degli indirizzi) cercava il
 * PRIMO nome che combacia e si fermava lì (break al primo match), IGNORANDO quanti
 * argomenti il call site aveva realmente scritto. Risultato: OGNI chiamata a "Somma"
 * (2 o 3 argomenti) risolveva all'indirizzo del PRIMO overload compilato (es. quello a 2
 * parametri, se il file che lo definisce viene elaborato per primo) — una chiamata a 3
 * argomenti finiva quindi nel corpo a 2 parametri, con il terzo argomento pushato sullo
 * stack ma mai consumato dal prologo (stack squilibrato, risultato spazzatura). Fix:
 * questa variante porta anche l'argc del call site nel patch record — cc_patch_all,
 * quando lo risolve, preferisce fra i nomi che combaciano quello la cui arità dichiarata
 * (arity_vals, popolato in lockstep con proc_addrs da cc_proc) combacia ESATTAMENTE con
 * argc, prima di ripiegare sul primo match (comportamento pre-fix, per compatibilità con
 * override/casi non gestiti qui). */
static void cc_patch_add_argc(gmb_cc_t* c, int pos, int kind, const char* name, int argc)
{
    cc_patch_add(c, pos, kind, name);
    c->patches[c->npatch - 1].argc = argc;
}

/* ── forward: espressioni ── */
static void cc_expr(gmb_cc_t* c, const gmb_expr_t* e);

/* argomento di chiamata in posizione BYREF + variabile semplice → box byref
   (parità C#: OP_MAKE_BYREF/OP_MAKE_BYREF_GLOBAL prima di CALL). Altrimenti
   push del valore come sempre. */
static void cc_call_arg(gmb_cc_t* c, uint64_t byref_mask, int idx, const gmb_expr_t* a)
{
    if (a && ((byref_mask >> idx) & 1) && a->kind == GMB_EXPR_VAR && a->var && a->nargs == 0) {
        int l = cc_local(c, a->var);
        if (l >= 0) { cc_byte(c, GMB_OP_MAKE_BYREF); cc_u16(c, l); return; }
        int g = cc_global(c, a->var, 0);
        if (g >= 0) { cc_byte(c, GMB_OP_MAKE_BYREF_GLOBAL); cc_u16(c, g); return; }
    }
    cc_expr(c, a);
}

/* ── store di un target ── */
static void cc_push_str_const(gmb_cc_t* c, const char* str);
static void cc_syscall(gmb_cc_t* c, int code, int argc);
static void cc_prescan_dyn(gmb_cc_t* c, gmb_stmt_t** list, int n)
{ for (int i=0;i<n;i++){ gmb_stmt_t* s=list[i]; if(!s) continue;
    if (s->kind==GMB_STMT_DIM && s->dim_is_dynamic && s->dim_var && c->n_dyn<64) {
        int dup=0; for (int k=0;k<c->n_dyn;k++) if (strcasecmp(c->dyn_names[k], s->dim_var)==0) dup=1;
        if (!dup) c->dyn_names[c->n_dyn++]=strdup(s->dim_var); }
    if (s->func_body && s->n_funcbody) cc_prescan_dyn(c, s->func_body, s->n_funcbody); } }

static void cc_store_target(gmb_cc_t* c, const char* name, int line)
{
    (void)line;
    if (name && c->current_proc_name
        && (strcasecmp(name, "FUNCTION") == 0 || strcasecmp(name, "OPERATOR") == 0))
        name = c->current_proc_name;
    if (cc_dyn_is(c, name) && cc_local(c, name) < 0) {
        cc_push_str_const(c, name); cc_syscall(c, 475, 2); return;
    }
    int l = cc_local(c, name);
    if (l >= 0) { cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, l); }
    else {
        int u = cc_lambda_upvalue(c, name, 1);
        if (u >= 0) { cc_byte(c, GMB_OP_STORE_UPVALUE); cc_byte(c, (uint8_t)u); return; }
        int g = cc_global(c, name, 1);
        cc_byte(c, GMB_OP_STORE_GLOBAL); cc_u16(c, g);
    }
}

static void cc_load_target(gmb_cc_t* c, const char* name)
{
    if (name && c->current_proc_name
        && (strcasecmp(name, "FUNCTION") == 0 || strcasecmp(name, "OPERATOR") == 0))
        name = c->current_proc_name;
    int l = cc_local(c, name);
    if (l >= 0) { cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, l); }
    else {
        int u = cc_lambda_upvalue(c, name, 1);
        if (u >= 0) { cc_byte(c, GMB_OP_LOAD_UPVALUE); cc_byte(c, (uint8_t)u); return; }
        int g = cc_global(c, name, 1);
        cc_byte(c, GMB_OP_LOAD_GLOBAL); cc_u16(c, g);
    }
}

/* store del top-of-stack nella variabile DIM (locale o globale): dentro una
   procedura la DIM dichiara SEMPRE un locale (parità C# AddLocal) — una DIM
   globale per variabile di SUB condivisa fra goroutine è una race. */
static void cc_store_dim_target(gmb_cc_t* c, const char* name, int line)
{
    int l = cc_local(c, name);
    if (l < 0 && c->in_proc) l = cc_local_add(c, name);
    if (l >= 0) { cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, l); }
    else {
        int g = cc_global(c, name, 1);
        cc_byte(c, GMB_OP_STORE_GLOBAL); cc_u16(c, g);
    }
    (void)line;
}

/* ── costanti utente (CONST) ── */
static int cc_const_get(gmb_cc_t* c, const char* name, double* v, int* isstr, const char** strv)
{
    if (!c || !name) return 0;
    for (int i = 0; i < c->nuc; i++)
        if (strcasecmp(c->ucn[i], name) == 0) { *v = c->ucv[i]; *isstr = (c->ucs[i] != NULL); *strv = c->ucs[i]; return 1; }
    return 0;
}
static void cc_const_add(gmb_cc_t* c, const char* name, double v, char* strv)
{
    if (!c || !name) return;
    for (int i = 0; i < c->nuc; i++)
        if (strcasecmp(c->ucn[i], name) == 0) {
            c->ucv[i] = v;
            free(c->ucs[i]); c->ucs[i] = strv;
            return;
        }
    if (c->nuc >= c->ucap) { c->ucap = c->ucap ? c->ucap * 2 : 16; c->ucn = (char**)realloc(c->ucn, (size_t)c->ucap * sizeof(char*)); c->ucv = (double*)realloc(c->ucv, (size_t)c->ucap * sizeof(double)); c->ucs = (char**)realloc(c->ucs, (size_t)c->ucap * sizeof(char*)); }
    c->ucn[c->nuc] = strdup(name);
    c->ucv[c->nuc] = v;
    c->ucs[c->nuc] = strv;
    c->nuc++;
}
static int cc_const_eval(gmb_cc_t* c, const gmb_expr_t* e, double* out, char** outs)
{
    if (!e) return 0;
    *outs = NULL;
    switch (e->kind)
    {
        case GMB_EXPR_NUM: *out = e->num; return 1;
        case GMB_EXPR_STR: *outs = strdup(e->str ? e->str : ""); *out = 0; return 1;
        case GMB_EXPR_VAR: {
            double v; int is; const char* sv;
            if (e->var && cc_const_get(c, e->var, &v, &is, &sv)) { *out = v; if (is && sv) *outs = strdup(sv); return 1; }
            double bv;
            if (e->var && e->var[0] == '%' && cc_pconst_lookup(e->var + 1, &bv)) { *out = bv; return 1; }
            return 0;
        }
        case GMB_EXPR_BINARY: {
            double l, r; char* ls = NULL; char* rs = NULL;
            if (!cc_const_eval(c, e->left, &l, &ls) || !cc_const_eval(c, e->right, &r, &rs)) { free(ls); free(rs); return 0; }
            free(ls); free(rs);
            const char* op = e->op ? e->op : "";
            if (!strcmp(op, "+")) *out = l + r;
            else if (!strcmp(op, "-")) *out = l - r;
            else if (!strcmp(op, "*")) *out = l * r;
            else if (!strcmp(op, "/")) *out = r != 0 ? l / r : 0;
            else if (!strcmp(op, "^")) { *out = pow(l, r); }
            else { *out = 0; }
            return 1;
        }
        default: return 0;
    }
}

/* ── statement ── */
static void cc_stmt(gmb_cc_t* c, const gmb_stmt_t* s);
static void cc_emit_lambda(gmb_cc_t* c, const gmb_expr_t* e);

/* ── helper TEAM: costante stringa + argomento stringa + syscall ──────────── */
static void cc_push_str_const(gmb_cc_t* c, const char* str)
{
    int idx = cc_add_const(c, GMB_CCONST_STRING, 0, str ? str : "");
    if (c->error) return;
    cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, idx);
}

/* Registra un TYPE (campi nudi, no metodi/costruttori — quelli restano fuori scope,
 * vedi il case GMB_STMT_CLASS in cc_stmt). Chiamata in pre-pass da gmb_compile prima
 * di compilare qualunque DIM che vi faccia riferimento (parità cc_procset_add). */
static void cc_udt_register(gmb_cc_t* c, const gmb_stmt_t* s)
{
    if (!s || !s->class_name || cc_udt_find(c, s->class_name) >= 0) return;
    if (c->nudt >= c->udt_cap) { c->udt_cap = c->udt_cap ? c->udt_cap * 2 : 8; c->udts = (struct cc_udt*)realloc(c->udts, (size_t)c->udt_cap * sizeof(*c->udts)); }
    struct cc_udt* u = &c->udts[c->nudt++];
    memset(u, 0, sizeof(*u));
    u->desc_const_idx = -1;
    snprintf(u->name, sizeof(u->name), "%s", s->class_name);
    int n = s->n_cls_fields < 32 ? s->n_cls_fields : 32;
    u->n_fields = n;
    for (int i = 0; i < n; i++) {
        snprintf(u->field_names[i], sizeof(u->field_names[i]), "%s", s->cls_field_names[i] ? s->cls_field_names[i] : "");
        snprintf(u->field_types[i], sizeof(u->field_types[i]), "%s", s->cls_field_types[i] ? s->cls_field_types[i] : "");
        u->field_is_str[i] = (s->cls_field_types[i] && strcasecmp(s->cls_field_types[i], "STRING") == 0) ? 1 : 0;
    }
}

/* variabile (nome, locale o globale) -> nome del TYPE con cui è stata DIMensionata */
static void cc_var_udt_set(gmb_cc_t* c, const char* var, const char* udt_name)
{
    if (!var) return;
    for (int i = 0; i < c->nvt; i++)
        if (strcasecmp(c->vt_names[i], var) == 0) { free(c->vt_types[i]); c->vt_types[i] = strdup(udt_name); return; }
    if (c->nvt >= c->vt_cap) { c->vt_cap = c->vt_cap ? c->vt_cap * 2 : 16; c->vt_names = (char**)realloc(c->vt_names, (size_t)c->vt_cap * sizeof(char*)); c->vt_types = (char**)realloc(c->vt_types, (size_t)c->vt_cap * sizeof(char*)); }
    c->vt_names[c->nvt] = strdup(var);
    c->vt_types[c->nvt] = strdup(udt_name);
    c->nvt++;
}
static const char* cc_var_udt_get(gmb_cc_t* c, const char* var)
{
    if (!var) return NULL;
    for (int i = 0; i < c->nvt; i++)
        if (strcasecmp(c->vt_names[i], var) == 0) return c->vt_types[i];
    return NULL;
}

/* Lascia sullo stack una NUOVA istanza UDT vuota (dict con i campi inizializzati) — per i
 * campi di tipo UDT (struct annidate) crea ricorsivamente il subdict invece di uno 0.0. */
static void cc_emit_udt_value(gmb_cc_t* c, const char* udt_name)
{
    char tname[40]; snprintf(tname, sizeof tname, "__udtini_%d", ++c->comp_uid);
    int tmp = cc_local_add(c, tname);
    cc_byte(c, GMB_OP_MK_DICT); cc_u16(c, 0);
    cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, tmp);
    int ui = cc_udt_find(c, udt_name);
    if (ui >= 0) {
        const struct cc_udt* u = &c->udts[ui];
        for (int i = 0; i < u->n_fields; i++) {
            if (cc_udt_find(c, u->field_types[i]) >= 0) cc_emit_udt_value(c, u->field_types[i]);
            else if (u->field_is_str[i]) { int si = cc_add_const(c, GMB_CCONST_STRING, 0, ""); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, si); }
            else { int di = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, di); }
            if (c->error) return;
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, tmp);
            int fi = cc_add_const(c, GMB_CCONST_STRING, 0, u->field_names[i]);
            if (c->error) return;
            cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi);
        }
    }
    cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, tmp);
}

/* Emette il codice per costruire un'istanza UDT vuota (o clonata da src_slot, se
 * src_slot >= 0), il cui dict di destinazione è già stato creato/store-ato in dim_var
 * (MK_DICT + cc_store_dim_target già emessi dal chiamante): un STORE_FIELD per campo. */
static void cc_emit_udt_init(gmb_cc_t* c, const struct cc_udt* u, const char* dim_var, int src_slot)
{
    for (int i = 0; i < u->n_fields; i++) {
        if (src_slot >= 0) {
            /* clona dal record sorgente: LOAD src.field */
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, src_slot);
            cc_push_str_const(c, u->field_names[i]);
            if (c->error) return;
            cc_byte(c, GMB_OP_LOAD_ELEMENT); cc_byte(c, 1);
        } else if (cc_udt_find(c, u->field_types[i]) >= 0) {
            /* campo di tipo UDT (struct annidata): subdict ricorsivo, non 0.0. */
            cc_emit_udt_value(c, u->field_types[i]);
        } else if (u->field_is_str[i]) {
            int si = cc_add_const(c, GMB_CCONST_STRING, 0, "");
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, si);
        } else {
            int di = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, di);
        }
        cc_load_target(c, dim_var);
        int fi = cc_add_const(c, GMB_CCONST_STRING, 0, u->field_names[i]);
        if (c->error) return;
        cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi);
    }
    /* membri-array (es. Ports(1) AS INTEGER): NON sono nella lista field_names → il
     * clone per-campo li ometteva (ToObject perdeva Ports). Una copia wholesale finale
     * (OP_REC_COPY, 209) porta tutti i campi residui del sorgente nel record. */
    if (src_slot >= 0) {
        cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, src_slot);
        cc_load_target(c, dim_var);
        cc_byte(c, GMB_OP_REC_COPY);
    }
}

/* ── descrittori composti (dict/list annidati nel constant pool) — CLASS ── */
static gmb_cconst_node_t* cc_node_new(void)
{
    gmb_cconst_node_t* n = (gmb_cconst_node_t*)calloc(1, sizeof(*n));
    return n;
}
static gmb_cconst_entry_t* cc_node_add(gmb_cconst_node_t* n)
{
    if (n->n >= n->cap) { n->cap = n->cap ? n->cap * 2 : 8; n->entries = (gmb_cconst_entry_t*)realloc(n->entries, (size_t)n->cap * sizeof(*n->entries)); }
    gmb_cconst_entry_t* e = &n->entries[n->n++];
    memset(e, 0, sizeof(*e));
    return e;
}
/* entry con chiave STRINGA (VTable: nome metodo -> indirizzo) */
static gmb_cconst_entry_t* cc_node_add_str_key(gmb_cconst_node_t* n, const char* key)
{
    gmb_cconst_entry_t* e = cc_node_add(n);
    e->key_is_int = 0; e->key_str = strdup(key ? key : "");
    return e;
}
/* entry con chiave INTERA (FieldNames/FieldTypes: indice campo -> valore; Constructors:
   indice lista -> indirizzo, la chiave è ignorata dal consumatore) */
static gmb_cconst_entry_t* cc_node_add_int_key(gmb_cconst_node_t* n, int key)
{
    gmb_cconst_entry_t* e = cc_node_add(n);
    e->key_is_int = 1; e->key_int = key;
    return e;
}
static void cc_node_free(gmb_cconst_node_t* n)
{
    if (!n) return;
    for (int i = 0; i < n->n; i++) {
        free(n->entries[i].key_str);
        free(n->entries[i].val_s);
        cc_node_free(n->entries[i].val_node);
    }
    free(n->entries);
    free(n);
}
/* Aggiunge una costante composta (DICT/LIST) al pool — MAI dedup (a differenza di
   cc_add_const): il tree è mutabile (patch degli indirizzi dopo la passata 4). */
static int cc_add_const_tree(gmb_cc_t* c, int type, gmb_cconst_node_t* tree)
{
    gmb_compiled_chunk_t* ch = c->chunk;
    if (ch->consts_len >= 65535) { cc_err(c, "constant pool piena"); return 0; }
    ch->consts = (gmb_cconst_t*)realloc(ch->consts, (size_t)(ch->consts_len + 1) * sizeof(*ch->consts));
    int idx = (int)ch->consts_len;
    ch->consts[idx].type = type;
    ch->consts[idx].d = 0;
    ch->consts[idx].s = NULL;
    ch->consts[idx].tree = tree;
    ch->consts_len++;
    return idx;
}

/* Descrittore minimo (solo FieldNames/FieldTypes, senza VTable/Constructors/Property*
 * come per le CLASS — un TYPE...END TYPE non ha metodi) per un TYPE puro, nella STESSA
 * forma letta da vm_make_default_instance (native/gmb_vm_c/gmb_vm_core.c): serve per
 * `DIM arr(n) AS Tipo` via OP_REDIM, che (a differenza di `DIM x AS Tipo` — vedi
 * cc_emit_udt_init, campo-per-campo via bytecode) materializza le nuove istanze a
 * runtime da un descrittore in costante, come già fanno le CLASS. Cache in
 * u->desc_const_idx: costruito una sola volta per TYPE, anche se usato in più DIM. */
static int cc_udt_desc_const_idx(gmb_cc_t* c, struct cc_udt* u)
{
    if (u->desc_const_idx >= 0) return u->desc_const_idx;
    gmb_cconst_node_t* fieldNames = cc_node_new();
    gmb_cconst_node_t* fieldTypes = cc_node_new();
    for (int i = 0; i < u->n_fields; i++) {
        gmb_cconst_entry_t* en = cc_node_add_int_key(fieldNames, i);
        en->val_type = GMB_CCONST_STRING; en->val_s = strdup(u->field_names[i]);
        gmb_cconst_entry_t* et = cc_node_add_int_key(fieldTypes, i);
        et->val_type = GMB_CCONST_STRING; et->val_s = strdup(u->field_is_str[i] ? "STRING" : "LONG");
    }
    gmb_cconst_node_t* desc = cc_node_new();
    gmb_cconst_entry_t* dfn = cc_node_add_str_key(desc, "FieldNames"); dfn->val_type = GMB_CCONST_DICT; dfn->val_node = fieldNames;
    gmb_cconst_entry_t* dft = cc_node_add_str_key(desc, "FieldTypes"); dft->val_type = GMB_CCONST_DICT; dft->val_node = fieldTypes;
    u->desc_const_idx = cc_add_const_tree(c, GMB_CCONST_DICT, desc);
    return u->desc_const_idx;
}

/* ── CLASS registry (CLASS...END CLASS / TYPE...END TYPE con metodi propri) ── */
static int cc_class_find(gmb_cc_t* c, const char* name)
{
    if (!name) return -1;
    for (int i = 0; i < c->nclass; i++)
        if (strcasecmp(c->classes[i].name, name) == 0) return i;
    return -1;
}

/* Registra che la procedura `proc` dichiara "AS ClassName"/"AS TipoUDT" come tipo di
 * ritorno — vedi gmb_cc_t.ret_class_procs. Chiamata in pre-pass, stesso giro di
 * cc_procset_add. Nonostante il nome (storico: CLASS era l'unico caso), accetta anche
 * un TYPE...END TYPE (UDT) — serve a cc_proc_ret_class per tracciare il tipo elemento
 * di "DIM x() = AMAP(arr, ProcCheRestituisceUnUDT)" (vedi quel branch in GMB_STMT_DIM):
 * i consumer esistenti filtrano già con cc_class_find, quindi un nome UDT registrato
 * qui viene semplicemente ignorato da loro, non causa comportamento errato. */
static void cc_ret_class_register(gmb_cc_t* c, const char* proc, const char* class_name)
{
    if (!proc || !class_name) return;
    if (cc_class_find(c, class_name) < 0 && cc_udt_find(c, class_name) < 0) return;
    if (c->n_ret_class >= c->ret_class_cap) { c->ret_class_cap = c->ret_class_cap ? c->ret_class_cap * 2 : 8; c->ret_class_procs = (char**)realloc(c->ret_class_procs, (size_t)c->ret_class_cap * sizeof(char*)); c->ret_class_names = (char**)realloc(c->ret_class_names, (size_t)c->ret_class_cap * sizeof(char*)); }
    c->ret_class_procs[c->n_ret_class] = strdup(proc);
    c->ret_class_names[c->n_ret_class] = strdup(class_name);
    c->n_ret_class++;
}
/* Nome CLASS restituito da `proc` (NULL se non tracciata/non una classe). */
static const char* cc_proc_ret_class(gmb_cc_t* c, const char* proc)
{
    if (!proc) return NULL;
    for (int i = 0; i < c->n_ret_class; i++)
        if (strcasecmp(c->ret_class_procs[i], proc) == 0) return c->ret_class_names[i];
    return NULL;
}

/* Risoluzione implicita SELF.Campo: un nome NUDO (nessun punto) dentro un corpo di
 * metodo/costruttore (c->cur_class >= 0) che combacia con un campo della classe
 * corrente e NON è un local/parametro (già coperto da cc_local altrove) è un accesso
 * al campo dell'istanza — stessa convenzione implicita di VB.NET Me./PowerBASIC THIS.
 * Ritorna 1 se `name` è un campo della classe in compilazione, 0 altrimenti. */
static int cc_class_field_is(gmb_cc_t* c, const char* name)
{
    if (c->cur_class < 0 || !name) return 0;
    struct cc_class* cl = &c->classes[c->cur_class];
    for (int i = 0; i < cl->n_fields; i++)
        if (strcasecmp(cl->field_names[i], name) == 0) return 1;
    return 0;
}

/* `cl` ha un metodo (incluso un OPERATOR sintetico "OP_PLUS" ecc., o una PROPERTY
   "PropGet_X"/"PropSet_X") chiamato `name`? Usato per il dispatch di OPERATOR overload. */
static int cc_class_has_method(const struct cc_class* cl, const char* name)
{
    if (!cl || !name) return 0;
    for (int i = 0; i < cl->n_methods; i++)
        if (strcasecmp(cl->method_names[i], name) == 0) return 1;
    return 0;
}

/* Nome VTable sintetico di un OPERATORE binario (specchio della tabella nel parser,
 * gmb_p_parse_class — vedi "OPERATOR <simbolo>..." in gmb_parser_statements.c: OGNI
 * simbolo lì produce un nome "OP_XXX" fisso, qui riprodotto identico). NULL se `op`
 * non ha un operatore sovraccaricabile corrispondente. */
static const char* cc_operator_method_name(const char* op)
{
    if (!op) return NULL;
    if (strcmp(op, "+") == 0) return "OP_PLUS";
    if (strcmp(op, "-") == 0) return "OP_MINUS";
    if (strcmp(op, "*") == 0) return "OP_MUL";
    if (strcmp(op, "/") == 0) return "OP_DIV";
    if (strcmp(op, "\\") == 0) return "OP_IDIV";
    if (strcasecmp(op, "MOD") == 0) return "OP_MOD";
    if (strcmp(op, "^") == 0) return "OP_POW";
    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) return "OP_EQ";
    if (strcmp(op, "<>") == 0) return "OP_NE";
    if (strcmp(op, "<") == 0) return "OP_LT";
    if (strcmp(op, "<=") == 0) return "OP_LE";
    if (strcmp(op, ">") == 0) return "OP_GT";
    if (strcmp(op, ">=") == 0) return "OP_GE";
    if (strcasecmp(op, "AND") == 0) return "OP_AND";
    if (strcasecmp(op, "OR") == 0) return "OP_OR";
    return NULL;
}
/* Nome VTable sintetico di un OPERATORE unario (solo "-" e "NOT" — stessa tabella). */
static const char* cc_operator_method_name_unary(const char* op)
{
    if (!op) return NULL;
    if (strcmp(op, "-") == 0) return "OP_MINUS";
    if (strcasecmp(op, "NOT") == 0) return "OP_NOT";
    return NULL;
}

/* L'operando `e` è una variabile semplice (nessun punto/indice) il cui tipo tracciato
 * è una CLASS con un metodo OPERATOR sintetico `opname` registrato? Ritorna l'indice
 * in c->classes[], -1 altrimenti — usato da GMB_EXPR_BINARY/UNARY per instradare
 * l'operatore su OP_CALL_MEMBER invece dell'opcode aritmetico generico. */
static int cc_operand_class_with_op(gmb_cc_t* c, const gmb_expr_t* e, const char* opname)
{
    if (!opname || !e || e->kind != GMB_EXPR_VAR || !e->var || strchr(e->var, '.') || e->nargs != 0) return -1;
    const char* tn = cc_var_udt_get(c, e->var);
    int cli = tn ? cc_class_find(c, tn) : -1;
    if (cli < 0 || !cc_class_has_method(&c->classes[cli], opname)) return -1;
    return cli;
}

/* Slot globale di un campo STATIC di `cl` chiamato `name` (-1 se non è STATIC). */
static int cc_class_static_slot_of(const struct cc_class* cl, const char* name)
{
    if (!cl || !name) return -1;
    for (int i = 0; i < cl->n_statics; i++)
        if (strcasecmp(cl->static_names[i], name) == 0) return cl->static_slots[i];
    return -1;
}

/* `name` è una PROPERTY (getter/setter sintetizzati dal parser) di `cl`? */
static int cc_class_is_prop_of(const struct cc_class* cl, const char* name)
{
    if (!cl || !name) return 0;
    for (int i = 0; i < cl->n_props; i++)
        if (strcasecmp(cl->prop_names[i], name) == 0) return 1;
    return 0;
}

/* Registra una CLASS: costruisce SUBITO il descrittore (FieldNames/FieldTypes/VTable/
 * Constructors) con indirizzi placeholder (0), tenendo i nodi vtable/ctor in memoria per
 * il patch successivo (cc_class_patch_method/ctor) una volta compilati i corpi (passata 4).
 * Chiamata in pre-pass da gmb_compile, PRIMA di qualunque codice che possa referenziare
 * il descrittore (NEW ClassName(...), obj.Metodo(...)) — stesso schema di cc_udt_register/
 * cc_procset_add. INHERITS: la classe base deve essere GIÀ registrata (dichiarata prima
 * nel file — verificato qui, errore esplicito altrimenti); campi e NOMI dei metodi
 * ereditati (non sovrascritti) sono uniti SUBITO (nessun indirizzo coinvolto, sicuro
 * farlo qui), gli INDIRIZZI VTable/Constructors ereditati sono aggiunti dopo, in
 * cc_class_merge_inherited — vedi il commento lì per il perché. Ancora fuori scope
 * (rifiutato esplicitamente da cc_stmt): IMPLEMENTS, campi STATIC, PROPERTY, OPERATOR —
 * qui si assume che chi chiama abbia già validato la loro assenza. */
static void cc_class_register(gmb_cc_t* c, const gmb_stmt_t* s)
{
    if (!s || !s->class_name || cc_class_find(c, s->class_name) >= 0) return;
    if (c->nclass >= c->class_cap) { c->class_cap = c->class_cap ? c->class_cap * 2 : 8; c->classes = (struct cc_class*)realloc(c->classes, (size_t)c->class_cap * sizeof(*c->classes)); }
    struct cc_class* cl = &c->classes[c->nclass++];
    memset(cl, 0, sizeof(*cl));
    snprintf(cl->name, sizeof(cl->name), "%s", s->class_name);
    cl->base_idx = -1;

    /* INTERFACE (2.9): registrazione semplificata — solo i nomi dei metodi dichiarati
     * (firme, senza corpo — il parser popola cls_methods con func_body vuoto), per
     * abilitare il controllo compile-time "obj.Metodo()" su una variabile dichiarata
     * AS <Interfaccia>. Nessun campo/VTable/costruttore/costante descrittore: un
     *'interfaccia non è mai istanziata direttamente (NEW IFoo() non ha senso), quindi
     * niente da patchare in passata 4b — SALTATA esplicitamente lì (vedi gmb_compile). */
    if (s->class_is_interface) {
        cl->is_interface = 1;
        for (int i = 0; i < s->n_cls_methods && cl->n_methods < 32; i++) {
            const gmb_stmt_t* m = s->cls_methods[i];
            if (m && m->func_name) { snprintf(cl->method_names[cl->n_methods], sizeof(cl->method_names[cl->n_methods]), "%s", m->func_name); cl->n_methods++; }
        }
        cl->desc_const_idx = -1;
        return;
    }

    struct cc_class* base = NULL;
    if (s->class_base && s->class_base[0]) {
        /* `INHERITS Control`: classe base BUILTIN del linguaggio (UserControl VB6-style).
         * Non è dichiarata nel programma: trattata come base vuota (nessun campo/metodo
         * ereditato). Il frontend nativo non ha un registro di classi builtin. */
        if (strcasecmp(s->class_base, "Control") == 0) {
            cl->base_idx = -1;
            base = NULL;
        } else {
        int bi = cc_class_find(c, s->class_base);
        if (bi < 0) {
            char buf[192];
            snprintf(buf, sizeof(buf), "classe base '%s' non trovata per '%s' (deve essere dichiarata PRIMA della classe derivata)", s->class_base, s->class_name);
            cc_err(c, buf);
            return;
        }
        cl->base_idx = bi;
        base = &c->classes[bi];
        }
    }

    /* campi: quelli della base PRIMA (già transitivamente risolti — la classe base,
     * se a sua volta derivata, è stata registrata prima e ha già i propri campi
     * ereditati uniti), poi quelli propri. */
    int nf = 0;
    if (base) for (int i = 0; i < base->n_fields && nf < 32; i++, nf++) {
        snprintf(cl->field_names[nf], sizeof(cl->field_names[nf]), "%s", base->field_names[i]);
        cl->field_is_str[nf] = base->field_is_str[i];
    }
    for (int i = 0; i < s->n_cls_fields && nf < 32; i++, nf++) {
        snprintf(cl->field_names[nf], sizeof(cl->field_names[nf]), "%s", s->cls_field_names[i] ? s->cls_field_names[i] : "");
        const char* ftype = s->cls_field_types[i] ? s->cls_field_types[i] : "";
        cl->field_is_str[nf] = (strcasecmp(ftype, "STRING") == 0) ? 1 : 0;
    }
    cl->n_fields = nf;
    gmb_cconst_node_t* fieldNames = cc_node_new();
    gmb_cconst_node_t* fieldTypes = cc_node_new();
    for (int i = 0; i < nf; i++) {
        gmb_cconst_entry_t* en = cc_node_add_int_key(fieldNames, i);
        en->val_type = GMB_CCONST_STRING; en->val_s = strdup(cl->field_names[i]);
        gmb_cconst_entry_t* et = cc_node_add_int_key(fieldTypes, i);
        /* solo STRING vs tutto il resto conta per il default tipizzato a runtime
           (vm_dict_get_ci + strstr("STRING") in OP_NEW_CLASS_INSTANCE) */
        et->val_type = GMB_CCONST_STRING; et->val_s = strdup(cl->field_is_str[i] ? "STRING" : "LONG");
    }

    /* campi STATIC (2.9): un GLOBALE dedicato per campo, condiviso da tutte le istanze
     * (mai nel dict-istanza). Il nome globale include il nome della classe per evitare
     * collisioni fra classi diverse con campi STATIC omonimi; "::" non è un separatore
     * valido in un identificatore BASIC, quindi non collide mai con un nome utente vero.
     * L'inizializzatore (se presente) è accodato in c->static_inits — emesso UNA SOLA
     * VOLTA a inizio programma da gmb_compile, non qui (qui siamo nel pre-pass, PRIMA
     * che qualunque bytecode sia stato emesso). */
    int ns = s->n_cls_statics < 8 ? s->n_cls_statics : 8;
    cl->n_statics = ns;
    for (int i = 0; i < ns; i++) {
        char gname[128];
        snprintf(gname, sizeof(gname), "%s::%s", s->class_name, s->cls_static_names[i] ? s->cls_static_names[i] : "");
        snprintf(cl->static_names[i], sizeof(cl->static_names[i]), "%s", s->cls_static_names[i] ? s->cls_static_names[i] : "");
        int slot = cc_global(c, gname, 1);
        cl->static_slots[i] = slot;
        if (s->cls_static_inits[i]) {
            if (c->n_static_inits >= c->static_inits_cap) { c->static_inits_cap = c->static_inits_cap ? c->static_inits_cap * 2 : 8; c->static_inits = (void*)realloc(c->static_inits, (size_t)c->static_inits_cap * sizeof(*c->static_inits)); }
            c->static_inits[c->n_static_inits].slot = slot;
            c->static_inits[c->n_static_inits].init = s->cls_static_inits[i];
            c->n_static_inits++;
        }
    }

    /* metodi: nomi EREDITATI (non sovrascritti dai metodi propri sotto) prima — solo per
     * il controllo a compile-time "obj.Metodo()" fuori dal corpo della classe; gli
     * indirizzi VTable ereditati sono aggiunti dopo (cc_class_merge_inherited), quando
     * finalmente noti (passata 4b compila anche i metodi propri di questa classe DOPO
     * questa registrazione). */
    int nm = 0;
    if (base) {
        for (int bi2 = 0; bi2 < base->n_methods && nm < 32; bi2++) {
            int overridden = 0;
            for (int k = 0; k < s->n_cls_methods; k++) {
                const gmb_stmt_t* mk = s->cls_methods[k];
                if (mk && mk->func_name && strcasecmp(mk->func_name, base->method_names[bi2]) == 0) { overridden = 1; break; }
            }
            if (!overridden) { snprintf(cl->method_names[nm], sizeof(cl->method_names[nm]), "%s", base->method_names[bi2]); nm++; }
        }
    }
    cl->n_methods = nm;
    /* proprietà ereditate (nomi nudi, per cc_class_is_prop_of nel corpo/uso della
       derivata) — stesso schema dei metodi, gli indirizzi si uniscono dopo. */
    if (base) for (int bi2 = 0; bi2 < base->n_props && cl->n_props < 8; bi2++) {
        if (!cc_class_is_prop_of(cl, base->prop_names[bi2])) { snprintf(cl->prop_names[cl->n_props], sizeof(cl->prop_names[cl->n_props]), "%s", base->prop_names[bi2]); cl->n_props++; }
    }

    gmb_cconst_node_t* vtable = cc_node_new();
    cl->vtable_node = vtable;
    gmb_cconst_node_t* propget = cc_node_new();
    gmb_cconst_node_t* propset = cc_node_new();
    cl->propget_node = propget;
    cl->propset_node = propset;
    /* EVENT (2.9): nomi ereditati dalla base (se presente) prima, poi quelli propri —
     * stesso schema di campi/metodi/proprietà. Nessun indirizzo coinvolto (sono solo
     * chiavi dict per un registro handler, non metodi), quindi nessun problema di
     * timing come per VTable/Constructors — tutto risolto qui, subito. */
    if (base) for (int i = 0; i < base->n_events && cl->n_events < 8; i++) {
        snprintf(cl->event_names[cl->n_events], sizeof(cl->event_names[cl->n_events]), "%s", base->event_names[i]);
        cl->n_events++;
    }
    for (int i = 0; i < s->n_cls_events && cl->n_events < 8; i++) {
        int already = 0;
        for (int k = 0; k < cl->n_events; k++) if (strcasecmp(cl->event_names[k], s->cls_event_names[i]) == 0) { already = 1; break; }
        if (!already) { snprintf(cl->event_names[cl->n_events], sizeof(cl->event_names[cl->n_events]), "%s", s->cls_event_names[i]); cl->n_events++; }
    }

    int ctorIdx = -1;
    for (int i = 0; i < s->n_cls_methods; i++) {
        const gmb_stmt_t* m = s->cls_methods[i];
        if (!m || !m->func_name) continue;
        if (strcasecmp(m->func_name, "NEW") == 0) { ctorIdx = i; continue; }   /* costruttore: non in VTable */
        /* DESTRUCTOR: NON è un metodo normale (mai chiamato come obj.Destructor()) — è un
         * campo scalar descriptor["Destructor"] patchato col suo indirizzo (per
         * OP_FINALIZE). Non va nel VTable né in method_names. */
        if (strcasecmp(m->func_name, "DESTRUCTOR") == 0) { cl->has_destructor = 1; continue; }
        if (cl->n_methods < 32) {
            snprintf(cl->method_names[cl->n_methods], sizeof(cl->method_names[cl->n_methods]), "%s", m->func_name);
            cl->n_methods++;
        }
        gmb_cconst_entry_t* e = cc_node_add_str_key(vtable, m->func_name);
        e->val_type = GMB_CCONST_DOUBLE; e->val_d = 0.0;   /* placeholder, patchato in passata 4 */
        /* PROPERTY <Nome> ... GET/SET ... END PROPERTY: il parser sintetizza già
         * "PropGet_<Nome>"/"PropSet_<Nome>" come metodi normali (sopra, nel VTable) —
         * qui si traccia in più il nome NUDO (senza prefisso) per riconoscere
         * `obj.Nome`/`Nome` (bare, senza parentesi — mai una CALL) come lettura tramite
         * getter invece di un campo — vedi cc_class_is_prop_of. */
        if (strncasecmp(m->func_name, "PropGet_", 8) == 0) {
            const char* bare = m->func_name + 8;
            if (!cc_class_is_prop_of(cl, bare) && cl->n_props < 8) { snprintf(cl->prop_names[cl->n_props], sizeof(cl->prop_names[cl->n_props]), "%s", bare); cl->n_props++; }
            gmb_cconst_entry_t* pe = cc_node_add_str_key(propget, bare);
            pe->val_type = GMB_CCONST_DOUBLE; pe->val_d = 0.0;
        } else if (strncasecmp(m->func_name, "PropSet_", 8) == 0) {
            const char* bare = m->func_name + 8;
            if (!cc_class_is_prop_of(cl, bare) && cl->n_props < 8) { snprintf(cl->prop_names[cl->n_props], sizeof(cl->prop_names[cl->n_props]), "%s", bare); cl->n_props++; }
            gmb_cconst_entry_t* pe = cc_node_add_str_key(propset, bare);
            pe->val_type = GMB_CCONST_DOUBLE; pe->val_d = 0.0;
        }
    }
    gmb_cconst_node_t* ctorList = cc_node_new();
    cl->ctor_list_node = ctorList;
    if (ctorIdx >= 0) {
        gmb_cconst_entry_t* e = cc_node_add_int_key(ctorList, 0);
        e->val_type = GMB_CCONST_DOUBLE; e->val_d = 0.0;   /* placeholder */
    } else if (cl->n_events > 0) {
        /* EVENT ma nessun SUB NEW proprio: senza un costruttore non c'è modo di
         * emettere il prologo che inizializza i registri handler (vedi
         * cc_class_member) — sintetizzato qui un costruttore vuoto (corpo nullo:
         * cc_class_member emette comunque il prologo prima del corpo, che qui è
         * vuoto) e AGGIUNTO a cls_methods (mutato — `s` è tecnicamente const, ma
         * questo AST è di proprietà esclusiva del compilatore in questa fase, mai
         * riletto se non da passata 4b, che itera lo STESSO puntatore) così che
         * passata 4b lo compili come qualunque altro costruttore. */
        gmb_stmt_t* synCtor = gmb_stmt_new(GMB_STMT_FUNCTION, s->line);
        synCtor->func_name = strdup("NEW");
        gmb_stmt_t* sm = (gmb_stmt_t*)s;
        sm->cls_methods = (gmb_stmt_t**)realloc(sm->cls_methods, (size_t)(sm->n_cls_methods + 1) * sizeof(gmb_stmt_t*));
        sm->cls_methods[sm->n_cls_methods++] = synCtor;
        gmb_cconst_entry_t* e = cc_node_add_int_key(ctorList, 0);
        e->val_type = GMB_CCONST_DOUBLE; e->val_d = 0.0;   /* placeholder, patchato in passata 4b */
    }
    gmb_cconst_node_t* desc = cc_node_new();
    gmb_cconst_entry_t* dfn = cc_node_add_str_key(desc, "FieldNames"); dfn->val_type = GMB_CCONST_DICT; dfn->val_node = fieldNames;
    gmb_cconst_entry_t* dft = cc_node_add_str_key(desc, "FieldTypes"); dft->val_type = GMB_CCONST_DICT; dft->val_node = fieldTypes;
    gmb_cconst_entry_t* dvt = cc_node_add_str_key(desc, "VTable"); dvt->val_type = GMB_CCONST_DICT; dvt->val_node = vtable;
    gmb_cconst_entry_t* dct = cc_node_add_str_key(desc, "Constructors"); dct->val_type = GMB_CCONST_LIST; dct->val_node = ctorList;
    gmb_cconst_entry_t* dpg = cc_node_add_str_key(desc, "PropertyGetters"); dpg->val_type = GMB_CCONST_DICT; dpg->val_node = propget;
    gmb_cconst_entry_t* dps = cc_node_add_str_key(desc, "PropertySetters"); dps->val_type = GMB_CCONST_DICT; dps->val_node = propset;
    if (cl->has_destructor) {
        gmb_cconst_entry_t* dd = cc_node_add_str_key(desc, "Destructor");
        dd->val_type = GMB_CCONST_DOUBLE; dd->val_d = 0.0;   /* placeholder, patchato in passata 4b */
        cl->dtor_entry = dd;
    }
    cl->desc_const_idx = cc_add_const_tree(c, GMB_CCONST_DICT, desc);
}

/* Patcha l'indirizzo del DESTRUCTOR in descriptor["Destructor"] — chiamato da passata
 * 4b dopo aver compilato il corpo del metodo DESTRUCTOR. */
static void cc_class_patch_dtor(struct cc_class* cl, double addr)
{
    if (cl && cl->dtor_entry) cl->dtor_entry->val_d = addr;
}

/* Patcha (in-place, nessuna realloc dopo cc_class_register) l'indirizzo di un metodo nel
 * VTable, o dell'unico costruttore in Constructors[0] — chiamato subito dopo aver
 * compilato il corpo corrispondente in passata 4 (l'indirizzo è c->code_len PRIMA di
 * compilare quel corpo). */
static void cc_class_patch_method(struct cc_class* cl, const char* method_name, double addr)
{
    for (int i = 0; i < cl->vtable_node->n; i++)
        if (strcasecmp(cl->vtable_node->entries[i].key_str, method_name) == 0) { cl->vtable_node->entries[i].val_d = addr; break; }
    /* PropGet_X/PropSet_X sono ANCHE una entry (chiave nuda "X") in propget_node/
       propset_node, aggiunta in cc_class_register — patchata qui insieme al VTable. */
    const char* bare = NULL; gmb_cconst_node_t* target = NULL;
    if (strncasecmp(method_name, "PropGet_", 8) == 0) { bare = method_name + 8; target = cl->propget_node; }
    else if (strncasecmp(method_name, "PropSet_", 8) == 0) { bare = method_name + 8; target = cl->propset_node; }
    if (bare && target)
        for (int i = 0; i < target->n; i++)
            if (strcasecmp(target->entries[i].key_str, bare) == 0) { target->entries[i].val_d = addr; break; }
}
static void cc_class_patch_ctor(struct cc_class* cl, double addr)
{
    if (cl->ctor_list_node->n > 0) cl->ctor_list_node->entries[0].val_d = addr;
}

/* Aggiunge al VTable/Constructors di ogni classe derivata gli indirizzi EREDITATI (non
 * sovrascritti) dalla propria base — chiamata DOPO che passata 4b ha compilato e
 * patchato i corpi di TUTTE le classi, quando gli indirizzi reali sono finalmente noti
 * (cc_class_register aveva già unito nomi/campi, ma non poteva scrivere indirizzi che
 * ancora non esistevano). Va invocata iterando c->classes[] in ORDINE DI REGISTRAZIONE
 * (0..nclass-1): una classe derivata ha SEMPRE indice > della propria base (verificato
 * in cc_class_register, che rifiuta una base non ancora registrata), quindi quando si
 * processa l'indice i la sua base a indice < i ha già il proprio merge (se a sua volta
 * derivata) completato — l'ereditarietà multi-livello si risolve così transitivamente. */
/* Copia in `dst` (chiave stringa) le entry di `src` il cui nome NON è già presente in
 * `dst` — usata per VTable/PropertyGetters/PropertySetters ereditati: un'entry già
 * presente vuol dire OVERRIDE (la classe derivata ha il proprio metodo/proprietà con lo
 * stesso nome), che vince sempre. */
static void cc_node_merge_missing_str(gmb_cconst_node_t* dst, const gmb_cconst_node_t* src)
{
    for (int si = 0; si < src->n; si++) {
        const char* name = src->entries[si].key_str;
        int already = 0;
        for (int di = 0; di < dst->n; di++)
            if (strcasecmp(dst->entries[di].key_str, name) == 0) { already = 1; break; }
        if (already) continue;
        gmb_cconst_entry_t* e = cc_node_add_str_key(dst, name);
        e->val_type = GMB_CCONST_DOUBLE; e->val_d = src->entries[si].val_d;
    }
}

static void cc_class_merge_inherited(gmb_cc_t* c)
{
    for (int i = 0; i < c->nclass; i++) {
        struct cc_class* cl = &c->classes[i];
        if (cl->is_interface || cl->base_idx < 0) continue;
        struct cc_class* base = &c->classes[cl->base_idx];
        if (base->is_interface) continue;   /* INHERITS è per classi, non interfacce (IMPLEMENTS) */
        cc_node_merge_missing_str(cl->vtable_node, base->vtable_node);
        cc_node_merge_missing_str(cl->propget_node, base->propget_node);
        cc_node_merge_missing_str(cl->propset_node, base->propset_node);
        if (cl->ctor_list_node->n == 0 && base->ctor_list_node->n > 0) {
            gmb_cconst_entry_t* e = cc_node_add_int_key(cl->ctor_list_node, 0);
            e->val_type = GMB_CCONST_DOUBLE; e->val_d = base->ctor_list_node->entries[0].val_d;
        }
    }
}

/* Compila il corpo di un metodo/costruttore di classe: SELF è SEMPRE il local 0 (poi i
 * parametri) — specchio del frame class-aware della VM (push_class_frame). Bug reale
 * scoperto in verifica: a differenza di una CALL normale (cc_proc), dove gli argomenti
 * arrivano sullo STACK OPERANDI e vanno "scaricati" nei locali con STORE_LOCAL, sia
 * OP_NEW_CLASS_INSTANCE (costruttore) SIA il dispatch VTable di OP_CALL_MEMBER (metodi)
 * scrivono SELF e gli argomenti DIRETTAMENTE nei locals del frame tramite l'array
 * "slots" passato a push_class_frame — NIENTE arriva sullo stack operandi. Emettere
 * comunque gli STORE_LOCAL (come un cc_proc qualunque) popperebbe silenziosamente
 * QUALUNQUE valore si trovasse in cima allo stack del CHIAMANTE al momento della
 * chiamata (residuo di un'espressione precedente, dato che non c'entra nulla con gli
 * argomenti) — corrompendo sia quel valore sia il parametro (mai scritto col vero
 * argomento, che restava quello già presente in locals[] dal frame). Risultato
 * osservato: `SELF.Valore = iniziale` nel costruttore lasciava Valore a 0 invece di
 * scrivere il valore passato a NEW Contatore(10). I costruttori (is_ctor=1) inoltre NON
 * emettono il PUSH_NIL/RETURN implicito standard di cc_proc: un RETURN "nudo" fa cadere
 * OP_RETURN sul fallback f->constructor_instance (ritorna SELF) SOLO se lo stack operandi
 * non ha residui oltre saved_sp — un PUSH_NIL incondizionato ci finirebbe SEMPRE sopra,
 * facendo tornare NIL invece dell'istanza (vedi OP_NEW_CLASS_INSTANCE/OP_RETURN in
 * gmb_vm_core.c). I metodi normali (is_ctor=0) restano SUB/FUNCTION comuni per il resto:
 * stessa convenzione return-slot di cc_proc (non registrati però in ch->proc_names/
 * arity — non richiamabili con CALL/nome semplice, solo via VTable/Constructors). */
static void cc_class_member(gmb_cc_t* c, const gmb_stmt_t* m, int is_ctor)
{
    c->nlocals = 0;
    cc_local_add(c, "SELF");
    /* i parametri DEVONO occupare gli slot 1..n_params SENZA buchi: push_class_frame
     * scrive slots[0]=SELF, slots[1]=arg0, slots[2]=arg1... POSIZIONALMENTE (nessun
     * STORE_LOCAL li rimappa, a differenza di una CALL normale — vedi il commento sopra
     * cc_class_member). Il local return-slot ("MethodName = espr", stessa convenzione di
     * cc_proc) va quindi allocato DOPO i parametri, mai in mezzo — altrimenti Ruberebbe lo
     * slot 1 al primo parametro, disallineando tutti gli argomenti di uno. */
    for (int i = 0; i < m->n_params; i++) cc_local_add(c, m->func_params[i]);
    /* PropGet_Nome (2.9): il parser sintetizza il nome della FUNZIONE come "PropGet_Nome",
     * ma il corpo GET...END GET dell'utente segue la stessa convenzione di una FUNCTION
     * — assegna al nome della PROPERTY nudo ("Nome = espr"), non al nome sintetico
     * interno mai visto dall'utente — quindi il local return-slot va registrato col nome
     * NUDO, altrimenti "Nome = espr" scriverebbe in un global scollegato (mai letto) e
     * il getter restituirebbe sempre NIL. */
    const char* retname = m->func_name;
    if (retname && strncasecmp(retname, "PropGet_", 8) == 0) retname = retname + 8;
    /* PropSet_Nome (2.9): invocato dalla VM in modo IMPLICITO da OP_STORE_FIELD (il
     * dispatch PropertySetters dentro l'assegnazione `obj.Nome = valore`, MAI da una vera
     * CALL/OP_CALL_MEMBER compilata da questo compilatore) — quel chiamante non si
     * aspetta e non consuma NESSUN valore di ritorno (un plain field-store non lascia
     * mai nulla sullo stack). Se PropSet_Nome, come un metodo normale, spinge comunque
     * un valore (LOAD_LOCAL retSlot o PUSH_NIL) prima del RETURN, quel valore resta
     * ORFANO sullo stack — nessuno lo pop mai. Bug reale osservato: `SELF.Raggio = r`
     * dentro un COSTRUTTORE lasciava quel valore orfano lì, e al bare RETURN del
     * costruttore stesso (que conta gli operandi residui oltre saved_sp per decidere se
     * ritornare SELF, vedi il commento su is_ctor sotto) quel residuo dirottava
     * l'istanza — `c.X` falliva con "indexing a numeric scalar" perché "c" non era più
     * il dict-istanza. Fix: PropSet_Nome è trattato come i costruttori, RETURN nudo
     * (nessun push) — nessun valore da orfanare. */
    int is_propset = retname && strncasecmp(retname, "PropSet_", 8) == 0;
    int retSlot = (!is_ctor && !is_propset && retname) ? cc_local_add(c, retname) : -1;
    /* Bug reale scoperto in verifica: la convenzione "NomeProprio = espr" per un
     * OPERATOR funziona SOLO se la riga compare al livello più esterno del corpo
     * dell'operatore — lì il parser (gmb_p_parse_class) riconosce esplicitamente
     * "OPERATOR = espr" e scrive target=<nome sintetico, es. "OP_EQ">. Una riga
     * IDENTICA ma ANNIDATA (dentro un IF/WHILE/ecc., quindi raggiunta tramite
     * gmb_p_parse_stmt ricorsivo, non quel loop speciale) cade nel percorso generico
     * "keyword seguita da '='" (kw_eq in gmb_p_parse_stmt), che non conosce il nome
     * sintetico e produce invece target="OPERATOR" LETTERALE — una variabile GLOBALE
     * estranea creata al volo, MAI collegata al vero local return-slot. Risultato: un
     * OPERATOR con `IF cond THEN OPERATOR = -1 ELSE OPERATOR = 0 END IF` restituiva
     * sempre 0/NIL (il valore del local return-slot, mai aggiornato dai rami annidati)
     * anche se un PRINT interno di "OPERATOR" (letto/scritto in modo autoconsistente
     * sulla stessa variabile estranea) sembrava mostrare il valore "giusto". Fix:
     * riusa lo stesso meccanismo generico già esistente per "FUNCTION" (usato dalle
     * closure, vedi cc_store_target/cc_load_target) — imposta current_proc_name al
     * nome sintetico e fai risolvere "OPERATOR" letterale allo stesso modo. */
    c->current_proc_name = retname;
    cc_byte(c, GMB_OP_CLEAR_RETURN);
    /* EVENT (2.9): prologo del costruttore — inizializza OGNI registro handler a una
     * lista vuota PRIMA del corpo utente (che potrebbe già fare ADD HANDLER se il
     * costruttore stesso, insolito ma non vietato, lo facesse). Emesso qui (non nel
     * corpo, mai visto dal parser) — vedi cc_class_register per il perché serve
     * SEMPRE un costruttore (anche sintetizzato) quando la classe ha eventi. */
    if (is_ctor && c->cur_class >= 0) {
        struct cc_class* ecl = &c->classes[c->cur_class];
        for (int i = 0; i < ecl->n_events; i++) {
            cc_byte(c, GMB_OP_MK_ARRAY); cc_u16(c, 0);
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, 0);   /* SELF */
            char fkey[80]; snprintf(fkey, sizeof(fkey), "__event_%s", ecl->event_names[i]);
            int fi = cc_add_const(c, GMB_CCONST_STRING, 0, fkey);
            if (c->error) return;
            cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi);
        }
    }
    c->in_proc = 1;
    for (int i = 0; i < m->n_funcbody; i++) cc_stmt(c, m->func_body[i]);
    c->in_proc = 0;
    c->current_proc_name = NULL;
    if (is_ctor || is_propset) {
        /* Bug reale scoperto in verifica: gmb_task_store_local (gmb_gpm.c) aggiorna
         * INCONDIZIONATAMENTE f->return_value ad OGNI STORE_LOCAL, qualunque sia lo
         * slot — non solo quelli deliberatamente destinati al valore di ritorno. Per un
         * costruttore, OP_RETURN dà priorità a return_value (se non zero) SU
         * constructor_instance (il fallback "ritorna SELF") — quindi QUALUNQUE local
         * variable assegnata nel corpo del costruttore (persino un `DIM temp` innocuo, o
         * — come nel caso scoperto per primo — un parametro riscritto su se stesso per
         * una collisione di nome case-insensitive con un campo, es. `R = r`) lascia
         * return_value diverso da zero, dirottando OP_RETURN: invece dell'istanza,
         * ritorna l'ULTIMO valore locale assegnato (spesso un NUMERO) — poi ogni accesso
         * successivo a un campo su quella "istanza" (in realtà un numero) fallisce con
         * "indexing a numeric scalar". Fix: CLEAR_RETURN appena prima del RETURN nudo,
         * azzera return_value indipendentemente da cosa sia successo nel corpo — il
         * fallback constructor_instance torna così l'unico candidato con priorità. */
        cc_byte(c, GMB_OP_CLEAR_RETURN);
        cc_byte(c, GMB_OP_RETURN);
    } else {
        if (retSlot >= 0) { cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, retSlot); }
        else cc_byte(c, GMB_OP_PUSH_NIL);
        cc_byte(c, GMB_OP_RETURN);
    }
}

/* VAR/ident → costante stringa col NOME; STR → costante; altro → expr runtime */
static void cc_push_str_arg(gmb_cc_t* c, const gmb_expr_t* e)
{
    if (!e) { cc_push_str_const(c, ""); return; }
    if (e->kind == GMB_EXPR_STR) { cc_push_str_const(c, e->str ? e->str : ""); return; }
    if (e->kind == GMB_EXPR_VAR) { const char* nm = e->var ? e->var : ""; if (nm[0] == '%') nm++; cc_push_str_const(c, nm); return; }
    cc_expr(c, e);
}

static void cc_syscall(gmb_cc_t* c, int code, int argc)
{
    cc_byte(c, GMB_OP_SYSCALL);
    cc_u16(c, code);
    cc_byte(c, argc);
}

static void cc_block(gmb_cc_t* c, gmb_stmt_t** body, int n)
{
    for (int i = 0; i < n; i++) cc_stmt(c, body[i]);
}

static void cc_reljump(gmb_cc_t* c, int cond_op, int target);
static void cc_reljump_back(gmb_cc_t* c, int from_pos);

/* Aggiunge l'elemento al risultato comprehension: lista/set → AADD(sel) su builtin 529;
 * dict → dict[comp_key] = comp_sel via STORE_ELEMENT(1) sul ref del risultato. */
static void cc_comp_emit_put(gmb_cc_t* c, const gmb_expr_t* e, const char* res_name)
{
    if (e->comp_ctype == 1) {   /* dict */
        cc_expr(c, e->comp_sel);   /* value */
        if (c->error) return;
        cc_load_target(c, res_name);
        if (c->error) return;
        cc_expr(c, e->comp_key);   /* key */
        if (c->error) return;
        cc_byte(c, GMB_OP_STORE_ELEMENT); cc_byte(c, 1);
        return;
    }
    cc_load_target(c, res_name);
    if (c->error) return;
    cc_expr(c, e->comp_sel);
    if (c->error) return;
    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 529); cc_byte(c, 2);
    cc_byte(c, GMB_OP_POP);
}

/* Emette il loop di una clausola comprehension e ricorre sulle successive. All'ultima
 * clausola (ci==total) applica WHERE (IF) e AADD della proiezione (lista). */
static void cc_comp_clause(gmb_cc_t* c, const gmb_expr_t* e, int ci, int total,
                           const char* const* vars, gmb_expr_t* const* srcs,
                           int res_slot, const char* res_name)
{
    if (ci == total) {
        if (c->error) return;
        if (e->comp_where) {
            cc_expr(c, e->comp_where);
            if (c->error) return;
            { int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z); }
            cc_byte(c, GMB_OP_CMP_EQ);
            cc_reljump(c, GMB_OP_JUMP_IF_TRUE, -1);
            int skipadd_jmp = c->nrel - 1;
            cc_comp_emit_put(c, e, res_name);
            if (c->error) return;
            int after = c->code_len;
            if (c->reljumps[skipadd_jmp].target < 0) c->reljumps[skipadd_jmp].target = after;
        } else {
            cc_comp_emit_put(c, e, res_name);
        }
        return;
    }
    cc_expr(c, srcs[ci]);
    if (c->error) return;
    char sname[32]; snprintf(sname, sizeof(sname), "__comp_s%d_%d", c->comp_uid, ci);
    char iname[32]; snprintf(iname, sizeof(iname), "__comp_i%d_%d", c->comp_uid, ci);
    char fname[32]; snprintf(fname, sizeof(fname), "__comp_f%d_%d", c->comp_uid, ci);
    int arr_slot = cc_local_add(c, sname);
    int idx_slot = cc_local_add(c, iname);
    int flag_slot = cc_local_add(c, fname);
    cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, arr_slot);
    { int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z); }
    cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, idx_slot);
    int v_slot = cc_local_add(c, vars[ci]);
    int ld = c->loop_depth < 15 ? c->loop_depth++ : 15;
    int loop_start = c->code_len;
    cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, arr_slot);
    cc_byte(c, GMB_OP_FE_DICT); cc_u16(c, idx_slot); cc_byte(c, 1);
    cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, flag_slot);
    cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, v_slot);
    cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, flag_slot);
    { int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z); }
    cc_byte(c, GMB_OP_CMP_EQ);
    cc_reljump(c, GMB_OP_JUMP_IF_TRUE, -1);
    int exit_jmp = c->nrel - 1;
    cc_comp_clause(c, e, ci + 1, total, vars, srcs, res_slot, res_name);
    if (c->error) return;
    cc_reljump_back(c, loop_start);
    int end = c->code_len;
    if (c->reljumps[exit_jmp].target < 0) c->reljumps[exit_jmp].target = end;
    for (int i = 0; i < c->exit_count[ld]; i++) { int eoff = end - (c->exit_pos[ld][i] + 2); uint8_t* pc = c->code + c->exit_pos[ld][i]; pc[0] = (uint8_t)((eoff >> 8) & 0xFF); pc[1] = (uint8_t)(eoff & 0xFF); }
    c->exit_count[ld] = 0;
    c->loop_depth--;
}

/* Lowering comprehension (LINQ FROM...SELECT e `[x FOR ...]`/`{...FOR...}`): crea la lista
 * risultato (OP_MK_LIST 0), itera le clausole (FE, nidificate), WHERE/IF filtra, AADD(proiezione),
 * ORDER BY → ASORT; lascia il risultato sullo stack. */
static void cc_comp_lowering(gmb_cc_t* c, const gmb_expr_t* e)
{
    if (c->error) return;
    if (!e->comp_var || !e->comp_src || !e->comp_sel) { cc_err(c, "comprehension: clausole incomplete"); return; }
    char res_name[32]; snprintf(res_name, sizeof(res_name), "__comp_r%d", ++c->comp_uid);
    if (e->comp_ctype == 1) cc_byte(c, GMB_OP_MK_DICT);  /* dict comprehension: dict vuoto */
    else cc_byte(c, GMB_OP_MK_LIST);
    cc_u16(c, 0);
    int res_slot = cc_local_add(c, res_name);
    cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, res_slot);
    int total = 1 + e->n_comp_more;
    const char** vars = (const char**)alloca((size_t)total * sizeof(char*));
    gmb_expr_t** srcs = (gmb_expr_t**)alloca((size_t)total * sizeof(gmb_expr_t*));
    vars[0] = e->comp_var; srcs[0] = e->comp_src;
    for (int i = 1; i < total; i++) { vars[i] = e->comp_more_var[i - 1]; srcs[i] = e->comp_more_src[i - 1]; }
    cc_comp_clause(c, e, 0, total, vars, srcs, res_slot, res_name);
    if (c->error) return;
    if (e->comp_order) {
        cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, res_slot);
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 535); cc_byte(c, e->comp_desc ? 3 : 1);
        cc_byte(c, GMB_OP_POP);
    }
    cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, res_slot);
}

/* ── FFI (DECLARE ... LIB): emissione OP_CALL_FFI_T ─────────────────────────────────────
 * Stesso formato/valori del compilatore C# (Compiler.Expressions.cs) e della VM/C
 * (gmb_vm_core.c case OP_CALL_FFI_T). Copre: scalari DOUBLE/SINGLE e interi a 8 byte
 * (anche MISTI LONG+DOUBLE), array BYREF (dst() AS DOUBLE), STRING (parametro e ritorno),
 * BYREF scalare. UDT per valore e callback NON ancora supportati (verrebbero mal
 * marshallati): i loro campi restano a 0/255, quindi la VM li tratta come scalari. */
static int cc_ffi_is_int_type(const char* t)
{
    if (!t || !t[0]) return 0;
    switch (gmb_keyword_tok(t)) {
        case GMB_TOK_Byte: case GMB_TOK_SByte: case GMB_TOK_Integer: case GMB_TOK_UInteger:
        case GMB_TOK_Long: case GMB_TOK_ULong: case GMB_TOK_Quad: case GMB_TOK_UQuad:
            return 1;
        default: return 0;
    }
}
static int cc_ffi_is_string_type(const char* t)
{
    return t && gmb_keyword_tok(t) == GMB_TOK_StringType;
}
/* Foglia scalare di un UDT (path-nome punteggiato per i nidificati + kind). */
typedef struct { char path[128]; int is_int; } cc_ffi_leaf_t;
static void cc_udt_leaves_rec(gmb_cc_t* c, const char* udt_name, const char* prefix,
                              cc_ffi_leaf_t* out, int* n, int max, int depth)
{
    int ui = cc_udt_find(c, udt_name);
    if (ui < 0 || depth > 4) return;
    const struct cc_udt* u = &c->udts[ui];
    for (int i = 0; i < u->n_fields && *n < max; i++) {
        char np[128];
        if (prefix && prefix[0]) snprintf(np, sizeof np, "%s.%s", prefix, u->field_names[i]);
        else snprintf(np, sizeof np, "%s", u->field_names[i]);
        int nested = cc_udt_find(c, u->field_types[i]);
        if (nested >= 0) cc_udt_leaves_rec(c, u->field_types[i], np, out, n, max, depth + 1);
        else if (*n < max) { snprintf(out[*n].path, sizeof out[*n].path, "%s", np); out[*n].is_int = cc_ffi_is_int_type(u->field_types[i]); (*n)++; }
    }
}
/* CSV di SymbolId-path per una foglia (un id per SEGMENTO del path). Il compilatore C non
 * ha SymbolId stabili per i campi: si emettono zeri (la VM li usa solo per il fallback
 * "__m<id>"); l'accesso ai campi avviene per NOME, scritto da vm_ffi_set_leaf. */
static void cc_ffi_symid_csv(const char* path, char* out, size_t cap)
{
    int segs = 1; for (const char* q = path; *q; q++) if (*q == '.') segs++;
    size_t o = 0; out[0] = 0;
    for (int s = 0; s < segs && o + 3 < cap; s++) o += (size_t)snprintf(out + o, cap - o, s ? ",0" : "0");
}
static void cc_emit_ffi_call(gmb_cc_t* c, const gmb_expr_t* e, const gmb_stmt_t* pd)
{
    int argc = e->nargs;
    int nparams = pd->n_params;
    if (argc > 8) { cc_err(c, "FFI: massimo 8 argomenti supportati dal marshalling"); return; }
    const char* lib = pd->ffi_lib ? pd->ffi_lib : "";
    const char* sym = pd->ffi_symbol ? pd->ffi_symbol : pd->func_name;
    int is_func = !pd->is_sub;
    const char* rt = pd->func_ret_type;
    int ret_is_string = (is_func && cc_ffi_is_string_type(rt)) ? 1 : 0;
    int ret_udt_idx = (is_func && rt) ? cc_udt_find(c, rt) : -1;

    unsigned char is_array[8] = {0}, is_string[8] = {0}, is_byref[8] = {0}, is_plain[8] = {0}, is_udt[8] = {0}, is_cb[8] = {0}, p_int[8] = {0};
    cc_ffi_leaf_t param_leaves[8][8]; int n_param_leaves[8] = {0};
    for (int i = 0; i < argc && i < 8; i++) {
        const char* pt = (pd->param_types && i < nparams) ? pd->param_types[i] : NULL;
        is_array[i] = (pd->param_is_array && pd->param_is_array[i]) ? 1 : 0;
        is_string[i] = (!is_array[i] && cc_ffi_is_string_type(pt)) ? 1 : 0;
        is_cb[i] = (!is_array[i] && !is_string[i] && pd->param_fn_arity && pd->param_fn_arity[i] >= 0) ? 1 : 0;
        is_byref[i] = (!is_array[i] && !is_string[i] && !is_cb[i] && pd->param_byref && pd->param_byref[i]) ? 1 : 0;
        is_udt[i] = (!is_array[i] && !is_string[i] && !is_cb[i] && !is_byref[i] && pt && cc_udt_find(c, pt) >= 0) ? 1 : 0;
        is_plain[i] = (!is_array[i] && !is_string[i] && !is_byref[i] && !is_udt[i] && !is_cb[i]) ? 1 : 0;
        p_int[i] = cc_ffi_is_int_type(pt) ? 1 : 0;
        if (is_udt[i]) cc_udt_leaves_rec(c, pt, "", param_leaves[i], &n_param_leaves[i], 8, 0);
    }
    cc_ffi_leaf_t ret_leaves[8]; int n_ret_leaves = 0;
    if (ret_udt_idx >= 0) cc_udt_leaves_rec(c, rt, "", ret_leaves, &n_ret_leaves, 8, 0);
    int ret_udt_mixed = 0;
    if (n_ret_leaves > 1) { int f0 = ret_leaves[0].is_int; for (int i = 1; i < n_ret_leaves; i++) if (ret_leaves[i].is_int != f0) ret_udt_mixed = 1; }
    int ret_udt_kind = (n_ret_leaves > 0) ? ret_leaves[0].is_int : 0;

    /* kind di fallback (SUB o FUNCTION a ritorno misto): kind del PRIMO parametro NON-UDT-misto
     * — un parametro UDT a kind misto è ESCLUSO da paramKinds (come Compiler.Expressions.cs),
     * quindi con soli parametri misti il fallback è Int64. */
    int first_p_kind = 1;
    for (int i = 0; i < argc && i < 8; i++) {
        if (is_udt[i]) {
            int k0 = (n_param_leaves[i] > 0) ? param_leaves[i][0].is_int : 0;
            int mixed = 0;
            for (int f = 0; f < n_param_leaves[i]; f++) if (param_leaves[i][f].is_int != k0) { mixed = 1; break; }
            if (mixed) continue;
            first_p_kind = k0; break;
        }
        first_p_kind = (is_array[i] || is_string[i] || is_byref[i] || is_cb[i]) ? 1 : p_int[i];
        break;
    }
    /* kind: FUNCTION non-mixed-return → kind del ritorno; mixed return/SUB → 1° parametro. */
    int kind;
    if (is_func) {
        if (ret_is_string) kind = 1;
        else if (ret_udt_idx >= 0 && !ret_udt_mixed) kind = ret_udt_kind;
        else if (ret_udt_idx >= 0) kind = first_p_kind;
        else kind = cc_ffi_is_int_type(rt) ? 1 : 0;
    } else kind = first_p_kind;

    unsigned char scalarKindMask = 0, arrayMask = 0, arrayElemIntMask = 0, stringMask = 0;
    unsigned char byrefScalarMask = 0, byrefScalarElemIntMask = 0;
    unsigned char udtParamFieldCount[8] = {0}, udtParamFieldKindMask[8] = {0};
    unsigned char retUdtFieldCount = (unsigned char)n_ret_leaves, retUdtFieldKindMask = 0;
    unsigned char cbArity[8], cbKindMask = 0, cbHasRetMask = 0;
    for (int i = 0; i < 8; i++) cbArity[i] = 255;
    for (int i = 0; i < argc && i < 8; i++) {
        if (is_array[i]) { arrayMask |= (unsigned char)(1 << i); if (p_int[i]) arrayElemIntMask |= (unsigned char)(1 << i); }
        else if (is_string[i]) { stringMask |= (unsigned char)(1 << i); }
        else if (is_byref[i]) { byrefScalarMask |= (unsigned char)(1 << i); if (p_int[i]) byrefScalarElemIntMask |= (unsigned char)(1 << i); }
        else if (is_udt[i]) {
            udtParamFieldCount[i] = (unsigned char)n_param_leaves[i];
            for (int f = 0; f < n_param_leaves[i]; f++) if (param_leaves[i][f].is_int) udtParamFieldKindMask[i] |= (unsigned char)(1 << f);
        } else if (is_cb[i]) {
            int ar = pd->param_fn_arity[i];
            cbArity[i] = (unsigned char)(ar > 4 ? 4 : ar);
            int cb_int = (ar > 0) ? ((pd->param_fn_kind_mask && (pd->param_fn_kind_mask[i] & 1)) ? 1 : 0)
                                  : ((pd->param_fn_ret_int && pd->param_fn_ret_int[i]) ? 1 : 0);
            if (cb_int) cbKindMask |= (unsigned char)(1 << i);
            if (pd->param_fn_has_ret && pd->param_fn_has_ret[i]) cbHasRetMask |= (unsigned char)(1 << i);
        }
        /* I parametri STRING sono PUNTATORI (classe intera) → bit sempre 1, anche in una
         * firma "mista" a ritorno scalare (es. MARKETBUY(LONG, STRING, DOUBLE...)): il path
         * mixed li passa come int64 con args[i] = indirizzo dei dati (vedi gmb_vm_core.c). */
        int bit = is_plain[i] ? p_int[i] : (is_string[i] ? 1 : (kind == 1));
        if (bit) scalarKindMask |= (unsigned char)(1 << i);
    }
    for (int f = 0; f < n_ret_leaves; f++) if (ret_leaves[f].is_int) retUdtFieldKindMask |= (unsigned char)(1 << f);

    /* Argomenti: i BYREF scalari viaggiano come puntatore (OP_MAKE_BYREF) — come una CALL
     * normale (cc_call_arg), altrimenti il copy-out della VM non ha dove scrivere. */
    uint64_t ffi_brm = 0;
    for (int i = 0; i < argc && i < 8; i++) if (is_byref[i]) ffi_brm |= ((uint64_t)1 << i);
    for (int i = 0; i < argc; i++) { cc_call_arg(c, ffi_brm, i, e->args[i]); if (c->error) return; }

    int libIdx = cc_add_const(c, GMB_CCONST_STRING, 0, lib);
    int nameIdx = cc_add_const(c, GMB_CCONST_STRING, 0, sym);
    if (c->error) return;

    /* Firma all-DOUBLE senza puntatori/UDT/callback → opcode LEGACY OP_CALL_FFI (unico che
     * copre la chiamata a 0 argomenti, es. BARS()). */
    int any_ptr = (ret_is_string || ret_udt_idx >= 0) ? 1 : 0;
    for (int i = 0; i < argc && i < 8; i++)
        if (is_array[i] || is_string[i] || is_byref[i] || is_udt[i] || is_cb[i]) any_ptr = 1;
    if (!any_ptr && kind == 0 && scalarKindMask == 0) {
        cc_byte(c, GMB_OP_CALL_FFI);
        cc_u16(c, (uint16_t)libIdx);
        cc_u16(c, (uint16_t)nameIdx);
        cc_byte(c, (uint8_t)argc);
        return;
    }

    cc_byte(c, GMB_OP_CALL_FFI_T);
    cc_u16(c, (uint16_t)libIdx);
    cc_u16(c, (uint16_t)nameIdx);
    cc_byte(c, (uint8_t)argc);
    cc_byte(c, (uint8_t)kind);
    cc_byte(c, scalarKindMask);
    cc_byte(c, arrayMask);
    cc_byte(c, arrayElemIntMask);
    cc_byte(c, stringMask);
    cc_byte(c, (uint8_t)ret_is_string);
    cc_byte(c, byrefScalarMask);
    cc_byte(c, byrefScalarElemIntMask);
    for (int i = 0; i < 8; i++) cc_byte(c, udtParamFieldCount[i]);
    for (int i = 0; i < 8; i++) cc_byte(c, udtParamFieldKindMask[i]);
    cc_byte(c, retUdtFieldCount);
    cc_byte(c, retUdtFieldKindMask);
    for (int i = 0; i < 8; i++) cc_byte(c, cbArity[i]);
    cc_byte(c, cbKindMask);
    cc_byte(c, cbHasRetMask);
    /* Coda: per ogni foglia UDT (prima i parametri, poi il ritorno), DUE u16 (path-nomi,
     * SymbolId-path CSV). */
    char csv[64];
    for (int i = 0; i < argc && i < 8; i++) {
        if (!is_udt[i]) continue;
        for (int f = 0; f < n_param_leaves[i]; f++) {
            cc_ffi_symid_csv(param_leaves[i][f].path, csv, sizeof csv);
            int pi = cc_add_const(c, GMB_CCONST_STRING, 0, param_leaves[i][f].path);
            int si = cc_add_const(c, GMB_CCONST_STRING, 0, csv);
            if (c->error) return;
            cc_u16(c, (uint16_t)pi);
            cc_u16(c, (uint16_t)si);
        }
    }
    if (ret_udt_idx >= 0) {
        for (int f = 0; f < n_ret_leaves; f++) {
            cc_ffi_symid_csv(ret_leaves[f].path, csv, sizeof csv);
            int pi = cc_add_const(c, GMB_CCONST_STRING, 0, ret_leaves[f].path);
            int si = cc_add_const(c, GMB_CCONST_STRING, 0, csv);
            if (c->error) return;
            cc_u16(c, (uint16_t)pi);
            cc_u16(c, (uint16_t)si);
        }
    }
}

/* Zucchero GraphicsContext su VM/C: `g.Clear()/SetColor/Line/Rect/FillRect/Circle/
 * FillCircle/Text/Plot(...)` con g = parametro ANY del metodo Paint → syscall 443
 * (CONTROL DRAW) con handle = valore del receiver. Parità con il rewrite C# (AnyType). */
/* Indice evento controllo (stessa tabella di Compiler.Pixel.CtrlEvIndex / gmb_form_ev_index). */
static int cc_ev_index(const char* n)
{
    if (!n) return 0;
    static const struct { const char* n; int v; } m[] = {
        {"CLICK",1},{"DBLCLICK",2},{"CHANGE",3},{"GOTFOCUS",4},{"LOSTFOCUS",5},
        {"MOUSEDOWN",6},{"MOUSEUP",7},{"MOUSEMOVE",8},{"KEYDOWN",9},{"KEYPRESS",10},
        {"KEYUP",11},{"DRAGDROP",12},{"DRAGOVER",13},{"VALIDATE",14},{"SCROLL",15},
        {"DROPDOWN",16},{"UPCLICK",17},{"DOWNCLICK",18},{"NODECLICK",19},{"EXPAND",20},
        {"COLLAPSE",21},{"SELCHANGE",22},{"ROWCOLCHANGE",23},{"WHEN",24}
    };
    for (size_t i = 0; i < sizeof(m)/sizeof(m[0]); i++)
        if (strcasecmp(n, m[i].n) == 0) return m[i].v;
    return 0;
}

static int cc_gfxctx_sugar(gmb_cc_t* c, const gmb_expr_t* e)
{
    if (!e || e->kind != GMB_EXPR_CALL || !e->var) return 0;
    const char* dot = strchr(e->var, '.');
    if (!dot || strchr(dot + 1, '.')) return 0;
    size_t rl = (size_t)(dot - e->var);
    if (rl == 0 || rl >= 64) return 0;
    char recv[64]; memcpy(recv, e->var, rl); recv[rl] = 0;
    const char* m = dot + 1;
    int op = -1, na = 0, text_arg = -1;
    if (!strcasecmp(m, "Clear")) { op = 0; na = 0; }
    else if (!strcasecmp(m, "SetColor")) { op = 1; na = 1; }
    else if (!strcasecmp(m, "Line")) { op = 2; na = 4; }
    else if (!strcasecmp(m, "Rect")) { op = 3; na = 4; }
    else if (!strcasecmp(m, "FillRect")) { op = 4; na = 4; }
    else if (!strcasecmp(m, "Circle")) { op = 5; na = 3; }
    else if (!strcasecmp(m, "FillCircle")) { op = 6; na = 3; }
    else if (!strcasecmp(m, "Plot")) { op = 8; na = 2; }
    else if (!strcasecmp(m, "Text")) { op = 7; na = 2; text_arg = 0; }
    else return 0;
    /* il receiver deve essere una variabile reale (parametro/local/global) */
    if (cc_local(c, recv) < 0 && cc_global(c, recv, 0) < 0) return 0;
    /* Metodi Control (classi che ereditano Control): Move/SetFocus/ZOrder/Invalidate/Refresh
     * → CONTROL MOVE/FOCUS/ZORDER/INVALIDATE/REFRESH (nome = variabile dell'istanza). */
    {
        int code = 0, na = 1;
        if (!strcasecmp(m, "Move")) { code = 407; na = 5; }
        else if (!strcasecmp(m, "SetFocus")) { code = 408; na = 1; }
        else if (!strcasecmp(m, "ZOrder")) { code = 409; na = 2; }
        else if (!strcasecmp(m, "Invalidate")) { code = 448; na = 1; }
        else if (!strcasecmp(m, "Refresh")) { code = 449; na = 1; }
        if (code) {
            cc_push_str_const(c, recv);
            for (int i = 1; i < na; i++) {
                const gmb_expr_t* a = (i - 1 < e->nargs) ? e->args[i - 1] : NULL;
                if (a) cc_expr(c, a); else cc_byte(c, GMB_OP_PUSH_NIL);
                if (c->error) return 1;
            }
            cc_syscall(c, code, na);
            { gmb_expr_t zero; memset(&zero, 0, sizeof zero); zero.kind = GMB_EXPR_NUM; cc_expr(c, &zero); }
            return 1;
        }
    }
    cc_load_target(c, recv);                     /* handle */
    if (c->error) return 1;
    { gmb_expr_t num; memset(&num, 0, sizeof num); num.kind = GMB_EXPR_NUM; num.num = (double)op; cc_expr(c, &num); }
    for (int i = 0; i < 4; i++) {
        const gmb_expr_t* a = NULL;
        if (text_arg >= 0) { if (i < na) a = (text_arg + 1 + i < e->nargs) ? e->args[text_arg + 1 + i] : NULL; }
        else if (i < na) a = (i < e->nargs) ? e->args[i] : NULL;
        if (a) cc_expr(c, a); else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return 1;
    }
    if (text_arg >= 0 && text_arg < e->nargs && e->args[text_arg]) cc_expr(c, e->args[text_arg]);
    else cc_push_str_const(c, "");
    cc_syscall(c, 443, 7);
    /* valore fittizio (il sugar è usato come istruzione; il POP dello stmt lo scarta) */
    { gmb_expr_t zero; memset(&zero, 0, sizeof zero); zero.kind = GMB_EXPR_NUM; zero.num = 0; cc_expr(c, &zero); }
    return 1;
}

static void cc_expr(gmb_cc_t* c, const gmb_expr_t* e)
{
    if (!e) { cc_byte(c, GMB_OP_PUSH_NIL); return; }
    /* `.VALUE` su variabile semplice (tipo nullable: value = valore, o 0/nil se vuoto):
     * identità della base, niente LOAD_ELEMENT con chiave (che su uno scalare darebbe
     * "indexing a numeric scalar"). Solo se la base è una variabile locale/globale reale. */
    switch (e->kind) {
    case GMB_EXPR_LAMBDA:
        cc_emit_lambda(c, e);
        break;
    case GMB_EXPR_COMPREHENSION:
        /* Lowering: OP_MK_LIST(0) crea la lista risultato; poi loop sulla sorgente (FE),
         * filtro WHERE, AADD(proiezione), ASORT se ORDER BY; il risultato resta sullo stack. */
        cc_comp_lowering(c, e);
        break;
    case GMB_EXPR_NUM: {
        int idx = cc_add_const(c, GMB_CCONST_DOUBLE, e->num, NULL);
        if (c->error) return;
        cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, idx);
        break;
    }
    case GMB_EXPR_STR: {
        int idx = cc_add_const(c, GMB_CCONST_STRING, 0, e->str ? e->str : "");
        if (c->error) return;
        cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, idx);
        break;
    }
    case GMB_EXPR_VAR: {
        if (e->var && e->nargs == 0 && strchr(e->var, '.')) {
            const char* dot = strchr(e->var, '.');
            size_t bl = (size_t)(dot - e->var);
            if (bl < 127) {
                char base[128], field[128];
                memcpy(base, e->var, bl); base[bl] = 0;
                snprintf(field, sizeof(field), "%s", dot + 1);
                int ti = cc_tuple_field_index(base, field);
                if (ti >= 0) {
                    gmb_expr_t** ia = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                    ia[0] = gmb_expr_var(base, e->line);
                    ia[1] = gmb_expr_num((double)ti, e->line);
                    gmb_expr_t* ic = gmb_expr_call("_idx_", ia, 2, e->line);
                    cc_expr(c, ic);
                    gmb_expr_free(ic);
                    break;
                }
            }
        }
        if (e->var && !strchr(e->var, '.') && e->nargs == 0
            && cc_dyn_is(c, e->var) && cc_local(c, e->var) < 0) {
            cc_push_str_const(c, e->var); cc_syscall(c, 473, 1); break;
        }
        /* costante utente (CONST) dichiarata nel programma: risolta in compile-time */
        if (e->var && !strchr(e->var, '.') && e->nargs == 0) {
            double cv; int cis; const char* csv;
            if (cc_const_get(c, e->var, &cv, &cis, &csv)) {
                if (cis && csv) {
                    int ci2 = cc_add_const(c, GMB_CCONST_STRING, 0, csv);
                    cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ci2);
                } else {
                    int ci2 = cc_add_const(c, GMB_CCONST_DOUBLE, cv, NULL);
                    cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ci2);
                }
                break;
            }
        }
        /* %NOME: costante builtin (specchio StdLib.Constants.cs). Il prefisso % arriva
           dal parser; se la costante non è in tabella si ripiega sul comportamento
           pregresso (nome senza %) per non rompere i costrutti che lo usano come token. */
        if (e->var && e->var[0] == '%') {
            double v;
            if (cc_pconst_lookup(e->var + 1, &v)) {
                int idx = cc_add_const(c, GMB_CCONST_DOUBLE, v, NULL);
                if (c->error) return;
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, idx);
                break;
            }
            cc_load_target(c, e->var + 1);
            break;
        }
        /* accesso a membro senza parentesi: obj.Member → LOAD_ELEMENT con chiave stringa
         * (campo normale) — con estensioni 2.9 per STATIC (ClassName.Campo o
         * obj.Campo, un GLOBALE dedicato invece del dict-istanza) e PROPERTY
         * (obj.Nome con getter → OP_CALL_MEMBER 0 argomenti, PropertyGetters lato VM). */
        if (e->var) {
            /* Accesso a campo NIDIFICATO `a.b.c` (>=2 punti): catena di LOAD_ELEMENT
             * (LOAD a; elem "b"; elem "c"). Senza questo il ramo a punto singolo sotto
             * risolveva "a.b" come nome-variabile (auto-globale) → nesting rotto. */
            const char* fdot = strchr(e->var, '.');
            const char* ldot = strrchr(e->var, '.');
            if (fdot && fdot != ldot) {
                char hd[256]; size_t hl = (size_t)(fdot - e->var);
                if (hl < sizeof hd) {
                    memcpy(hd, e->var, hl); hd[hl] = 0;
                    if ((cc_local(c, hd) >= 0 || cc_global(c, hd, 0) >= 0)
                        && cc_local(c, e->var) < 0 && cc_global(c, e->var, 0) < 0) {
                        cc_load_target(c, hd);
                        const char* seg = fdot + 1;
                        while (seg && *seg) {
                            const char* nd = strchr(seg, '.');
                            size_t sl = nd ? (size_t)(nd - seg) : strlen(seg);
                            char s2[128]; if (sl >= sizeof s2) sl = sizeof s2 - 1;
                            memcpy(s2, seg, sl); s2[sl] = 0;
                            cc_push_str_const(c, s2);
                            if (c->error) return;
                            cc_byte(c, GMB_OP_LOAD_ELEMENT); cc_byte(c, 1);
                            seg = nd ? nd + 1 : NULL;
                        }
                        break;
                    }
                }
            }
            const char* dot = strrchr(e->var, '.');
            if (dot && dot != e->var) {
                char mbase[256];
                size_t mlen = (size_t)(dot - e->var);
                if (mlen < sizeof(mbase)) {
                    memcpy(mbase, e->var, mlen); mbase[mlen] = 0;
                    /* ClassName.Campo: STATIC referenziato tramite il nome della classe,
                       non un'istanza — mbase non è né local né global. */
                    int cli0 = cc_class_find(c, mbase);
                    if (cli0 >= 0) {
                        int slot0 = cc_class_static_slot_of(&c->classes[cli0], dot + 1);
                        if (slot0 >= 0) { cc_byte(c, GMB_OP_LOAD_GLOBAL); cc_u16(c, slot0); break; }
                    }
                    if (cc_local(c, mbase) >= 0 || cc_global(c, mbase, 0) >= 0) {
                        const char* tn0 = cc_var_udt_get(c, mbase);
                        int cli1 = tn0 ? cc_class_find(c, tn0) : -1;
                        if (cli1 >= 0) {
                            int slot1 = cc_class_static_slot_of(&c->classes[cli1], dot + 1);
                            if (slot1 >= 0) { cc_byte(c, GMB_OP_LOAD_GLOBAL); cc_u16(c, slot1); break; }
                            if (cc_class_is_prop_of(&c->classes[cli1], dot + 1)) {
                                cc_load_target(c, mbase);
                                int ai1 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
                                int ni1 = cc_add_const(c, GMB_CCONST_STRING, 0, dot + 1);
                                if (c->error) return;
                                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai1);
                                cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, ni1);
                                break;
                            }
                        }
                        cc_load_target(c, mbase);
                        cc_push_str_const(c, dot + 1);
                        if (c->error) return;
                        cc_byte(c, GMB_OP_LOAD_ELEMENT);
                        cc_byte(c, 1);
                        break;
                    }
                }
            }
        }
        /* campo implicito SELF.Campo dentro un corpo di metodo/costruttore (2.9): un
         * nome nudo che non è né local/parametro né globale — prima STATIC (un
         * GLOBALE dedicato, mai nel dict-istanza), poi PROPERTY (getter via
         * OP_CALL_MEMBER), infine campo normale su SELF — vedi cc_class_field_is. */
        if (e->var && cc_local(c, e->var) < 0 && cc_global(c, e->var, 0) < 0 && c->cur_class >= 0) {
            int sslot = cc_class_static_slot_of(&c->classes[c->cur_class], e->var);
            if (sslot >= 0) { cc_byte(c, GMB_OP_LOAD_GLOBAL); cc_u16(c, sslot); break; }
            if (cc_class_is_prop_of(&c->classes[c->cur_class], e->var)) {
                cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, 0);
                int ai2 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
                int ni2 = cc_add_const(c, GMB_CCONST_STRING, 0, e->var);
                if (c->error) return;
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai2);
                cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, ni2);
                break;
            }
        }
        if (e->var && cc_local(c, e->var) < 0 && cc_global(c, e->var, 0) < 0 && cc_class_field_is(c, e->var)) {
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, 0);   /* SELF è sempre il local 0 */
            cc_push_str_const(c, e->var);
            if (c->error) return;
            cc_byte(c, GMB_OP_LOAD_ELEMENT);
            cc_byte(c, 1);
            break;
        }
        /* Bug corretto (2026-08-31, audit lambda in builtin): un riferimento NUDO al nome
         * di una procedura usato come VALORE, non come chiamata (es. `gestoreSaluto =
         * Saluta`, "puntatore a funzione" assegnato a una variabile — FunctionsSubAsAction.
         * bas) cadeva nel fallback generico cc_load_target, che tratta il nome come una
         * variabile local/globale MAI dichiarata — restituiva quindi NIL/0, mai un
         * riferimento chiamabile. Invocare la variabile più avanti (CALL_INDIRECT) trovava
         * così un valore non riconoscibile e no-op'ava silenziosamente (nessun crash,
         * nessun output dal body della SUB referenziata). Fix: se il nome NON è una
         * variabile nota ma È una procedura nota, produce lo stesso OP_PUSH_CODE_REF già
         * usato per le callback NET_CMD (parità C# CompileProcedureToCodeRef) — generalizzato
         * a "prendi un riferimento chiamabile a QUESTA procedura" come espressione
         * qualunque, stesso schema già riusato per MakeByRefExpr/OP_MAKE_BYREF nel piano
         * BYREF-in-closure. */
        if (e->var && cc_local(c, e->var) < 0 && cc_global(c, e->var, 0) < 0 && cc_proc_known(c, e->var)) {
            int ci = cc_add_const_unique(c);
            if (c->error) return;
            cc_coderef_register(c, ci, e->var);
            cc_byte(c, GMB_OP_PUSH_CODE_REF); cc_u16(c, ci);
            break;
        }
        cc_load_target(c, e->var);
        break;
    }
    case GMB_EXPR_UNARY: {
        /* OPERATOR overload unario (2.9): stessa logica del ramo binario, un solo
         * argomento esplicito ("a", il parametro dichiarato nell'OPERATOR unario). */
        {
            const char* opname = cc_operator_method_name_unary(e->op);
            int cli = cc_operand_class_with_op(c, e->left, opname);
            if (cli >= 0) {
                cc_expr(c, e->left);    /* istanza (SELF) */
                cc_expr(c, e->left);    /* arg0 = "a" */
                if (c->error) return;
                int ai = cc_add_const(c, GMB_CCONST_DOUBLE, 1.0, NULL);
                int ni = cc_add_const(c, GMB_CCONST_STRING, 0, opname);
                if (c->error) return;
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai);
                cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, ni);
                break;
            }
        }
        cc_expr(c, e->left);
        if (c->error) return;
        if (e->op && strcasecmp(e->op, "-") == 0) cc_byte(c, GMB_OP_NEGATE);
        else if (e->op && strcmp(e->op, "!") == 0) cc_byte(c, GMB_OP_NOT);
        else if (e->op && strcasecmp(e->op, "NOT") == 0) {
            /* NOT = logico: (x == 0) ? -1/0 (CMP_EQ produce già -1/0). */
            int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z);
            cc_byte(c, GMB_OP_CMP_EQ);
        }
        break;
    }
    case GMB_EXPR_BINARY: {
        /* OPERATOR overload (2.9): l'operando sinistro è una variabile semplice il cui
         * tipo tracciato è una CLASS con un metodo OPERATOR sintetico registrato (es.
         * "OP_PLUS" per `+` — vedi cc_operator_method_name, stessa tabella nomi del
         * parser in gmb_p_parse_class) → dispatch via OP_CALL_MEMBER invece
         * dell'opcode aritmetico generico. Convenzione di chiamata: SELF (l'istanza,
         * = operando sinistro) E il valore dell'operando sinistro DI NUOVO come primo
         * argomento esplicito (il parametro dichiarato "a" nell'OPERATOR, un local a
         * sé stante — non un alias di SELF, quindi va scritto esplicitamente), poi
         * l'operando destro come secondo argomento (parametro "b"). */
        {
            const char* opname = cc_operator_method_name(e->op);
            int cli = cc_operand_class_with_op(c, e->left, opname);
            if (cli >= 0) {
                cc_expr(c, e->left);    /* istanza (SELF) */
                cc_expr(c, e->left);    /* arg0 = "a" (duplicato di SELF, un local a sé) */
                cc_expr(c, e->right);   /* arg1 = "b" */
                if (c->error) return;
                int ai = cc_add_const(c, GMB_CCONST_DOUBLE, 2.0, NULL);
                int ni = cc_add_const(c, GMB_CCONST_STRING, 0, opname);
                if (c->error) return;
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai);
                cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, ni);
                break;
            }
        }
        /* Operatori LOGICI (keyword): risultato normalizzato -1/0. Senza short-circuit
         * nel bytecode (valori equivalenti; C#/C99 fanno short-circuit). */
        if (e->op && (strcasecmp(e->op, "AND") == 0 || strcasecmp(e->op, "OR") == 0 || strcasecmp(e->op, "XOR") == 0)) {
            if (strcasecmp(e->op, "XOR") == 0) {
                /* XOR logico: (a!=0) ^ (b!=0) → -1/0 (no short-circuit). */
                cc_expr(c, e->left); if (c->error) return;
                { int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z); }
                cc_byte(c, GMB_OP_CMP_NE);
                cc_expr(c, e->right); if (c->error) return;
                { int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z); }
                cc_byte(c, GMB_OP_CMP_NE);
                cc_byte(c, GMB_OP_XOR);
                break;
            }
            /* AND/OR logici con SHORT-CIRCUIT e risultato normalizzato -1/0. */
            int is_and = (strcasecmp(e->op, "AND") == 0);
            cc_expr(c, e->left); if (c->error) return;
            cc_reljump(c, is_and ? GMB_OP_JUMP_IF_FALSE : GMB_OP_JUMP_IF_TRUE, -1);
            int short_jmp = c->nrel - 1;
            cc_expr(c, e->right); if (c->error) return;
            { int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z); }
            cc_byte(c, GMB_OP_CMP_NE);
            cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);
            int end_jmp = c->nrel - 1;
            if (c->reljumps[short_jmp].target < 0) c->reljumps[short_jmp].target = c->code_len;
            cc_byte(c, is_and ? GMB_OP_PUSH_FALSE : GMB_OP_PUSH_TRUE);
            if (c->reljumps[end_jmp].target < 0) c->reljumps[end_jmp].target = c->code_len;
            break;
        }
        cc_expr(c, e->left);
        cc_expr(c, e->right);
        if (c->error) return;
        int op = 0;
        if (e->op) {
            if (strcasecmp(e->op, "+") == 0) op = GMB_OP_ADD;
            else if (strcasecmp(e->op, "-") == 0) op = GMB_OP_SUBTRACT;
            else if (strcasecmp(e->op, "*") == 0) op = GMB_OP_MULTIPLY;
            else if (strcasecmp(e->op, "/") == 0) op = GMB_OP_DIVIDE;
            else if (strcasecmp(e->op, "\\") == 0) op = GMB_OP_INT_DIVIDE;
            else if (strcasecmp(e->op, "^") == 0) op = GMB_OP_POWER;
            else if (strcasecmp(e->op, "MOD") == 0) op = GMB_OP_MOD;
            else if (strcasecmp(e->op, "=") == 0 || strcasecmp(e->op, "==") == 0) op = GMB_OP_CMP_EQ;
            else if (strcasecmp(e->op, "<>") == 0) op = GMB_OP_CMP_NE;
            else if (strcasecmp(e->op, ">") == 0) op = GMB_OP_CMP_GT;
            else if (strcasecmp(e->op, ">=") == 0) op = GMB_OP_CMP_GTE;
            else if (strcasecmp(e->op, "<") == 0) op = GMB_OP_CMP_LT;
            else if (strcasecmp(e->op, "<=") == 0) op = GMB_OP_CMP_LTE;
            else if (strcmp(e->op, "&&") == 0) op = GMB_OP_BITAND;
            else if (strcmp(e->op, "||") == 0) op = GMB_OP_BITOR;
            else if (strcmp(e->op, "^^") == 0) op = GMB_OP_XOR;
            else if (strcasecmp(e->op, "IS") == 0) op = GMB_OP_CMP_EQ;   /* type-check → uguaglianza (approssimato) */
            else if (strcasecmp(e->op, "IN") == 0) op = GMB_OP_CMP_EQ;
            else if (strcasecmp(e->op, "HAS") == 0) op = GMB_OP_CMP_NE;
            else if (strcasecmp(e->op, "<>=") == 0) op = GMB_OP_CMP_NE;
        }
        if (!op) { cc_err(c, "operatore binario non supportato dal compilatore C"); return; }
        cc_byte(c, (uint8_t)op);
        break;
    }
    case GMB_EXPR_CALL: {
        if (cc_gfxctx_sugar(c, e)) return;
        if (e->var && (strcasecmp(e->var, "WINDOWCLOSED") == 0
                       || strcasecmp(e->var, "CTRLGETPROP") == 0
                       || strcasecmp(e->var, "CTRLGETPROPS") == 0)) {
            int sc = strcasecmp(e->var, "WINDOWCLOSED") == 0 ? 402
                   : strcasecmp(e->var, "CTRLGETPROP") == 0 ? 404 : 405;
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            cc_syscall(c, sc, e->nargs);
            break;
        }
        /* BASE.NEW(args): delega al costruttore della classe base (INHERITS) — deve
         * essere riconosciuto PRIMA del ramo generico "accesso a membro" più sotto
         * (BASE non è un local/global, cadrebbe nel fallback no-op). Valido solo dentro
         * un corpo di classe (c->cur_class >= 0) la cui classe ha una base con un
         * costruttore proprio già compilato — passata 4b compila le classi in ordine di
         * dichiarazione nel file, e la base DEVE essere dichiarata prima (verificato da
         * cc_class_register), quindi il suo indirizzo di costruttore è già noto/patchato
         * a questo punto. Se la base non ha un costruttore proprio, no-op sicuro: i campi
         * ereditati sono già stati zero-inizializzati da OP_NEW_CLASS_INSTANCE usando il
         * FieldNames/FieldTypes MERGED di questa classe, prima ancora che il costruttore
         * di questa classe iniziasse — nessuna doppia inizializzazione da recuperare. */
        if (e->var && strcasecmp(e->var, "BASE.NEW") == 0) {
            int bi = (c->cur_class >= 0) ? c->classes[c->cur_class].base_idx : -1;
            if (bi >= 0 && c->classes[bi].ctor_list_node && c->classes[bi].ctor_list_node->n > 0) {
                int baseCtorAddr = (int)c->classes[bi].ctor_list_node->entries[0].val_d;
                for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                if (c->error) return;
                cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, 0);   /* SELF */
                int ai = cc_add_const(c, GMB_CCONST_DOUBLE, (double)(e->nargs + 1), NULL);
                if (c->error) return;
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai);
                cc_byte(c, GMB_OP_CALL_CTOR); cc_u16(c, baseCtorAddr);
                break;
            }
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            for (int i = 0; i < e->nargs; i++) cc_byte(c, GMB_OP_POP);
            cc_byte(c, GMB_OP_PUSH_NIL);
            break;
        }
        if (e->var && strcmp(e->var, "_true_") == 0) { cc_byte(c, GMB_OP_PUSH_TRUE); break; }
        if (e->var && strcmp(e->var, "_false_") == 0) { cc_byte(c, GMB_OP_PUSH_FALSE); break; }
        if (e->var && strcmp(e->var, "_array_") == 0) {
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            cc_byte(c, GMB_OP_MK_ARRAY); cc_u16(c, e->nargs);
            break;
        }
        if (e->var && strcmp(e->var, "_prop_") == 0 && e->nargs == 2) {
            cc_expr(c, e->args[0]);
            if (c->error) return;
            const char* pkey = (e->args[1] && e->args[1]->str) ? e->args[1]->str : "";
            int pci = cc_add_const(c, GMB_CCONST_STRING, 0, pkey);
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, pci);
            cc_byte(c, GMB_OP_LOAD_ELEMENT); cc_byte(c, 1);
            break;
        }
        if (e->var && strcmp(e->var, "_dict_") == 0) {
            /* literal dizionario {"k":v, ...}: il parser produce CALL("_dict_", [k,v,...])
             * (specchio di "_array_") — senza questo ramo il literal cadeva nel fallback
             * "proc sconosciuta" e assegnava NIL: DICT vuote (Keys/Count/letture vuote). */
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            cc_byte(c, GMB_OP_MK_DICT); cc_u16(c, e->nargs / 2);
            break;
        }
        if (e->var && strcmp(e->var, "_macro_") == 0 && e->nargs == 1) {
            /* &espr come VALORE (Clipper macro/EVAL) — vedi gmb_p_parse_primary. Il
             * sorgente (tipicamente una stringa) va valutato normalmente, poi
             * OP_EVAL_MACRO_EXPR (vm_eval_macro, gmb_vm_core.c) pop-a quel valore, lo
             * compila/esegue come codice a runtime e ripusha il risultato — stesso
             * meccanismo già usato e funzionante su IL/PCode-managed. */
            cc_expr(c, e->args[0]);
            if (c->error) return;
            cc_byte(c, GMB_OP_EVAL_MACRO_EXPR);
            break;
        }
        if (e->var && strcmp(e->var, "_assign_") == 0 && e->nargs == 2) {
            /* pos := expr: [value, dup, store target] — lascia il valore */
            cc_expr(c, e->args[1]);
            if (c->error) return;
            cc_byte(c, GMB_OP_DUP);
            if (e->args[0]->var) cc_store_target(c, e->args[0]->var, e->args[0]->line);
            break;
        }
        if (e->var && strcmp(e->var, "_idx_") == 0 && e->nargs == 2) {
            /* FROMHEX(s)(0): base, indice → LOAD_ELEMENT */
            cc_expr(c, e->args[0]);
            cc_expr(c, e->args[1]);
            if (c->error) return;
            cc_byte(c, GMB_OP_LOAD_ELEMENT);
            cc_byte(c, 1);
            break;
        }
        /* QUERY["key"]/FORM("key"): keyword riservate delle pagine BSP, mai una DIM
         * normale — il bag è seminato per NOME dall'host PRIMA della run (gmb_vm_bsp.c:
         * gmb_vm_seed_global + match per nome in vm_apply_bsp_seeds contro
         * chunk->global_names), quindi NON possono passare dal check "variabile nota"
         * sotto (cc_global(...,0), sola lettura: sul PRIMO riferimento nel file QUERY/FORM
         * non sono ancora note, nessuna DIM le registra mai). Bug reale: prima di questo
         * caso, FORM("key") cadeva nel fallback finale "proc sconosciuta" (PUSH_NIL,
         * nessun errore — vedi sotto), e QUERY["key"] (dopo il fix del parser che prima
         * scartava del tutto il contenuto delle quadre) sarebbe caduta nello stesso posto.
         * cc_global(...,1) crea/trova lo slot per NOME (lo stesso nome che
         * vm_apply_bsp_seeds cerca), poi LOAD_ELEMENT applica gli indici — parità con
         * l'accesso "variabile nota" subito sotto, ma con auto-creazione dello slot. */
        if (e->var && (strcasecmp(e->var, "QUERY") == 0 || strcasecmp(e->var, "FORM") == 0)) {
            int g = cc_global(c, e->var, 1);
            cc_byte(c, GMB_OP_LOAD_GLOBAL); cc_u16(c, g);
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            cc_byte(c, GMB_OP_LOAD_ELEMENT);
            cc_byte(c, (uint8_t)(e->nargs > 0 ? e->nargs : 1));
            break;
        }
        /* accesso a elemento array: nome di una variabile nota → LOAD_ELEMENT.
         * ATTENZIONE: esclude i nomi di PROCEDURA nota (cc_proc_known) — bug reale scoperto
         * con la ricorsione: il fix di NativeCompiler-FunctionReturnValue-AlwaysNil riserva un
         * local con lo STESSO NOME della funzione (per `NomeFunzione = espr`), quindi DENTRO
         * il corpo di una funzione ricorsiva `cc_local(c, "Fact")` ora trova SEMPRE quel local
         * — una chiamata ricorsiva `Fact(N-1)` veniva quindi scambiata per un accesso a
         * indice sul local di ritorno (LOAD_ELEMENT su "Fact" con indice N-1) invece di una
         * vera chiamata: nessun errore visibile (il local parte a NIL/0, l'indicizzazione
         * silenziosa restituiva NIL), la funzione ricorsiva restituiva sempre 0 perché il
         * "passo ricorsivo" non chiamava mai realmente se stessa. Una procedura nota vince
         * sempre sull'interpretazione "variabile locale", indipendentemente da un local
         * omonimo. */
        if (e->var && cc_is_closure_var(c, e->var)) {
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            cc_load_target(c, e->var);
            if (c->error) return;
            cc_byte(c, GMB_OP_CALL_INDIRECT); cc_byte(c, (uint8_t)e->nargs);
            break;
        }
        if (e->var && !cc_proc_known(c, e->var) && (cc_local(c, e->var) >= 0 || cc_global(c, e->var, 0) >= 0)) {
            cc_load_target(c, e->var);
            /* Bug corretto (2026-08-31, audit PARAMARRAY/array-parametro): "arr()" — nome
             * NUDO con parentesi VUOTE, usato per riferirsi all'INTERO array/lista (es.
             * UBOUND(arr()), FOR EACH x IN arr()) — veniva trattato come un accesso a
             * elemento con numIndices FORZATO a 1 anche quando nargs è 0 (nessun indice
             * fornito): emetteva comunque un OP_LOAD_ELEMENT che si aspettava un indice
             * sullo stack, mai pushato, leggendo/consumando il valore SBAGLIATO (l'array
             * appena caricato, usato come se fosse il proprio indice) — risultato: UBOUND
             * restituiva sempre -1 e FOR EACH iterava zero elementi, pur essendo
             * l'indicizzazione ESPLICITA (arr(0), arr(1), ...) corretta. "arr()" con ZERO
             * indici deve lasciare l'array/lista così com'è sullo stack (nessun
             * LOAD_ELEMENT) — non "indice 0 implicito". */
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            if (e->nargs > 0) {
                cc_byte(c, GMB_OP_LOAD_ELEMENT);
                cc_byte(c, (uint8_t)e->nargs);   /* numIndices u8 */
            }
            break;
        }
        int bcode = e->var ? cc_builtin_code(e->var) : -1;
        if (bcode < 0 && e->var && e->ns && e->ns[0]) {
            /* gmb_p_strip_ns ha già tolto il namespace da e->var (regola d'oro:
             * il namespace non si dispatcha mai), ma g_cc_builtins registra alcuni
             * builtin (es. JSON.Stringify, FINANCE.PV) con il nome QUALIFICATO —
             * ricostruiscilo per il lookup quando la forma nuda non trova nulla. */
            char qualified[300];
            snprintf(qualified, sizeof(qualified), "%s.%s", e->ns, e->var);
            bcode = cc_builtin_code(qualified);
        }
        if (bcode >= 0
            && !(e->var && cc_user_accepts_arity(c, e->var, e->nargs)
                 && !cc_builtin_accepts_arity(e->ns, e->var, e->nargs))) {
            /* 2026-09-24 (overload vs builtin, parità C#): overload utente compatibile
             * e builtin incompatibile → salta il builtin (il percorso user sotto
             * seleziona per arità). Altrimenti legacy (builtin vince). */
            /* JSON.ToObject(<UDT>, json): parità col compilatore C# (JsonToObjectExpr).
             * Il tipo target NON è un argomento a runtime (il parser C lo produce comunque
             * come primo arg, un identificatore): va tolto. La conversione è il syscall
             * JSON_TO_OBJECT=272 (stringa JSON -> valore nativo: dict O lista), NON il builtin
             * 3408 (che riceve già un oggetto). Per un target UDT si allega poi il descrittore
             * (OP_ATTACH_UDT_DESC) all'istanza dict, altrimenti i campi (es. res.order) restano
             * irrisolti ("{...}"). */
            if (bcode == 3408 && e->nargs >= 2) {
                int ui = (e->args[0] && e->args[0]->kind == GMB_EXPR_VAR && e->args[0]->var)
                         ? cc_udt_find(c, e->args[0]->var) : -1;
                cc_expr(c, e->args[1]);
                if (c->error) return;
                cc_syscall(c, 272, 1);
                if (ui >= 0) {
                    int descIdx = cc_udt_desc_const_idx(c, &c->udts[ui]);
                    if (c->error) return;
                    cc_byte(c, GMB_OP_ATTACH_UDT_DESC);
                    cc_u16(c, (uint16_t)descIdx);
                }
                break;
            }
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            cc_byte(c, GMB_OP_CALL_BUILTIN);
            cc_u16(c, bcode);
            cc_byte(c, (uint8_t)e->nargs);
            break;
        }
        if (e->var && (strchr(e->var, '.') || strcmp(e->var, "_fn_") == 0 || strcmp(e->var, "_switch_") == 0
            || strcmp(e->var, "_dict_") == 0 || strcmp(e->var, "_array_") == 0)) {
            /* accesso a membro di oggetto/dict: base.Member(...) con base variabile nota
             * → LOAD_ELEMENT con chiave stringa, poi eventuali indici */
            const char* dot = e->var ? strrchr(e->var, '.') : NULL;
            if (dot && dot != e->var) {
                char mbase[256];
                size_t mlen = (size_t)(dot - e->var);
                if (mlen < sizeof(mbase)) {
                    memcpy(mbase, e->var, mlen); mbase[mlen] = 0;
                    if (cc_local(c, mbase) >= 0 || cc_global(c, mbase, 0) >= 0) {
                        /* Bug corretto (2026-09-02, seguito task AEVAL): "arr(i).Campo" e
                         * "obj.Campo(idx)"/"obj.Metodo(args)" producono la STESSA identica
                         * forma AST — CALL("base.Campo", [args]) — ma hanno ordine di
                         * valutazione OPPOSTO (indicizza-poi-campo contro campo-poi-
                         * indicizza) e il compilatore non può indovinare quale dei due
                         * intendeva l'autore solo dal nome. Disambigua nell'UNICO modo
                         * possibile senza type-inference vera: se "mbase" è tracciato
                         * (vt_names/vt_types, popolato da "DIM arr(n) AS Tipo") come array
                         * di un TYPE/CLASS che ha DAVVERO un campo chiamato "dot+1", allora
                         * È SICURAMENTE "arr(i).Campo" — nessun'altra lettura è plausibile
                         * per un campo REALE di un tipo REGISTRATO. Va prima di qualunque
                         * altro controllo sotto (kMemberMethods/metodi CLASS), altrimenti un
                         * campo che si chiama come un metodo builtin (es. "Add") sceglierebbe
                         * il ramo sbagliato. */
                        const char* elem_type = cc_var_udt_get(c, mbase);
                        int is_elem_field = 0;
                        int mrk = cc_arr_rank_get(c, mbase); /* -1 = record SCALARE tipizzato */
                        if (mrk != -1 && elem_type && e->nargs >= 1) {
                            int eui = cc_udt_find(c, elem_type);
                            if (eui >= 0)
                                for (int efi = 0; efi < c->udts[eui].n_fields; efi++)
                                    if (strcasecmp(c->udts[eui].field_names[efi], dot + 1) == 0) { is_elem_field = 1; break; }
                            if (!is_elem_field) {
                                int ecli = cc_class_find(c, elem_type);
                                if (ecli >= 0)
                                    for (int efi = 0; efi < c->classes[ecli].n_fields; efi++)
                                        if (strcasecmp(c->classes[ecli].field_names[efi], dot + 1) == 0) { is_elem_field = 1; break; }
                            }
                        }
                        if (is_elem_field) {
                            cc_load_target(c, mbase);
                            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                            if (c->error) return;
                            cc_byte(c, GMB_OP_LOAD_ELEMENT); cc_byte(c, (uint8_t)e->nargs);
                            cc_push_str_const(c, dot + 1);
                            if (c->error) return;
                            cc_byte(c, GMB_OP_LOAD_ELEMENT); cc_byte(c, 1);
                            break;
                        }
                        /* metodo builtin noto (dispatch CALL_MEMBER della VM) vs membro+indice
                           (es. result.Centroids(0)): il compilatore C non ha type-inference */
                        /* Bug reale: questa lista deve rispecchiare ESATTAMENTE i methodName
                         * che l'interprete (gmb_vm_core.c, OP_CALL_MEMBER) sa gestire — un nome
                         * assente qui (anche se implementato lato interprete) fa cadere la
                         * chiamata nel ramo "membro+indice" sotto, che tratta `t3.Join(",")`
                         * come un accesso a CAMPO "Join" (quasi certamente NIL su una lista)
                         * seguito da un'INDICIZZAZIONE di quel NIL con l'argomento — da cui
                         * l'errore "indexing a numeric scalar", scoperto con `t3.Join(",")`
                         * dopo un FOR EACH (Test 3, LoopsForEachComprehensive.bas): "Join" era
                         * assente qui pur mancando anche lato interprete (aggiunto in
                         * gmb_vm_core.c insieme a questa entry). Le voci Graph/BFS/DFS/... erano
                         * già implementate lato interprete ma MAI raggiungibili da qui — stesso
                         * bug, corretto contestualmente. */
                        int is_method = gmb_is_coll_member_tok(gmb_member_tok(dot + 1))
                                      || gmb_is_vm_member_method(dot + 1);
if (!is_method) {
                            /* metodo utente di una CLASS (2.9): mbase è stato DIM'ensionato
                             * (o assegnato da NEW ClassName(...)) con un tipo tracciato in
                             * vt_names/vt_types — se quel nome è una CLASS registrata col
                             * metodo richiesto in VTable, instrada su OP_CALL_MEMBER come le
                             * voci whitelisted sopra (stesso dispatch VM). */
                            const char* tn = cc_var_udt_get(c, mbase);
                            int cli = tn ? cc_class_find(c, tn) : -1;
                            if (cli >= 0) {
                                for (int mi = 0; mi < c->classes[cli].n_methods; mi++)
                                    if (strcasecmp(dot + 1, c->classes[cli].method_names[mi]) == 0) { is_method = 1; break; }
                            }
                        }
                        if (is_method) {
                            cc_load_target(c, mbase);
                            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                            int ai = cc_add_const(c, GMB_CCONST_DOUBLE, (double)e->nargs, NULL);
                            const char* mname = dot + 1;
                            /* F1e (regola d'oro): il chunk porta il NOME CANONICO derivato
                             * dal TokenKind (gmb_member_canon) — .count()/.Count()/.COUNT()
                             * emettono tutti "Count": una sola forma stabile su cui la VM
                             * strcmp dispatcha. I membri fuori tabella restano lexeme. */
                            gmb_tokenkind_t mmk = gmb_member_tok(mname);
                            const char* canon = gmb_member_canon(mmk);
                            if (canon) mname = canon;
                            const char* bt = cc_var_udt_get(c, mbase);
                            if (bt && cc_container_is_set(bt) && !strcasecmp(mname, "Add")) mname = "AddUnique";
                            int ni = cc_add_const(c, GMB_CCONST_STRING, 0, mname);
                            if (c->error) return;
                            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai);
                            cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, ni);
                            break;
                        }
                        cc_load_target(c, mbase);
                        cc_push_str_const(c, dot + 1);
                        if (c->error) return;
                        cc_byte(c, GMB_OP_LOAD_ELEMENT);
                        cc_byte(c, 1);
                        if (e->nargs > 0) {
                            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                            if (c->error) return;
                            cc_byte(c, GMB_OP_LOAD_ELEMENT);
                            cc_byte(c, (uint8_t)e->nargs);
                        }
                        break;
                    }
                }
            }
            /* metodo/oggetto/lambda/literal: no-op per compile-parity — consuma gli argomenti, ritorna NIL */
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            for (int i = 0; i < e->nargs; i++) cc_byte(c, GMB_OP_POP);
            cc_byte(c, GMB_OP_PUSH_NIL);
            break;
        }
        {
            /* NEW ClassName(args): il parser produce una GMB_EXPR_CALL identica a una
             * chiamata di funzione ordinaria (NEW Name(...) → gmb_expr_call(Name, args) —
             * vedi gmb_parser_expressions.c). Va riconosciuta PRIMA del fallback "proc
             * mancante" sotto, altrimenti ogni istanziazione di classe verrebbe
             * silenziosamente no-op'ata. La VM/C sa già zero-inizializzare i campi ed
             * eseguire il costruttore da FieldNames/FieldTypes/Constructors nel
             * descrittore (OP_NEW_CLASS_INSTANCE) — vedi cc_class_register. */
            int cli = cc_class_find(c, e->var);
            if (cli >= 0) {
                for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                if (c->error) return;
                int ai = cc_add_const(c, GMB_CCONST_DOUBLE, (double)e->nargs, NULL);
                if (c->error) return;
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai);
                cc_byte(c, GMB_OP_NEW_CLASS_INSTANCE); cc_u16(c, c->classes[cli].desc_const_idx);
                break;
            }
        }
        if (e->var && e->nargs == 0 && strchr(e->var, '.')) {
            /* Tuple nominata: t.campo → t(indice), usando i nomi registrati dal DIM. */
            const char* dot = strchr(e->var, '.');
            size_t bl = (size_t)(dot - e->var);
            if (bl < 127) {
                char base[128], field[128];
                memcpy(base, e->var, bl); base[bl] = 0;
                snprintf(field, sizeof(field), "%s", dot + 1);
                int ti = cc_tuple_field_index(base, field);
                if (ti >= 0) {
                    gmb_expr_t** ia = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                    ia[0] = gmb_expr_var(base, e->line);
                    ia[1] = gmb_expr_num((double)ti, e->line);
                    gmb_expr_t* ic = gmb_expr_call("_idx_", ia, 2, e->line);
                    cc_expr(c, ic);
                    gmb_expr_free(ic);
                    break;
                }
            }
        }
        if (e->var && strcmp(e->var, "_tuple_lit_") == 0) {
            /* Tuple literal (a, b, ...): il parser C la rappresenta come CALL sintetica.
             * Su VM/C il valore e' un array (PRINT lo rende [a, b] come C99/IL); i nomi
             * campo opzionali non sono ancora risolti qui. Senza questo ramo il simbolo
             * "_tuple_lit_" restava non definito (compile error). */
            for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
            if (c->error) return;
            cc_byte(c, GMB_OP_MK_ARRAY); cc_u16(c, e->nargs);
            break;
        }
        if (!cc_proc_known(c, e->var)) {
            /* NEW <Container> OF ... / NEW <Container><...> (DICT/SORTED/LIST/QUEUE/...):
             * il parser produce una CALL con var = nome del tipo; senza gestirla il literal
             * cadeva nel no-op NIL → dict/set/sorted legacy (= NEW ...) restavano vuoti. */
            int nck = e->var ? cc_container_kind(e->var) : 0;
            /* NEW <Container>: il parser produce CALL("COLLNEW:<Container>", ...) — il
             * tipo effettivo sta DOPO il prefisso (DICT/SORTED/LIST/QUEUE/...). */
            if (!nck && e->var && !strncmp(e->var, "COLLNEW:", 8)) nck = cc_container_kind(e->var + 8);
            /* F1 (bug preesistente dedicato, 2026-09-11): NEW LIST [OF <T>] — CALL
             * sintetica "LISTOF:<Elem>" del parser (gmb_parser_expressions.c:740). Il
             * nome sintetico non era risolvibile → "LISTOF:INTEGER non definita" a
             * compile sul toolchain C chunk (il C99Native lo gestiva, il chunk no).
             * Parità C# A1: OP_NEW typeCode 1 (LIST). */
            if (e->var && !strncmp(e->var, "LISTOF:", 7)) {
                for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                if (c->error) return;
                for (int i = 0; i < e->nargs; i++) cc_byte(c, GMB_OP_POP);
                cc_byte(c, GMB_OP_NEW); cc_u16(c, 1);
                break;
            }
            if (e->var && cc_container_is_graph(e->var + (strncmp(e->var, "COLLNEW:", 8) == 0 ? 8 : 0))) {
                for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                if (c->error) return;
                for (int i = 0; i < e->nargs; i++) cc_byte(c, GMB_OP_POP);
                int zc2 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, zc2);
                cc_byte(c, GMB_OP_NEW); cc_u16(c, 17);
                break;
            }
            if (nck == 1) {
                for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                if (c->error) return;
                for (int i = 0; i < e->nargs; i++) cc_byte(c, GMB_OP_POP);
                cc_byte(c, GMB_OP_MK_ARRAY); cc_u16(c, 0);
                break;
            }
            if (nck == 2) {
                for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                if (c->error) return;
                for (int i = 0; i < e->nargs; i++) cc_byte(c, GMB_OP_POP);
                cc_byte(c, GMB_OP_MK_DICT); cc_u16(c, 0);
                break;
            }
            /* SNIPPET a runtime: una procedura UTENTE non presente nel frammento (es.
             * EVAL("DoubleIt(21)") su C99, dove le FUNCTION sono C statiche) non è un
             * errore: si emette OP_CALL_EXTERNAL (nome + argc) e il motore che esegue il
             * frammento la risolve con il proprio callback (gmb_host_expr_call in C99). */
            if (c->snippet_mode) {
                for (int i = 0; i < e->nargs; i++) cc_expr(c, e->args[i]);
                if (c->error) return;
                int xci = cc_add_const(c, GMB_CCONST_STRING, 0, e->var);
                if (c->error) return;
                cc_byte(c, GMB_OP_CALL_EXTERNAL); cc_u16(c, xci); cc_byte(c, (uint8_t)e->nargs);
                break;
            }
            /* proc mancante / funzione ignota: MAI no-op NIL (antipattern storico compile-parity
             * che mascherava typo e builtin non supportati come NIL silenzioso — parity C#/C99Native:
             * errore esplicito a compile). Le classi e i container sono gestiti sopra. */
            if (!e->var || e->var[0] == 0) { cc_err(c, "simbolo non trovato: chiamata a funzione anonima"); break; }
            {
                char m[600];
                /* Registry canonico (P0): se il nome nudo è una funzione di libreria NOTA,
                 * suggerisci il namespace (qualifica/IMPORT) invece del generico. */
                const char* hint_ns = NULL;
                if (!strchr(e->var, '.'))
                    for (size_t ri = 0; ri < sizeof(gmb_reg_procs) / sizeof(gmb_reg_procs[0]); ri++)
                        if (strcasecmp(gmb_reg_procs[ri].name, e->var) == 0) { hint_ns = gmb_reg_procs[ri].ns; break; }
                if (hint_ns)
                    snprintf(m, sizeof(m), "simbolo non trovato: '%s' è una funzione di libreria (namespace %s); "
                             "usa la forma qualificata %s.%s(...) o aggiungi IMPORT %s (compilatore C)",
                             e->var, hint_ns, hint_ns, e->var, hint_ns);
                else
                    snprintf(m, sizeof(m), "simbolo non trovato: funzione o subroutine '%s' non definita "
                             "(compilatore C); se è di libreria usa la forma qualificata o IMPORT", e->var);
                cc_err(c, m);
            }
            break;
        }
        uint64_t bm = cc_byref_mask(c, e->var);
        /* Bug corretto (2026-08-31, audit PARAMARRAY/OPTIONAL): una GMB_OP_CALL normale non
         * porta un argc a runtime — il callee non ha modo di sapere quanti argomenti sono
         * stati REALMENTE passati (il suo prologo fa uno STORE_LOCAL posizionale fisso per
         * OGNI parametro dichiarato). Con meno argomenti forniti di quelli dichiarati
         * (parametri OPTIONAL omessi), quegli STORE_LOCAL leggevano/consumavano il residuo
         * sullo stack condiviso invece del default dichiarato (bug: FunctionsCall.bas,
         * "Ciao, Rossi" invece di "Ciao, Sig. Rossi"; FunctionsRecursionAndByRef.bas,
         * TestOpzionali con valori disallineati). Con un parametro PARAMARRAY, gli
         * argomenti eccedenti venivano scritti in un local scalare invece che raccolti in
         * un array (bug: FunctionsParams.bas, "indexing a numeric scalar is not allowed").
         * Fix: risolto qui, IN FASE DI COMPILAZIONE, al call-site (dove la firma completa
         * del callee — vedi cc_procdef_register — è nota): per ogni parametro OPTIONAL
         * omesso, pusha il valore di default dichiarato (compilato ORA, non a runtime);
         * per un PARAMARRAY, raccoglie gli argomenti eccedenti in un vero array
         * (GMB_OP_MK_ARRAY, già usato per "_array_") prima della CALL — così il prologo
         * della procedura (STORE_LOCAL posizionale fisso, invariato) riceve sempre
         * ESATTAMENTE tanti valori quanti parametri dichiara, senza bisogno di un argc a
         * runtime o di una nuova convenzione di chiamata. */
        const gmb_stmt_t* pd = cc_procdef_lookup_arity(c, e->var, e->nargs);
        /* FFI (DECLARE ... LIB): emette OP_CALL_FFI_T invece di una GMB_OP_CALL a una
         * procedura senza corpo (che a runtime tornerebbe NIL). */
        if (pd && pd->is_ffi) { cc_emit_ffi_call(c, e, pd); if (c->error) return; break; }
        int fixed_count = pd ? pd->n_params : e->nargs;
        int is_variadic_last = pd && pd->n_params > 0 && pd->param_variadic && pd->param_variadic[pd->n_params - 1];
        if (is_variadic_last) fixed_count = pd->n_params - 1;
        int provided_fixed = e->nargs < fixed_count ? e->nargs : fixed_count;
        for (int i = 0; i < provided_fixed; i++) cc_call_arg(c, bm, i, e->args[i]);
        for (int i = provided_fixed; i < fixed_count; i++) {
            if (pd && pd->param_defaults && pd->param_defaults[i]) cc_expr(c, pd->param_defaults[i]);
            else cc_byte(c, GMB_OP_PUSH_NIL);
        }
        if (is_variadic_last) {
            int variadic_argc = e->nargs > fixed_count ? e->nargs - fixed_count : 0;
            /* Convenzione (VB/VBA-style, usata anche dal sample dell'audit): un UNICO
             * argomento variadico che è GIÀ un riferimento nudo "arr()" a un array/lista
             * nota (nessun indice) va passato COSÌ COM'È al PARAMARRAY — non avvolto come
             * unico elemento di un nuovo array (darebbe un array-di-un-array, mai quello
             * che l'utente intende con "passa questo array esistente come argomenti").
             * Esempio: SommaTutto(numeri#()) — numeri#() è l'INTERO array, non un
             * argomento scalare da incapsulare. */
            int passthrough = 0;
            if (variadic_argc == 1) {
                gmb_expr_t* only = e->args[fixed_count];
                if (only && only->kind == GMB_EXPR_CALL && only->nargs == 0 && only->var
                    && !cc_proc_known(c, only->var)
                    && (cc_local(c, only->var) >= 0 || cc_global(c, only->var, 0) >= 0)) {
                    cc_expr(c, only);
                    if (c->error) return;
                    passthrough = 1;
                }
            }
            if (!passthrough) {
                for (int i = 0; i < variadic_argc; i++) cc_expr(c, e->args[fixed_count + i]);
                if (c->error) return;
                cc_byte(c, GMB_OP_MK_ARRAY); cc_u16(c, variadic_argc);
            }
        }
        if (c->error) return;
        if (cc_proc_is_gen(c, e->var)) {
            /* chiamata a GENERATOR PROCEDURE → crea oggetto generatore (task dedicato) */
            cc_byte(c, GMB_OP_NEW_GENERATOR_CALL);
            int gpos = c->code_len;
            cc_u16(c, 0);
            cc_patch_add(c, gpos, 1, e->var);
            cc_byte(c, (uint8_t)e->nargs);
        } else {
            cc_byte(c, GMB_OP_CALL);
            int pos = c->code_len;
            cc_u16(c, 0);
            cc_patch_add_argc(c, pos, 1, e->var, e->nargs);   /* procedura (arità del call site per l'overload) */
        }
        break;
    }
    default:
        cc_err(c, "espressione non supportata dal compilatore C");
        break;
    }
}

/* Compila il corpo anonimo fuori dal flusso normale e crea OP_CLOSURE. Le catture
 * vengono scoperte durante cc_load/store_target contro la tabella dei locali esterni;
 * OP_CLOSURE promuove quegli slot a box BYREF nella VM/C. */
static void cc_emit_lambda(gmb_cc_t* c, const gmb_expr_t* e)
{
    if (c->compiling_lambda) { cc_err(c, "lambda annidata non ancora supportata dal compilatore nativo"); return; }
    cc_byte(c, GMB_OP_JUMP_FORWARD);
    int skip_pos = c->code_len; cc_u16(c, 0);
    int address = c->code_len;

    cc_local_t* saved_locals = c->locals; int saved_nlocals = c->nlocals, saved_cap = c->nloc_cap;
    int saved_in_proc = c->in_proc;
    c->enclosing_locals = saved_locals; c->n_enclosing_locals = saved_nlocals;
    c->locals = NULL; c->nlocals = 0; c->nloc_cap = 0;
    c->compiling_lambda = 1;
    c->lambda_up_names = NULL; c->lambda_up_is_local = NULL; c->lambda_up_index = NULL;
    c->n_lambda_up = 0; c->lambda_up_cap = 0;
    c->in_proc = 1;

    for (int i = 0; i < e->n_lambda_params; i++) cc_local_add(c, e->lambda_params[i]);
    for (int i = e->n_lambda_params - 1; i >= 0; i--) {
        int slot = cc_local(c, e->lambda_params[i]);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, slot);
    }
    cc_byte(c, GMB_OP_CLEAR_RETURN);
    if (e->lambda_expr) {
        cc_expr(c, e->lambda_expr);
        if (!c->error) cc_byte(c, GMB_OP_RETURN);
    } else {
        for (int i = 0; i < e->n_lambda_body && !c->error; i++) cc_stmt(c, e->lambda_body[i]);
        if (!c->error) { cc_byte(c, GMB_OP_PUSH_NIL); cc_byte(c, GMB_OP_RETURN); }
    }

    int nup = c->n_lambda_up;
    uint8_t up_local[16], up_index[16];
    for (int i = 0; i < nup; i++) { up_local[i] = c->lambda_up_is_local[i]; up_index[i] = c->lambda_up_index[i]; }
    /* S_eval: snapshot nomi-locali della lambda (address = inizio corpo). */
    cc_snapshot_locals(c, address);
    for (int i = 0; i < c->nlocals; i++) free(c->locals[i].name);
    free(c->locals);
    for (int i = 0; i < c->n_lambda_up; i++) free(c->lambda_up_names[i]);
    free(c->lambda_up_names); free(c->lambda_up_is_local); free(c->lambda_up_index);
    c->locals = saved_locals; c->nlocals = saved_nlocals; c->nloc_cap = saved_cap;
    c->in_proc = saved_in_proc; c->compiling_lambda = 0;
    c->enclosing_locals = NULL; c->n_enclosing_locals = 0;
    c->lambda_up_names = NULL; c->lambda_up_is_local = NULL; c->lambda_up_index = NULL;
    c->n_lambda_up = c->lambda_up_cap = 0;
    if (c->error) return;

    int after = c->code_len;
    int off = after - (skip_pos + 2);
    c->code[skip_pos] = (uint8_t)((off >> 8) & 0xFF);
    c->code[skip_pos + 1] = (uint8_t)(off & 0xFF);
    int ci = cc_add_const(c, GMB_CCONST_DOUBLE, (double)address, NULL);
    if (c->error) return;
    cc_byte(c, GMB_OP_CLOSURE); cc_u16(c, ci); cc_byte(c, (uint8_t)nup);
    for (int i = 0; i < nup; i++) { cc_byte(c, up_local[i]); cc_byte(c, up_index[i]); }
}

/* Nomi delle FUNCTION/SUB annidate già emesse come closure (OP_CLOSURE + store in un
 * local/upvalue con il nome della funzione): la passata 4 NON deve compilarle di nuovo
 * come procs flat (doppio corpo). Lista dedicata (non closure_return_procs: quella marca
 * le procedure TOP-LEVEL che RESTITUISCONO una closure). */
static int cc_nested_closure_known(gmb_cc_t* c, const char* name)
{
    for (int i = 0; i < c->n_nested_closure; i++)
        if (c->nested_closure_names[i] && strcasecmp(c->nested_closure_names[i], name) == 0) return 1;
    return 0;
}
static void cc_nested_closure_add(gmb_cc_t* c, const char* name)
{
    if (!name || cc_nested_closure_known(c, name)) return;
    if (c->n_nested_closure >= c->nested_closure_cap) {
        c->nested_closure_cap = c->nested_closure_cap ? c->nested_closure_cap * 2 : 8;
        c->nested_closure_names = (char**)realloc(c->nested_closure_names, (size_t)c->nested_closure_cap * sizeof(char*));
    }
    c->nested_closure_names[c->n_nested_closure++] = strdup(name);
}

/* Emette una FUNCTION/SUB ANNIDATA come closure: corpo compilato inline (salto in avanti
 * sul flusso del corpo contenitore), upvalue catturate dal contesto esterno (parità C#
 * VisitLocalProcedureStatement), e la closure viene STORE in un local/upvalue con lo STESSO
 * nome della funzione. Poi un riferimento al nome (`RETURN Incrementa`, `LET f = Inner`) lo
 * trova come local e carica la closure; una chiamata `f(...)` su un local che la contiene
 * usa CALL_INDIRECT. La passata 4 NON compila più la funzione come proc flat (vedi
 * cc_nested_closure_known in gmb_compile) — il corpo vive qui, con la cattura per
 * riferimento delle variabili esterne (parametri BYREF inclusi). */
static void cc_emit_nested_function(gmb_cc_t* c, const gmb_stmt_t* s)
{
    if (c->compiling_lambda) { cc_err(c, "funzione annidata dentro una closure non ancora supportata dal compilatore nativo"); return; }
    cc_byte(c, GMB_OP_JUMP_FORWARD);
    int skip_pos = c->code_len; cc_u16(c, 0);
    int address = c->code_len;

    cc_local_t* saved_locals = c->locals; int saved_nlocals = c->nlocals, saved_cap = c->nloc_cap;
    int saved_in_proc = c->in_proc;
    const char* saved_proc_name = c->current_proc_name;
    c->enclosing_locals = saved_locals; c->n_enclosing_locals = saved_nlocals;
    c->locals = NULL; c->nlocals = 0; c->nloc_cap = 0;
    c->compiling_lambda = 1;
    c->lambda_up_names = NULL; c->lambda_up_is_local = NULL; c->lambda_up_index = NULL;
    c->n_lambda_up = 0; c->lambda_up_cap = 0;
    c->in_proc = 1;
    /* Convenzione `FUNCTION = espr` (retturn-slot col nome della funzione): dentro il corpo
       della closure il nome corrente è la funzione annidata, non il contenitore. */
    c->current_proc_name = s->func_name;
    cc_tuple_clear();

    int retSlot = s->func_name ? cc_local_add(c, s->func_name) : -1;
    for (int i = 0; i < s->n_params; i++) cc_local_add(c, s->func_params[i]);
    for (int i = s->n_params - 1; i >= 0; i--) {
        int slot = cc_local(c, s->func_params[i]);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, slot);
    }
    cc_byte(c, GMB_OP_CLEAR_RETURN);
    if (retSlot >= 0 && !s->is_sub && cc_ret_numeric(s->func_ret_type)) {
        int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
        cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, retSlot);
    }
    for (int i = 0; i < s->n_funcbody && !c->error; i++) cc_stmt(c, s->func_body[i]);
    if (!c->error) {
        if (retSlot >= 0) { cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, retSlot); }
        else cc_byte(c, GMB_OP_PUSH_NIL);
        cc_byte(c, GMB_OP_RETURN);
    }

    int nup = c->n_lambda_up;
    uint8_t up_local[16], up_index[16];
    for (int i = 0; i < nup && i < 16; i++) { up_local[i] = c->lambda_up_is_local[i]; up_index[i] = c->lambda_up_index[i]; }
    /* S_eval: snapshot nomi-locali della funzione annidata (address = inizio corpo). */
    cc_snapshot_locals(c, address);
    for (int i = 0; i < c->nlocals; i++) free(c->locals[i].name);
    free(c->locals);
    for (int i = 0; i < c->n_lambda_up; i++) free(c->lambda_up_names[i]);
    free(c->lambda_up_names); free(c->lambda_up_is_local); free(c->lambda_up_index);
    c->locals = saved_locals; c->nlocals = saved_nlocals; c->nloc_cap = saved_cap;
    c->in_proc = saved_in_proc; c->compiling_lambda = 0;
    c->enclosing_locals = NULL; c->n_enclosing_locals = 0;
    c->lambda_up_names = NULL; c->lambda_up_is_local = NULL; c->lambda_up_index = NULL;
    c->n_lambda_up = c->lambda_up_cap = 0;
    c->current_proc_name = saved_proc_name;
    if (c->error) return;

    int after = c->code_len;
    int off = after - (skip_pos + 2);
    c->code[skip_pos] = (uint8_t)((off >> 8) & 0xFF);
    c->code[skip_pos + 1] = (uint8_t)(off & 0xFF);
    int ci = cc_add_const(c, GMB_CCONST_DOUBLE, (double)address, NULL);
    if (c->error) return;
    cc_byte(c, GMB_OP_CLOSURE); cc_u16(c, ci); cc_byte(c, (uint8_t)nup);
    for (int i = 0; i < nup && i < 16; i++) { cc_byte(c, up_local[i]); cc_byte(c, up_index[i]); }

    /* store della closure in un local/upvalue con lo stesso nome della funzione: un
       riferimento successivo al nome la carica. Il local appartiene allo scope ESTERNO
       (c->locals è stato ripristinato sopra), così `RETURN Incrementa` la trova. */
    if (s->func_name) {
        int l = cc_local(c, s->func_name);
        if (l < 0 && c->in_proc) l = cc_local_add(c, s->func_name);
        if (l >= 0) { cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, l); }
        else {
            int u = cc_lambda_upvalue(c, s->func_name, 1);
            if (u >= 0) { cc_byte(c, GMB_OP_STORE_UPVALUE); cc_byte(c, (uint8_t)u); }
            else { int g = cc_global(c, s->func_name, 1); cc_byte(c, GMB_OP_STORE_GLOBAL); cc_u16(c, g); }
        }
        /* registra nome → indirizzo del corpo: una chiamata ricorsiva/diretta per nome
           (GMB_EXPR_CALL → cc_proc_known) si patch a questo indirizzo (parità C#:
           _procedureAddresses[procName] = functionAddress). La passata 4 NON lo
           ricompila (cc_nested_closure_known) quindi l'entry va aggiunta qui. */
        gmb_compiled_chunk_t* ch = c->chunk;
        uint32_t p;
        for (p = 0; p < ch->proc_count; p++)
            if (strcasecmp(ch->proc_names[p], s->func_name) == 0) { ch->proc_addrs[p] = (uint32_t)address; break; }
        if (p == ch->proc_count) {
            ch->proc_names = (char**)realloc(ch->proc_names, (size_t)(ch->proc_count + 1) * sizeof(char*));
            ch->proc_addrs = (uint32_t*)realloc(ch->proc_addrs, (size_t)(ch->proc_count + 1) * sizeof(uint32_t));
            ch->arity_addrs = (uint32_t*)realloc(ch->arity_addrs, (size_t)(ch->arity_count + 1) * sizeof(uint32_t));
            ch->arity_vals = (uint32_t*)realloc(ch->arity_vals, (size_t)(ch->arity_count + 1) * sizeof(uint32_t));
            ch->proc_names[ch->proc_count] = strdup(s->func_name);
            ch->proc_addrs[ch->proc_count] = (uint32_t)address;
            ch->arity_addrs[ch->arity_count] = (uint32_t)address;
            ch->arity_vals[ch->arity_count] = (uint32_t)s->n_params;
            ch->arity_count++;
            ch->proc_count++;
        }
        cc_nested_closure_add(c, s->func_name);
        /* La funzione annidata è ora un local/upvalue che CONTIENE una closure: una sua
           chiamata per nome (es. `EseguiCalcolo` che chiama `Moltiplica(x)`, o la ricorsione
           `FattorialeLocale(N-1)`) deve usare CALL_INDIRECT sul local/upvalue — che carica
           il box BYREF e imposta le upvalue sul frame del chiamato. Un direct CALL al corpo
           NON imposta le upvalue (frame->upvalues vuote) → la closure leggerebbe NIL
           (es. "Risultato interno ->" vuoto in FunctionsLocalAndSubs). */
        cc_closure_var_add(c, s->func_name);
    }
}

static void cc_reljump(gmb_cc_t* c, int cond_op, int target)
{
    /* emette cond_op + u16 placeholder relativo; record per patching */
    cc_byte(c, (uint8_t)cond_op);
    int pos = c->code_len;
    cc_u16(c, 0);
    if (c->nrel >= c->nrel_cap) { c->nrel_cap = c->nrel_cap ? c->nrel_cap * 2 : 16; c->reljumps = (cc_reljump_t*)realloc(c->reljumps, (size_t)c->nrel_cap * sizeof(*c->reljumps)); }
    c->reljumps[c->nrel].pos = pos;
    c->reljumps[c->nrel].target = target;
    c->reljumps[c->nrel].backward = 0;
    c->reljumps[c->nrel].cond = cond_op;
    c->nrel++;
}

static void cc_reljump_back(gmb_cc_t* c, int from_pos)
{
    cc_byte(c, GMB_OP_JUMP_BACKWARD);
    int pos = c->code_len;
    cc_u16(c, 0);
    if (c->nrel >= c->nrel_cap) { c->nrel_cap = c->nrel_cap ? c->nrel_cap * 2 : 16; c->reljumps = (cc_reljump_t*)realloc(c->reljumps, (size_t)c->nrel_cap * sizeof(*c->reljumps)); }
    c->reljumps[c->nrel].pos = pos;
    c->reljumps[c->nrel].target = from_pos;
    c->reljumps[c->nrel].backward = 1;
    c->reljumps[c->nrel].cond = 0;
    c->nrel++;
}

/* JSON/console file I/O per la VM/C: JSON OPEN / PRINT #h / INPUT #h / CLOSE #h
 * (implementazione mondo C pura — OP_JFILE_* in gmb_vm_core.c). I campi AST vengono
 * dai parser dedicati (gmb_parser_stmt_json.c / gmb_parser_stmt_files.c). */
static void cc_json_io_stmt(gmb_cc_t* c, const gmb_stmt_t* s)
{
    switch (s->kind)
    {
        case GMB_STMT_JSON_SAVE:
            /* JSON SAVE obj, path → serializza obj su path */
            cc_expr(c, s->channel);      /* oggetto */
            if (c->error) return;
            cc_expr(c, s->send_value);   /* path */
            if (c->error) return;
            cc_byte(c, GMB_OP_JFILE_SAVE);
            break;
        case GMB_STMT_JSON_WRITEVALUE:
            /* JSON WRITEVALUE key, value [, section] → documento impostazioni */
            cc_expr(c, s->go_args[0]);                 /* key */
            if (c->error) return;
            cc_expr(c, s->go_args[1]);                 /* value */
            if (c->error) return;
            if (s->channel) { cc_expr(c, s->channel); if (c->error) return; }
            else { int zi = cc_add_const(c, GMB_CCONST_STRING, 0, ""); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, zi); }
            cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3400); cc_byte(c, 3);
            cc_byte(c, GMB_OP_POP);
            break;
        case GMB_STMT_JSON_WRITESECTION:
            /* JSON WRITESECTION obj, "nomeSezione" → documento impostazioni */
            cc_expr(c, s->send_value);                 /* nome sezione */
            if (c->error) return;
            cc_expr(c, s->channel);                    /* sorgente UDT/obj */
            if (c->error) return;
            cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3401); cc_byte(c, 2);
            cc_byte(c, GMB_OP_POP);
            break;
        case GMB_STMT_JSON_OPEN:
            /* JSON OPEN file FOR mode AS #h → apre, pusha handle, store su h */
            cc_expr(c, s->channel);          /* file */
            if (c->error) return;
            cc_expr(c, s->send_value);       /* mode ("WRITE"/"READ"/"APPEND") */
            if (c->error) return;
            cc_byte(c, GMB_OP_JFILE_OPEN);
            if (s->win_name) cc_store_target(c, s->win_name, s->line);
            break;
        case GMB_STMT_PRINTFILE:
            /* PRINT #h, expr... → serializza ogni expr sul file */
            if (s->file_num) {
                for (int i = 0; i < s->nprint; i++) {
                    cc_expr(c, s->print_args[i]);
                    if (c->error) return;
                    cc_expr(c, s->file_num);
                    if (c->error) return;
                    cc_byte(c, GMB_OP_JFILE_WRITE);
                }
            }
            break;
        case GMB_STMT_INPUTFILE:
            /* INPUT #h, var... → legge il file, lo parsa e lo assegna a ogni var */
            for (int i = 0; i < s->n_read; i++) {
                if (!s->read_vars[i]) continue;
                if (s->file_num) { cc_expr(c, s->file_num); if (c->error) return; }
                cc_byte(c, GMB_OP_JFILE_READ);
                cc_store_target(c, s->read_vars[i], s->line);
                if (c->error) return;
            }
            break;
        case GMB_STMT_CLOSE:
            if (s->file_num) { cc_expr(c, s->file_num); if (c->error) return; }
            else {
                int zi = cc_add_const(c, GMB_CCONST_DOUBLE, -1.0, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, zi);
            }
            cc_byte(c, GMB_OP_JFILE_CLOSE);
            break;
        default:
            break;
    }
}

static void cc_stmt(gmb_cc_t* c, const gmb_stmt_t* s);
/* Controllo UI non-bloccante via syscall 400 (vm_pixel_ctrl): stack
 * [kind, name, spec, lista-argomenti] + MK_LIST. Usato da PANEL/BUTTON STYLE. */
static void cc_ui_control(gmb_cc_t* c, const gmb_stmt_t* s, const char* kind, int nargs, int style_idx)
{
    cc_push_str_const(c, kind);
    cc_push_str_const(c, "");
    const gmb_expr_t* st = (style_idx >= 0 && style_idx < s->n_control_args) ? s->control_args[style_idx] : NULL;
    if (st) cc_expr(c, st); else cc_push_str_const(c, "");
    for (int i = 0; i < nargs; i++) {
        const gmb_expr_t* e = (1 + i < s->n_control_args) ? s->control_args[1 + i] : NULL;
        if (e) cc_expr(c, e); else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
    }
    cc_byte(c, GMB_OP_MK_LIST); cc_u16(c, (unsigned)nargs);
    cc_syscall(c, 400, 4);
}

static void cc_stmt(gmb_cc_t* c, const gmb_stmt_t* s)
{
    if (!s) return;
    switch (s->kind) {
        case GMB_STMT_CONST:
            return;   /* solo dichiarativa (registrata in prepass) */
        case GMB_STMT_JSON_SAVE:
        case GMB_STMT_JSON_WRITEVALUE:
        case GMB_STMT_JSON_WRITESECTION:
        case GMB_STMT_JSON_OPEN:
        case GMB_STMT_PRINTFILE:
        case GMB_STMT_INPUTFILE:
        case GMB_STMT_CLOSE:
            cc_json_io_stmt(c, s);
            return;
        default:
            break;
    }
    c->current_line = s->line;
    /* Bug corretto (2026-08-31, audit lambda/closure): una riga numerata classica
     * (`100 PRINT ...`) non veniva MAI registrata come indirizzo raggiungibile per
     * nome — GOTO/GOSUB verso un numero di riga erano quindi no-op espliciti "per
     * compile-parity" (vedi i due case sotto), MA senza alcun errore visibile: il
     * salto semplicemente non avveniva, l'esecuzione proseguiva alla prossima
     * istruzione come se il GOTO/GOSUB non fosse mai stato scritto (bug:
     * FunctionsMain.bas, `GOSUB 100` non eseguiva mai il corpo della routine a riga
     * 100, "tornava" immediatamente). Registra QUI, per OGNI statement con un numero
     * di riga in testa (campo bas_line, popolato dal parser — vedi il fix gemello
     * sull'entry-point SUB Main più in basso in questo stesso file), l'indirizzo
     * corrente sotto il nome stringa del numero — stesso identico meccanismo
     * (cc_label) già usato per le label testuali, quindi GOTO/GOSUB verso un numero
     * di riga possono riusarlo SENZA bisogno di un opcode o un formato dedicato. */
    if (s->bas_line > 0) {
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%d", s->bas_line);
        cc_label(c, lbl);
    }
    switch (s->kind) {
    case GMB_STMT_PRINT: {
        if (s->nprint == 0) { cc_byte(c, GMB_OP_PRINT_NEWLINE); break; }
        for (int i = 0; i < s->nprint; i++) {
            cc_expr(c, s->print_args[i]);
            if (c->error) return;
            if (i == s->nprint - 1) cc_byte(c, GMB_OP_PRINT_LINE);
            else cc_byte(c, GMB_OP_PRINT_VALUE);
        }
        break;
    }
    case GMB_STMT_ASSIGN:
    case GMB_STMT_LET: {
        if (s->array_index) {
            /* Bug corretto (2026-09-02, seguito task AEVAL): "arr(i).Campo = valore" arriva
             * qui con s->target GIÀ concatenato in "arr.Campo" dal parser (vedi
             * gmb_parser_stmt_assign.c) E s->array_index popolato con "i" — ma sotto,
             * cc_load_target(c, s->target) fa un lookup NUDO del nome "arr.Campo" (nessuno
             * split sul punto), che non esiste come variabile: risolveva silenziosamente a
             * un GLOBALE NUOVO, scollegato dall'array vero "arr", e ci scriveva l'elemento
             * i-esimo — mai un errore, il campo restava semplicemente sempre vuoto/0 quando
             * riletto da "arr(i).Campo". Va distinto PRIMA di tutto dal caso "arr(i) = x"
             * (nessun punto nel target): "arr(i).Campo = x" è LOAD arr, indicizza con
             * OP_LOAD_ELEMENT_REF (non OP_LOAD_ELEMENT semplice — quello converte il
             * dict/lista in un valore EFFIMERO via gmb_adapter_val_to_stack, un
             * STORE_FIELD successivo scriverebbe su una copia mai vista da nessuno; _REF
             * usa gmb_adapter_val_to_stack_ref, che mantiene lo STESSO oggetto condiviso
             * dalla lista — esattamente il motivo per cui esiste, vedi il commento su
             * OP_LOAD_ELEMENT_REF in gmb_vm_core.c, già usato dal compilatore C# per lo
             * stesso identico caso ma MAI emesso dal compilatore nativo prima d'ora), poi
             * STORE_FIELD sul record — stesso schema già corretto per "obj.Campo = x"
             * (senza indice) poco sotto in questo stesso case. */
            const char* dot = s->target ? strrchr(s->target, '.') : NULL;
            if (dot && dot != s->target) {
                char base[256];
                size_t blen = (size_t)(dot - s->target);
                if (blen < sizeof(base)) {
                    memcpy(base, s->target, blen); base[blen] = 0;
                    /* base = record SCALARE tipizzato (rank -1): "y.Member(i) = v" assegna
                     * l'ELEMENTO del membro, NON l'index-poi-campo (solo array di record). */
                    if ((cc_local(c, base) >= 0 || cc_global(c, base, 0) >= 0)
                        && cc_arr_rank_get(c, base) == -1) {
                        cc_expr(c, s->value);
                        if (c->error) return;
                        cc_load_target(c, base);
                        int mfi = cc_add_const(c, GMB_CCONST_STRING, 0, dot + 1);
                        if (c->error) return;
                        cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, mfi);
                        cc_byte(c, GMB_OP_LOAD_ELEMENT_REF); cc_byte(c, 1);
                        cc_expr(c, s->array_index);
                        for (int ei = 0; ei < s->n_array_idx_extra && ei < 7 && !c->error; ei++)
                            cc_expr(c, s->array_idx_extra[ei]);
                        if (c->error) return;
                        cc_byte(c, GMB_OP_STORE_ELEMENT);
                        cc_byte(c, (uint8_t)(1 + s->n_array_idx_extra));
                        break;
                    }
                    cc_expr(c, s->value);
                    cc_load_target(c, base);
                    cc_expr(c, s->array_index);
                    if (c->error) return;
                    cc_byte(c, GMB_OP_LOAD_ELEMENT_REF);
                    cc_byte(c, 1);   /* numIndices u8 */
                    int fi = cc_add_const(c, GMB_CCONST_STRING, 0, dot + 1);
                    if (c->error) return;
                    cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi);
                    break;
                }
            }
            /* v(i) = x / v(i, j) = x: STORE_ELEMENT popa indici, poi array, poi valore.
             * Multi-dim (n_array_idx_extra): pusha TUTTI gli indici (esterno → interno),
             * parità con la lettura OP_LOAD_ELEMENT che usa e->nargs. */
            cc_expr(c, s->value);
            cc_load_target(c, s->target);
            cc_expr(c, s->array_index);
            for (int ei = 0; ei < s->n_array_idx_extra && ei < 7 && !c->error; ei++)
                cc_expr(c, s->array_idx_extra[ei]);
            if (c->error) return;
            cc_byte(c, GMB_OP_STORE_ELEMENT);
            cc_byte(c, (uint8_t)(1 + s->n_array_idx_extra));   /* numIndices u8 */
            break;
        }
        /* UDT (TYPE...END TYPE, campi nudi) è un VALUE TYPE: `p2 = p1` deve clonare i
         * campi, non condividere il dict (altrimenti p2.Campo = x muterebbe anche p1,
         * semantica da riferimento sbagliata per un tipo dichiarato per valore). Si
         * applica solo al caso semplice var-a-var (nessun indice/membro sul target o
         * sul value) — copia da espressioni più complesse resta la semantica generica. */
        if (s->target && !strchr(s->target, '.') && s->value && s->value->kind == GMB_EXPR_VAR
            && s->value->var && !strchr(s->value->var, '.') && s->value->nargs == 0) {
            const char* udt_name = cc_var_udt_get(c, s->value->var);
            int ui = udt_name ? cc_udt_find(c, udt_name) : -1;
            if (ui >= 0) {
                cc_byte(c, GMB_OP_MK_DICT); cc_u16(c, 0);
                cc_store_target(c, s->target, s->line);
                int src_slot = cc_local(c, s->value->var);
                if (src_slot < 0) {
                    /* sorgente globale: copiala in un local temporaneo per riusare
                       cc_emit_udt_init (che legge sempre da un LOAD_LOCAL). */
                    src_slot = cc_local_add(c, "__udt_src");
                    cc_load_target(c, s->value->var);
                    cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, src_slot);
                }
                cc_emit_udt_init(c, &c->udts[ui], s->target, src_slot);
                if (c->error) return;
                cc_var_udt_set(c, s->target, udt_name);
                break;
            }
        }
        /* x = NEW ClassName(...) o x = FuncCheRestituisceUnaClasse(...): CLASS è per
         * riferimento, nessuna clonazione — solo tracciamento del tipo (vt_names/
         * vt_types) per il dispatch di x.Metodo(...). */
        if (s->target && !strchr(s->target, '.') && s->value && s->value->kind == GMB_EXPR_CALL && s->value->var) {
            int cli = cc_class_find(c, s->value->var);
            const char* rcn = cli < 0 ? cc_proc_ret_class(c, s->value->var) : NULL;
            if (cli < 0 && rcn) cli = cc_class_find(c, rcn);
            if (cli >= 0) {
                /* BUG-7: riassegnare un GLOBAL di classe esistente finalizza la vecchia
                 * istanza (DESTRUCTOR) PRIMA di valutare il NEW (stack pulito). No-op se il
                 * vecchio non è classe con DESTRUCTOR. Guard: solo target globale semplice già
                 * esistente (mai DIM né locale). */
                if (s->target && !s->array_index && cc_local(c, s->target) < 0
                    && cc_global(c, s->target, 0) >= 0) {
                    cc_byte(c, GMB_OP_FINALIZE_GLOBAL); cc_u16(c, (uint16_t)cc_global(c, s->target, 1));
                }
                cc_expr(c, s->value);
                if (c->error) return;
                cc_store_target(c, s->target, s->line);
                cc_var_udt_set(c, s->target, c->classes[cli].name);
                break;
            }
        }
        /* x = y, dove y è già tracciata come istanza di una CLASS (es. `poly = sp` dopo
         * `DIM sp AS Derivata = NEW Derivata(...)`): propaga lo stesso tracciamento a x
         * — necessario per il polimorfismo (`DIM poly AS Base: poly = sp: poly.M()` deve
         * dispatchare via VTable la versione REALE dell'istanza) altrimenti x.Metodo()
         * cadrebbe nel vecchio fallback generico dopo la riassegnazione. */
        if (s->target && !strchr(s->target, '.') && s->value && s->value->kind == GMB_EXPR_VAR
            && s->value->var && !strchr(s->value->var, '.') && s->value->nargs == 0) {
            const char* tn = cc_var_udt_get(c, s->value->var);
            int cli = tn ? cc_class_find(c, tn) : -1;
            if (cli >= 0) {
                cc_expr(c, s->value);
                if (c->error) return;
                cc_store_target(c, s->target, s->line);
                cc_var_udt_set(c, s->target, c->classes[cli].name);
                break;
            }
        }
        /* campo STATIC = valore (2.9): un GLOBALE dedicato, mai il dict-istanza — copre
         * ClassName.Campo (mbase è il nome della classe, non una variabile),
         * obj.Campo (mbase è una variabile il cui tipo tracciato ha quel campo STATIC)
         * e l'implicito Campo = valore dentro un corpo di classe. Va PRIMA di
         * cc_expr(value)/store generico: la strategia di store dipende da quale ramo
         * matcha, ma il valore va comunque calcolato una volta sola. */
        if (s->target) {
            const char* sdot = strrchr(s->target, '.');
            int sslot = -1;
            if (sdot && sdot != s->target) {
                char smbase[256];
                size_t smlen = (size_t)(sdot - s->target);
                if (smlen < sizeof(smbase)) {
                    memcpy(smbase, s->target, smlen); smbase[smlen] = 0;
                    int scli = cc_class_find(c, smbase);
                    if (scli >= 0) sslot = cc_class_static_slot_of(&c->classes[scli], sdot + 1);
                    if (sslot < 0 && (cc_local(c, smbase) >= 0 || cc_global(c, smbase, 0) >= 0)) {
                        const char* stn = cc_var_udt_get(c, smbase);
                        int scli2 = stn ? cc_class_find(c, stn) : -1;
                        if (scli2 >= 0) sslot = cc_class_static_slot_of(&c->classes[scli2], sdot + 1);
                    }
                }
            } else if (c->cur_class >= 0) {
                sslot = cc_class_static_slot_of(&c->classes[c->cur_class], s->target);
            }
            if (sslot >= 0) {
                cc_expr(c, s->value);
                if (c->error) return;
                cc_byte(c, GMB_OP_STORE_GLOBAL); cc_u16(c, sslot);
                break;
            }
        }
        cc_expr(c, s->value);
        if (c->error) return;
        if (s->value && s->value->kind == GMB_EXPR_LAMBDA && s->target && !strchr(s->target, '.'))
            cc_closure_var_add(c, s->target);
        else if (s->value && s->value->kind == GMB_EXPR_CALL && s->value->var
            && cc_proc_returns_closure(c, s->value->var) && s->target && !strchr(s->target, '.'))
            cc_closure_var_add(c, s->target);
        /* Bug corretto (2026-08-31): stesso schema del ramo LAMBDA sopra, per un
         * riferimento NUDO a una procedura assegnato a una variabile (`gestoreSaluto =
         * Saluta`) — senza questa registrazione, invocare "gestoreSaluto(...)" più avanti
         * non veniva riconosciuto come chiamata indiretta (cc_is_closure_var false) e
         * cadeva nel fallback generico "proc sconosciuta". */
        else if (s->value && s->value->kind == GMB_EXPR_VAR && s->value->var
            && cc_local(c, s->value->var) < 0 && cc_global(c, s->value->var, 0) < 0
            && cc_proc_known(c, s->value->var) && s->target && !strchr(s->target, '.'))
            cc_closure_var_add(c, s->target);
        if (s->target) {
            const char* dot = strrchr(s->target, '.');
            if (dot && dot != s->target) {
                char mbase[256];
                size_t mlen = (size_t)(dot - s->target);
                if (mlen < sizeof(mbase)) {
                    memcpy(mbase, s->target, mlen); mbase[mlen] = 0;
                    if (cc_local(c, mbase) >= 0 || cc_global(c, mbase, 0) >= 0) {
                        /* obj.field = value → [value][obj] STORE_FIELD (parità OP_STORE_FIELD) */
                        cc_load_target(c, mbase);
                        int fi = cc_add_const(c, GMB_CCONST_STRING, 0, dot + 1);
                        if (c->error) return;
                        cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi);
                        break;
                    }
                }
            }
        }
        /* Accesso a campo NIDIFICATO `a.b.c = v` (>=2 punti): il valore è già sullo stack;
         * catena LOAD a; poi LOAD_ELEMENT_REF per i segmenti intermedi; STORE_FIELD sull'ultimo. */
        if (s->target && s->array_index == NULL && s->n_array_idx_extra == 0) {
            const char* fd = strchr(s->target, '.');
            const char* ld = strrchr(s->target, '.');
            if (fd && fd != ld) {
                char hd[256]; size_t hl = (size_t)(fd - s->target);
                if (hl < sizeof hd) {
                    memcpy(hd, s->target, hl); hd[hl] = 0;
                    if ((cc_local(c, hd) >= 0 || cc_global(c, hd, 0) >= 0)
                        && cc_local(c, s->target) < 0 && cc_global(c, s->target, 0) < 0) {
                        cc_load_target(c, hd);
                        const char* seg = fd + 1;
                        while (seg && *seg) {
                            const char* nd = strchr(seg, '.');
                            size_t sl = nd ? (size_t)(nd - seg) : strlen(seg);
                            char s2[128]; if (sl >= sizeof s2) sl = sizeof s2 - 1;
                            memcpy(s2, seg, sl); s2[sl] = 0;
                            if (!nd) {
                                int fi = cc_add_const(c, GMB_CCONST_STRING, 0, s2);
                                if (c->error) return;
                                cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi);
                                break;
                            }
                            cc_push_str_const(c, s2);
                            if (c->error) return;
                            cc_byte(c, GMB_OP_LOAD_ELEMENT_REF); cc_byte(c, 1);
                            seg = nd + 1;
                        }
                        break;
                    }
                }
            }
        }
        /* campo implicito SELF.Campo = valore (2.9): stessa risoluzione del ramo lettura
         * in GMB_EXPR_VAR — vedi cc_class_field_is. Il valore è già sullo stack
         * (cc_expr sopra), manca solo l'obj (SELF) prima di STORE_FIELD. */
        if (s->target && cc_local(c, s->target) < 0 && cc_global(c, s->target, 0) < 0 && cc_class_field_is(c, s->target)) {
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, 0);   /* SELF è sempre il local 0 */
            int fi = cc_add_const(c, GMB_CCONST_STRING, 0, s->target);
            if (c->error) return;
            cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi);
            break;
        }
        cc_store_target(c, s->target, s->line);
        break;
    }
    case GMB_STMT_DIM: {
        if (s->dim_is_dynamic && s->dim_var) {
            cc_push_str_const(c, s->dim_var);
            if (s->dim_init) cc_expr(c, s->dim_init); else cc_push_str_const(c, "");
            cc_syscall(c, 472, 2); break;
        }
        /* tipo GPM (MUTEX/COND/ONCE/CHANNEL/WAITGROUP/RWMUTEX/SYNCMAP/POOL/ATOMIC) -> NEW + store;
         * altrimenti variabile globale/locale → init 0.0 */
        int gpm_op = 0;
        int chan_capacity = 0;
        for (int i = 0; i < s->n_go_args && !gpm_op; i++) {
            const gmb_expr_t* a = s->go_args[i];
            if (a && a->kind == GMB_EXPR_VAR && a->var) {
                if (strcasecmp(a->var, "MUTEX") == 0) gpm_op = GMB_OP_NEW_MUTEX;
                else if (strcasecmp(a->var, "RWMUTEX") == 0) gpm_op = GMB_OP_NEW_RWMUTEX;
                else if (strcasecmp(a->var, "COND") == 0) gpm_op = GMB_OP_NEW_COND;
                else if (strcasecmp(a->var, "ONCE") == 0) gpm_op = GMB_OP_NEW_ONCE;
                else if (strcasecmp(a->var, "CHANNEL") == 0) {
                    gpm_op = GMB_OP_NEW_CHANNEL;
                    /* DIM ch AS CHANNEL OF T (n): dopo OF+tipo un eventuale numero è la
                       capacity (parità Go make(chan T, n)); default 0 = unbuffered. */
                    for (int j = i + 1; j < s->n_go_args; j++) {
                        const gmb_expr_t* t = s->go_args[j];
                        if (t && t->kind == GMB_EXPR_NUM) { chan_capacity = (int)t->num; break; }
                    }
                }
                else if (strcasecmp(a->var, "WAITGROUP") == 0) gpm_op = GMB_OP_NEW_WAITGROUP;
                else if (strcasecmp(a->var, "CONCURRENTMAP") == 0 || strcasecmp(a->var, "SYNCMAP") == 0) gpm_op = GMB_OP_NEW_CONCURRENTMAP;
                else if (strcasecmp(a->var, "ATOMIC") == 0) gpm_op = GMB_OP_NEW_ATOMIC;
                else if (strcasecmp(a->var, "POOL") == 0) gpm_op = GMB_OP_NEW_POOL;
            }
        }
        /* Bug corretto (2026-08-31, hang GoRoutinesWorkers.bas): "DIM x AS CHANNEL OF T (n)"
         * e "DIM x AS WAITGROUP" (la sintassi CANONICA, non quella con gli argomenti impilati
         * in go_args) vengono parsati in un ramo DIVERSO (gmb_parser_statements.c, blocco
         * "CHANNEL/WAITGROUP/MUTEX/...") che scrive s->dim_type/s->dim_chan_cap/s->dim_elem_str
         * invece di popolare s->go_args — mai consultato qui sopra. gpm_op restava sempre 0
         * per QUESTA sintassi, l'intero DIM cadeva nel percorso scalare generico (init 0.0):
         * ogni SEND/RECEIVE/WAIT/GO...IN su quella variabile operava su un NUMERO, non un vero
         * canale/waitgroup — nessun blocco/sincronizzazione reale, RECEIVE "succede" sempre
         * banalmente senza mai un valore vero (workers in loop stretto, mai il valore
         * sentinella di stop, CPU spin invece di un blocco pulito o un errore). */
        if (!gpm_op && s->dim_type) {
            if (strcasecmp(s->dim_type, "MUTEX") == 0) gpm_op = GMB_OP_NEW_MUTEX;
            else if (strcasecmp(s->dim_type, "RWMUTEX") == 0) gpm_op = GMB_OP_NEW_RWMUTEX;
            else if (strcasecmp(s->dim_type, "COND") == 0) gpm_op = GMB_OP_NEW_COND;
            else if (strcasecmp(s->dim_type, "ONCE") == 0) gpm_op = GMB_OP_NEW_ONCE;
            else if (strcasecmp(s->dim_type, "CHANNEL") == 0) {
                gpm_op = GMB_OP_NEW_CHANNEL;
                chan_capacity = s->dim_chan_cap > 0 ? s->dim_chan_cap : 0;
            }
            else if (strcasecmp(s->dim_type, "WAITGROUP") == 0) gpm_op = GMB_OP_NEW_WAITGROUP;
            else if (strcasecmp(s->dim_type, "CONCURRENTMAP") == 0 || strcasecmp(s->dim_type, "SYNCMAP") == 0) gpm_op = GMB_OP_NEW_CONCURRENTMAP;
            else if (strcasecmp(s->dim_type, "ATOMIC") == 0) gpm_op = GMB_OP_NEW_ATOMIC;
            else if (strcasecmp(s->dim_type, "POOL") == 0) gpm_op = GMB_OP_NEW_POOL;
        }
        if (gpm_op) {
            cc_byte(c, (uint8_t)gpm_op);
            if (gpm_op == GMB_OP_NEW_CHANNEL) {
                if (chan_capacity < 0) chan_capacity = 0;
                if (chan_capacity > 255) chan_capacity = 255;
                cc_byte(c, (uint8_t)chan_capacity);   /* capacity u8 (specchio Compiler.Assign.cs) */
            }
            cc_store_dim_target(c, s->dim_var, s->line);
            break;
        }
        if (s->dim_empty_parens && !s->dim_init) {
            /* DIM v(): array dinamico VUOTO (Clipper), cresce con AADD/AINS. OP_REDIM non
             * può esprimere "zero slot" (la sua size è sempre "lengths[i]+1", pensata per
             * l'indice più alto stile Clipper — vedi il ramo array_size sotto) — quindi si
             * costruisce la lista vuota con lo stesso meccanismo di un letterale "[]"
             * (GMB_OP_MK_ARRAY con 0 elementi), invece di passare da OP_REDIM.
             * "!s->dim_init" è essenziale: "DIM v() = {...}" (parentesi vuote SEGUITE da un
             * inizializzatore, es. FunctionsLambdaClosures.bas) setta ANCHE
             * dim_empty_parens durante il parsing delle parentesi, PRIMA che il parser
             * veda l'eventuale "= expr" — senza questo guard, l'inizializzatore reale
             * veniva ignorato e "v" diventava sempre una lista vuota. */
            cc_byte(c, GMB_OP_MK_ARRAY); cc_u16(c, 0);
            cc_store_dim_target(c, s->dim_var, s->line);
            break;
        }
        if (s->array_size) {
            /* DIM v(N) [AS Tipo]: array 1D di N slot. Bug corretto (2026-09-02, seguito
             * audit AEVAL): con "AS Tipo" (UDT) qui si passava SEMPRE udtDescIdx=0,
             * ignorando s->dim_type — 0 è un indice di costante VALIDO (non il sentinel
             * "non è un UDT", che è 0xFFFF/65535 per convenzione di OP_REDIM, vedi
             * gmb_vm_core.c), quindi ogni "DIM arr(n) AS Tipo" costruiva gli slot come
             * NIL/scalari invece che istanze del tipo — un successivo `arr(i).Campo`
             * indicizzava uno scalare e faceva crashare la VM ("indexing a numeric
             * scalar"). Fix: risolve il TYPE (se presente) e passa il suo descrittore
             * (FieldNames/FieldTypes, costruito on-demand — vedi cc_udt_desc_const_idx),
             * altrimenti il vero sentinel "nessun UDT". */
            int udt_desc = 65535;
            if (s->dim_type) {
                int ui = cc_udt_find(c, s->dim_type);
                if (ui >= 0) udt_desc = cc_udt_desc_const_idx(c, &c->udts[ui]);
            }
            cc_expr(c, s->array_size);
            if (c->error) return;
            for (int dk = 0; dk < s->n_dim_extra && dk < 7 && !c->error; dk++)
                cc_expr(c, s->dim_extra[dk]);
            if (c->error) return;
            cc_byte(c, GMB_OP_REDIM);
            cc_byte(c, (uint8_t)(1 + s->n_dim_extra));   /* numDims */
            cc_u16(c, (uint16_t)udt_desc);   /* udtDescIdx (0xFFFF = nessun UDT) */
            cc_store_dim_target(c, s->dim_var, s->line);
            cc_arr_rank_set(c, s->dim_var, 1 + s->n_dim_extra);
            /* traccia il tipo elemento (vt_names/vt_types, stesso registro usato per gli
             * UDT scalari): serve a cc_expr per distinguere "arr(i).Campo" (index-poi-
             * campo) da "obj.Campo(idx)" (campo-poi-index) quando entrambi arrivano come
             * la STESSA forma AST CALL("base.Campo", [idx]) — vedi il disambiguatore in
             * cc_expr, ramo GMB_EXPR_CALL con un punto nel nome. */
            if (udt_desc != 65535 && s->dim_type) cc_var_udt_set(c, s->dim_var, s->dim_type);
            /* DIM x(N) = {literal}: REDIM ha allocato N+1 slot ma IGNORAVA l'inizializzatore
             * (leggere x(0) restava vuoto). Riempie i primi slot dal literal `{...}`
             * (parser: CALL "_array_", un elemento per arg) — parità PCode (x(k)=literal[k],
             * i restanti slot restano default). Bug: CurveFitSciPyParity / DIM-array-da-literal. */
            if (s->dim_init && s->dim_init->kind == GMB_EXPR_CALL && s->dim_init->var
                && strcasecmp(s->dim_init->var, "_array_") == 0) {
                for (int li = 0; li < s->dim_init->nargs && !c->error; li++) {
                    cc_expr(c, s->dim_init->args[li]);          /* valore */
                    if (c->error) return;
                    cc_load_target(c, s->dim_var);              /* array */
                    int lidx = cc_add_const(c, GMB_CCONST_DOUBLE, (double)li, NULL);
                    cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, lidx);   /* indice li */
                    if (c->error) return;
                    cc_byte(c, GMB_OP_STORE_ELEMENT); cc_byte(c, 1);
                }
            }
            break;
        }
        /* DIM v AS <Tipo> [= altravar]: <Tipo> è un TYPE registrato (UDT, campi nudi,
         * value-type) → costruisci un record dict indipendente invece dello 0.0 di
         * default generico (vedi cc_udt_register/cc_emit_udt_init). Con inizializzatore
         * a variabile dello stesso schema di campi, CLONA i valori (semantica per
         * valore) invece di condividere il dict (che sarebbe la semantica per
         * riferimento delle CLASS, non ancora supportate da questo compilatore).
         * NB: MAI quando s->dim_empty_parens — "DIM x() AS Udt = AFILTER(...)" dichiara
         * un ARRAY dinamico con inizializzatore-lista, NON una variabile-scalare UDT: quel
         * caso deve andare al ramo generico sotto (valuta l'expr e la conserva come lista). */
        if (s->dim_type && !s->dim_empty_parens) {
            int ui = cc_udt_find(c, s->dim_type);
            if (ui >= 0) {
                int src_slot = -1;
                if (s->dim_init) {
                    cc_expr(c, s->dim_init);
                    if (c->error) return;
                    src_slot = cc_local_add(c, "__udt_src");
                    cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, src_slot);
                }
                cc_byte(c, GMB_OP_MK_DICT); cc_u16(c, 0);
                cc_store_dim_target(c, s->dim_var, s->line);
                cc_emit_udt_init(c, &c->udts[ui], s->dim_var, src_slot);
                if (c->error) return;
                cc_var_udt_set(c, s->dim_var, c->udts[ui].name);
                cc_arr_rank_set(c, s->dim_var, -1);   /* record SCALARE tipizzato (non array) */
                break;
            }
            int cli = cc_class_find(c, s->dim_type);
            if (cli >= 0 && s->dim_init) {
                /* DIM x AS ClassName = NEW ClassName(...): CLASS è per riferimento (a
                 * differenza degli UDT sopra) — nessuna clonazione, solo tracciamento del
                 * tipo (vt_names/vt_types) per abilitare il dispatch di x.Metodo(...) via
                 * OP_CALL_MEMBER più avanti nel programma. */
                cc_expr(c, s->dim_init);
                if (c->error) return;
                cc_store_dim_target(c, s->dim_var, s->line);
                cc_var_udt_set(c, s->dim_var, c->classes[cli].name);
                break;
            }
        }
        if (!s->dim_type && s->dim_init && s->dim_init->kind == GMB_EXPR_CALL && s->dim_init->var) {
            /* DIM x = NEW ClassName(...) o = FuncCheRestituisceUnaClasse(...) (senza AS
             * esplicito): stesso tracciamento, inferito dal nome della "chiamata" NEW
             * (che il parser produce identica a una CALL — vedi gmb_parser_expressions.c)
             * o dal tipo di ritorno dichiarato della funzione (cc_ret_class_register). */
            int cli = cc_class_find(c, s->dim_init->var);
            const char* rcn = cli < 0 ? cc_proc_ret_class(c, s->dim_init->var) : NULL;
            if (cli < 0 && rcn) cli = cc_class_find(c, rcn);
            if (cli >= 0) {
                cc_expr(c, s->dim_init);
                if (c->error) return;
                cc_store_dim_target(c, s->dim_var, s->line);
                cc_var_udt_set(c, s->dim_var, c->classes[cli].name);
                break;
            }
        }
        if (!s->dim_type && s->dim_init && s->dim_init->kind == GMB_EXPR_VAR && s->dim_init->var
            && !strchr(s->dim_init->var, '.') && s->dim_init->nargs == 0) {
            /* DIM x = y (senza AS), y già tracciata come istanza di classe — stesso
             * motivo del ramo analogo in GMB_STMT_ASSIGN (polimorfismo dopo copia). */
            const char* tn = cc_var_udt_get(c, s->dim_init->var);
            int cli = tn ? cc_class_find(c, tn) : -1;
            if (cli >= 0) {
                cc_expr(c, s->dim_init);
                if (c->error) return;
                cc_store_dim_target(c, s->dim_var, s->line);
                cc_var_udt_set(c, s->dim_var, c->classes[cli].name);
                break;
            }
        }
        if (!s->dim_type && s->dim_init && s->dim_init->kind == GMB_EXPR_CALL && s->dim_init->var
            && strcasecmp(s->dim_init->var, "AMAP") == 0 && s->dim_init->nargs >= 2
            && s->dim_init->args[1] && s->dim_init->args[1]->kind == GMB_EXPR_VAR && s->dim_init->args[1]->var) {
            /* Bug corretto (2026-09-06): "DIM x() = AMAP(arr, ProcNominata)" — se
             * ProcNominata dichiara "AS Tipo"/"AS Classe" come ritorno, l'elemento
             * risultante NON è il tipo dell'array di input ma quello della callback:
             * senza tracciarlo, "x(i).Campo" non viene riconosciuto come indice-poi-
             * campo dal branch GMB_EXPR_CALL più sotto (richiede cc_var_udt_get(mbase)
             * valorizzato) e l'accesso a campo sul risultato di AMAP restituisce
             * silenziosamente NIL — pur essendo i DATI creati da AMAP corretti
             * (verificabile con JSON.STRINGIFY sull'elemento). */
            const char* rtn = cc_proc_ret_class(c, s->dim_init->args[1]->var);
            if (rtn) {
                cc_expr(c, s->dim_init);
                if (c->error) return;
                cc_store_dim_target(c, s->dim_var, s->line);
                cc_var_udt_set(c, s->dim_var, rtn);
                break;
            }
        }
        if (s->dim_init) {
            /* DIM v [AS tipo] = expr: inizializzatore reale (parità C#) */
            if (s->dim_init->kind == GMB_EXPR_CALL && s->dim_init->var
                && strcmp(s->dim_init->var, "_tuple_lit_") == 0
                && s->dim_init->ns && s->dim_init->ns[0])
                cc_tuple_set(s->dim_var, s->dim_init->ns);
            /* Registra la closure PRIMA di compilare l'inizializzatore: la
             * compilazione della lambda può riscrivere il nodo (e in un frammento
             * runtime il controllo `cc_is_closure_var` al call-site deve vederla). */
            if (s->dim_init->kind == GMB_EXPR_LAMBDA) cc_closure_var_add(c, s->dim_var);
            cc_expr(c, s->dim_init);
            if (c->error) return;
            if (s->dim_init->kind == GMB_EXPR_LAMBDA) cc_closure_var_add(c, s->dim_var);
            else if (s->dim_init->kind == GMB_EXPR_CALL && s->dim_init->var
                && cc_proc_returns_closure(c, s->dim_init->var)) cc_closure_var_add(c, s->dim_var);
            if (cc_container_is_set(s->dim_type)) { cc_byte(c, GMB_OP_LIST_DEDUP); cc_var_udt_set(c, s->dim_var, "HASHSET"); }
            if (cc_container_is_sorted(s->dim_type)) cc_byte(c, GMB_OP_DICT_SORTED);
            /* Array dinamico tipizzato "DIM x() AS <Udt> = <lista>": traccia il tipo
             * elemento (vt) perché x(i).Campo si risolva come indice-poi-campo. */
            if (s->dim_type) {
                int gui = cc_udt_find(c, s->dim_type);
                if (gui >= 0) { cc_var_udt_set(c, s->dim_var, s->dim_type); cc_arr_rank_set(c, s->dim_var, 1); }
            }
            /* Stesso schema del ramo analogo in GMB_STMT_ASSIGN (2026-08-31): un
             * riferimento NUDO a una procedura assegnato in DIM x = ProcName (senza
             * AS esplicito) non veniva mai registrato come closure var — senza questa
             * registrazione, invocare "x(...)" più avanti cadeva nel fallback generico
             * "proc sconosciuta" invece di risolversi come chiamata indiretta. */
            else if (s->dim_init->kind == GMB_EXPR_VAR && s->dim_init->var
                && cc_local(c, s->dim_init->var) < 0 && cc_global(c, s->dim_init->var, 0) < 0
                && cc_proc_known(c, s->dim_init->var)) cc_closure_var_add(c, s->dim_var);
            cc_store_dim_target(c, s->dim_var, s->line);
            break;
        }
        if (s->dim_type && !strcasecmp(s->dim_type, "STRINGBUILDER")) {
            cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3410); cc_byte(c, 0);
            cc_store_dim_target(c, s->dim_var, s->line);
            break;
        }
        if (s->dim_type && cc_container_is_graph(s->dim_type)) {
            int zc = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, zc);
            cc_byte(c, GMB_OP_NEW); cc_u16(c, 17);
            cc_store_dim_target(c, s->dim_var, s->line);
            break;
        }
        /* DIM v AS <TipoConMetodi> (TYPE/CLASS con metodi, no init): crea l'istanza di
         * default via OP_NEW_CLASS_INSTANCE (descriptor con VTable) invece dello 0.0 —
         * altrimenti r.Area() colpiva uno scalare. */
        if (s->dim_type && !s->dim_empty_parens) {
            int dcli = cc_class_find(c, s->dim_type);
            if (dcli >= 0 && !s->dim_init) {
                int ai3 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai3);
                cc_byte(c, GMB_OP_NEW_CLASS_INSTANCE); cc_u16(c, c->classes[dcli].desc_const_idx);
                cc_store_dim_target(c, s->dim_var, s->line);
                cc_var_udt_set(c, s->dim_var, c->classes[dcli].name);
                break;
            }
        }
        int ck = s->dim_type ? cc_container_kind(s->dim_type) : 0;
        if (ck == 1) {
            cc_byte(c, GMB_OP_MK_ARRAY); cc_u16(c, 0);
            cc_store_dim_target(c, s->dim_var, s->line);
            break;
        }
        if (ck == 2) {
            cc_byte(c, GMB_OP_MK_DICT); cc_u16(c, 0);
            cc_store_dim_target(c, s->dim_var, s->line);
            break;
        }
        int idx = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
        if (c->error) return;
        cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, idx);
        cc_store_dim_target(c, s->dim_var, s->line);
        break;
    }
    case GMB_STMT_CLASS: {
        /* TYPE...END TYPE (UDT, campi nudi), CLASS/TYPE con metodi propri, o INTERFACE:
         * campi/metodi/firme sono già stati registrati nel pre-pass di gmb_compile
         * (cc_udt_register/cc_class_register — quest'ultimo include già il descrittore
         * VTable/Constructors con gli indirizzi placeholder per una CLASS, o solo i
         * method_names per un'INTERFACE), niente bytecode da emettere qui — puramente
         * dichiarativo (i corpi di metodi/costruttore sono compilati a parte, in passata
         * 4b, vedi gmb_compile — le INTERFACE non hanno corpi, saltate lì
         * esplicitamente). INHERITS/campi STATIC/PROPERTY/OPERATOR/IMPLEMENTS/INTERFACE
         * sono tutti supportati — un errore sulla base non trovata è già stato segnalato
         * in cc_class_register (cc_err), qui non c'è altro da validare. */
        break;
    }
    case GMB_STMT_ADD_HANDLER:
    case GMB_STMT_REMOVE_HANDLER: {
        /* ADD/REMOVE HANDLER obj.NomeEvento, procedura (2.9): il registro handler è
         * una LISTA per-istanza sotto "__event_<Nome>" (inizializzata a vuota dal
         * prologo del costruttore — vedi cc_class_member/cc_class_register, SEMPRE
         * presente perché una classe con EVENT ha sempre un costruttore, sintetizzato
         * se necessario). "procedura" diventa una CLOSURE senza upvalue (0 catture —
         * l'indirizzo si risolve con lo stesso meccanismo a patch differito di
         * PUSH_CODE_REF/GO FUNCTION, cc_add_const_unique+cc_coderef_register). ADD fa
         * .Add(closure); REMOVE cerca l'indice con .IndexOf(closure) (uguaglianza per
         * VALORE della chiusura — stesso indirizzo/upvalue, non identità del puntatore
         * C, verificato in test) e rimuove con .RemoveAt(idx) solo se trovato
         * (idx >= 0) — evita un errore silenzioso se l'handler non era registrato.
         * Bug reale scoperto in verifica: gmb_adapter_val_to_stack (gmb_adapter.c) NON
         * preserva l'identità per un valore GMB_TYPE_OBJECT che avvolge una lista —
         * crea SEMPRE un gmb_obj_shared_val_t NUOVO con gli elementi COPIATI (mai lo
         * stesso wrapper). Quindi ogni LOAD_ELEMENT su un campo-lista (incluso questo
         * registro handler) restituisce una COPIA indipendente, mai un alias dell'
         * originale nel dict — .Add()/.RemoveAt() sulla copia non si riflettono mai
         * nel campo. Fix: dopo ogni mutazione, RISCRIVI esplicitamente il risultato
         * nel campo con STORE_FIELD (stesso schema di un `obj.Campo = espr` normale). */
        if (!s->channel || s->channel->kind != GMB_EXPR_VAR || !s->channel->var) { cc_err(c, "ADD/REMOVE HANDLER: riferimento evento non valido"); return; }
        const char* dot = strrchr(s->channel->var, '.');
        if (!dot || dot == s->channel->var) { cc_err(c, "ADD/REMOVE HANDLER: atteso obj.NomeEvento"); return; }
        char mbase[256]; size_t mlen = (size_t)(dot - s->channel->var);
        if (mlen >= sizeof(mbase)) { cc_err(c, "ADD/REMOVE HANDLER: nome troppo lungo"); return; }
        memcpy(mbase, s->channel->var, mlen); mbase[mlen] = 0;
        const char* eventName = dot + 1;
        if (!s->send_value || s->send_value->kind != GMB_EXPR_VAR || !s->send_value->var) { cc_err(c, "ADD/REMOVE HANDLER: atteso il nome di una procedura come handler"); return; }
        const char* procName = s->send_value->var;
        char fkey[80]; snprintf(fkey, sizeof(fkey), "__event_%s", eventName);

        int listSlot = cc_local_add(c, "__evt_list");
        cc_load_target(c, mbase);
        cc_push_str_const(c, fkey);
        if (c->error) return;
        cc_byte(c, GMB_OP_LOAD_ELEMENT); cc_byte(c, 1);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, listSlot);

        if (s->kind == GMB_STMT_ADD_HANDLER) {
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, listSlot);
            int ci = cc_add_const_unique(c);
            if (c->error) return;
            cc_coderef_register(c, ci, procName);
            cc_byte(c, GMB_OP_CLOSURE); cc_u16(c, ci); cc_byte(c, 0);
            int ai = cc_add_const(c, GMB_CCONST_DOUBLE, 1.0, NULL);
            int ni = cc_add_const(c, GMB_CCONST_STRING, 0, "Add");
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai);
            cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, ni);
            cc_byte(c, GMB_OP_POP);
            /* riscrivi la lista (mutata in-place) nel campo — vedi nota sopra. */
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, listSlot);
            cc_load_target(c, mbase);
            int fi = cc_add_const(c, GMB_CCONST_STRING, 0, fkey);
            if (c->error) return;
            cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi);
        } else {
            /* Bug reale scoperto in verifica: .IndexOf(closure) usa gmb_val_equals, che
             * per un OBJECT confronta per IDENTITÀ DI PUNTATORE C — ma ADD e REMOVE
             * HANDLER creano ciascuno una CLOSURE NUOVA (gmb_closure_new) anche per la
             * STESSA procedura, mai lo stesso puntatore: IndexOf non trova MAI un match,
             * idx sempre -1, RemoveAt mai eseguita. Fix: scansione manuale (stesso
             * schema FE_NEXT di RAISE EVENT) confrontando ->address ESPOSTO come numero
             * tramite CALL_MEMBER "Address" (vedi gmb_vm_core.c) — i NUMERI si confrontano
             * correttamente con CMP_EQ, a differenza dei riferimenti oggetto. */
            int targetSlot = cc_local_add(c, "__evt_target");
            int ci = cc_add_const_unique(c);
            if (c->error) return;
            cc_coderef_register(c, ci, procName);
            cc_byte(c, GMB_OP_CLOSURE); cc_u16(c, ci); cc_byte(c, 0);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, targetSlot);
            int targetAddrSlot = cc_local_add(c, "__evt_target_addr");
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, targetSlot);
            int a0 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            int nAddr1 = cc_add_const(c, GMB_CCONST_STRING, 0, "Address");
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, a0);
            cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, nAddr1);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, targetAddrSlot);

            int scanIdxSlot = cc_local_add(c, "__evt_scan_idx");
            int okSlot = cc_local_add(c, "__evt_scan_ok");
            int hSlot = cc_local_add(c, "__evt_scan_h");
            int hAddrSlot = cc_local_add(c, "__evt_scan_haddr");
            int foundIdxSlot = cc_local_add(c, "__evt_found_idx");
            int zi = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            int negOne = cc_add_const(c, GMB_CCONST_DOUBLE, -1.0, NULL);
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, zi);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, scanIdxSlot);
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, negOne);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, foundIdxSlot);

            int loop_start = c->code_len;
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, listSlot);
            cc_byte(c, GMB_OP_FE_NEXT); cc_u16(c, scanIdxSlot);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, okSlot);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, hSlot);
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, okSlot);
            int z1 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z1);
            cc_byte(c, GMB_OP_CMP_EQ);
            cc_reljump(c, GMB_OP_JUMP_IF_TRUE, -1);   /* lista esaurita → esci dalla scansione */
            int exhausted_jmp = c->nrel - 1;
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, hSlot);
            int a1 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            int nAddr2 = cc_add_const(c, GMB_CCONST_STRING, 0, "Address");
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, a1);
            cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, nAddr2);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, hAddrSlot);
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, hAddrSlot);
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, targetAddrSlot);
            cc_byte(c, GMB_OP_CMP_EQ);
            cc_reljump(c, GMB_OP_JUMP_IF_FALSE, -1);   /* non è questo handler → prossimo giro */
            int not_matched_jmp = c->nrel - 1;
            /* trovato: FE_NEXT ha già avanzato scanIdxSlot a (indice+1) — sottrai 1. */
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, scanIdxSlot);
            int o1 = cc_add_const(c, GMB_CCONST_DOUBLE, 1.0, NULL);
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, o1);
            cc_byte(c, GMB_OP_SUBTRACT);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, foundIdxSlot);
            cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);   /* trovato → esci dalla scansione */
            int found_jmp = c->nrel - 1;
            if (c->reljumps[not_matched_jmp].target < 0) c->reljumps[not_matched_jmp].target = c->code_len;
            cc_reljump_back(c, loop_start);
            int scan_end = c->code_len;
            if (c->reljumps[exhausted_jmp].target < 0) c->reljumps[exhausted_jmp].target = scan_end;
            if (c->reljumps[found_jmp].target < 0) c->reljumps[found_jmp].target = scan_end;

            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, foundIdxSlot);
            int z0 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z0);
            cc_byte(c, GMB_OP_CMP_LT);
            cc_reljump(c, GMB_OP_JUMP_IF_TRUE, -1);   /* mai trovato: salta la RemoveAt */
            int skip_jmp = c->nrel - 1;
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, listSlot);
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, foundIdxSlot);
            int ai2 = cc_add_const(c, GMB_CCONST_DOUBLE, 1.0, NULL);
            int nra = cc_add_const(c, GMB_CCONST_STRING, 0, "RemoveAt");
            if (c->error) return;
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ai2);
            cc_byte(c, GMB_OP_CALL_MEMBER); cc_u16(c, nra);
            /* RemoveAt (a differenza di Add) non spinge alcun valore di ritorno (vedi
             * gmb_vm_core.c) — nessun POP da fare qui (bug reale: un POP qui avrebbe
             * scartato un operando ESTRANEO, mai il "ritorno" di RemoveAt). */
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, listSlot);
            cc_load_target(c, mbase);
            int fi2 = cc_add_const(c, GMB_CCONST_STRING, 0, fkey);
            if (c->error) return;
            cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, fi2);
            if (c->reljumps[skip_jmp].target < 0) c->reljumps[skip_jmp].target = c->code_len;
        }
        break;
    }
    case GMB_STMT_RAISE_EVENT: {
        /* RAISE EVENT Nome(args) (2.9): implicito su SELF (mai richiamabile fuori da un
         * metodo di classe — s->proc_name porta solo il NOME, catturato dal parser come
         * GMB_EXPR_VAR). Itera il registro handler ("__event_Nome") con lo stesso
         * schema di GMB_OP_FE_NEXT usato per FOR EACH (vedi quel case), chiamando ogni
         * handler con CALL_INDIRECT — una closure a 0 upvalue creata da ADD HANDLER
         * punta a una procedura NORMALE (compilata con cc_proc: argomenti sullo STACK,
         * mai il frame class-aware — CALL_INDIRECT è compatibile, vedi la sua gestione
         * in gmb_vm_core.c). */
        if (!s->proc_name || s->proc_name->kind != GMB_EXPR_VAR || !s->proc_name->var || c->cur_class < 0) {
            cc_err(c, "RAISE EVENT: valido solo dentro un metodo di classe con nome evento noto");
            return;
        }
        char fkey[80]; snprintf(fkey, sizeof(fkey), "__event_%s", s->proc_name->var);
        int listSlot = cc_local_add(c, "__evt_raise_list");
        cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, 0);   /* SELF */
        cc_push_str_const(c, fkey);
        if (c->error) return;
        cc_byte(c, GMB_OP_LOAD_ELEMENT); cc_byte(c, 1);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, listSlot);
        int idxSlot = cc_local_add(c, "__evt_raise_idx");
        int okSlot = cc_local_add(c, "__evt_raise_ok");
        int hSlot = cc_local_add(c, "__evt_raise_h");
        int z0 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
        if (c->error) return;
        cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z0);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, idxSlot);
        int loop_start = c->code_len;
        cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, listSlot);
        cc_byte(c, GMB_OP_FE_NEXT); cc_u16(c, idxSlot);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, okSlot);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, hSlot);
        cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, okSlot);
        int z1 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
        if (c->error) return;
        cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z1);
        cc_byte(c, GMB_OP_CMP_EQ);
        cc_reljump(c, GMB_OP_JUMP_IF_TRUE, -1);   /* lista esaurita → esci */
        int exit_jmp = c->nrel - 1;
        for (int i = 0; i < s->n_go_args; i++) { cc_expr(c, s->go_args[i]); if (c->error) return; }
        cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, hSlot);
        cc_byte(c, GMB_OP_CALL_INDIRECT); cc_byte(c, (uint8_t)s->n_go_args);
        cc_byte(c, GMB_OP_POP);
        cc_reljump_back(c, loop_start);
        if (c->reljumps[exit_jmp].target < 0) c->reljumps[exit_jmp].target = c->code_len;
        break;
    }
    case GMB_STMT_EXPR:
    case GMB_STMT_CALL: {
        /* `&(cmd)` / `EVAL(cmd)` usato come STATEMENT: è una MACRO-COMANDO, non
         * un'espressione → OP_EVAL_MACRO_STMT (il frammento viene compilato come
         * sequenza di statement, senza valore di ritorno). Prima emetteva la
         * variante EXPR anche qui: `&("PRINT 42")` / `&("X = X + 5")` finivano
         * compilati come espressione (RETURN PRINT…, `=` come uguaglianza). */
        if (s->expr && s->expr->kind == GMB_EXPR_CALL && s->expr->var
            && strcmp(s->expr->var, "_macro_") == 0 && s->expr->nargs == 1) {
            cc_expr(c, s->expr->args[0]);
            if (c->error) return;
            cc_byte(c, GMB_OP_EVAL_MACRO_STMT);
            break;
        }
        cc_expr(c, s->expr);
        if (c->error) return;
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_NET_CMD: {
        if (s->net_sub == 255) {
            /* FETCH url[, opts] INTO Callback (parity Compiler.Network.cs VisitFetchStatement): url, opts-o-nil, code-ref, OP_FETCH variant=1, POP */
            cc_expr(c, s->n_netargs > 0 ? s->net_args[0] : NULL);
            if (c->error) return;
            if (s->n_netargs > 1 && s->net_args[1]) cc_expr(c, s->net_args[1]); else cc_byte(c, GMB_OP_PUSH_NIL);
            if (c->error) return;
            int ci = cc_add_const_unique(c);
            if (c->error) return;
            cc_coderef_register(c, ci, s->net_coderef);
            cc_byte(c, GMB_OP_PUSH_CODE_REF); cc_u16(c, ci);
            cc_byte(c, GMB_OP_FETCH); cc_byte(c, 1);
            cc_byte(c, GMB_OP_POP);
            break;
        }
        /* OP_NET_CMD: push argomenti in ordine (entry NULL → NIL), code-ref alla
           posizione coderef_pos, poi [opcode][sub][argc] (parità Compiler.Network.cs) */
        int total = s->n_netargs + (s->net_coderef ? 1 : 0);
        int argi = 0;
        for (int i = 0; i < total; i++) {
            if (s->net_coderef && i == s->coderef_pos) {
                int ci = cc_add_const_unique(c);
                if (c->error) return;
                cc_coderef_register(c, ci, s->net_coderef);
                cc_byte(c, GMB_OP_PUSH_CODE_REF); cc_u16(c, ci);
            } else {
                cc_expr(c, argi < s->n_netargs ? s->net_args[argi] : NULL);
                argi++;
            }
            if (c->error) return;
        }
        cc_byte(c, GMB_OP_NET_CMD);
        cc_byte(c, (uint8_t)s->net_sub);
        cc_byte(c, (uint8_t)total);
        /* risultati multipli: il primo target pesca il top-of-stack (parità AssignVariable C#) */
        if (s->net_assign) { cc_store_dim_target(c, s->net_assign, s->line); if (c->error) return; }
        if (s->net_assign2) { cc_store_dim_target(c, s->net_assign2, s->line); if (c->error) return; }
        if (s->net_assign3) { cc_store_dim_target(c, s->net_assign3, s->line); if (c->error) return; }
        break;
    }
    case GMB_STMT_IF: {
        cc_expr(c, s->cond);
        if (c->error) return;
        cc_reljump(c, GMB_OP_JUMP_IF_FALSE, -1);   /* → else/end */
        int cond_jmp = c->nrel - 1;
        cc_block(c, s->then_body, s->n_then);
        if (s->n_else > 0) {
            cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);  /* salta l'else */
            int skip_jmp = c->nrel - 1;
            /* patch per INDICE catturato: i costrutti annidati nei body spingono i
               propri reljump, quindi nrel-2/nrel-1 non puntano più a questi jump
               (bug: WHILE con IF nel body non usciva mai) */
            if (c->reljumps[cond_jmp].target < 0) c->reljumps[cond_jmp].target = c->code_len;
            cc_block(c, s->else_body, s->n_else);
            /* patch lo skip dell'else: dopo il body ELSE si prosegue oltre */
            if (c->reljumps[skip_jmp].target < 0) c->reljumps[skip_jmp].target = c->code_len;
        } else {
            if (c->reljumps[cond_jmp].target < 0) c->reljumps[cond_jmp].target = c->code_len;
        }
        break;
    }
    case GMB_STMT_WHILE: {
        int start = c->code_len;
        int ld = c->loop_depth < 15 ? c->loop_depth++ : 15;
        cc_expr(c, s->cond);
        if (c->error) return;
        cc_reljump(c, GMB_OP_JUMP_IF_FALSE, -1);
        int exit_jmp = c->nrel - 1;
        cc_block(c, s->while_body, s->n_while);
        cc_reljump_back(c, start);
        if (c->reljumps[exit_jmp].target < 0) c->reljumps[exit_jmp].target = c->code_len;
        for (int i = 0; i < c->exit_count[ld]; i++) { int eoff = c->code_len - (c->exit_pos[ld][i] + 2); uint8_t* pc = c->code + c->exit_pos[ld][i]; pc[0] = (uint8_t)((eoff >> 8) & 0xFF); pc[1] = (uint8_t)(eoff & 0xFF); }
        c->exit_count[ld] = 0;
        c->loop_depth--;
        break;
    }
    case GMB_STMT_DO: {
        int start = c->code_len;
        int ld = c->loop_depth < 15 ? c->loop_depth++ : 15;
        if (s->do_while) {
            /* test in cima: DO WHILE/UNTIL cond — esci se falsa (WHILE) o vera (UNTIL) */
            cc_expr(c, s->do_while);
            if (c->error) return;
            cc_reljump(c, s->flag ? GMB_OP_JUMP_IF_TRUE : GMB_OP_JUMP_IF_FALSE, -1);
            int exit_jmp = c->nrel - 1;
            cc_block(c, s->do_body, s->n_do);
            cc_reljump_back(c, start);
            if (c->reljumps[exit_jmp].target < 0) c->reljumps[exit_jmp].target = c->code_len;
        } else {
            cc_block(c, s->do_body, s->n_do);
            if (s->do_until) {
                /* test in coda: LOOP UNTIL/WHILE cond */
                cc_expr(c, s->do_until);
                if (c->error) return;
                cc_reljump(c, s->flag ? GMB_OP_JUMP_IF_TRUE : GMB_OP_JUMP_IF_FALSE, -1);
                int exit_jmp = c->nrel - 1;
                cc_reljump_back(c, start);
                if (c->reljumps[exit_jmp].target < 0) c->reljumps[exit_jmp].target = c->code_len;
            } else {
                cc_reljump_back(c, start);
            }
        }
        for (int i = 0; i < c->exit_count[ld]; i++) { int eoff = c->code_len - (c->exit_pos[ld][i] + 2); uint8_t* pc = c->code + c->exit_pos[ld][i]; pc[0] = (uint8_t)((eoff >> 8) & 0xFF); pc[1] = (uint8_t)(eoff & 0xFF); }
        c->exit_count[ld] = 0;
        c->loop_depth--;
        break;
    }
    case GMB_STMT_FOR: {
        int ld = c->loop_depth < 15 ? c->loop_depth++ : 15;
        if (s->flag) {
            /* FOR EACH a, b, c IN matrice — destructuring per righe (OP_FE_MATRIX_D) */
            if (s->for_to && s->for_to->kind == GMB_EXPR_VAR && s->for_to->var
                && s->for_step && s->for_step->kind == GMB_EXPR_VAR && s->for_step->var) {
                cc_expr(c, s->for_from);
                if (c->error) return;
                int ma2 = cc_local_add(c, "__fe_arr");
                int mi2 = cc_local_add(c, "__fe_idx");
                int mf2 = cc_local_add(c, "__fe_ok");
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, ma2);
                int mz1 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, mz1);
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, mi2);
                int msa = cc_local_add(c, s->for_var);
                int msb = cc_local_add(c, s->for_to->var);
                int msc = cc_local_add(c, s->for_step->var);
                int mloop2 = c->code_len;
                cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, ma2);
                cc_byte(c, GMB_OP_FE_MATRIX_D); cc_u16(c, mi2);
                cc_byte(c, 3);
                cc_u16(c, (unsigned)msa); cc_u16(c, (unsigned)msb); cc_u16(c, (unsigned)msc);
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, mf2);
                cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, mf2);
                int mz2 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, mz2);
                cc_byte(c, GMB_OP_CMP_EQ);
                cc_reljump(c, GMB_OP_JUMP_IF_TRUE, -1);
                int me2 = c->nrel - 1;
                cc_block(c, s->for_body, s->n_for);
                if (c->error) return;
                cc_reljump_back(c, mloop2);
                int m2end = c->code_len;
                if (c->reljumps[me2].target < 0) c->reljumps[me2].target = m2end;
                for (int i = 0; i < c->exit_count[ld]; i++) { int eoff = m2end - (c->exit_pos[ld][i] + 2); uint8_t* pc = c->code + c->exit_pos[ld][i]; pc[0] = (uint8_t)((eoff >> 8) & 0xFF); pc[1] = (uint8_t)(eoff & 0xFF); }
                c->exit_count[ld] = 0;
                c->loop_depth--;
                break;
            }
            /* FOR EACH k, v IN <DICT> — coppia chiave/valore (OP_FE_DICT mode 2):
               [coll] STORE arr ; idx=0 STORE idx ;
               loop: LOAD arr, FE_DICT idx,2 → pusha key,value,ok ;
               STORE ok, STORE valSlot, STORE keySlot ; ok==0 → esci ; body ; JUMP loop. */
            if (s->for_to && s->for_to->kind == GMB_EXPR_VAR && s->for_to->var
                && !(s->for_step && s->for_step->kind == GMB_EXPR_VAR && s->for_step->var)) {
                cc_expr(c, s->for_from);
                if (c->error) return;
                int a2 = cc_local_add(c, "__fe_arr");
                int i2 = cc_local_add(c, "__fe_idx");
                int o2 = cc_local_add(c, "__fe_ok");
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, a2);
                int zk = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, zk);
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, i2);
                int key_slot = cc_local_add(c, s->for_var);
                int val_slot = cc_local_add(c, s->for_to->var);
                int lstart2 = c->code_len;
                cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, a2);
                cc_byte(c, GMB_OP_FE_DICT); cc_u16(c, i2); cc_byte(c, 2);
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, o2);
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, val_slot);
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, key_slot);
                cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, o2);
                int zo = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, zo);
                cc_byte(c, GMB_OP_CMP_EQ);
                cc_reljump(c, GMB_OP_JUMP_IF_TRUE, -1);
                int xj2 = c->nrel - 1;
                cc_block(c, s->for_body, s->n_for);
                if (c->error) return;
                cc_reljump_back(c, lstart2);
                int e2 = c->code_len;
                if (c->reljumps[xj2].target < 0) c->reljumps[xj2].target = e2;
                for (int i = 0; i < c->exit_count[ld]; i++) { int eoff = e2 - (c->exit_pos[ld][i] + 2); uint8_t* pc = c->code + c->exit_pos[ld][i]; pc[0] = (uint8_t)((eoff >> 8) & 0xFF); pc[1] = (uint8_t)(eoff & 0xFF); }
                c->exit_count[ld] = 0;
                c->loop_depth--;
                break;
            }
            /* FOR EACH v IN <coll> — GMB_OP_FE_NEXT fa il dispatch a runtime tra canale
               (parità Go for v := range ch) e lista/dict/array (indice+conteggio), vedi
               il commento su GMB_OP_FE_NEXT in gmb_compiler.h per il bug che questo
               sostituisce (RECV_OK incondizionato = type confusion su non-canali).
               specchio Compiler.Others.cs VisitForEachInit/AdvanceAndAssign (isChannel,
               lì risolto a compile-time con vera type-inference):
               [coll] STORE arrSlot ; idx=0 STORE idxSlot ;
               loop: LOAD arrSlot, FE_NEXT idxSlot, STORE flagSlot,
               STORE v, LOAD flagSlot, push 0, CMP_EQ, JUMP_IF_TRUE → end ; body ;
               JUMP_BACKWARD loop. */
            cc_expr(c, s->for_from);
            if (c->error) return;
            int arr_slot = cc_local_add(c, "__fe_arr");
            int idx_slot = cc_local_add(c, "__fe_idx");
            int flag_slot = cc_local_add(c, "__fe_ok");
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, arr_slot);
            int z0 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z0);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, idx_slot);
            int v_slot = cc_local_add(c, s->for_var);
            int loop_start = c->code_len;
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, arr_slot);
            /* FOR EACH su matrice 2D+ (var dichiarata con >=2 indici): iterazione per RIGHE
             * (slice) — OP_FE_MATRIX_ROW; altrimenti dispatch normale lista/dict/stringa. */
            {
                const char* fe_src = (s->for_from && s->for_from->kind == GMB_EXPR_VAR) ? s->for_from->var : NULL;
                int fe_rank = fe_src ? cc_arr_rank_get(c, fe_src) : 0;
                if (fe_rank >= 2)
                {
                    cc_byte(c, GMB_OP_FE_MATRIX_ROW); cc_u16(c, idx_slot); cc_byte(c, 1);
                }
                else
                {
                    cc_byte(c, GMB_OP_FE_DICT); cc_u16(c, idx_slot); cc_byte(c, 1);
                }
            }
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, flag_slot);
            cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, v_slot);
            cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, flag_slot);
            int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
            cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z);
            cc_byte(c, GMB_OP_CMP_EQ);
            cc_reljump(c, GMB_OP_JUMP_IF_TRUE, -1);   /* canale chiuso e svuotato → esci */
            int exit_jmp = c->nrel - 1;
            cc_block(c, s->for_body, s->n_for);
            if (c->error) return;
            cc_reljump_back(c, loop_start);
            int end = c->code_len;
            if (c->reljumps[exit_jmp].target < 0) c->reljumps[exit_jmp].target = end;
            for (int i = 0; i < c->exit_count[ld]; i++) { int eoff = end - (c->exit_pos[ld][i] + 2); uint8_t* pc = c->code + c->exit_pos[ld][i]; pc[0] = (uint8_t)((eoff >> 8) & 0xFF); pc[1] = (uint8_t)(eoff & 0xFF); }
            c->exit_count[ld] = 0;
            c->loop_depth--;
            break;
        }
        /* for_var = from ; push end, step ; FOR_PREPARE slot */
        cc_expr(c, s->for_from);
        cc_expr(c, s->for_to);
        if (s->for_step) cc_expr(c, s->for_step);
        else { int z = cc_add_const(c, GMB_CCONST_DOUBLE, 1.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z); }
        if (c->error) return;
        int slot = cc_local_add(c, s->for_var);
        cc_byte(c, GMB_OP_FOR_PREPARE); cc_u16(c, slot);
        cc_reljump(c, GMB_OP_JUMP_IF_FALSE, -1);      /* se should==false esci */
        int exit_jmp = c->nrel - 1;
        int loop_start = c->code_len;
        cc_block(c, s->for_body, s->n_for);
        /* OP_FOR_LOOP slot, back_offset */
        cc_byte(c, GMB_OP_FOR_LOOP); cc_u16(c, slot);
        int bpos = c->code_len;
        cc_u16(c, 0);
        if (c->nrel >= c->nrel_cap) { c->nrel_cap = c->nrel_cap ? c->nrel_cap * 2 : 16; c->reljumps = (cc_reljump_t*)realloc(c->reljumps, (size_t)c->nrel_cap * sizeof(*c->reljumps)); }
        c->reljumps[c->nrel].pos = bpos;
        c->reljumps[c->nrel].target = loop_start;
        c->reljumps[c->nrel].backward = 1;
        c->reljumps[c->nrel].cond = 0;
        c->nrel++;
        int end = c->code_len;
        if (c->reljumps[exit_jmp].target < 0) c->reljumps[exit_jmp].target = end;
        for (int i = 0; i < c->exit_count[ld]; i++) { int eoff = end - (c->exit_pos[ld][i] + 2); uint8_t* pc = c->code + c->exit_pos[ld][i]; pc[0] = (uint8_t)((eoff >> 8) & 0xFF); pc[1] = (uint8_t)(eoff & 0xFF); }
        c->exit_count[ld] = 0;
        c->loop_depth--;
        break;
    }
    case GMB_STMT_GOTO:
        /* GOTO a linea numerata: la riga è ora registrata come label da cc_stmt
           (bas_line, vedi sopra) — stesso identico patch/risoluzione di una label
           testuale, nessun ramo speciale più necessario. */
        cc_byte(c, GMB_OP_GOTO);
        cc_patch_add(c, c->code_len, 0, s->target_label);
        cc_u16(c, 0);
        break;
    case GMB_STMT_GOSUB:
        cc_byte(c, GMB_OP_GOSUB);
        cc_patch_add(c, c->code_len, 0, s->target_label);
        cc_u16(c, 0);
        break;
    case GMB_STMT_LABEL:
        cc_label(c, s->target_label);
        break;
    case GMB_STMT_RETURN: {
        if (s->ret_value) cc_expr(c, s->ret_value);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_syscall(c, 471, 0);
        cc_byte(c, GMB_OP_RETURN);
        break;
    }
    case GMB_STMT_END:
        cc_byte(c, GMB_OP_HALT);
        break;
    case GMB_STMT_REM:
        /* commento / direttiva IMPORT skippata: nessun codice */
        break;
    case GMB_STMT_END_FUNCTION:
        cc_byte(c, GMB_OP_PUSH_NIL);
        cc_byte(c, GMB_OP_RETURN);
        break;
    case GMB_STMT_STOP:
    case GMB_STMT_RUN:
        if (s->run_file) {
            const char* rf = s->run_file;
            size_t rl = strlen(rf);
            if (rl >= 2 && rf[0] == '"' && rf[rl - 1] == '"') { rf++; rl -= 2; }
            char* tmp = (char*)malloc(rl + 1);
            if (tmp) {
                memcpy(tmp, rf, rl); tmp[rl] = 0;
                cc_push_str_const(c, tmp);
                free(tmp);
            }
            cc_byte(c, GMB_OP_SYSCALL);
            cc_u16(c, 237);
            cc_byte(c, 1);
        }
        break;
    case GMB_STMT_DATA: {
        for (int i = 0; i < s->n_data; i++) {
            const gmb_expr_t* d = s->data_values[i];
            if (d->kind == GMB_EXPR_NUM) {
                int j = cc_add_const(c, GMB_CCONST_DOUBLE, d->num, NULL);
                (void)j;
            } else if (d->kind == GMB_EXPR_STR) {
                int j = cc_add_const(c, GMB_CCONST_STRING, 0, d->str ? d->str : "");
                (void)j;
            } else { cc_err(c, "DATA: valore non letterale"); return; }
        }
        break;
    }
    case GMB_STMT_ATFORM: {
        /* @SAY/@GET Clipper: stessi syscall della PCodeVM (AT_SAY=273, AT_GET=274),
         * così la gamba VM/C ha la stessa superficie delle altre (nessuna logica qui:
         * la VM chiama il runtime condiviso gmb_read_add_field_typed/run). */
        int valid_addr = 0, when_addr = 0;
        if (s->act_tk == 2) {
            /* I predicati leggono il valore DURANTE READ. Emettili fuori dal flusso
             * normale e registra l'indirizzo reale del primo opcode, non un offset
             * stimato o una costante placeholder condivisa con un letterale. */
            for (int i = 0; i + 1 < s->n_control_args; i += 2) {
                const gmb_expr_t* k = s->control_args[i];
                if (!k || !k->var) continue;
                int* addr = strcasecmp(k->var, "VALID") == 0 ? &valid_addr :
                            (strcasecmp(k->var, "WHEN") == 0 ? &when_addr : NULL);
                if (!addr || *addr) continue;
                cc_byte(c, GMB_OP_JUMP_FORWARD);
                int skip_pos = c->code_len; cc_u16(c, 0);
                *addr = c->code_len;
                cc_expr(c, s->control_args[i + 1]);
                if (c->error) return;
                cc_byte(c, GMB_OP_END_VALID);
                int offset = c->code_len - (skip_pos + 2);
                if (offset > UINT16_MAX) { cc_err(c, "predicato @GET troppo lungo"); return; }
                c->code[skip_pos] = (uint8_t)(offset >> 8);
                c->code[skip_pos + 1] = (uint8_t)offset;
            }
        }
        if (s->act_tk == 1) {                    /* SAY */
            if (s->win_coords[0]) cc_expr(c, s->win_coords[0]); else cc_byte(c, GMB_OP_PUSH_NIL);
            if (s->win_coords[1]) cc_expr(c, s->win_coords[1]); else cc_byte(c, GMB_OP_PUSH_NIL);
            if (s->on_expr) cc_expr(c, s->on_expr); else cc_push_str_const(c, "");
            cc_syscall(c, 273, 3);
        } else if (s->act_tk == 2 && s->n_read >= 1 && s->read_vars && s->read_vars[0]) {
            const char* vn = s->read_vars[0];
            if (s->win_coords[0]) cc_expr(c, s->win_coords[0]); else cc_byte(c, GMB_OP_PUSH_NIL);
            if (s->win_coords[1]) cc_expr(c, s->win_coords[1]); else cc_byte(c, GMB_OP_PUSH_NIL);
            {   /* target READ: BYREF per locali/globali, nome per DYNAMIC */
                int l = cc_local(c, vn);
                if (l >= 0) { cc_byte(c, GMB_OP_MAKE_BYREF); cc_u16(c, l); }
                else if (cc_dyn_is(c, vn)) cc_push_str_const(c, vn);
                else { int g = cc_global(c, vn, 1);
                       if (g >= 0) { cc_byte(c, GMB_OP_MAKE_BYREF_GLOBAL); cc_u16(c, g); }
                       else cc_byte(c, GMB_OP_PUSH_NIL); }
            }
            if (s->on_expr) cc_expr(c, s->on_expr); else cc_byte(c, GMB_OP_PUSH_NIL);   /* PICTURE */
            if (valid_addr) { int ci = cc_add_const(c, GMB_CCONST_DOUBLE, (double)valid_addr, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ci); }
            else cc_byte(c, GMB_OP_PUSH_NIL);
            {   /* RANGE lo, hi */
                int found_lo = 0, found_hi = 0;
                for (int i = 0; i + 1 < s->n_control_args; i += 2) {
                    const gmb_expr_t* k = s->control_args[i];
                    if (!k || !k->var) continue;
                    if (strcasecmp(k->var, "MIN") == 0) { cc_expr(c, s->control_args[i + 1]); found_lo = 1; }
                    if (strcasecmp(k->var, "MAX") == 0) { /* differito sotto */ }
                }
                if (!found_lo) cc_byte(c, GMB_OP_PUSH_NIL);
                int found_hi2 = 0;
                for (int i = 0; i + 1 < s->n_control_args; i += 2) {
                    const gmb_expr_t* k = s->control_args[i];
                    if (k && k->var && strcasecmp(k->var, "MAX") == 0) { cc_expr(c, s->control_args[i + 1]); found_hi2 = 1; }
                }
                (void)found_hi;
                if (!found_hi2) cc_byte(c, GMB_OP_PUSH_NIL);
            }
            cc_byte(c, GMB_OP_PUSH_NIL);                                                 /* COLOR */
            if (s->channel) cc_expr(c, s->channel); else cc_byte(c, GMB_OP_PUSH_NIL);     /* TYPE */
            {   int pushed = 0;
                for (int i = 0; i + 1 < s->n_control_args; i += 2) {
                    const gmb_expr_t* k = s->control_args[i];
                    if (k && k->var && strcasecmp(k->var, "ITEMS") == 0) { cc_expr(c, s->control_args[i + 1]); pushed = 1; break; }
                }
                if (!pushed) cc_byte(c, GMB_OP_PUSH_NIL);                                 /* ITEMS */
            }
            if (s->win_coords[2]) cc_expr(c, s->win_coords[2]); else cc_byte(c, GMB_OP_PUSH_NIL);  /* w */
            if (s->win_coords[3]) cc_expr(c, s->win_coords[3]); else cc_byte(c, GMB_OP_PUSH_NIL);  /* h */
            cc_byte(c, GMB_OP_PUSH_NIL);                                                 /* CAPTION */
            {   const gmb_expr_t* e = NULL;
                for (int i = 0; i + 1 < s->n_control_args; i += 2) { const gmb_expr_t* k = s->control_args[i];
                    if (k && k->var && strcasecmp(k->var, "STEP") == 0) { e = s->control_args[i + 1]; break; } }
                if (e) cc_expr(c, e); else cc_byte(c, GMB_OP_PUSH_NIL); }
            cc_byte(c, GMB_OP_PUSH_NIL);                                                 /* MIN */
            cc_byte(c, GMB_OP_PUSH_NIL);                                                 /* MAX */
            if (when_addr) { int ci = cc_add_const(c, GMB_CCONST_DOUBLE, (double)when_addr, NULL);
                cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, ci); }
            else cc_byte(c, GMB_OP_PUSH_NIL);
            cc_syscall(c, 274, 17);
            /* MESSAGE/ERROR (Clipper): testo di aiuto/errore del campo. Syscall separate
             * (481/482) per non cambiare l'arità di AT_GET; agiscono sull'ultimo campo. */
            for (int pass = 0; pass < 2; pass++) {
                const char* key = pass == 0 ? "MESSAGE" : "ERROR";
                const gmb_expr_t* e = NULL;
                for (int i = 0; i + 1 < s->n_control_args; i += 2) {
                    const gmb_expr_t* k = s->control_args[i];
                    if (k && k->var && strcasecmp(k->var, key) == 0) { e = s->control_args[i + 1]; break; }
                }
                if (!e) continue;
                cc_expr(c, e);
                if (c->error) return;
                cc_syscall(c, pass == 0 ? 481 : 482, 1);
            }
        }
        break;
    }
    case GMB_STMT_READ: {
        if (s->n_read == 0) { cc_syscall(c, 275, 0); break; }   /* READ Clipper (dopo @GET) */
        for (int i = 0; i < s->n_read; i++) {
            cc_byte(c, GMB_OP_READ);
            cc_store_target(c, s->read_vars[i], s->line);
        }
        break;
    }
    case GMB_STMT_RESTORE:
        cc_byte(c, GMB_OP_RESTORE);
        break;
    case GMB_STMT_SELECT: {
        cc_expr(c, s->cond);
        if (c->error) return;
        int nrel_rel_start = c->nrel;
        int end_pos = -1;
        /* ogni case: DUP switch, test, CMP_EQ, JUMP_IF_FALSE -> next; body; JUMP_FORWARD -> end */
        for (int ci = 0; ci < s->n_select_cases; ci++) {
            const gmb_stmt_t* cs = s->select_cases[ci];
            if (cs->case_else) {
                cc_block(c, cs->case_body, cs->n_case_body);
                cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);
                continue;
            }
            for (int t = 0; t < cs->n_case_tests; t++) {
                cc_byte(c, GMB_OP_DUP);
                cc_expr(c, cs->case_tests[t]);
                if (c->error) return;
                cc_byte(c, GMB_OP_CMP_EQ);
                cc_reljump(c, GMB_OP_JUMP_IF_FALSE, -1);
                int test_jmp = c->nrel - 1;
                cc_block(c, cs->case_body, cs->n_case_body);
                cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);
                /* patch il jump-if-false di questo test all'inizio del test successivo
                   (per indice catturato: il body del case può spingere altri reljump) */
                int nt = c->code_len;
                if (c->reljumps[test_jmp].target < 0) c->reljumps[test_jmp].target = nt;
                end_pos = -1;   /* segnale generico */
            }
        }
        cc_byte(c, GMB_OP_POP);   /* rimuove lo switch */
        for (int i = nrel_rel_start; i < c->nrel; i++)
            if (c->reljumps[i].target < 0) c->reljumps[i].target = c->code_len;
        break;
    }
    case GMB_STMT_SEND:
        cc_expr(c, s->send_value);
        cc_expr(c, s->channel);
        if (c->error) return;
        cc_byte(c, GMB_OP_SEND);
        break;
    case GMB_STMT_RECEIVE: {
        cc_expr(c, s->channel);
        if (c->error) return;
        if (s->ok_flag) {
            /* RECV_OK pusha (valore, flag) con flag in cima (specchio C#: prima lo
               store del flag in ok_flag, poi il valore nella destinazione). */
            cc_byte(c, GMB_OP_RECV_OK);
            cc_store_target(c, s->ok_flag->var ? s->ok_flag->var : "_", s->line);
            cc_store_target(c, s->receive_dest, s->line);
        } else {
            cc_byte(c, GMB_OP_RECV);
            cc_store_target(c, s->receive_dest, s->line);
        }
        break;
    }
    case GMB_STMT_WAIT:
        cc_expr(c, s->channel); if (c->error) return;
        cc_byte(c, GMB_OP_WG_WAIT);
        break;
    case GMB_STMT_DONE:
        cc_expr(c, s->channel); if (c->error) return;
        cc_byte(c, GMB_OP_WG_DONE);
        break;
    case GMB_STMT_SLEEP:
        cc_expr(c, s->channel); if (c->error) return;
        cc_byte(c, GMB_OP_SLEEP);
        break;
    case GMB_STMT_CHANNEL:
        cc_expr(c, s->channel); if (c->error) return;
        cc_byte(c, GMB_OP_CHANNEL_CLOSE);
        break;
    case GMB_STMT_MUTEX:
        cc_expr(c, s->channel); if (c->error) return;
        if (s->mutex_action && strcasecmp(s->mutex_action, "RLOCK") == 0) cc_byte(c, GMB_OP_RLOCK);
        else if (s->mutex_action && strcasecmp(s->mutex_action, "RUNLOCK") == 0) cc_byte(c, GMB_OP_RUNLOCK);
        else if (s->mutex_action && strcasecmp(s->mutex_action, "UNLOCK") == 0) cc_byte(c, GMB_OP_UNLOCK);
        else cc_byte(c, GMB_OP_LOCK);
        break;
    case GMB_STMT_COND:
        if (s->mutex_action && strcasecmp(s->mutex_action, "WAIT") == 0) {
            cc_expr(c, s->channel);
            cc_expr(c, s->cond_mutex);
            if (c->error) return;
            cc_byte(c, GMB_OP_COND_WAIT);
        } else {
            cc_expr(c, s->channel); if (c->error) return;
            cc_byte(c, strcasecmp(s->mutex_action, "SIGNAL") == 0 ? GMB_OP_COND_SIGNAL : GMB_OP_COND_BROADCAST);
        }
        break;
    case GMB_STMT_GOEXIT:
        cc_byte(c, GMB_OP_GO_EXIT);
        break;
    case GMB_STMT_ONCE: {
        /* ONCE RUN <once>, <proc>: layout identico a C# VisitOnceStatement.
           [push once] [OP_ONCE_BEGIN][skip u16 abs] [OP_CALL_N][addr u16][argc=0] [OP_ONCE_END] */
        cc_expr(c, s->channel);
        if (c->error) return;
        int begin_pos = c->code_len;
        cc_byte(c, GMB_OP_ONCE_BEGIN);
        cc_u16(c, 0);
        cc_byte(c, GMB_OP_CALL_N);
        int call_pos = c->code_len;
        cc_u16(c, 0);
        cc_patch_add(c, call_pos, 1, s->proc_name && s->proc_name->var ? s->proc_name->var : "");
        cc_byte(c, 0);   /* arity 0 (sync.Once.Do esegue f() senza argomenti) */
        cc_byte(c, GMB_OP_ONCE_END);
        int skip_addr = c->code_len + c->code_base;
        c->code[begin_pos + 1] = (uint8_t)((skip_addr >> 8) & 0xFF);
        c->code[begin_pos + 2] = (uint8_t)(skip_addr & 0xFF);
        break;
    }
    case GMB_STMT_DEFER: {
        /* DEFER: layout identico a C# VisitDeferStatement.
           [OP_DEFER][bodyAddr u16 abs] [OP_JUMP_FORWARD][rel] body [OP_DEFER_RET] */
        int defer_pos = c->code_len;
        cc_byte(c, GMB_OP_DEFER);
        cc_u16(c, 0);
        cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);   /* salta il body nel flusso normale */
        int body_addr = c->code_len;
        cc_stmt(c, s->go_comm);
        if (c->error) return;
        cc_byte(c, GMB_OP_DEFER_RET);
        c->code[defer_pos + 1] = (uint8_t)(((body_addr + c->code_base) >> 8) & 0xFF);
        c->code[defer_pos + 2] = (uint8_t)(body_addr & 0xFF);
        if (c->nrel > 0 && c->reljumps[c->nrel - 1].target < 0)
            c->reljumps[c->nrel - 1].target = c->code_len;
        break;
    }
    case GMB_STMT_CLS: {
        /* CLS [color] → [push color|nil] OP_SYSCALL SysCall.CLS(0) argc=1 */
        cc_byte(c, GMB_OP_PUSH_NIL);
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 0);
        cc_byte(c, 1);
        break;
    }
    case GMB_STMT_LOCATE: {
        if (s->row) cc_expr(c, s->row);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (s->col) cc_expr(c, s->col);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 1);
        cc_byte(c, 2);
        break;
    }
    case GMB_STMT_COLOR: {
        if (s->fg) cc_expr(c, s->fg);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (s->bg) cc_expr(c, s->bg);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 3);
        cc_byte(c, 2);
        break;
    }
    case GMB_STMT_WAITKEY: {
        /* WAITKEY: [push niente] OP_SYSCALL code=8 argc=0 → VM/C vm_console_7 park-aware.
         * Il risultato (tasto, scartato) resta in cima → OP_POP. */
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 8);
        cc_byte(c, 0);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_GETKEY: {
        /* GETKEY var: [push niente] OP_SYSCALL code=7 argc=0 → pusha il tasto → store. */
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 7);
        cc_byte(c, 0);
        if (s->target) cc_store_target(c, s->target, s->line);
        else cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_CAPTION: {
        /* CAPTION <expr> → [push titolo|NIL] OP_SYSCALL code=321 argc=1 (vm_console_321).
         * Titolo in on_expr (stesso riuso di SENDKEY). */
        if (s->on_expr) cc_expr(c, s->on_expr);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_syscall(c, 321, 1);
        break;
    }
    case GMB_STMT_SAVESCREEN: {
        /* SAVE SCREEN [nome] → [push nome|NIL] OP_SYSCALL code=230 argc=1. */
        if (s->on_expr) cc_expr(c, s->on_expr);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_syscall(c, 230, 1);
        break;
    }
    case GMB_STMT_RESTORESCREEN: {
        /* RESTORE SCREEN [nome] → [push nome|NIL] OP_SYSCALL code=231 argc=1. */
        if (s->on_expr) cc_expr(c, s->on_expr);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_syscall(c, 231, 1);
        break;
    }
    case GMB_STMT_SCROLLMODE: {
        /* SCROLLMODE [mode] → [push mode|NIL] OP_SYSCALL code=323 argc=1 (vm_console_323). */
        if (s->on_expr) cc_expr(c, s->on_expr);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_syscall(c, 323, 1);
        break;
    }
    case GMB_STMT_INKEY: {
        /* INKEY [var][, timeout]: [timeout|NIL] OP_SYSCALL code=6 argc=1 → pusha il tasto
         * (o "") → store/POP. Semantica: senza timeout non-bloccante (vm_console_6 → inkекy),
         * con timeout attesa limitata. */
        if (s->channel) cc_expr(c, s->channel);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 6);
        cc_byte(c, 1);
        if (s->target) cc_store_target(c, s->target, s->line);
        else cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_SENDKEY: {
        if (s->on_expr) cc_expr(c, s->on_expr);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN);
        /* 3614: era 1408 = FROMBASE64URL (BuiltInFunction) → collisione con l'handler crypto. */
        cc_u16(c, 3614);
        cc_byte(c, 1);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_INPUT: {
        /* INPUT ["prompt";] var[, var...]: push prompt|NIL → OP_SYSCALL code=4 argc=nVars
         * (vm_console_4: stampa il prompt, legge la riga, splitta per virgola e pusha nVars
         * campi in ordine). Poi per ogni variabile (dall'ultima in cima allo stack alla prima,
         * come il backend IL/bytecode C#): stringa ($) → store diretto; numerica → VAL
         * (func_code 1) converte string→double e store. */
        if (s->input_prompt) cc_expr(c, s->input_prompt);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 4);
        cc_byte(c, (uint8_t)(s->n_read > 255 ? 255 : s->n_read));
        for (int i = s->n_read - 1; i >= 0 && !c->error; i--) {
            const char* nm = s->read_vars[i];
            if (nm && *nm) {
                char last = nm[strlen(nm) - 1];
                if (last != '$') { /* variabile numerica: converti il campo (stringa) in double */
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 1); cc_byte(c, 1); /* VAL */
                }
                cc_store_target(c, nm, s->line);
            } else cc_byte(c, GMB_OP_POP);
        }
        break;
    }
    case GMB_STMT_LINEINPUT: {
        /* LINE INPUT console ["prompt";] var$ (sintassi 'LINE INPUT', GMB_STMT_LINEINPUT senza
         * file_num): push prompt|NIL → OP_SYSCALL code=5 argc=1 (vm_console_5: stampa prompt,
         * legge la riga, pusha). La variabile riceve la riga intera (stringa → store diretto;
         * numerica → VAL). Il LINE INPUT da FILE (LINEINPUT #n, var → file_num) resta no-op. */
        if (s->file_num) break;
        if (s->input_prompt) cc_expr(c, s->input_prompt);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 5);
        cc_byte(c, 1);
        if (s->n_read > 0 && s->read_vars[0] && s->read_vars[0][0]) {
            const char* nm = s->read_vars[0];
            char last = nm[strlen(nm) - 1];
            if (last != '$') { cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 1); cc_byte(c, 1); /* VAL */ }
            cc_store_target(c, nm, s->line);
        } else cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_GOSELECT: {
        /* GO SELECT: push send_values, poi i slot (canale/durata) per ogni case, GO_SELECT_SETUP */
        int num_sends = 0;
        for (int i = 0; i < s->n_go_cases; i++) {
            gmb_go_case_t* cs = &s->go_cases[i];
            if (cs->comm && cs->comm->kind == GMB_STMT_SEND) num_sends++;
        }
        /* 1. valori SEND */
        for (int i = 0; i < s->n_go_cases; i++) {
            gmb_go_case_t* cs = &s->go_cases[i];
            if (cs->comm && cs->comm->kind == GMB_STMT_SEND && cs->comm->send_value) cc_expr(c, cs->comm->send_value);
        }
        /* 2. slot per ogni case (canale o durata TIMEOUT) */
        for (int i = 0; i < s->n_go_cases; i++) {
            gmb_go_case_t* cs = &s->go_cases[i];
            if (cs->comm && cs->comm->kind == GMB_STMT_TIMEOUT) { if (cs->comm->channel) cc_expr(c, cs->comm->channel); else cc_byte(c, GMB_OP_PUSH_NIL); }
            else if (cs->comm && cs->comm->kind == GMB_STMT_RECEIVE && cs->comm->channel) cc_expr(c, cs->comm->channel);
            else if (cs->comm && cs->comm->kind == GMB_STMT_SEND && cs->comm->channel) cc_expr(c, cs->comm->channel);
            else cc_byte(c, GMB_OP_PUSH_NIL);
        }
        if (c->error) return;
        /* 3. GO_SELECT_SETUP num_cases u16, num_sends u16, default_off u16, case_addr u16 × num_cases */
        cc_byte(c, GMB_OP_GO_SELECT_SETUP);
        cc_u16(c, s->n_go_cases);
        cc_u16(c, num_sends);
        int defpos = c->code_len;
        cc_u16(c, 0xFFFF);   /* sentinel C# Compiler.GoRoutines.cs: nessun DEFAULT */
        int* cpos = (int*)calloc((size_t)(s->n_go_cases > 0 ? s->n_go_cases : 1), sizeof(int));
        for (int i = 0; i < s->n_go_cases; i++) { cpos[i] = c->code_len; cc_u16(c, 0); }
        int* cendjmp = (int*)calloc((size_t)(s->n_go_cases > 0 ? s->n_go_cases : 1), sizeof(int));
        /* 4. corpi dei case */
        for (int i = 0; i < s->n_go_cases; i++) {
            gmb_go_case_t* cs = &s->go_cases[i];
            /* patch indirizzo del case */
            c->code[cpos[i]] = (uint8_t)(((c->code_len + c->code_base) >> 8) & 0xFF);
            c->code[cpos[i] + 1] = (uint8_t)((c->code_len + c->code_base) & 0xFF);
            if (cs->comm && cs->comm->kind == GMB_STMT_RECEIVE && cs->comm->receive_dest) {
                /* RECEIVE dest: registra la destinazione come LOCAL (parità C#
                   AssignVariable→AddLocal). Prima si provava cc_local/cc_global:
                   con dest non DIMmato lo slot era sbagliato e il valore ricevuto
                   finiva in un altro slot (bug: TOTAL=0 in SelectLoserCleanup). */
                int slot = cc_local_add(c, cs->comm->receive_dest);
                cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, slot);
            }
            cc_block(c, cs->body, cs->n_body);
            if (c->error) return;
            cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);
            cendjmp[i] = c->nrel - 1;
        }
        /* 5. default case (comm == NULL): patcha defpos SOLO se esiste davvero */
        int has_default = 0;
        for (int i = 0; i < s->n_go_cases; i++) if (!s->go_cases[i].comm) { has_default = 1; break; }
        if (has_default) {
            int def_addr = c->code_len + c->code_base;
            c->code[defpos] = (uint8_t)((def_addr >> 8) & 0xFF);
            c->code[defpos + 1] = (uint8_t)(def_addr & 0xFF);
            for (int i = 0; i < s->n_go_cases; i++) {
                gmb_go_case_t* cs = &s->go_cases[i];
                if (!cs->comm) { cc_block(c, cs->body, cs->n_body); break; }
            }
        }
        /* patch i jump dei case alla fine (per indice: il body del default può
           spingere altri reljump e invalidare il riferimento posizionale) */
        for (int i = 0; i < s->n_go_cases; i++)
            if (c->reljumps[cendjmp[i]].target < 0) c->reljumps[cendjmp[i]].target = c->code_len;
        free(cendjmp);
        free(cpos);
        break;
    }
    case GMB_STMT_GO: {
        uint64_t gobm = cc_byref_mask(c, s->proc_name ? s->proc_name->var : NULL);
        for (int i = 0; i < s->n_go_args; i++) cc_call_arg(c, gobm, i, s->go_args[i]);
        if (s->go_wait) cc_expr(c, s->go_wait);
        else cc_byte(c, GMB_OP_PUSH_NIL);   /* placeholder wg: OP_GO popa la cima come waitgroup */
        if (c->error) return;
        if (s->flag && s->go_into) {
            /* GO FUNCTION ... INTO target (parità C# VisitGoFunctionExpression +
               VisitLetStatement): OP_GO_FUNCTION lascia il future sullo stack,
               poi STORE nel target. Lettura del target = implicit-await (VM/C). */
            cc_byte(c, GMB_OP_GO_FUNCTION);
            int pos = c->code_len;
            cc_u16(c, 0);
            cc_patch_add(c, pos, 1, s->proc_name ? s->proc_name->var : "");
            cc_store_target(c, s->go_into, s->line);
        } else {
            cc_byte(c, GMB_OP_GO);
            int pos = c->code_len;
            cc_u16(c, 0);
            cc_patch_add(c, pos, 1, s->proc_name ? s->proc_name->var : "");
        }
        break;
    }
    case GMB_STMT_EXIT: {
        const char* et = s->exit_target;
        int is_loop_exit = et && (strcasecmp(et, "FOR") == 0 || strcasecmp(et, "WHILE") == 0
                                 || strcasecmp(et, "DO") == 0 || strcasecmp(et, "TICK") == 0);
        if (is_loop_exit && c->loop_depth > 0) {
            int l = c->loop_depth - 1;
            cc_byte(c, GMB_OP_JUMP_FORWARD);
            int pos = c->code_len;
            cc_u16(c, 0);
            if (c->exit_count[l] < 64) c->exit_pos[l][c->exit_count[l]++] = pos;
        } else {
            /* EXIT SUB / EXIT FUNCTION (o EXIT nudo fuori da un loop): termina la
             * procedura. Per una FUNCTION restituisce il return-slot assegnato con
             * `NomeFunzione = ...`; prima il nodo veniva ignorato e il flusso
             * continuava (EXIT FUNCTION non usciva). */
            int l = c->current_proc_name ? cc_local(c, c->current_proc_name) : -1;
            if (l >= 0) { cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, l); }
            else cc_byte(c, GMB_OP_PUSH_NIL);
            cc_syscall(c, 471, 0);
            cc_byte(c, GMB_OP_RETURN);
        }
        break;
    }
    case GMB_STMT_DOMAIN: {
        /* MODEL COMPILER LLM (2026-09-23): i verbi LLM PLAN/POLICY/QUANT/MERGE/PRUNE/EXTRACT/
         * PROFILE/ARCH sono eseguibili a RUNTIME anche nel VM/C (non solo in C99Native/
         * emitter C#). Emette CALL_BUILTIN 3604..3611: il VM/C li dispatcha al runtime CONDIVISO
         * gmb_runtime_c (llmmodelio_model_*), la stessa sorgente degli altri backend. Verbo non
         * gestito → errore ESPLICITO, mai no-op silenzioso (§0bis). */
        /* TRAIN (F0, additivo): namespace PUL per la produzione/alterazione pesi. Il dispatcher
         * DELEGA ai verbi dei namespace AI/LLM; i verbi supportati dal compiler chunk (QUALITY)
         * ricevono lo stesso codice, gli altri l'errore ESPLICITO (mai no-op). */
        if (s->win_name && (strcasecmp(s->win_name, "LLM") == 0 || strcasecmp(s->win_name, "TRAIN") == 0)) {
            const char* v = s->mutex_action ? s->mutex_action : "";
            int is_train = (strcasecmp(s->win_name, "TRAIN") == 0);
            int llm_code = 0;
            int llm_first = 0;   /* primo go_arg da spingere (LORA: salta il sotto-verbo APPLY) */
            int llm_print = 0;   /* TRAIN EVAL GATE: comando-query → stampa il verdetto (§1) */
            /* TRAIN RUNS/CONFIG/STATUS/EXPORT/STOP: comandi §4 gestiti dal blocco dedicato più
             * sotto (non sono verbi del compiler chunk) → non far scattare l'errore qui. */
            int train_subcmd = is_train && (strcasecmp(v, "RUNS") == 0 || strcasecmp(v, "CONFIG") == 0
                    || strcasecmp(v, "STATUS") == 0 || strcasecmp(v, "EXPORT") == 0
                    || strcasecmp(v, "STOP") == 0);
            /* [HARD MOVE TRAIN 2026-10-02] Il training/produzione-pesi vive nel namespace TRAIN.
             * LLM = model compiler (PLAN/POLICY/QUANT/MERGE/PRUNE/EXTRACT/PROFILE/ARCH/COMPILE);
             * TRAIN = FINETUNE->1993 (model, json), LORA->1994 (ckpt, json), QUALITY/EVAL->3628. */
            if (is_train) {
                if (strcasecmp(v, "FINETUNE") == 0 || strcasecmp(v, "RESUME") == 0) { llm_code = 1993; llm_first = 0; }
                else if (strcasecmp(v, "DPO") == 0) { llm_code = 2308; llm_first = 0; }
                else if (strcasecmp(v, "GRPO") == 0) { llm_code = 2314; llm_first = 0; }
                else if (strcasecmp(v, "LORA") == 0) { llm_code = 1994; llm_first = 1; }
                else if (strcasecmp(v, "QUALITY") == 0) llm_code = 3628;
                else if (strcasecmp(v, "EVAL") == 0) {
                    /* TRAIN EVAL GATE <ref>, <cand>: gate DEDICATO → builtin 2311, con stampa. */
                    if (s->n_go_args >= 1 && s->go_args[0] && s->go_args[0]->kind == GMB_EXPR_VAR
                        && s->go_args[0]->var && strcasecmp(s->go_args[0]->var, "GATE") == 0) {
                        llm_code = 2311; llm_first = 1; llm_print = 1;
                    } else llm_code = 3628;
                }
            } else {
                if (strcasecmp(v, "PLAN") == 0) llm_code = 3615;
                else if (strcasecmp(v, "POLICY") == 0) llm_code = 3616;
                else if (strcasecmp(v, "QUANT") == 0) llm_code = 3617;
                else if (strcasecmp(v, "MERGE") == 0) llm_code = 3618;
                else if (strcasecmp(v, "PRUNE") == 0) llm_code = 3619;
                else if (strcasecmp(v, "EXTRACT") == 0) llm_code = 3620;
                else if (strcasecmp(v, "PROFILE") == 0) llm_code = 3621;
                else if (strcasecmp(v, "ARCH") == 0) llm_code = 3622;
                else if (strcasecmp(v, "COMPILE") == 0) llm_code = 3627;
            }
            if (llm_code) {
                int llm_max = (llm_code == 1993 || llm_code == 1994 || llm_code == 2308 || llm_code == 2314) ? 2 : (s->n_go_args - llm_first);
                int pushed = 0;
                for (int li = llm_first; li < s->n_go_args && pushed < llm_max; li++) { cc_expr(c, s->go_args[li]); if (c->error) return; pushed++; }
                cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, llm_code); cc_byte(c, (uint8_t)pushed);
                cc_byte(c, llm_print ? GMB_OP_PRINT_LINE : GMB_OP_POP);
                break;
            }
            if (train_subcmd) { /* gestito dal blocco §4 più sotto: non errore, non break */ }
            else { char buf[160];
              snprintf(buf, sizeof(buf), "%s '%s': verbo non supportato dal compilatore chunk", s->win_name, v);
              cc_err(c, buf);
              break; }
        }
        /* TRAIN RUNS|CONFIG|STATUS (§4): comandi di registro/configurazione che mappano alle
         * funzioni LLM (stessi codici del C#). go_args[0] = sotto-verbo (LIST/SHOW/PRUNE/SET). */
        if (s->win_name && strcasecmp(s->win_name, "TRAIN") == 0 && s->mutex_action) {
            const char* v = s->mutex_action;
            int code = 0, first = 1, max = 0;
            const char* sub = (s->n_go_args >= 1 && s->go_args[0] && s->go_args[0]->kind == GMB_EXPR_VAR)
                              ? s->go_args[0]->var : "";
            if (strcasecmp(v, "RUNS") == 0) {
                if (strcasecmp(sub, "LIST") == 0) { code = 2300; max = 0; }
                else if (strcasecmp(sub, "PRUNE") == 0) { code = 2302; max = 0; }
                else if (strcasecmp(sub, "SHOW") == 0) { code = 2305; max = 1; }
            } else if (strcasecmp(v, "CONFIG") == 0) {
                if (strcasecmp(sub, "SHOW") == 0) { code = 2303; max = 0; }
                else if (strcasecmp(sub, "SET") == 0) { code = 2304; max = 2; }
            } else if (strcasecmp(v, "STATUS") == 0) {
                first = 0;
                if (s->n_go_args >= 1) { code = 2305; max = 1; } else { code = 2300; max = 0; }
            } else if (strcasecmp(v, "LORA") == 0 && strcasecmp(sub, "INFO") == 0) {
                code = 2307; max = 0;
            } else if (strcasecmp(v, "EXPORT") == 0) {
                code = 2309; first = 0; max = 2;   /* TRAIN EXPORT <out> | <run>, <out> */
            } else if (strcasecmp(v, "STOP") == 0) {
                code = 2310; first = 0; max = 1;
            }
            if (code) {
                int pushed = 0;
                for (int i = first; i < s->n_go_args && pushed < max; i++) { cc_expr(c, s->go_args[i]); if (c->error) return; pushed++; }
                cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, code); cc_byte(c, (uint8_t)pushed);
                /* Comandi-query §4: il comando AGISCE e STAMPA il risultato (§1) — parità con
                 * il parser C# che emette `PRINT LLM.<fn>(...)`. */
                cc_byte(c, GMB_OP_PRINT_LINE);
                break;
            }
        }
        /* comando namespace: mappa (ns, verbo) -> syscall; argomenti spinti, POP di bilancio */
        static const struct { const char* ns; const char* v; int code; } dmap[] = {
            {"SQL","OPEN",185},{"SQL","EXEC",187},{"SQL","QUERY",188},{"SQL","CLOSE",186},
            {"SQL","BEGIN",191},{"SQL","COMMIT",192},{"SQL","ROLLBACK",193},{"SQL","SAVEPOINT",194},
            {"SQL","RELEASE",195},{"SQL","ATTACH",197},{"SQL","DETACH",198},{"SQL","BACKUP",199},
            {"SQL","FETCH",189},
            {"ISAM","USE",65},{"ISAM","OPEN",65},{"ISAM","APPEND",76},{"ISAM","SKIP",68},{"ISAM","SEEK",69},
            {"ISAM","DELETE",70},{"ISAM","RECALL",71},{"ISAM","PACK",72},{"ISAM","REINDEX",73},
            {"ISAM","REPAIR",74},{"ISAM","BACKUP",75},{"ISAM","CLOSE",75},
            {"RDD","USE",32},{"RDD","UNLOCK",35},{"RDD","RLOCK",33},{"RDD","FLOCK",34},
            {"RDD","SET",31},{"RDD","ON",40},{"RDD","OFF",41},{"RDD","CREATE",53},{"RDD","CLOSE",54},
            {"DBF","USE",105},{"DBF","CLOSE",119},{"DBF","GO",116},{"DBF","SKIP",117},
            {"SPEED","OPEN",151},{"SPEED","USE",152},{"SPEED","PUT",156},{"SPEED","GET",155},
        };
        int code = -1;
        /* DBF/ISAM SET PATH|DEFAULT [TO] <path>: syscall 145/144 con SOLO il path (parità C#).
           go_args = [var(PATH|DEFAULT), var(TO)?, pathExpr]; il path è l'ultimo termine non-var("TO"). */
        if (s->win_name && s->mutex_action && strcasecmp(s->mutex_action, "SET") == 0
            && (strcasecmp(s->win_name, "DBF") == 0 || strcasecmp(s->win_name, "ISAM") == 0)
            && s->n_go_args >= 1 && s->go_args[0]->kind == GMB_EXPR_VAR) {
            const char* sub = s->go_args[0]->var;
            if (sub && (strcasecmp(sub, "PATH") == 0 || strcasecmp(sub, "DEFAULT") == 0)) {
                int path_idx = -1;
                for (int gi = s->n_go_args - 1; gi >= 1; gi--) {
                    const char* vn = (s->go_args[gi]->kind == GMB_EXPR_VAR) ? s->go_args[gi]->var : NULL;
                    if (vn && (strcasecmp(vn, "TO") == 0 || strcasecmp(vn, "PATH") == 0 || strcasecmp(vn, "DEFAULT") == 0)) continue;
                    path_idx = gi; break;
                }
                if (path_idx >= 1) cc_expr(c, s->go_args[path_idx]);
                else cc_byte(c, GMB_OP_PUSH_NIL);   /* SET PATH TO (vuoto) → clear */
                if (c->error) return;
                cc_byte(c, GMB_OP_SYSCALL);
                cc_u16(c, strcasecmp(sub, "PATH") == 0 ? 145 : 144);
                cc_byte(c, 1);
                break;
            }
        }
        /* AI VAULT/NOTE (Second Brain L2): i comandi del domain-parser mappano sui builtin
         * del runtime gmb_secondbrain (CALL_BUILTIN + POP: comando senza valore). go_args[0] =
         * sotto-comando; per NOTE il vault è "" (vault attivo, parità C99/C#). */
        if (s->win_name && s->mutex_action && strcasecmp(s->win_name, "AI") == 0) {
            const char* verb = s->mutex_action;
            int vsub = (s->n_go_args >= 1 && s->go_args[0]->kind == GMB_EXPR_VAR) ? 1 : 0;
            if (strcasecmp(verb, "VAULT") == 0 && vsub && s->go_args[0]->var) {
                const char* sub = s->go_args[0]->var;
                int code = -1;
                if (strcasecmp(sub, "CREATE") == 0 && s->n_go_args >= 3) code = 1272;
                else if (strcasecmp(sub, "INDEX") == 0 && s->n_go_args >= 2) code = 1273;
                if (code >= 0) {
                    for (int i = 1; i < s->n_go_args; i++) cc_expr(c, s->go_args[i]);
                    if (c->error) return;
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, code); cc_byte(c, (uint8_t)(s->n_go_args - 1));
                    cc_byte(c, GMB_OP_POP);
                    break;
                }
            }
            if (strcasecmp(verb, "NOTE") == 0 && vsub && s->go_args[0]->var) {
                const char* sub = s->go_args[0]->var;
                #define CC_PUSH_EMPTY_STR() do { int _si = cc_add_const(c, GMB_CCONST_STRING, 0, ""); \
                    if (c->error) return; cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, _si); } while (0)
                if (strcasecmp(sub, "CREATE") == 0 && s->n_go_args >= 2) {
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    if (s->n_go_args >= 3) { cc_expr(c, s->go_args[2]); if (c->error) return; }
                    else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (s->n_go_args >= 4) { cc_expr(c, s->go_args[3]); if (c->error) return; }
                    else cc_byte(c, GMB_OP_PUSH_NIL);
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 1301); cc_byte(c, 3); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if ((strcasecmp(sub, "UPDATE") == 0 || strcasecmp(sub, "APPEND") == 0) && s->n_go_args >= 3) {
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    cc_expr(c, s->go_args[2]); if (c->error) return;
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, strcasecmp(sub, "UPDATE") == 0 ? 1451 : 1452); cc_byte(c, 2); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (strcasecmp(sub, "DELETE") == 0 && s->n_go_args >= 2) {
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 1304); cc_byte(c, 1); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (strcasecmp(sub, "MOVE") == 0 && s->n_go_args >= 3) {
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    cc_expr(c, s->go_args[2]); if (c->error) return;
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 1305); cc_byte(c, 2); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (strcasecmp(sub, "LINK") == 0 && s->n_go_args >= 4) {
                    CC_PUSH_EMPTY_STR();
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    cc_expr(c, s->go_args[3]); if (c->error) return;
                    if (s->n_go_args >= 6) { cc_expr(c, s->go_args[5]); if (c->error) return; }
                    else cc_byte(c, GMB_OP_PUSH_NIL);
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 1344); cc_byte(c, 4); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (strcasecmp(sub, "UNLINK") == 0 && s->n_go_args >= 4) {
                    CC_PUSH_EMPTY_STR();
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    cc_expr(c, s->go_args[3]); if (c->error) return;
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 1345); cc_byte(c, 3); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (strcasecmp(sub, "SET") == 0 && s->n_go_args >= 7
                    && s->go_args[1]->kind == GMB_EXPR_VAR && s->go_args[1]->var
                    && strcasecmp(s->go_args[1]->var, "ALIAS") == 0) {
                    CC_PUSH_EMPTY_STR();
                    cc_expr(c, s->go_args[2]); if (c->error) return;
                    cc_expr(c, s->go_args[4]); if (c->error) return;
                    cc_expr(c, s->go_args[6]); if (c->error) return;
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 1346); cc_byte(c, 4); cc_byte(c, GMB_OP_POP);
                    break;
                }
                #undef CC_PUSH_EMPTY_STR
            }
        }
        /* AI WORKER NEW/RUN/LOOP/MANAGE, AI WORKER "<task>" e AI REPLAY AUTO:
         * prima erano NO-OP SILENZIOSI (il ramo generico consumava solo gli argomenti
         * e faceva POP). Mappatura sui builtin del runtime condiviso:
         *   3559 new(name,opts) · 3560 run(name) · 3568 invoke(task)
         *   3569 loop(task,rounds,require) · 3570 manage(cycles,require,plan)
         *   3592 replay_auto(session)                                          */
        if (s->win_name && s->mutex_action && strcasecmp(s->win_name, "AI") == 0) {
            const char* verb = s->mutex_action;
            gmb_expr_t* a0 = (s->n_go_args >= 1) ? s->go_args[0] : NULL;
            const char* sub = (a0 && a0->kind == GMB_EXPR_VAR) ? a0->var : NULL;
            if (strcasecmp(verb, "WORKER") == 0 && a0) {
                if (sub && strcasecmp(sub, "NEW") == 0 && s->n_go_args >= 2) {
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    if (s->n_go_args >= 3) { cc_expr(c, s->go_args[2]); if (c->error) return; }
                    else cc_byte(c, GMB_OP_PUSH_NIL);
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3559); cc_byte(c, 2); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (sub && strcasecmp(sub, "RUN") == 0 && s->n_go_args >= 2) {
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3560); cc_byte(c, 1); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (sub && strcasecmp(sub, "LOOP") == 0 && s->n_go_args >= 2) {
                    cc_expr(c, s->go_args[1]); if (c->error) return;
                    int rounds_i = -1, req_i = -1;
                    for (int i = 2; i < s->n_go_args; i++) {
                        gmb_expr_t* e = s->go_args[i];
                        int is_req = (e && e->kind == GMB_EXPR_VAR && e->var
                                      && strcasecmp(e->var, "REQUIRE") == 0)
                                     || (e && e->tk == GMB_TOK_Require);
                        if (is_req) { if (i + 1 < s->n_go_args) req_i = i + 1; break; }
                        if (rounds_i < 0) rounds_i = i;
                    }
                    if (rounds_i > 0) { cc_expr(c, s->go_args[rounds_i]); if (c->error) return; }
                    else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (req_i > 0) { cc_expr(c, s->go_args[req_i]); if (c->error) return; }
                    else cc_byte(c, GMB_OP_PUSH_NIL);
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3569); cc_byte(c, 3); cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (sub && strcasecmp(sub, "MANAGE") == 0 && s->n_go_args >= 2) {
                    int plan_i = -1, cycles_i = -1, req_i = -1, i = 1;
                    if (i < s->n_go_args && s->go_args[i]->kind == GMB_EXPR_STR) { plan_i = i; i++; }
                    for (; i < s->n_go_args; i++) {
                        gmb_expr_t* e = s->go_args[i];
                        int is_req = (e && e->kind == GMB_EXPR_VAR && e->var
                                      && strcasecmp(e->var, "REQUIRE") == 0)
                                     || (e && e->tk == GMB_TOK_Require);
                        if (is_req) { if (i + 1 < s->n_go_args) req_i = i + 1; break; }
                        if (cycles_i < 0) cycles_i = i;
                    }
                    if (cycles_i > 0) { cc_expr(c, s->go_args[cycles_i]); if (c->error) return; }
                    else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (req_i > 0) { cc_expr(c, s->go_args[req_i]); if (c->error) return; }
                    else { int _si = cc_add_const(c, GMB_CCONST_STRING, 0, ""); if (c->error) return;
                           cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, _si); }
                    if (plan_i > 0) { cc_expr(c, s->go_args[plan_i]); if (c->error) return; }
                    else { int _si = cc_add_const(c, GMB_CCONST_STRING, 0, ""); if (c->error) return;
                           cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, _si); }
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3570); cc_byte(c, 3); cc_byte(c, GMB_OP_POP);
                    break;
                }
                /* AI WORKER "<task>" (invoke) */
                if (!sub && a0->kind == GMB_EXPR_STR) {
                    cc_expr(c, a0); if (c->error) return;
                    cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3568); cc_byte(c, 1); cc_byte(c, GMB_OP_POP);
                    break;
                }
            }
            if (strcasecmp(verb, "REPLAY") == 0 && sub && strcasecmp(sub, "AUTO") == 0
                && s->n_go_args >= 2) {
                cc_expr(c, s->go_args[1]); if (c->error) return;
                cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3592); cc_byte(c, 1); cc_byte(c, GMB_OP_POP);
                break;
            }
        }
        /* TRAIN LORA SAVE|LOAD (§4): adapter RAW <-> .safetensors (builtin 2312/2313).
         * go_args[0] = SAVE|LOAD, [1] = ckpt/adapter, [2] = out. */
        if (s->win_name && strcasecmp(s->win_name, "TRAIN") == 0 && s->mutex_action
            && strcasecmp(s->mutex_action, "LORA") == 0 && s->n_go_args >= 1 && s->go_args[0]
            && s->go_args[0]->var
            && (strcasecmp(s->go_args[0]->var, "SAVE") == 0 || strcasecmp(s->go_args[0]->var, "LOAD") == 0)) {
            int lcode = strcasecmp(s->go_args[0]->var, "SAVE") == 0 ? 2312 : 2313;
            int pushed = 0;
            for (int i = 1; i < s->n_go_args && pushed < 2; i++) { cc_expr(c, s->go_args[i]); if (c->error) return; pushed++; }
            cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, lcode); cc_byte(c, (uint8_t)pushed); cc_byte(c, GMB_OP_POP);
            break;
        }
        /* AI DISTILL (Stream D): comandi -> runtime condiviso gmb_distill_*_opts via CALL_BUILTIN.
         * go_args[0] = verbo (BUILD/SAMPLE/EVAL/APPLY/RESTORE/TRAIN); gli argomenti successivi
         * sono il file (e le opts). Verbo non gestito -> errore ESPLICITO (§0bis, mai no-op). */
        /* [HARD MOVE TRAIN 2026-10-02] solo TRAIN (AI DISTILL non è più valido). */
        if (s->win_name && s->mutex_action && strcasecmp(s->win_name, "TRAIN") == 0
            && (strcasecmp(s->mutex_action, "DISTILL") == 0 || strcasecmp(s->mutex_action, "DATASET") == 0)
            && s->n_go_args >= 1 && s->go_args[0]) {
            gmb_expr_t* v0 = s->go_args[0];
            const char* vs = v0->var ? v0->var : "";
            int dcode = 0;
            if (v0->tk == GMB_TOK_Build || strcasecmp(vs, "BUILD") == 0) dcode = 2028;
            else if (v0->tk == GMB_TOK_Sample || strcasecmp(vs, "SAMPLE") == 0) dcode = 2030;
            else if (v0->tk == GMB_TOK_Eval || strcasecmp(vs, "EVAL") == 0) dcode = 2033;
            else if (v0->tk == GMB_TOK_AiSubLoraApply || strcasecmp(vs, "APPLY") == 0) dcode = 2208;
            else if (v0->tk == GMB_TOK_Restore || strcasecmp(vs, "RESTORE") == 0) dcode = 2209;
            else if (v0->tk == GMB_TOK_Train || strcasecmp(vs, "TRAIN") == 0) dcode = 2210;
            else if (strcasecmp(vs, "STATS") == 0) dcode = 2306;   /* TRAIN DATASET STATS <file> */
            if (dcode == 0) { cc_err(c, "AI DISTILL: verbo non supportato dal compilatore chunk"); break; }
            int dpushed = 0;
            for (int i = 1; i < s->n_go_args; i++) { cc_expr(c, s->go_args[i]); if (c->error) return; dpushed++; }
            cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, dcode); cc_byte(c, (uint8_t)dpushed);
            /* TRAIN DATASET STATS (2306) è un comando-query → stampa; BUILD/SAMPLE/… agiscono
             * e scartano il report. */
            cc_byte(c, dcode == 2306 ? GMB_OP_PRINT_LINE : GMB_OP_POP);
            break;
        }
        if (s->win_name && s->mutex_action) {
            for (size_t i = 0; i < sizeof(dmap)/sizeof(dmap[0]); i++)
                if (strcasecmp(dmap[i].ns, s->win_name) == 0 && strcasecmp(dmap[i].v, s->mutex_action) == 0) { code = dmap[i].code; break; }
        }
        int nargs = s->n_go_args;
        for (int i = 0; i < nargs; i++) cc_expr(c, s->go_args[i]);
        if (c->error) return;
        if (code >= 0) {
            cc_byte(c, GMB_OP_SYSCALL);
            cc_u16(c, code);
            cc_byte(c, (uint8_t)nargs);
        } else {
            /* non mappato: consuma gli argomenti per non sporcare lo stack */
            for (int i = 0; i < nargs; i++) cc_byte(c, GMB_OP_POP);
        }
        break;
    }
    case GMB_STMT_ON_ERROR: {
        if (s->on_kind && strcmp(s->on_kind, "ERROR_GOTO") == 0) {
            if (s->target_label && strcmp(s->target_label, "0") == 0) {
                /* SOLO "0" disabilita il gestore; gli altri numeri sono label di
                   riga e vanno patchati (parità C# _lineNumberJumpsToPatch) */
                cc_byte(c, GMB_OP_SET_ERR_HANDLER_GOTO);
                cc_u16(c, 0);
            } else {
                cc_byte(c, GMB_OP_SET_ERR_HANDLER_GOTO);
                cc_patch_add(c, c->code_len, 0, s->target_label);
                cc_u16(c, 0);
            }
        } else if (s->on_kind && (strcmp(s->on_kind, "ERROR_CALL") == 0 || strcmp(s->on_kind, "ERROR_GOSUB") == 0)) {
            cc_byte(c, GMB_OP_SET_ERR_HANDLER_GOSUB);
            if (s->on_call) cc_patch_add(c, c->code_len, 1, s->on_call->var);
            else cc_patch_add(c, c->code_len, 0, s->target_label);
            cc_u16(c, 0);
        } else if (s->on_kind && strcmp(s->on_kind, "ERROR_RESUME_HANDLER") == 0) {
            /* ON ERROR RESUME NEXT: imposta l'handler resume-next (nessun salto). */
            cc_byte(c, GMB_OP_SET_ERR_HANDLER_RESUME);
        } else if (s->on_kind && strcmp(s->on_kind, "ERROR_RESUME") == 0) {
            /* OP_RESUME ha operando u16 = MODO (parità C# VisitResumeStatement):
               0=RESUME NEXT, 1=RESUME retry, altro=indirizzo label patchato */
            cc_byte(c, GMB_OP_RESUME);
            if (s->flag == 2 && s->target_label) {
                cc_patch_add(c, c->code_len, 0, s->target_label);
                cc_u16(c, 0);
            } else {
                cc_u16(c, s->flag == 1 ? 1 : 0);
            }
        }
        break;
    }
    case GMB_STMT_TRY: {
        /* Layout parità Compiler.Exceptions.cs VisitTryCatchStatement:
           PUSH_TRY → handler ; try body ; POP_TRY ; JUMP → finally ;
           handler: [per catch: STORE ex ; body ; JUMP → finally] ;
           finally: body ; end. Il FINALLY è attraversato SIA dall'uscita normale
           SIA da ogni CATCH. La side-table TryRegions (tryIp→finally/end/hasCatch)
           è richiesta dalla VM/C per eseguire FINALLY e l'unwind sospendibile
           (RETURN/GOTO/THROW attraverso FINALLY). */
        int try_opcode_addr = c->code_len;
        cc_byte(c, GMB_OP_PUSH_TRY);
        int hend = c->code_len;
        cc_u16(c, 0);
        cc_block(c, s->try_body, s->n_try);
        if (c->error) return;
        cc_byte(c, GMB_OP_POP_TRY);
        cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);   /* uscita normale → FINALLY */
        int normal_jmp = c->nrel - 1;
        int handler_addr = c->code_len;
        int catch_exits[16]; int n_catch_exits = 0;
        for (int i = 0; i < s->n_catch; i++) {
            gmb_stmt_t* cl = s->catch_body[i];
            if (cl && cl->kind == GMB_STMT_CATCH) {
                /* l'eccezione è in cima allo stack all'handler: STORE nella variabile
                   CATCH (parità C#: OP_STORE_LOCAL dello slot di "Ex AS ERR") */
                if (cl->catch_var) cc_store_target(c, cl->catch_var, cl->line);
                else cc_byte(c, GMB_OP_POP);
                cc_block(c, cl->catch_body, cl->n_catch);
                if (c->error) return;
                cc_reljump(c, GMB_OP_JUMP_FORWARD, -1);   /* uscita catch → FINALLY */
                if (n_catch_exits < 16) catch_exits[n_catch_exits++] = c->nrel - 1;
            }
        }
        int finally_addr = c->code_len;
        if (c->reljumps[normal_jmp].target < 0) c->reljumps[normal_jmp].target = finally_addr;
        for (int i = 0; i < n_catch_exits; i++)
            if (c->reljumps[catch_exits[i]].target < 0) c->reljumps[catch_exits[i]].target = finally_addr;
        if (s->n_finally > 0) cc_block(c, s->finally_body, s->n_finally);
        if (c->error) return;
        int end_addr = c->code_len;
        /* patch OP_PUSH_TRY: offset RELATIVO dalla fine dell'operando (parità
           PatchAllJumps C#: PUSH_TRY NON è nella lista degli assoluti; la VM/C
           calcola handler_ip = ip + offset). Prima era assoluto → l'handler saltava
           nel punto sbagliato (THROW singolo livello saltava il CATCH; THROW in
           CALL annidati = hang). */
        int handler_off = handler_addr - (hend + 2);
        if (handler_off < 0) handler_off = 0;
        uint8_t* pc = c->code + hend;
        pc[0] = (uint8_t)((handler_off >> 8) & 0xFF);
        pc[1] = (uint8_t)(handler_off & 0xFF);
        /* side-table TryRegions (parità CompiledChunk.TryRegions) */
        if (c->ntryregs >= c->tryreg_cap) {
            c->tryreg_cap = c->tryreg_cap ? c->tryreg_cap * 2 : 8;
            c->tryregs = (cc_tryregion_t*)realloc(c->tryregs, (size_t)c->tryreg_cap * sizeof(cc_tryregion_t));
        }
        c->tryregs[c->ntryregs].try_ip = try_opcode_addr;
        c->tryregs[c->ntryregs].finally_ip = finally_addr;
        c->tryregs[c->ntryregs].end_ip = end_addr;
        c->tryregs[c->ntryregs].has_catch = s->n_catch > 0;
        c->ntryregs++;
        break;
    }
    case GMB_STMT_CATCH:
    case GMB_STMT_FINALLY:
        break;
    case GMB_STMT_THROW:
        cc_expr(c, s->throw_expr ? s->throw_expr : s->error_code);
        if (c->error) return;
        cc_byte(c, GMB_OP_THROW);
        break;
    case GMB_STMT_ERROR:
        /* parità C# VisitErrorStatement: [code][desc|NIL] SYSCALL THROW_ERROR(212) nargs=2 */
        if (s->error_code) cc_expr(c, s->error_code);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (s->throw_expr) cc_expr(c, s->throw_expr);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_byte(c, GMB_OP_SYSCALL);
        cc_u16(c, 212);   /* SysCall.THROW_ERROR: ERROR n, "msg" */
        cc_byte(c, 2);
        break;
    /* DELEGATE FUNCTION: dichiarazione di tipo delegato — i riferimenti a funzione
     * (assegnazioni/invocazioni tramite variabile) sono già gestiti altrove */
    case GMB_STMT_DELEGATE:
    /* AI MCP ... — statement → builtin VM (gmb_mcp_exec.c). Prima questo case era
     * raggruppato col blocco "no-op per compile-parity" che termina in cc_err():
     * AI MCP REGISTER TOOL non compilava affatto sulla VM/C (errore fuorviante
     * "CONTROL (widget TUI)..."). mutex_action = sottocomando, go_args = argomenti
     * (parità Parser.Stmt.Mcp.cs e st_mcp del codegen C99). */
    case GMB_STMT_MCP: {
        gmb_expr_t** a = s->go_args; int na = s->n_go_args;
        int code = 0;
        switch (s->act_tk) {
        case GMB_TOK_McpRegisterTool:          code = 432; break;
        case GMB_TOK_McpUnregisterTool:        code = 433; break;
        case GMB_TOK_McpServe:                 code = 435; break;
        case GMB_TOK_McpStop:                  code = 436; break;
        case GMB_TOK_McpRegisterAgent:         code = 439; break;
        case GMB_TOK_McpServerAdd:             code = 720; break;
        case GMB_TOK_McpServerRemove:          code = 721; break;
        case GMB_TOK_McpServerEnable:          code = 722; break;
        case GMB_TOK_McpServerDisable:         code = 723; break;
        case GMB_TOK_McpServerRefresh:         code = 724; break;
        case GMB_TOK_McpServerMarketplaceGet:  code = 731; break;
        default:
            cc_err(c, "AI MCP: sottocomando non supportato dal compilatore P-Code");
            break;
        }
        if (!code) break;
        int pushed = 0;
        if (code == 432) {
            /* Firma NORMALIZZATA a 6 argomenti (il runtime la legge posizionale):
             *   tool, proc, desc, readonly, inputSchema(compile-time), outputSchema
             * L'inputSchema è derivato dai parametri reali della FUNCTION con
             * l'helper CONDIVISO col codegen C99 (mcp_schema.c, §0bis);
             * l'outputSchema è il 5° argomento BASIC opzionale. */
            for (int i = 0; i < 2; i++) {
                if (i < na && a[i]) cc_expr(c, a[i]); else cc_push_str_const(c, "");
                if (c->error) return;
                pushed++;
            }
            if (na >= 3 && a[2]) cc_expr(c, a[2]); else cc_push_str_const(c, "");
            if (c->error) return;
            pushed++;
            if (na >= 4 && a[3]) cc_expr(c, a[3]); else cc_byte(c, GMB_OP_PUSH_NIL);
            if (c->error) return;
            pushed++;
            /* Il nome della procedura è un LITERALE STRINGA (`->str`), non un
             * identificatore (`->var`): con `->var` lo schema derivato era sempre {}. */
            const char* pname = (na >= 2 && a[1]) ? (a[1]->str ? a[1]->str : a[1]->var) : NULL;
            if (!pname) pname = "";
            char* schema = gmb_mcp_input_schema_json(cc_procdef_lookup_arity(c, pname, -1));
            cc_push_str_const(c, schema);
            free(schema);
            if (c->error) return;
            pushed++;
            if (na >= 5 && a[4]) cc_expr(c, a[4]); else cc_push_str_const(c, "");
            if (c->error) return;
            pushed++;
        } else {
            for (int i = 0; i < na; i++) {
                if (a[i]) cc_expr(c, a[i]); else cc_byte(c, GMB_OP_PUSH_NIL);
                if (c->error) return;
                pushed++;
            }
        }
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, code); cc_byte(c, pushed);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    /* WINDOW THEME "nome": imposta il tema globale (runtime gmb_ui_set_theme_name via
     * syscall 312). Prima finiva nel gruppo no-op che condivideva il corpo di CONTROL e
     * faceva fallire la compilazione ("CONTROL non riconosciuto") su VM/C. */
    /* WINDOW PUMP <ms> / DOEVENTS: un frame del form/UI (syscall 401 FORM_FRAME).
     * Prima il compilatore C non emetteva WINDOW PUMP (solo il compilatore C# lo faceva). */
    case GMB_STMT_WINDOW_PUMP:
        if (s->channel) cc_expr(c, s->channel); else { int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z); }
        if (!c->error) cc_syscall(c, 401, 1);
        break;
    case GMB_STMT_WINDOW_THEME:
    {
        const gmb_expr_t* th = s->channel;
        if (th) cc_expr(c, th); else cc_push_str_const(c, "");
        if (c->error) return;
        cc_syscall(c, 312, 1);
        break;
    }
    /* console/finestra/mouse: nessuna semantica headless — no-op */
    case GMB_STMT_SET:
    case GMB_STMT_SHELL:
    case GMB_STMT_WINDOW_DESKTOP:
    case GMB_STMT_WINDOW_FRAME:
    case GMB_STMT_WINDOW_SHADOW:
    case GMB_STMT_WINDOW_SELECT:
    case GMB_STMT_WINDOW_DEFINE:
    case GMB_STMT_WINDOW_SETTITLE:
    case GMB_STMT_WINDOW_APPEND:
    case GMB_STMT_WINDOW_CONTENT:
    case GMB_STMT_WINDOW_CLEAR:
    case GMB_STMT_WINDOW_SCROLL:
    case GMB_STMT_WINDOW_EDIT:
    case GMB_STMT_WINDOW_MOVEDESKTOP:
    case GMB_STMT_WINDOW_ACTION:
    case GMB_STMT_MOUSE_CONFIG:
    case GMB_STMT_MOUSE_VISIBILITY:
    case GMB_STMT_MOUSE_SET:
    case GMB_STMT_MOUSE_LIMIT:
    case GMB_STMT_MOUSE_CURSOR:
    case GMB_STMT_MOUSE_WAIT:
    case GMB_STMT_ON_KEY:
    case GMB_STMT_ON_MOUSE:
    case GMB_STMT_ON_WINDOW:
    case GMB_STMT_JSON_SAVE:
    case GMB_STMT_JSON_WRITEVALUE:
    case GMB_STMT_JSON_WRITESECTION:
    case GMB_STMT_JSON_OPEN:
    /* file/console/array: no-op per compile-parity (runtime file I/O = follow-up) */
    case GMB_STMT_OPEN:
    case GMB_STMT_CLOSE:
    case GMB_STMT_GET:
    case GMB_STMT_PUT:
    case GMB_STMT_PRINTFILE:
    case GMB_STMT_KILL:
    case GMB_STMT_MKDIR:
    case GMB_STMT_CHDIR:
    case GMB_STMT_RMDIR:
    case GMB_STMT_RENAMEFILE:
    case GMB_STMT_COPYFILE:
    case GMB_STMT_INPUTFILE:
        break;                       /* no-op: NON cade nel corpo di CONTROL */
    case GMB_STMT_CONTROL:
        /* TUI controls: dispatch per nome, emetti OP_CALL_BUILTIN al func_code VMC (3610..3613).
         * NB: 1404..1407 erano GIA' di HMACSHA256/384/512 e TOBASE64URL (BuiltInFunction):
         * riusarli faceva eseguire l'handler HMAC al posto del widget (collisione silenziosa). */
        if (s->n_control_args >= 2 && s->control_args[0] && s->control_args[0]->var) {
            const char* cn = s->control_args[0]->var;
            /* FIRE EVENT handle, "EVENTO" (parser: [0]=FIRE [1]=EVENT [2]=handle [3]=nome). */
            if (strcasecmp(cn, "FIRE") == 0) {
                if (s->control_args[2]) cc_expr(c, s->control_args[2]); else cc_byte(c, GMB_OP_PUSH_NIL);
                if (s->control_args[3]) cc_expr(c, s->control_args[3]); else cc_push_str_const(c, "");
                if (!c->error) cc_syscall(c, 421, 2);
                break;
            }
            /* CONTROL ADD/REMOVE/DRAW (UserControl): [0]=CONTROL [1]=sub [2]=istanza ... */
            if (strcasecmp(cn, "CONTROL") == 0) {
                const char* sub = (s->control_args[1] && s->control_args[1]->var) ? s->control_args[1]->var : "";
                if (strcasecmp(sub, "ADD") == 0 && s->n_control_args >= 6) {
                    const gmb_expr_t* inst = s->control_args[2];
                    const gmb_expr_t* ax = s->control_args[3];
                    const gmb_expr_t* ay = s->control_args[4];
                    const gmb_expr_t* aw = s->control_args[5];
                    const gmb_expr_t* ah = s->control_args[6];
                    if (inst) cc_expr(c, inst); else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (ax) cc_expr(c, ax); else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (ay) cc_expr(c, ay); else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (aw) cc_expr(c, aw); else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (ah) cc_expr(c, ah); else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (c->error) return;
                    { int pidx = s->flag ? 7 : -1;   /* PARENT presente? [7]=parent */
                      int nidx = s->flag ? 8 : 7;     /* nome esplicito dopo parent */
                      if (pidx > 0 && s->n_control_args > pidx && s->control_args[pidx]) cc_expr(c, s->control_args[pidx]);
                      else cc_push_str_const(c, "");
                      if (s->n_control_args > nidx && s->control_args[nidx]) cc_expr(c, s->control_args[nidx]);
                      else if (inst && inst->kind == GMB_EXPR_VAR && inst->var) cc_push_str_const(c, inst->var);
                      else cc_push_str_const(c, "");
                    }
                    cc_syscall(c, 441, 7);
                    if (c->error) return;
                    /* INTO h: handle del controllo appena creato (FORM_LAST_HANDLE 410). */
                    if (s->on_expr && s->on_expr->kind == GMB_EXPR_VAR && s->on_expr->var) {
                        cc_syscall(c, 410, 0);
                        cc_store_target(c, s->on_expr->var, s->line);
                    }
                    /* §11.7-3: metodi della classe con nome di evento → binding handle,ev
                     * (FORM_BIND_UC_EVENT 450). Il linker C# sintetizza le triple; qui la
                     * classe si risolve dalla variabile (cc_var_udt_get). */
                    if (inst && inst->kind == GMB_EXPR_VAR && inst->var) {
                        const char* cn2 = cc_var_udt_get(c, inst->var);
                        int ci = cn2 ? cc_class_find(c, cn2) : -1;
                        if (ci >= 0) {
                            for (int mi = 0; mi < c->classes[ci].n_methods; mi++) {
                                const char* mn = c->classes[ci].method_names[mi];
                                int evi = cc_ev_index(mn);
                                if (evi <= 0) continue;
                                cc_syscall(c, 410, 0);                 /* handle */
                                { gmb_expr_t ev; memset(&ev,0,sizeof ev); ev.kind=GMB_EXPR_NUM; ev.num=(double)evi; cc_expr(c,&ev); }
                                cc_push_str_const(c, mn);              /* metodo */
                                { gmb_expr_t z; memset(&z,0,sizeof z); z.kind=GMB_EXPR_NUM; z.num=0; cc_expr(c,&z); }
                                cc_expr(c, inst);                      /* istanza */
                                cc_syscall(c, 450, 5);
                                if (c->error) return;
                            }
                        }
                    }
                    break;
                }
                if (strcasecmp(sub, "REMOVE") == 0 && s->n_control_args >= 3) {
                    cc_expr(c, s->control_args[2]);
                    if (c->error) return;
                    cc_syscall(c, 442, 1);
                    break;
                }
                if (strcasecmp(sub, "DRAW") == 0 && s->n_control_args >= 4) {
                    for (int i = 2; i <= 8; i++) {
                        const gmb_expr_t* a = (i < s->n_control_args) ? s->control_args[i] : NULL;
                        if (a) cc_expr(c, a); else if (i == 8) cc_push_str_const(c, ""); else cc_byte(c, GMB_OP_PUSH_NIL);
                        if (c->error) return;
                    }
                    cc_syscall(c, 443, 7);
                    break;
                }
                /* Proprietà/stato/geometria: stesso stack delle gambe C# (VM 404..409,448,449). */
                if (strcasecmp(sub, "SET") == 0 && s->n_control_args >= 5) {
                    const gmb_expr_t* an = s->control_args[2];   /* nome controllo */
                    const gmb_expr_t* ap = s->control_args[3];   /* proprietà */
                    if (an) cc_expr(c, an); else cc_push_str_const(c, "");
                    if (ap) cc_expr(c, ap); else cc_push_str_const(c, "");
                    if (s->control_args[4]) cc_expr(c, s->control_args[4]); else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (s->n_control_args >= 6 && s->control_args[5]) cc_expr(c, s->control_args[5]); else cc_push_str_const(c, "");
                    cc_syscall(c, 406, 4);
                    break;
                }
                if (strcasecmp(sub, "GET") == 0 && s->n_control_args >= 5) {
                    if (s->control_args[2]) cc_expr(c, s->control_args[2]); else cc_push_str_const(c, "");
                    if (s->control_args[3]) cc_expr(c, s->control_args[3]); else cc_push_str_const(c, "");
                    cc_syscall(c, 404, 2);
                    if (s->control_args[4] && s->control_args[4]->kind == GMB_EXPR_VAR && s->control_args[4]->var)
                        cc_store_target(c, s->control_args[4]->var, s->line);
                    else cc_byte(c, GMB_OP_POP);
                    break;
                }
                if (strcasecmp(sub, "GETS") == 0 && s->n_control_args >= 5) {
                    if (s->control_args[2]) cc_expr(c, s->control_args[2]); else cc_push_str_const(c, "");
                    if (s->control_args[3]) cc_expr(c, s->control_args[3]); else cc_push_str_const(c, "");
                    cc_syscall(c, 405, 2);
                    if (s->control_args[4] && s->control_args[4]->kind == GMB_EXPR_VAR && s->control_args[4]->var)
                        cc_store_target(c, s->control_args[4]->var, s->line);
                    else cc_byte(c, GMB_OP_POP);
                    break;
                }
                if ((strcasecmp(sub, "MOVE") == 0 || strcasecmp(sub, "FOCUS") == 0 ||
                     strcasecmp(sub, "ZORDER") == 0 || strcasecmp(sub, "INVALIDATE") == 0 ||
                     strcasecmp(sub, "REFRESH") == 0) && s->n_control_args >= 3) {
                    int code = strcasecmp(sub, "MOVE") == 0 ? 407
                             : strcasecmp(sub, "FOCUS") == 0 ? 408
                             : strcasecmp(sub, "ZORDER") == 0 ? 409
                             : strcasecmp(sub, "INVALIDATE") == 0 ? 448 : 449;
                    int na = (code == 407) ? 5 : (code == 409) ? 2 : 1;
                    const gmb_expr_t* an = s->control_args[2];
                    if (an) cc_expr(c, an); else cc_push_str_const(c, "");
                    for (int i = 3; i < 2 + na; i++) {
                        const gmb_expr_t* a = (i < s->n_control_args) ? s->control_args[i] : NULL;
                        if (a) cc_expr(c, a); else cc_byte(c, GMB_OP_PUSH_NIL);
                        if (c->error) return;
                    }
                    cc_syscall(c, code, na);
                    break;
                }
                cc_err(c, "CONTROL: sottocomando non supportato dal chunk compiler");
                break;
            }
            /* PANEL / BUTTON+STYLE: creazione NON bloccante via syscall 400 (stesso bridge
             * gmb_form_* condiviso con C99); prima cadevano nell'errore CONTROL. */
            if (strcasecmp(cn, "PANEL") == 0 && s->n_control_args >= 6) {
                cc_ui_control(c, s, "PANEL", 5, 7);
                break;
            }
            if (strcasecmp(cn, "BUTTON") == 0 && s->n_control_args >= 6) {
                cc_ui_control(c, s, "BUTTON", 5, 7);
                break;
            }
            /* Tutti gli altri controlli: creazione generica NON bloccante via syscall 400
             * (dispatch gmb_form/gmb_tui per kind), stessa superficie del compilatore C#
             * e del C99Native. Prima solo CHECKBOX/TOGGLE/MENU/LISTBOX erano emessi e ogni
             * altro verbo (RADIO/SLIDER/PROGRESS/TAB/GRID/...) falliva la compilazione. */
            static const char* kinds[] = {
                "CHECKBOX","CHECK","RADIO","RADIOGROUP","SLIDER","TRACK","COMBOBOX","DROPDOWN",
                "PROGRESS","TOGGLE","SPINNER","TAB","LISTBOX","COLORPICKER","GRID","TBROWSE",
                "TREEVIEW","STATUSBAR","MENU","DATEPICKER","FILEDIALOG","TEXTBOX","CANVAS","IMAGE",
                "IMAGEBUTTON","QR","RICHTEXT","HALFBLOCK","TILING","FUNCTIONKEYBAR","CANDLESTICK","CHART", NULL };
            int known = 0;
            for (int k = 0; kinds[k]; k++) if (strcasecmp(cn, kinds[k]) == 0) { known = 1; break; }
            if (known) {
                int npos = (s->control_into > 0 ? s->control_into : s->n_control_args) - 1;
                if (npos < 0) npos = 0;
                cc_push_str_const(c, cn);
                cc_push_str_const(c, "");
                cc_push_str_const(c, (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) ? s->set_attr[0] : "");
                for (int i = 0; i < npos; i++) {
                    const gmb_expr_t* e = (1 + i < s->n_control_args) ? s->control_args[1 + i] : NULL;
                    if (e) cc_expr(c, e); else cc_byte(c, GMB_OP_PUSH_NIL);
                    if (c->error) return;
                }
                cc_byte(c, GMB_OP_MK_LIST); cc_u16(c, (unsigned)npos);
                cc_syscall(c, 400, 4);
                /* Handle del controllo appena creato (unificato TUI/form): serve per ID e
                 * per il target INTO. syscall 295 lascia il valore sullo stack; se nessuno
                 * dei due lo consuma resta per il POP dello statement. */
                if (s->id_var && *s->id_var) { cc_syscall(c, 410, 0); cc_store_target(c, s->id_var, s->line); }
                if (s->control_into > 0 && s->control_into < s->n_control_args &&
                    s->control_args[s->control_into] && s->control_args[s->control_into]->var) {
                    cc_syscall(c, 410, 0); cc_store_target(c, s->control_args[s->control_into]->var, s->line);
                } else if (!(s->id_var && *s->id_var)) cc_syscall(c, 410, 0);
                break;
            }
        }
        cc_err(c, "CONTROL non riconosciuto o argomenti mancanti");
        break;
    case GMB_STMT_GFX: {
        /* Statement grafici (parser nativo: GMB_STMT_GFX + net_sub). Il runtime VM/C ha
         * già gli handler GMB_SC_*; mancava l'emissione dal chunk compiler, quindi LINE/
         * PSET/CIRCLE/... davano "costrutto non ancora compilabile". */
        int sub = s->net_sub, code = 0, argc = 0;
        switch (sub) {
            case 2:  code = 243; argc = 0; break;                                  /* SYNC */
            case 1:  code = 213; argc = 4; break;                                  /* SCREEN */
            case 3:  code = 216; argc = 2; break;                                  /* POINT (x,y) -> val */
            case 7:  code = 214; argc = 3; break;                                  /* PSET x,y[,c] */
            case 8:  code = 215; argc = 2; break;                                  /* PRESET x,y */
            case 9:  code = 217; argc = 6; break;                                  /* LINE x1,y1,x2,y2[,c[,style]] */
            case 10: code = 218; argc = 5; break;                                  /* CIRCLE x,y,r[,c[,aspect]] */
            case 11: code = 219; argc = 4; break;                                  /* PAINT x,y[,fill[,border]] */
            default: cc_err(c, "statement grafico non supportato dal chunk compiler"); return;
        }
        for (int i = 0; i < argc; i++) {
            const gmb_expr_t* a = (i < s->n_netargs) ? s->net_args[i] : NULL;
            if (a) cc_expr(c, a); else cc_byte(c, GMB_OP_PUSH_NIL);
            if (c->error) return;
        }
        if (sub == 9) {
            /* stile (B/BF/n) è salvato dal parser in net_assign come NOME, non in args */
            if (s->net_assign && s->net_assign[0]) cc_push_str_const(c, s->net_assign);
            else cc_push_str_const(c, "");
        }
        cc_syscall(c, code, argc);
        if (sub == 3 && s->net_assign && s->net_assign[0])
            cc_store_target(c, s->net_assign, s->line);
        else if (sub == 3)
            cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_SWAP:
    case GMB_STMT_REQUIRE:
    case GMB_STMT_ZIP:
    case GMB_STMT_UNZIP:
        break;
    /* Bug corretto (2026-09-02, seguito task AEVAL/ClipperCodeBlocks): questi 5 statement
     * erano un no-op COMPLETO (raggruppati sopra con file/console I/O "per compile-parity")
     * — la VM ha già handler funzionanti per tutti e cinque (gmb_bf_520/529/530/531/535 in
     * gmb_builtin.c: shuffle Fisher-Yates in-place, append, insert, remove-at, sort con
     * comparatore opzionale a 2 argomenti + ordine ASC/DESC) ma nessuno aveva mai una
     * entry in g_cc_builtins né un case dedicato qui — l'istruzione compilava e non
     * faceva assolutamente nulla, mai un errore. Ognuno diventa: carica l'array, impila
     * gli argomenti dallo statement (array_value/array_position, opzionali secondo la
     * sintassi), OP_CALL_BUILTIN sul func_code già esistente, POP del risultato (sempre
     * scartato: questi sono STATEMENT, non espressioni). */
    case GMB_STMT_YIELD:
        /* GENERATOR: push del valore, poi sospensione (OP_NEXT_GENERATOR). */
        if (s->value) { cc_expr(c, s->value); if (c->error) return; }
        else cc_byte(c, GMB_OP_PUSH_NIL);
        cc_byte(c, GMB_OP_NEXT_GENERATOR);
        break;
    case GMB_STMT_AADD: {
        cc_load_target(c, s->array_var);
        if (s->array_value) cc_expr(c, s->array_value); else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 529); cc_byte(c, 2);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_AINS: {
        cc_load_target(c, s->array_var);
        if (s->array_position) cc_expr(c, s->array_position); else cc_byte(c, GMB_OP_PUSH_NIL);
        int nargs = 2;
        if (s->array_value) { cc_expr(c, s->array_value); nargs = 3; }
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 530); cc_byte(c, (uint8_t)nargs);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_ADEL: {
        cc_load_target(c, s->array_var);
        if (s->array_position) cc_expr(c, s->array_position); else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 531); cc_byte(c, 2);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_AFILL: {
        cc_load_target(c, s->array_var);
        if (s->array_value) cc_expr(c, s->array_value); else cc_byte(c, GMB_OP_PUSH_NIL);
        int nargs = 2;
        if (s->array_start) { cc_expr(c, s->array_start); nargs++; } else cc_byte(c, GMB_OP_PUSH_NIL);
        if (s->array_count) { cc_expr(c, s->array_count); nargs++; }
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 533); cc_byte(c, (uint8_t)nargs);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_ACOPY: {
        cc_load_target(c, s->array_var);
        if (s->array_var2) cc_load_target(c, s->array_var2); else cc_byte(c, GMB_OP_PUSH_NIL);
        int nargs = 2;
        if (s->array_start) { cc_expr(c, s->array_start); nargs++; } else cc_byte(c, GMB_OP_PUSH_NIL);
        if (s->array_count) { cc_expr(c, s->array_count); nargs++; }
        if (s->array_extra) { cc_expr(c, s->array_extra); nargs++; }
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 534); cc_byte(c, (uint8_t)nargs);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_ASHUFFLE: {
        cc_load_target(c, s->array_var);
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 520); cc_byte(c, 1);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_ASORT: {
        cc_load_target(c, s->array_var);
        int nargs = 1;
        if (s->array_value) {
            cc_expr(c, s->array_value); nargs = 2;
            if (s->array_position) { cc_expr(c, s->array_position); nargs = 3; }
        } else if (s->array_position) {
            /* "ASORT arr,, order" — comparatore vuoto esplicito, solo l'ordine */
            cc_byte(c, GMB_OP_PUSH_NIL);
            cc_expr(c, s->array_position);
            nargs = 3;
        }
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 535); cc_byte(c, (uint8_t)nargs);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    /* AI SERVER START/STOP/STATUS/MODEL/UNLOAD — gestione server LLM come processo esterno.
     * Statement: push arg (dir per START/MODEL) → OP_CALL_BUILTIN sul func_code dedicato
     * (1396..1400 = BuiltInFunction enum) → POP (statement, risultato scartato). */
    case GMB_STMT_AISERVER_START: {
        int na = 0;
        if (s->n_go_args >= 1) { cc_expr(c, s->go_args[0]); na = 1; }
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (s->n_go_args >= 2) { cc_expr(c, s->go_args[1]); na = 2; }
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3505); cc_byte(c, (uint8_t)na);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_AISERVER_STOP: {
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3506); cc_byte(c, 0);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_AISERVER_STATUS: {
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3507); cc_byte(c, 0);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_AISERVER_MODEL: {
        if (s->n_go_args >= 1) cc_expr(c, s->go_args[0]);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3508); cc_byte(c, 1);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_AISERVER_UNLOAD: {
        cc_byte(c, GMB_OP_CALL_BUILTIN); cc_u16(c, 3509); cc_byte(c, 0);
        cc_byte(c, GMB_OP_POP);
        break;
    }
    case GMB_STMT_FUNCTION:
        /* FUNCTION/SUB annidata dentro un corpo: closure con upvalue capture (parità C#),
           NON proc flat. La passata 4 la salta (cc_nested_closure_known in gmb_compile). */
        cc_emit_nested_function(c, s);
        break;
    case GMB_STMT_REDIM:
        /* REDIM v(expr) — OP_REDIM: numDims(u8) + udtDesc(u16). Bug corretto
         * (2026-09-07): qui veniva emesso SOLO cc_u16(0) dopo l'opcode — ma la runtime
         * OP_REDIM legge numDims u8 PRIMA del desc u16 (3 byte di operandi, come il
         * ramo DIM) → ogni REDIM statement consumava un byte in MENO e disallineava
         * tutte le istruzioni successive della funzione (chiamate/assegnamenti visti
         * fuori allineamento). Ora emette numDims=1 + desc UDT risolto come nel DIM,
         * carica la variabile esistente (la runtime la ridimensiona in-place) e
         * riscrive il risultato (bilanciato, niente residui sullo stack). */
        /* REDIM su membro/UDT (y.Ports(n)): dimensiona il campo-array del RECORD e lo
         * riscrive nel campo, SENZA toccare il resto del record (prima il parser leggeva
         * solo "y" → il REDIM ricostruiva l'intero record, perdendo gli altri campi). */
        if (s->dim_var) {
            const char* rdot = strrchr(s->dim_var, '.');
            if (rdot && rdot != s->dim_var) {
                char rb[256];
                size_t rbl = (size_t)(rdot - s->dim_var);
                if (rbl < sizeof(rb)) {
                    memcpy(rb, s->dim_var, rbl); rb[rbl] = 0;
                    if (cc_local(c, rb) >= 0 || cc_global(c, rb, 0) >= 0) {
                        if (s->array_size) cc_expr(c, s->array_size);
                        else { int zi2 = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL); cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, zi2); }
                        if (c->error) return;
                        cc_byte(c, GMB_OP_REDIM);
                        cc_byte(c, 1);
                        cc_u16(c, 65535);
                        cc_load_target(c, rb);
                        int rfi = cc_add_const(c, GMB_CCONST_STRING, 0, rdot + 1);
                        if (c->error) return;
                        cc_byte(c, GMB_OP_STORE_FIELD); cc_u16(c, rfi);
                        break;
                    }
                }
            }
        }
        cc_load_target(c, s->dim_var);
        if (s->array_size) cc_expr(c, s->array_size);
        else cc_byte(c, GMB_OP_PUSH_NIL);
        if (c->error) return;
        {
            int rudt_desc = 65535;
            const char* ret = s->dim_var ? cc_var_udt_get(c, s->dim_var) : NULL;
            int rui = (ret && !ret[0]) ? -1 : (ret ? cc_udt_find(c, ret) : -1);
            if (rui >= 0) rudt_desc = cc_udt_desc_const_idx(c, &c->udts[rui]);
            cc_byte(c, GMB_OP_REDIM);
            cc_byte(c, 1);   /* numDims */
            cc_u16(c, (uint16_t)rudt_desc);
            cc_store_dim_target(c, s->dim_var, s->line);
        }
        break;
    case GMB_STMT_TEAM_CREATE: cc_push_str_const(c, s->win_name); cc_syscall(c, 92, 1); break;
    case GMB_STMT_TEAM_SAVE: cc_push_str_const(c, s->win_name); cc_syscall(c, 100, 1); break;
    case GMB_STMT_TEAM_LOAD: cc_push_str_const(c, s->win_name); cc_syscall(c, 101, 1); break;
    case GMB_STMT_TEAM_ADDAGENT: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 93, 2); break;
    case GMB_STMT_TEAM_REMOVEAGENT: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 94, 2); break;
    case GMB_STMT_TEAM_RUN: cc_expr(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 96, 2); break;
    case GMB_STMT_TEAM_MESSAGE: cc_expr(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 95, 2); break;
    case GMB_STMT_TEAM_SETROLE: cc_push_str_arg(c, s->send_value); cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 97, 3); break;
    case GMB_STMT_TEAM_SETSTRATEGY: cc_push_str_arg(c, s->send_value); cc_push_str_const(c, s->win_name); cc_syscall(c, 98, 2); break;
    case GMB_STMT_TEAM_SETCOORDINATOR: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 99, 2); break;
    case GMB_STMT_TEAM_SETRECURSION: cc_expr(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 363, 2); break;
    case GMB_STMT_TEAM_SETLATENT: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 364, 2); break;
    case GMB_STMT_TEAM_SETLINK: cc_push_str_arg(c, s->send_value); cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 365, 3); break;
    case GMB_STMT_TEAM_SETTHOUGHTS: cc_expr(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 366, 2); break;
    case GMB_STMT_TEAM_TRAININNER: cc_push_str_arg(c, s->channel); cc_push_str_arg(c, s->send_value); cc_push_str_const(c, s->win_name); cc_syscall(c, 367, 3); break;
    case GMB_STMT_TEAM_TRAINOUTER: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 368, 2); break;
    case GMB_STMT_TEAM_SETDAG: cc_expr(c, s->cond_mutex); cc_expr(c, s->send_value); cc_expr(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 369, 4); break;
    case GMB_STMT_TEAM_SETCOMPLEXITY: cc_expr(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 370, 2); break;
    case GMB_STMT_TEAM_SETREFLECTION: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 371, 2); break;
    case GMB_STMT_TEAM_SETCRITIC: cc_push_str_arg(c, s->send_value); cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 372, 3); break;
    case GMB_STMT_TEAM_SETPRUNE: cc_expr(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 373, 2); break;
    case GMB_STMT_TEAM_TRAINTHINKING: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 374, 2); break;
    case GMB_STMT_TEAM_TRAINACTORCRITIC: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 375, 2); break;
    case GMB_STMT_TEAM_DISTILLTHINKING: cc_push_str_arg(c, s->channel); cc_push_str_const(c, s->win_name); cc_syscall(c, 376, 2); break;
    /* IMPORT <ns> [AS alias]: direttiva di namespace (linker/grafo), nessun codice. */
    case GMB_STMT_IMPORT: break;
    default:
    {
        char buf[160];
        snprintf(buf, sizeof(buf), "costrutto non ancora compilabile (kind=%d)", (int)s->kind);
        cc_err(c, buf);
        break;
    }
    }
}

/* ── compilazione programma ── */
static void cc_patch_all(gmb_cc_t* c)
{
    /* jump relativi */
    for (int i = 0; i < c->nrel; i++) {
        int off;
        if (c->reljumps[i].backward)
            off = (c->reljumps[i].pos + 2) - c->reljumps[i].target;   /* ip già dopo l'istruzione */
        else
            off = c->reljumps[i].target - (c->reljumps[i].pos + 2);
        if (off < 0) off = 0;
        int pos = c->reljumps[i].pos;
        c->code[pos] = (uint8_t)((off >> 8) & 0xFF);
        c->code[pos + 1] = (uint8_t)(off & 0xFF);
    }
    /* patch assoluti (etichette/procedure) */
    for (int i = 0; i < c->npatch; i++) {
        int addr = -1;
        int external = 0;
        if (c->patches[i].kind == 0) {
            for (int l = 0; l < c->nlabels; l++)
                if (strcasecmp(c->labels[l].name, c->patches[i].name) == 0) { addr = c->labels[l].addr; break; }
            if (addr < 0) addr = c->code_len;   /* label non trovata: salta alla fine (safe stop) */
            addr += c->code_base;               /* snippet: spazio indirizzi del chunk merged */
        } else {
            /* Arity-aware (2026-09-07, fix overload cross-file): fra tutte le entry che
             * combaciano per nome (due o più con overload omonimi, es. Somma 2-arg e
             * 3-arg da file diversi dello stesso namespace, entrambe registrate da
             * cc_proc senza dedup), preferisce quella la cui arità dichiarata
             * (arity_vals, in lockstep con proc_addrs) combacia ESATTAMENTE con
             * l'argc del call site (c->patches[i].argc, -1 se non tracciato per questo
             * patch — allora si ripiega SEMPRE sul primo match, comportamento
             * invariato). Senza questo, cc_patch_all risolveva sempre al PRIMO nome
             * che combacia, ignorando quanti argomenti il call site aveva scritto: una
             * chiamata a 3 argomenti poteva finire nel corpo a 2 parametri (o
             * viceversa), stack squilibrato, risultato spazzatura. */
            gmb_compiled_chunk_t* ch = c->chunk;
            int argc = c->patches[i].argc;
            int fallback_addr = -1, exact_addr = -1;
            int fallback_ext = 0, exact_ext = 0;
            for (uint32_t p = 0; p < ch->proc_count; p++) {
                if (strcasecmp(ch->proc_names[p], c->patches[i].name) != 0) continue;
                int is_ext = (int)p < c->n_ext_procs;
                if (fallback_addr < 0) { fallback_addr = (int)ch->proc_addrs[p]; fallback_ext = is_ext; }
                if (argc >= 0 && exact_addr < 0 && p < ch->arity_count && (int)ch->arity_vals[p] == argc)
                    { exact_addr = (int)ch->proc_addrs[p]; exact_ext = is_ext; }
            }
            addr = exact_addr >= 0 ? exact_addr : fallback_addr;
            external = exact_addr >= 0 ? exact_ext : fallback_ext;
            if (addr < 0) { char m[256]; snprintf(m, sizeof(m), "procedura non trovata: %s", c->patches[i].name); cc_err(c, m); return; }
            /* Le proc dell'OSPITE hanno indirizzi già assoluti: mai ri-basare. */
            if (!external && c->code_base) addr += c->code_base;
        }
        int pos = c->patches[i].pos;
        c->code[pos] = (uint8_t)((addr >> 8) & 0xFF);
        c->code[pos + 1] = (uint8_t)(addr & 0xFF);
    }
    /* patch code-ref: costante → indirizzo procedura */
    for (int i = 0; i < c->ncr; i++) {
        int addr = -1;
        int external = 0;
        gmb_compiled_chunk_t* ch = c->chunk;
        for (uint32_t p = 0; p < ch->proc_count; p++)
            if (strcasecmp(ch->proc_names[p], c->cr_names[i]) == 0)
                { addr = (int)ch->proc_addrs[p]; external = (int)p < c->n_ext_procs; break; }
        if (addr < 0) { char m[256]; snprintf(m, sizeof(m), "procedura non trovata (code-ref): %s", c->cr_names[i]); cc_err(c, m); return; }
        if (!external && c->code_base) addr += c->code_base;
        ch->consts[c->cr_consts[i]].d = (double)addr;
    }
}

static void cc_proc(gmb_cc_t* c, const gmb_stmt_t* s)
{
    /* registra procedura (nome → indirizzo attuale) e arity */
    gmb_compiled_chunk_t* ch = c->chunk;
    ch->proc_names = (char**)realloc(ch->proc_names, (size_t)(ch->proc_count + 1) * sizeof(char*));
    ch->proc_addrs = (uint32_t*)realloc(ch->proc_addrs, (size_t)(ch->proc_count + 1) * sizeof(uint32_t));
    ch->proc_names[ch->proc_count] = strdup(s->func_name);
    ch->proc_addrs[ch->proc_count] = (uint32_t)(c->code_len + c->code_base);
    ch->arity_addrs = (uint32_t*)realloc(ch->arity_addrs, (size_t)(ch->arity_count + 1) * sizeof(uint32_t));
    ch->arity_vals = (uint32_t*)realloc(ch->arity_vals, (size_t)(ch->arity_count + 1) * sizeof(uint32_t));
    ch->arity_addrs[ch->arity_count] = (uint32_t)(c->code_len + c->code_base);
    ch->arity_vals[ch->arity_count] = (uint32_t)s->n_params;
    ch->arity_count++;
    ch->proc_count++;

    /* prologo: store dei parametri nei locali (ordine inverso) */
    c->nlocals = 0;
    /* Convenzione classica BASIC: `NomeFunzione = espr` dentro il corpo imposta il valore di
     * ritorno (nessun RETURN esplicito necessario). Prima di questo fix il corpo compilava
     * `Twice = n * 2` come un ASSIGN qualunque (cc_store_target per nome — vedi GMB_STMT_LET/
     * GMB_STMT_ASSIGN), cioè scriveva in un local/globale chiamato "Twice" a caso, senza
     * alcun collegamento col vero valore di ritorno: il RETURN implicito a fine funzione
     * faceva sempre PUSH_NIL incondizionato, ignorando quell'assegnazione — bug reale,
     * QUALUNQUE funzione che segue questa convenzione (la maggioranza del codice BASIC
     * classico) restituiva sempre NIL. Riservando qui un local con lo STESSO nome della
     * funzione, `cc_store_target` (già per-nome) lo trova da solo — nessuna modifica serve
     * al codice di compilazione degli ASSIGN. */
    c->current_proc_name = s->func_name;
    cc_tuple_clear();
    int retSlot = s->func_name ? cc_local_add(c, s->func_name) : -1;
    for (int i = 0; i < s->n_params; i++) {
        cc_local_add(c, s->func_params[i]);
        /* parametro AS ClassName (2.9): tracciamento del tipo per il dispatch di
         * obj.Metodo() dentro il corpo — prima un'istanza passata come argomento perdeva
         * il tipo tracciato (vt_names/vt_types popolato solo da DIM/ASSIGN), quindi
         * .Metodo() sul parametro cadeva nel vecchio fallback generico. */
        if (s->param_types && s->param_types[i] && cc_class_find(c, s->param_types[i]) >= 0)
            cc_var_udt_set(c, s->func_params[i], s->param_types[i]);
        /* Bug corretto (2026-08-31, audit lambda in builtin): un parametro dichiarato
         * "predicate AS FUNCTION(...) AS ..." (callback di prima classe passato dal
         * chiamante) non veniva MAI registrato come closure var — cc_closure_var_add è
         * chiamato solo per DIM/ASSIGN con inizializzatore letterale FN(...), mai per i
         * parametri. Una chiamata "predicate(x)" dentro il corpo cadeva quindi nel
         * fallback generico "proc sconosciuta" (no-op per compile-parity: consuma gli
         * argomenti, restituisce sempre NIL/0) invece di un OP_CALL_INDIRECT sul valore
         * di closure passato — bug: una FUNCTION utente con parametro FUNCTION-tipizzato
         * (es. CountIf(arr, predicate AS FUNCTION(DOUBLE) AS BIT), FunctionsLambda.bas)
         * restituiva sempre 0, la lambda passata non veniva mai davvero invocata. Il
         * parser cattura solo il PRIMO token dopo "AS" in param_types[i] — per un
         * parametro FUNCTION quel token è letteralmente la keyword "FUNCTION" (o "SUB":
         * SUB/FUNCTION condividono lo stesso token kind, distinti solo dal lessema — vedi
         * `AS SUB(...)`, zucchero sintattico per un callback void, FunctionsSubAsAction.bas). */
        if (s->param_types && s->param_types[i]
            && (strcasecmp(s->param_types[i], "FUNCTION") == 0 || strcasecmp(s->param_types[i], "SUB") == 0))
            cc_closure_var_add(c, s->func_params[i]);
    }
    for (int i = s->n_params - 1; i >= 0; i--) {
        int slot = cc_local(c, s->func_params[i]);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, slot);
    }
    cc_byte(c, GMB_OP_CLEAR_RETURN);
    if (retSlot >= 0 && !s->is_sub && cc_ret_numeric(s->func_ret_type)) {
        int z = cc_add_const(c, GMB_CCONST_DOUBLE, 0.0, NULL);
        cc_byte(c, GMB_OP_PUSH_CONST); cc_u16(c, z);
        cc_byte(c, GMB_OP_STORE_LOCAL); cc_u16(c, retSlot);
    }

    /* body (le label restano accumulate per il patching globale).
       in_proc=1 → DIM locali; se la procedura contiene SUB/FUNCTION annidate
       le DIM restano globali: sono il veicolo della cattura by-reference. */
    c->in_proc = !cc_body_has_nested_func(s->func_body, s->n_funcbody);
    /* Frammento a runtime: la DIM resta LOCALE (una lambda nel corpo non deve far
     * diventare globali le variabili del frammento). */
    if (c->snippet_mode) c->in_proc = 1;
    cc_syscall(c, 470, 0);   /* SCOPE DINAMICO: frame di procedura (§2.5) */
    for (int i = 0; i < s->n_funcbody; i++) cc_stmt(c, s->func_body[i]);
    c->in_proc = 0;
    /* RETURN implicito: se il corpo ha assegnato al nome della funzione, il valore è nel
     * local retSlot (0/NIL se mai assegnato — locali azzerati all'ingresso frame, stesso
     * comportamento di prima per le funzioni che uscono sempre con RETURN esplicito, che
     * non arrivano mai qui). Le SUB (func_name eventualmente assente/irrilevante) restano
     * NIL come prima. */
    if (retSlot >= 0) { cc_byte(c, GMB_OP_LOAD_LOCAL); cc_u16(c, retSlot); }
    else cc_byte(c, GMB_OP_PUSH_NIL);
    /* il valore di ritorno è già sullo stack: pusha/popa il frame PRIMA di RETURN,
       ma il pop deve avvenire senza perdere il valore → lo teniamo sotto al pop? */
    cc_syscall(c, 471, 0);
    cc_byte(c, GMB_OP_RETURN);
    /* S_eval: snapshot nomi-locali di procedura (slot monotonici, esatto). */
    cc_snapshot_locals(c, (int)ch->proc_addrs[ch->proc_count - 1]);
    c->current_proc_name = NULL;
}

/* §4.10(f): fold delle funzioni del semantic graph (AI.SYMBOL/IMPACT/CALLMAP) — def. in gmb_semfold.c */
void gmb_semfold_program(gmb_stmt_t** stmts, int n, const char* file);

int gmb_compile(const gmb_program_t* prog, uint8_t** out_data, size_t* out_len, char** error)
{
    if (prog && prog->stmts && prog->n > 0)
        gmb_semfold_program(prog->stmts, prog->n, "");   /* pre-pass: sostituisce le call sem. graph */
    if (!prog || !out_data || !out_len) { if (error) *error = strdup("gmb_compile: argomenti nulli"); return -1; }
    gmb_compiled_chunk_t ch = {0};
    gmb_cc_t c = {0};
    c.cur_class = -1;
    c.chunk = &ch;
    if (g_snip_ext) snip_seed(&c, g_snip_ext);
    cc_tuple_clear();

    /* padding 2 byte: l'indirizzo 0 è il sentinella "nessun handler/nessuna
       destinazione" per la VM/C (error_handler_ip>0, GOTO 0...), ma le label
       top-level del __main partono proprio da 0 (ON ERROR GOTO <label> con
       handler dichiarato prima delle SUB finiva disabilitato). PUSH_NIL+POP è
       innocuo se eseguito. */
    cc_byte(&c, GMB_OP_PUSH_NIL);
    cc_byte(&c, GMB_OP_POP);

    if (prog && prog->stmts && prog->n) cc_prescan_dyn(&c, prog->stmts, prog->n);
    /* passata 1: separa body principale e procedure; raccoglie i DATA */
    int* is_proc = (int*)calloc((size_t)(prog->n ? prog->n : 1), sizeof(int));
    for (int i = 0; i < prog->n; i++)
        if (prog->stmts[i] && prog->stmts[i]->kind == GMB_STMT_FUNCTION) {
            is_proc[i] = 1;
            cc_procset_add(&c, prog->stmts[i]->func_name);
            if (prog->stmts[i]->is_generator) {
                c.gen_names = (char**)realloc(c.gen_names, (size_t)(c.n_gen_names + 1) * sizeof(char*));
                c.gen_names[c.n_gen_names++] = strdup(prog->stmts[i]->func_name);
            }
            cc_procdef_register(&c, prog->stmts[i]->func_name, prog->stmts[i]);
            /* 2026-09-24: "esistente e ridefinita" (parità C#/C99Native) — vedi sopra. */
            cc_check_shadow(&c, prog->stmts[i]);
            if (c.error) { free(is_proc); if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }
            uint64_t bm = 0;
            for (int k = 0; k < prog->stmts[i]->n_params && k < 64; k++)
                if (prog->stmts[i]->param_byref && prog->stmts[i]->param_byref[k]) bm |= (uint64_t)1 << k;
            cc_byref_register(&c, prog->stmts[i]->func_name, bm);
            if (c.n_closure_return_procs < 64 && (cc_body_assigns_lambda(
                    (const gmb_stmt_t* const*)prog->stmts[i]->func_body,
                    prog->stmts[i]->n_funcbody, prog->stmts[i]->func_name)
                || cc_body_returns_nested_func(
                    (const gmb_stmt_t* const*)prog->stmts[i]->func_body,
                    prog->stmts[i]->n_funcbody))) {
                snprintf(c.closure_return_procs[c.n_closure_return_procs++], 64, "%s",
                         prog->stmts[i]->func_name ? prog->stmts[i]->func_name : "");
            }
        }

    /* UDT (TYPE...END TYPE, campi nudi): registrati PRIMA di compilare qualunque DIM,
     * così una DIM che precede testualmente la dichiarazione del TYPE (rara, ma non
     * vietata dalla grammatica) risolve comunque — stessa logica di cc_procset_add sopra. */
    for (int i = 0; i < prog->n; i++)
        if (prog->stmts[i] && prog->stmts[i]->kind == GMB_STMT_CLASS && prog->stmts[i]->class_is_udt
            && prog->stmts[i]->n_cls_methods == 0)
            cc_udt_register(&c, prog->stmts[i]);
    /* Bug corretto (2026-09-02, seguito task AEVAL): un TYPE...END TYPE dichiarato DENTRO
     * il corpo di una SUB/FUNCTION (non solo a livello di programma) non veniva MAI
     * registrato qui — questo pre-pass scandiva solo prog->stmts[] (livello top). Un DIM
     * che referenzia quel TYPE, anche nella STESSA SUB dopo la dichiarazione, non lo
     * trovava mai (cc_udt_find sempre -1) e ricadeva nel path scalare/array generico
     * invece di costruire un vero record UDT — mai un errore, solo un successivo
     * `arr(i).Campo` che indicizzava uno scalare (crash) o un campo sempre vuoto/0.
     * Un livello di annidamento (SUB/FUNCTION dirette in prog->stmts[]) è sufficiente:
     * la grammatica non prevede TYPE dentro un IF/FOR/altro blocco annidato più a fondo. */
    for (int i = 0; i < prog->n; i++)
        if (prog->stmts[i] && prog->stmts[i]->kind == GMB_STMT_FUNCTION)
            for (int j = 0; j < prog->stmts[i]->n_funcbody; j++) {
                gmb_stmt_t* inner = prog->stmts[i]->func_body[j];
                if (inner && inner->kind == GMB_STMT_CLASS && inner->class_is_udt && inner->n_cls_methods == 0)
                    cc_udt_register(&c, inner);
            }

    /* CLASS vera, o TYPE con metodi propri (2.9): stesso schema di registrazione
     * anticipata, ma con un descrittore VTable/Constructors — vedi cc_class_register
     * (INHERITS/STATIC/PROPERTY supportati lì). Fuori scope, rifiutato esplicitamente
     * da cc_stmt: IMPLEMENTS/INTERFACE/OPERATOR. */
    for (int i = 0; i < prog->n; i++)
        if (prog->stmts[i] && prog->stmts[i]->kind == GMB_STMT_CLASS
            && !(prog->stmts[i]->class_is_udt && prog->stmts[i]->n_cls_methods == 0))
            cc_class_register(&c, prog->stmts[i]);
    /* stesso fix di sopra, ma per CLASS vere/TYPE-con-metodi annidate in una SUB/FUNCTION. */
    for (int i = 0; i < prog->n; i++)
        if (prog->stmts[i] && prog->stmts[i]->kind == GMB_STMT_FUNCTION)
            for (int j = 0; j < prog->stmts[i]->n_funcbody; j++) {
                gmb_stmt_t* inner = prog->stmts[i]->func_body[j];
                if (inner && inner->kind == GMB_STMT_CLASS
                    && !(inner->class_is_udt && inner->n_cls_methods == 0))
                    cc_class_register(&c, inner);
            }
    /* una base INHERITS non trovata (cc_err dentro cc_class_register) lascia quella
     * entry di c.classes[] a metà (vtable_node/ctor_list_node NULL — return anticipato
     * lì): bail-out SUBITO, prima che cc_class_merge_inherited o qualunque altro codice
     * possa dereferenziarli. */
    if (c.error) { free(is_proc); if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }

    /* FUNCTION ... AS ClassName (2.9): tracciamento del tipo di ritorno DOPO che le
     * classi sono registrate (cc_class_find deve trovarle) — usato da `x = Func(...)`/
     * `DIM x = Func(...)` per riconoscere che il risultato è un'istanza di classe e
     * abilitare il dispatch di x.Metodo() (altrimenti un'istanza che transita per una
     * FUNCTION ordinaria perdeva il tipo tracciato). */
    for (int i = 0; i < prog->n; i++)
        if (prog->stmts[i] && prog->stmts[i]->kind == GMB_STMT_FUNCTION && prog->stmts[i]->func_ret_type)
            cc_ret_class_register(&c, prog->stmts[i]->func_name, prog->stmts[i]->func_ret_type);

    /* procedure annidate dentro i body (SUB dentro SUB): raccolta piatta */
    const gmb_stmt_t** nested = NULL; int n_nested = 0, cap_nested = 0;
    for (int i = 0; i < prog->n; i++)
        if (is_proc[i])
            cc_collect_nested_procs(&c, prog->stmts[i]->func_body, prog->stmts[i]->n_funcbody, &nested, &n_nested, &cap_nested);

    /* passata 2: DATA pool (valori letterali, in ordine di apparizione) */
    for (int i = 0; i < prog->n; i++) {
        const gmb_stmt_t* s = prog->stmts[i];
        if (s && s->kind == GMB_STMT_DATA) {
            for (int k = 0; k < s->n_data; k++) {
                const gmb_expr_t* d = s->data_values[k];
                if (d->kind == GMB_EXPR_NUM) {
                    ch.data = realloc(ch.data, (size_t)(ch.data_len + 1) * sizeof(*ch.data));
                    ch.data[ch.data_len].type = GMB_CCONST_DOUBLE; ch.data[ch.data_len].d = d->num; ch.data[ch.data_len].s = NULL; ch.data[ch.data_len].tree = NULL;
                    ch.data_len++;
                } else if (d->kind == GMB_EXPR_STR) {
                    ch.data = realloc(ch.data, (size_t)(ch.data_len + 1) * sizeof(*ch.data));
                    ch.data[ch.data_len].type = GMB_CCONST_STRING; ch.data[ch.data_len].d = 0; ch.data[ch.data_len].s = strdup(d->str ? d->str : ""); ch.data[ch.data_len].tree = NULL;
                    ch.data_len++;
                } else { free(is_proc); if (error) *error = strdup("DATA: valore non letterale"); gmb_compiled_chunk_free(&ch); return -1; }
            }
        }
    }

    /* passata 3: body principale (non procedure) → __main */
    int main_addr = c.code_len;
    /* inizializzatori dei campi STATIC (2.9): emessi qui, PRIMA di qualunque statement
     * utente — main_addr (= entry_point) punta esattamente qui, quindi vengono eseguiti
     * una sola volta all'avvio del programma, mai per-istanza (accodati durante il
     * pre-pass di registrazione delle classi, vedi cc_class_register). */
    for (int i = 0; i < c.n_static_inits; i++) {
        cc_expr(&c, c.static_inits[i].init);
        if (c.error) { free(is_proc); if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }
        cc_byte(&c, GMB_OP_STORE_GLOBAL); cc_u16(&c, c.static_inits[i].slot);
    }
    int has_main = cc_proc_known(&c, "Main");
    /* se esiste SUB Main e il top-level contiene label (handler ON ERROR
       GOSUB/GOTO fra le SUB), il body della prima label NON deve eseguire al
       boot: il fall-through ci arrivava e il RETURN dell'handler terminava
       __main prima del CALL Main. Emetti solo la parte pre-label, poi
       CALL Main + HALT, e i body delle label DOPO l'HALT (restano raggiungibili
       via jump, mai per fall-through — parità C#). */
    int first_label = -1;
    if (has_main) {
        for (int i = 0; i < prog->n; i++)
            /* Bug corretto (2026-08-31, audit lambda/closure): una riga NUMERATA
             * classica (`10 REM ...`, `20 PRINT ...`) non produce mai un nodo
             * GMB_STMT_LABEL — il parser scarta il token LineNumber in testa
             * (gmb_parser_statements.c, gmb_p_parse_stmt_impl) e memorizza il numero
             * SUL nodo statement stesso (campo bas_line), non come nodo a parte. Il
             * controllo qui sopra, cercando SOLO GMB_STMT_LABEL, non trovava mai il
             * confine per un programma "ibrido" (SUB Main + codice a righe numerate,
             * NESSUNA label testuale) — first_label restava -1, stop=prog->n, e
             * l'INTERO blocco di codice a righe numerate veniva eseguito PRIMA di
             * CALL Main (con un END al suo interno, l'HALT arrivava lì e SUB Main non
             * veniva mai chiamata affatto — bug: FunctionsMain.bas eseguiva la riga 20,
             * esplicitamente commentata "non deve mai essere eseguita all'avvio", e
             * nessun output di SUB Main compariva). Fix: riconosce anche il primo
             * statement con bas_line > 0 come confine, non solo un GMB_STMT_LABEL. */
            if (!is_proc[i] && prog->stmts[i]
                && (prog->stmts[i]->kind == GMB_STMT_LABEL || prog->stmts[i]->bas_line > 0)) { first_label = i; break; }
    }
    int stop = (first_label >= 0) ? first_label : prog->n;
    /* costanti utente (CONST): registrazione in prepass, 3 passate per i forward-ref */
    for (int pass = 0; pass < 3; pass++)
        for (int i = 0; i < stop; i++) {
            if (!prog->stmts[i] || prog->stmts[i]->kind != GMB_STMT_CONST || !prog->stmts[i]->target) continue;
            double cv = 0; char* cs = NULL;
            if (cc_const_eval(&c, prog->stmts[i]->value, &cv, &cs))
                cc_const_add(&c, prog->stmts[i]->target, cv, cs);
            else free(cs);
        }
    for (int i = 0; i < stop; i++) {
        if (is_proc[i]) continue;
        cc_stmt(&c, prog->stmts[i]);
        if (c.error) { free(is_proc); free(nested); if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }
    }
    /* se esiste SUB Main, il top-level lo chiama alla fine (parità C#: le DIM
       globali con inizializzatore e il codice top-level girano prima di Main).
       Se il top-level contiene GIA' una chiamata diretta `Main()` (es. i sample che
       chiudono con una riga esplicita), NON duplicare la CALL automatica. */
    int did_explicit_main = 0;
    if (has_main) {
        for (int di = 0; di < stop && !did_explicit_main; di++) {
            if (is_proc[di]) continue;
            gmb_stmt_t* dst = prog->stmts[di];
            if (dst && (dst->kind == GMB_STMT_EXPR || dst->kind == GMB_STMT_CALL)
                && dst->expr && dst->expr->kind == GMB_EXPR_CALL && dst->expr->var
                && dst->expr->nargs == 0 && strcasecmp(dst->expr->var, "Main") == 0)
                did_explicit_main = 1;
        }
    }
    if (has_main && !did_explicit_main) {
        cc_byte(&c, GMB_OP_CALL);
        int mpos = c.code_len;
        cc_u16(&c, 0);
        cc_patch_add(&c, mpos, 1, "Main");
        /* Il valore di ritorno di FUNCTION Main e' l'EXIT CODE (syscall PROGRAM_EXIT=453);
         * SUB Main ritorna NIL → 0. */
        cc_byte(&c, GMB_OP_SYSCALL); cc_u16(&c, 453); cc_byte(&c, 1);
        cc_byte(&c, GMB_OP_POP);
    }
    /* (2026-09-07) DESTRUCTOR a fine programma: finalizza ogni global ancora vivo in ordine
     * INVERSO di registrazione (parity EmitGlobalDestructorFinalizers/Compiler.cs).
     * OP_FINALIZE_GLOBAL è un no-op se il global non è un'istanza classe/UDT con DESTRUCTOR. */
    for (int gi = (int)c.gcount - 1; gi >= 0; gi--) {
        cc_byte(&c, GMB_OP_FINALIZE_GLOBAL);
        cc_u16(&c, (uint16_t)gi);
    }
    cc_byte(&c, GMB_OP_HALT);
    /* body delle label top-level: dopo l'HALT, solo via jump */
    for (int i = stop; i < prog->n; i++) {
        if (is_proc[i]) continue;
        cc_stmt(&c, prog->stmts[i]);
        if (c.error) { free(is_proc); free(nested); if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }
    }

    /* passata 4: procedure */
    /* pre-pass closure-as-arg: registra i param-closure dei callee PRIMA di compilare i
     * corpi (ordine di dichiarazione in pass 4 non garantisce che il call-site di Main sia
     * compilato prima del callee). Vedi cc_scan_closure_body. */
    for (int i = 0; i < prog->n; i++) {
        if (!is_proc[i] || !prog->stmts[i]) continue;
        cc_scan_closure_body(&c, (const gmb_stmt_t* const*)prog->stmts[i]->func_body, prog->stmts[i]->n_funcbody);
    }
    for (int i = 0; i < prog->n; i++) {
        if (!is_proc[i]) continue;
        cc_proc(&c, prog->stmts[i]);
        if (c.error) { free(is_proc); free(nested); if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }
    }
    for (int i = 0; i < n_nested; i++) {
        /* FUNCTION/SUB annidate già emesse come closure inline da cc_emit_nested_function
           (durante la compilazione del corpo contenitore): NON compilarle di nuovo come
           procs flat — doppio corpo, e la cattura per riferimento si perderebbe. */
        if (cc_nested_closure_known(&c, nested[i]->func_name)) continue;
        cc_proc(&c, nested[i]);
        if (c.error) { free(is_proc); free(nested); if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }
    }
    /* passata 4b: corpi di metodi/costruttore delle CLASS registrate (indirizzi
     * patchati SUBITO nel descrittore già in constant pool — vedi cc_class_register/
     * cc_class_patch_method/ctor — nessuna realloc dei nodi VTable/Constructors da qui
     * in poi, i puntatori restano validi). */
    for (int i = 0; i < prog->n; i++) {
        const gmb_stmt_t* s = prog->stmts[i];
        if (!s || s->kind != GMB_STMT_CLASS) continue;
        int ci = cc_class_find(&c, s->class_name);
        if (ci < 0) continue;   /* già scartata da cc_stmt (IMPLEMENTS/STATIC/.../base non trovata) */
        struct cc_class* cl = &c.classes[ci];
        if (cl->is_interface) continue;   /* solo firme, nessun corpo/VTable da patchare */
        for (int k = 0; k < s->n_cls_methods; k++) {
            const gmb_stmt_t* m = s->cls_methods[k];
            if (!m || !m->func_name) continue;
            int is_ctor = strcasecmp(m->func_name, "NEW") == 0;
            int addr = c.code_len;
            c.cur_class = ci;
            cc_class_member(&c, m, is_ctor);
            c.cur_class = -1;
            if (c.error) { free(is_proc); free(nested); if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }
            if (is_ctor) cc_class_patch_ctor(cl, (double)addr);
            else if (cl->has_destructor && strcasecmp(m->func_name, "DESTRUCTOR") == 0) cc_class_patch_dtor(cl, (double)addr);
            else cc_class_patch_method(cl, m->func_name, (double)addr);
        }
    }
    /* indirizzi VTable/Constructors EREDITATI (INHERITS): solo ora, a passata 4b
     * completata per TUTTE le classi, sono noti — vedi cc_class_merge_inherited. */
    cc_class_merge_inherited(&c);
    free(is_proc);
    free(nested);
    for (int i = 0; i < c.nprocset; i++) free(c.procset[i]);
    free(c.procset);
    free(c.udts);
    free(c.classes);
    free(c.static_inits);
    for (int i = 0; i < c.nvt; i++) { free(c.vt_names[i]); free(c.vt_types[i]); }
    free(c.vt_names); free(c.vt_types);
    for (int i = 0; i < c.n_ret_class; i++) { free(c.ret_class_procs[i]); free(c.ret_class_names[i]); }
    free(c.ret_class_procs); free(c.ret_class_names);
    for (int i = 0; i < c.n_nested_closure; i++) free(c.nested_closure_names[i]);
    free(c.nested_closure_names);

    cc_patch_all(&c);
    for (int i = 0; i < c.ncr; i++) free(c.cr_names[i]);
    free(c.cr_names);
    free(c.cr_consts);
    if (c.error) { if (error) *error = c.error; else free(c.error); gmb_compiled_chunk_free(&ch); free(c.code); free(c.lines); return -1; }

    /* entry point: sempre __main (chiama SUB Main se esiste, così gli
       inizializzatori top-level girano prima) */
    ch.entry_point = main_addr;

    ch.global_count = (uint32_t)c.gcount;
    ch.global_names = (char**)calloc((size_t)(c.gcount ? c.gcount : 1), sizeof(char*));
    for (int i = 0; i < c.gcount; i++) ch.global_names[i] = strdup(c.gnames[i]);
    ch.global_name_count = (uint32_t)c.gcount;
    ch.local_count = (uint32_t)c.nloc_cap > 0 ? (uint32_t)c.nloc_cap : 0;

    /* S_eval: la tabella snapshot passa al chunk (move, niente dup); il top-level non ha
     * locali (in_proc=0 → DIM globali) quindi nessun snapshot __main serve. Se il top-level
     * avesse lasciato locali residui in c->locals, sarebbero senza procedura — scartati. */
    ch.ploc_addrs = c.ploc_addrs; ch.ploc_names = c.ploc_names;
    ch.ploc_counts = c.ploc_counts; ch.ploc_count = (uint32_t)(c.nploc > 0 ? c.nploc : 0);
    c.ploc_addrs = NULL; c.ploc_names = NULL; c.ploc_counts = NULL; c.nploc = c.capploc = 0;
    /* Guardia top-level: __main (entry_point) DEVE risolvere una voce — altrimenti un EVAL
     * a top-level (ip oltre l'ultima procedura) erediterebbe i locali dell'ultima proc.
     * Voce vuota = zero locali → fallback globali, corretto (il top-level C non ha locali). */
    {
        int have_main = 0;
        for (uint32_t pi = 0; pi < ch.ploc_count; pi++)
            if ((int)ch.ploc_addrs[pi] == main_addr) { have_main = 1; break; }
        if (!have_main) {
            ch.ploc_addrs = (uint32_t*)realloc(ch.ploc_addrs, (size_t)(ch.ploc_count + 1) * sizeof(uint32_t));
            ch.ploc_names = (char***)realloc(ch.ploc_names, (size_t)(ch.ploc_count + 1) * sizeof(char**));
            ch.ploc_counts = (uint32_t*)realloc(ch.ploc_counts, (size_t)(ch.ploc_count + 1) * sizeof(uint32_t));
            if (ch.ploc_addrs && ch.ploc_names && ch.ploc_counts) {
                ch.ploc_addrs[ch.ploc_count] = (uint32_t)main_addr;
                ch.ploc_names[ch.ploc_count] = NULL;
                ch.ploc_counts[ch.ploc_count] = 0;
                ch.ploc_count++;
            }
        }
    }

    /* TryRegions side-table (parità C# ChunkA1Serializer; lette da OP_PUSH_TRY VM/C) */
    ch.try_region_count = (uint32_t)c.ntryregs;
    if (c.ntryregs > 0) {
        ch.try_ips = (uint32_t*)malloc((size_t)c.ntryregs * sizeof(uint32_t));
        ch.try_finally_ips = (uint32_t*)malloc((size_t)c.ntryregs * sizeof(uint32_t));
        ch.try_end_ips = (uint32_t*)malloc((size_t)c.ntryregs * sizeof(uint32_t));
        ch.try_has_catch = (uint8_t*)malloc((size_t)c.ntryregs * sizeof(uint8_t));
        for (int i = 0; i < c.ntryregs; i++) {
            ch.try_ips[i] = (uint32_t)c.tryregs[i].try_ip;
            ch.try_finally_ips[i] = (uint32_t)c.tryregs[i].finally_ip;
            ch.try_end_ips[i] = (uint32_t)c.tryregs[i].end_ip;
            ch.try_has_catch[i] = (uint8_t)c.tryregs[i].has_catch;
        }
    }
    free(c.tryregs);

    ch.code = c.code; ch.code_len = (uint32_t)c.code_len;
    ch.lines = c.lines;

    if (g_snip_out) {            /* modalità snippet: chunk in memoria, niente serializzazione */
        *g_snip_out = ch;
        return 0;
    }

    int rc = gmb_compiled_chunk_serialize(&ch, out_data, out_len, error);
    gmb_compiled_chunk_free(&ch);
    return rc;
}

/* ── serializer A1/GMBC ── */
typedef struct { uint8_t* b; size_t n, cap; } buf_t;
static void buf_byte(buf_t* b, uint8_t v)
{
    if (b->n >= b->cap) { b->cap = b->cap ? b->cap * 2 : 256; b->b = (uint8_t*)realloc(b->b, b->cap); }
    b->b[b->n++] = v;
}
static void buf_u32(buf_t* b, uint32_t v)
{
    buf_byte(b, (uint8_t)((v >> 24) & 0xFF)); buf_byte(b, (uint8_t)((v >> 16) & 0xFF));
    buf_byte(b, (uint8_t)((v >> 8) & 0xFF)); buf_byte(b, (uint8_t)(v & 0xFF));
}
static void buf_str(buf_t* b, const char* s)
{
    uint32_t len = s ? (uint32_t)strlen(s) : 0;
    buf_u32(b, len);
    for (uint32_t i = 0; i < len; i++) buf_byte(b, (uint8_t)s[i]);
}
static void buf_f64(buf_t* b, double d)
{
    uint64_t bits;
    memcpy(&bits, &d, 8);
    buf_u32(b, (uint32_t)(bits >> 32));
    buf_u32(b, (uint32_t)(bits & 0xFFFFFFFF));
}
/* Formato dict/list annidato: specchio ESATTO di rd_dict in gmb_chunk.c (VM/C) — u32
 * count, poi per entry: [u8 ktag: 5=chiave intera (u32 a seguire) / altro=chiave stringa
 * (rd_str: u32 len + bytes, IL VALORE DI ktag STESSO VIENE IGNORATO in quel caso, quindi
 * qui si scrive sempre 2 per leggibilità)][u8 vtag][valore secondo vtag, ricorsivo per
 * DICT/LIST]. Mai emesso prima d'ora dal compilatore nativo (i tipi CCONST_DICT/LIST
 * esistevano solo lato reader, per i chunk .NET) — introdotto per i descrittori di CLASS
 * (FieldNames/FieldTypes/VTable/Constructors, vedi cc_class_register). */
static void buf_dict_val(buf_t* b, int vtype, double vd, const char* vs, const gmb_cconst_node_t* vnode);
static void buf_dict_tree(buf_t* b, const gmb_cconst_node_t* node)
{
    int n = node ? node->n : 0;
    buf_u32(b, (uint32_t)n);
    for (int i = 0; i < n; i++) {
        const gmb_cconst_entry_t* e = &node->entries[i];
        if (e->key_is_int) { buf_byte(b, 5); buf_u32(b, (uint32_t)e->key_int); }
        else { buf_byte(b, 2); buf_str(b, e->key_str ? e->key_str : ""); }
        buf_dict_val(b, e->val_type, e->val_d, e->val_s, e->val_node);
    }
}
static void buf_dict_val(buf_t* b, int vtype, double vd, const char* vs, const gmb_cconst_node_t* vnode)
{
    buf_byte(b, (uint8_t)vtype);
    switch (vtype) {
    case GMB_CCONST_DOUBLE: buf_f64(b, vd); break;
    case GMB_CCONST_STRING: buf_str(b, vs ? vs : ""); break;
    case GMB_CCONST_INT: buf_u32(b, (uint32_t)(int32_t)vd); break;
    case GMB_CCONST_DATATYPE: buf_byte(b, (uint8_t)vd); break;
    case GMB_CCONST_DICT: case GMB_CCONST_LIST: buf_dict_tree(b, vnode); break;
    default: break;
    }
}
static void buf_const(buf_t* b, const gmb_cconst_t* cc)
{
    buf_byte(b, (uint8_t)cc->type);
    switch (cc->type) {
    case GMB_CCONST_DOUBLE: buf_f64(b, cc->d); break;
    case GMB_CCONST_STRING: buf_str(b, cc->s); break;
    case GMB_CCONST_INT: buf_u32(b, (uint32_t)(int32_t)cc->d); break;
    case GMB_CCONST_DATATYPE: buf_byte(b, (uint8_t)cc->d); break;
    case GMB_CCONST_DICT: case GMB_CCONST_LIST: buf_dict_tree(b, cc->tree); break;
    default: break;
    }
}

int gmb_compiled_chunk_serialize(const gmb_compiled_chunk_t* c, uint8_t** out_data, size_t* out_len, char** error)
{
    (void)error;
    buf_t b = {0};
    /* MAGIC + VERSION */
    buf_byte(&b, 'G'); buf_byte(&b, 'M'); buf_byte(&b, 'B'); buf_byte(&b, 'C');
    buf_byte(&b, 1);
    buf_u32(&b, (uint32_t)c->entry_point);
    buf_u32(&b, c->global_count);
    buf_u32(&b, c->data_len);
    buf_u32(&b, c->code_len);
    buf_u32(&b, c->consts_len);
    /* code */
    for (uint32_t i = 0; i < c->code_len; i++) buf_byte(&b, c->code[i]);
    /* lines */
    for (uint32_t i = 0; i < c->code_len; i++) buf_u32(&b, c->lines ? c->lines[i] : 0);
    /* constants */
    for (uint32_t i = 0; i < c->consts_len; i++) buf_const(&b, &c->consts[i]);
    /* data */
    for (uint32_t i = 0; i < c->data_len; i++) buf_const(&b, &c->data[i]);
    /* procs */
    buf_u32(&b, c->proc_count);
    for (uint32_t i = 0; i < c->proc_count; i++) { buf_str(&b, c->proc_names[i]); buf_u32(&b, c->proc_addrs[i]); }
    /* aritys */
    buf_u32(&b, c->arity_count);
    for (uint32_t i = 0; i < c->arity_count; i++) { buf_u32(&b, c->arity_addrs[i]); buf_u32(&b, c->arity_vals[i]); }
    /* global names (coda opzionale) */
    buf_u32(&b, c->global_name_count);
    for (uint32_t i = 0; i < c->global_name_count; i++) buf_str(&b, c->global_names[i]);
    /* local count + try regions (coda A1) */
    buf_u32(&b, c->local_count);
    buf_u32(&b, c->try_region_count);
    for (uint32_t i = 0; i < c->try_region_count; i++) {
        buf_u32(&b, c->try_ips[i]);
        buf_u32(&b, c->try_finally_ips[i]);
        buf_u32(&b, c->try_end_ips[i]);
        buf_u32(&b, c->try_has_catch[i] ? 1u : 0u);
    }
    /* proc local names (coda opzionale S_eval): u32 count, per voce u32 addr,
     * u32 nameCount, nameCount stringhe ("" = slot anonimo). */
    buf_u32(&b, c->ploc_count);
    for (uint32_t i = 0; i < c->ploc_count; i++) {
        buf_u32(&b, c->ploc_addrs[i]);
        buf_u32(&b, c->ploc_counts[i]);
        for (uint32_t j = 0; j < c->ploc_counts[i]; j++)
            buf_str(&b, (c->ploc_names[i] && c->ploc_names[i][j]) ? c->ploc_names[i][j] : "");
    }

    *out_data = b.b;
    *out_len = b.n;
    return 0;
}

void gmb_compiled_chunk_free(gmb_compiled_chunk_t* c)
{
    if (!c) return;
    free(c->code);
    free(c->lines);
    for (uint32_t i = 0; i < c->consts_len; i++) { free(c->consts[i].s); cc_node_free(c->consts[i].tree); }
    free(c->consts);
    for (uint32_t i = 0; i < c->data_len; i++) { free(c->data[i].s); cc_node_free(c->data[i].tree); }
    free(c->data);
    for (uint32_t i = 0; i < c->proc_count; i++) free(c->proc_names[i]);
    free(c->proc_names); free(c->proc_addrs);
    free(c->arity_addrs); free(c->arity_vals);
    for (uint32_t i = 0; i < c->global_name_count; i++) free(c->global_names[i]);
    free(c->global_names);
    if (c->ploc_names) {
        for (uint32_t i = 0; i < c->ploc_count; i++) {
            if (c->ploc_names[i]) {
                for (uint32_t j = 0; j < c->ploc_counts[i]; j++) free(c->ploc_names[i][j]);
                free(c->ploc_names[i]);
            }
        }
        free(c->ploc_names);
    }
    free(c->ploc_addrs); free(c->ploc_counts);
    free(c->try_ips); free(c->try_finally_ips); free(c->try_end_ips); free(c->try_has_catch);
    memset(c, 0, sizeof(*c));
}

/* ============================================================================
 * REQUIRE — risoluzione pacchetti nel compilatore C nativo.
 * REQUIRE "nome[@ver]" -> risolve ~/.gmb/packages (o GMB_PKG_CACHE), legge
 * gmb-package.json e INIETTA il contenuto degli export .inc/.bas PRIMA del sorgente,
 * cosi' le DECLARE/simboli del pacchetto sono visibili al parser. (La lib FFI viene
 * poi trovata a runtime dalla ricerca nella cache in gmb_ffi_dl.c.)
 * ============================================================================ */
#if !defined(_WIN32) && !defined(_WIN64)
#include <dirent.h>
#endif

/* Prefisso case-insensitive: serve a riconoscere la keyword VERSION senza dipendere da
 * strncasecmp (non usata altrove in questo file). */
static int cc_starts_ci(const char* h, const char* prefix)
{
    size_t i;
    for (i = 0; prefix[i]; i++) {
        char a = h[i], b = prefix[i];
        if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
        if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
        if (a != b) return 0;
        if (a == 0) return 1;
    }
    return 1;
}

static const char* cc_strcasestr(const char* h, const char* n)
{
    size_t nl = strlen(n);
    if (nl == 0) return h;
    for (; *h; h++)
        if (strncasecmp(h, n, nl) == 0) return h;
    return NULL;
}

static void pkg_buf_add(char** b, size_t* n, size_t* cap, const char* s, size_t l)
{
    if (*n + l + 2 > *cap) { while (*n + l + 2 > *cap) *cap = *cap ? *cap * 2 : 4096; *b = (char*)realloc(*b, *cap); }
    memcpy(*b + *n, s, l); *n += l; (*b)[(*n)++] = '\n'; (*b)[*n] = 0;
}

static int pkg_has_ext(const char* name)
{
    size_t n = strlen(name);
    return (n > 4 && (strcasecmp(name + n - 4, ".inc") == 0 || strcasecmp(name + n - 4, ".bas") == 0));
}

static void pkg_add_file(const char* fp, char** b, size_t* n, size_t* cap)
{
    FILE* f = fopen(fp, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    if (l > 0) { char* tmp = (char*)malloc((size_t)l); if (tmp && fread(tmp, 1, (size_t)l, f) == (size_t)l) pkg_buf_add(b, n, cap, tmp, (size_t)l); free(tmp); }
    fclose(f);
}

static void pkg_inject_path(const char* base, const char* rel, char** b, size_t* n, size_t* cap)
{
    char fp[8192];
    snprintf(fp, sizeof(fp), "%s/%s", base, rel);
#if !defined(_WIN32) && !defined(_WIN64)
    { DIR* d = opendir(fp);
      if (d) { struct dirent* e;
          while ((e = readdir(d))) { if (e->d_name[0] == '.' || !pkg_has_ext(e->d_name)) continue;
              { char fp2[8192]; snprintf(fp2, sizeof(fp2), "%s/%s", fp, e->d_name); pkg_add_file(fp2, b, n, cap); } }
          closedir(d); return; } }
#endif
    if (pkg_has_ext(rel)) pkg_add_file(fp, b, n, cap);
}

/* ============================================================================
 * REQUIRE — risoluzione dei pacchetti (parita' con il lato C#).
 *
 * SINTASSI CANONICA (identica su tutti i backend):
 *     REQUIRE "nome"                      -> versione PIU' RECENTE presente nel package manager
 *     REQUIRE "nome" VERSION "1.0"        -> vincolo di versione
 * Forma storica ancora accettata per compatibilita': REQUIRE "nome@1.0" (la '@' dentro
 * la stringa). Non e' canonica: le guide vanno allineate alla forma con VERSION.
 *
 * PARITA' (Fase 0, 2026-10-06). Prima questo resolver era una scansione testuale che:
 *   - non riconosceva la keyword VERSION;
 *   - senza versione sceglieva il nome di cartella MAX LESSICOGRAFICO ("1.5.6" > "1.5.10"),
 *     quindi poteva scegliere la versione SBAGLIATA;
 *   - con versione usava la stringa verbatim come nome cartella ("^1.0" -> inesistente);
 *   - su pacchetto mancante faceva `continue`: NESSUN errore, compilazione in silenzio;
 *   - non risolveva le dipendenze TRANSITIVE (leggeva solo "exports").
 * Ora: vincoli valutati con gmb_semver (stessa grammatica del C#), massimo SemVer,
 * errore esplicito, transitive con guardia sui cicli.
 *
 * REQUIRE come CLAUSOLA di un comando (es. `AI VERIFY ... REQUIRE "<expr>"`) NON e' una
 * direttiva pacchetto: si accetta solo a inizio statement, come fa il C# in
 * `Parser.ScanRequirementsFromTokens` (guardia atStatementStart).
 * ============================================================================ */

/* Il pacchetto e' gia' iniettato/in coda? 1 = si', 0 = no. */
static int pkg_seen(char seen[64][200], int* nseen, const char* name)
{
    int i;
    for (i = 0; i < *nseen; i++) if (strcmp(seen[i], name) == 0) return 1;
    if (*nseen < 64) snprintf(seen[(*nseen)++], 200, "%s", name);
    return 0;
}

/* Estrae il primo valore stringa ("...") che segue `from`. Torna 1 se trovato. */
static int pkg_next_string(const char* from, char* out, size_t outsz)
{
    const char* q = strchr(from, '"');
    const char* e;
    size_t n;
    if (!q) return 0;
    q++;
    e = strchr(q, '"');
    if (!e) return 0;
    n = (size_t)(e - q);
    if (n >= outsz) n = outsz - 1;
    memcpy(out, q, n);
    out[n] = 0;
    return 1;
}

/* "nome" -> nome; "nome@1.0" -> nome + vincolo (forma storica). */
static void pkg_split_at(const char* spec, char* name, size_t namesz, char* ver, size_t versz)
{
    const char* at = strchr(spec, '@');
    size_t n;
    ver[0] = 0;
    if (at) {
        n = (size_t)(at - spec);
        if (n >= namesz) n = namesz - 1;
        memcpy(name, spec, n);
        name[n] = 0;
        snprintf(ver, versz, "%s", at + 1);
    } else {
        snprintf(name, namesz, "%s", spec);
    }
}

/* Sceglie la versione da usare per `name` dentro `pkgdir`:
 *   - constraint "*"/vuoto  -> massimo SemVer (NON lessicografico);
 *   - constraint espresso   -> massimo SemVer che lo soddisfa;
 * Restituisce 1 e scrive la versione in `best`, 0 se nessuna versione soddisfa.
 * `err` riceve un messaggio esplicito quando il pacchetto non e' installato. */
static int pkg_pick_version(const char* pkgdir, const char* constraint,
                            char* best, size_t bestsz, char* err, size_t errsz)
{
    DIR* d;
    struct dirent* de;
    gmb_semver_t bestv;
    int have = 0;
    const char* c = (constraint && *constraint) ? constraint : "*";

    d = opendir(pkgdir);
    if (!d) {
        snprintf(err, errsz, "Pacchetto '%s' non trovato: cartella %s assente.", pkgdir, pkgdir);
        return 0;
    }
    while ((de = readdir(d))) {
        gmb_semver_t v;
        if (de->d_name[0] == '.') continue;
        if (!gmb_semver_parse(de->d_name, &v)) continue;      /* non SemVer: non e' una versione */
        if (!gmb_semver_satisfies(&v, c)) continue;
        if (!have || gmb_semver_cmp(&v, &bestv) > 0) { bestv = v; have = 1; }
    }
    closedir(d);
    if (!have) {
        snprintf(err, errsz,
                 "Nessuna versione di '%s' in %s soddisfa il vincolo \"%s\"%s.",
                 pkgdir, pkgdir, c,
                 (!strcmp(c, "*") || !*constraint)
                     ? " (disponibili: nessuna versione SemVer installata)"
                     : "");
        return 0;
    }
    snprintf(best, bestsz, "%d.%d.%d%s%s", bestv.major, bestv.minor, bestv.patch,
             bestv.prerelease[0] ? "-" : "", bestv.prerelease);
    return 1;
}

/* Legge "exports" dal manifest e inietta i file (.inc/.bas) nel buffer.
 * `queue`/`qn` ricevono i pacchetti ancora da risolvere (transitive). */
static void pkg_inject_exports(const char* pkgver, const char* mf,
                               char** b, size_t* n, size_t* cap,
                               char queue[64][256], int* qn)
{
    FILE* f = fopen(mf, "rb");
    char* man = NULL;
    long ml;
    char* ex;
    char* end;
    char* r;
    if (!f) return;
    fseek(f, 0, SEEK_END); ml = ftell(f); fseek(f, 0, SEEK_SET);
    if (ml <= 0 || ml > (1 << 20)) { fclose(f); return; }   /* manifest assurdo: ignora */
    man = (char*)malloc((size_t)ml + 1);
    if (!man) { fclose(f); return; }
    if (fread(man, 1, (size_t)ml, f) != (size_t)ml) { free(man); fclose(f); return; }
    man[ml] = 0; fclose(f);

    ex = strstr(man, "\"exports\"");
    if (ex) ex = strchr(ex, '[');
    if (ex) {
        end = strchr(ex, ']');
        r = ex + 1;
        while (end && r < end) {
            char* q1 = strchr(r, '"');
            char* q2;
            char rel[512];
            if (!q1 || q1 >= end) break;
            q2 = strchr(q1 + 1, '"');
            if (!q2 || q2 > end) break;
            { size_t rl = (size_t)(q2 - q1 - 1); if (rl >= sizeof(rel)) rl = sizeof(rel) - 1; memcpy(rel, q1 + 1, rl); rel[rl] = 0; }
            pkg_inject_path(pkgver, rel, b, n, cap);
            r = q2 + 1;
        }
    }

    /* Dipendenze transitive: "dependencies": { "nome": "vincolo", ... }.
     * Il C# le risolve con una coda e guardia sui cicli; qui idem. */
    ex = strstr(man, "\"dependencies\"");
    if (ex && *qn < 64) {          /* qn qui e' un int* (parametro): niente `qn < 64` */
        char* ob = strchr(ex, '{');
        char* cb = ob ? strchr(ob, '}') : NULL;
        if (ob && cb) {
            char* p = ob + 1;
            while (p && p < cb) {
                char dep[200], dver[128];
                char* k1 = strchr(p, '"');
                char *k2, *colon;
                if (!k1 || k1 >= cb) break;
                k2 = strchr(k1 + 1, '"');
                if (!k2 || k2 > cb) break;
                { size_t n = (size_t)(k2 - k1 - 1); if (n >= sizeof(dep)) n = sizeof(dep) - 1; memcpy(dep, k1 + 1, n); dep[n] = 0; }
                colon = strchr(k2, ':');
                if (!colon || colon > cb) { p = k2 + 1; continue; }
                if (!pkg_next_string(colon, dver, sizeof dver)) dver[0] = 0;
                if (dep[0] && *qn < 64) {
                    snprintf(queue[*qn], 256, "%s%s%s", dep, *dver ? "@" : "", dver);
                    (*qn)++;
                }
                p = colon + 1;
            }
        }
    }
    free(man);
}

/* Directory di un path (stringa nel buffer `out`). Usata dai caller per passare il project_dir
 * del pacchettiera. Se il path non ha '/', risulta "." (cwd). */
const char* proj_dir_of(const char* path)
{
    static _Thread_local char buf[4096];
    const char* slash = path ? strrchr(path, '/') : NULL;
    size_t n;
    if (!path || !*path) { snprintf(buf, sizeof buf, "."); return buf; }
    if (!slash) { snprintf(buf, sizeof buf, "."); return buf; }
    n = (size_t)(slash - path);
    if (n == 0) { snprintf(buf, sizeof buf, "/"); return buf; }   /* "/foo.bas" */
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, path, n);
    buf[n] = 0;
    return buf;
}

/* Legge gmb-package.lock dal project_dir nella mappa nome->versione.
 * Formato identico a PackageManager.ReadLockFile: righe "#" e vuote ignorate, "nome@versione|...".
 * Ritorna il numero di voci lette (0 = nessun lock). */
/* Legge da <project_dir>/gmb-package.json la sezione "dependencies" ({ "nome": "vincolo", ... })
 * e accoda "nome@vincolo" (o solo "nome") alla coda REQUIRE. Fase 4 della parità col C#: il
 * compilatore C unisce le dipendenze del MANIFESTO di progetto a quelle dei REQUIRE.
 * Mini-parser JSON self-contained (gmb_bytecode non linka gmb_runtime_c). Ritorna il n. aggiunto. */
static int pkg_read_manifest_deps(const char* project_dir, char queue[][256], int max, int* qn)
{
    char path[4096];
    FILE* f;
    long len;
    char* js;
    const char* d;
    const char* b;
    const char* p;
    int added = 0;
    if (!project_dir || !*project_dir) return 0;
    snprintf(path, sizeof path, "%s/gmb-package.json", project_dir);
    f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (1 << 20)) { fclose(f); return 0; }
    js = (char*)malloc((size_t)len + 1);
    if (!js) { fclose(f); return 0; }
    if (fread(js, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(js); return 0; }
    js[len] = 0; fclose(f);

    d = strstr(js, "\"dependencies\"");
    b = d ? strchr(d + 14, '{') : NULL;
    if (b) {
        p = b + 1;
        while (*p && *p != '}')
        {
            char key[200];
            char val[128];
            size_t ki = 0, vi = 0;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',') p++;
            if (*p != '"') break;
            p++;
            while (*p && *p != '"' && ki < sizeof(key) - 1) key[ki++] = *p++;
            key[ki] = 0;
            if (*p == '"') p++;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
            if (*p != ':') break;
            p++;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
            val[0] = 0;
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && vi < sizeof(val) - 1) val[vi++] = *p++;
                val[vi] = 0;
                if (*p == '"') p++;
            } else {
                /* valore oggetto/numero: consuma fino alla virgola o al '}' di chiusura */
                while (*p && *p != ',' && *p != '}') p++;
            }
            if (key[0] && *qn < max) {
                snprintf(queue[*qn], 256, "%s%s%s", key, val[0] ? "@" : "", val);
                (*qn)++;
                added++;
            }
        }
    }
    free(js);
    return added;
}

static int pkg_read_lock(const char* project_dir, char names[64][200], char vers[64][64])
{
    char path[4096];
    FILE* f;
    char line[1024];
    int n = 0;
    if (!project_dir || !*project_dir) return 0;
    snprintf(path, sizeof path, "%s/gmb-package.lock", project_dir);
    f = fopen(path, "rb");
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char* l = line;
        char* at;
        char* bar;
        size_t i;
        char nm[200], vr[64];
        while (*l == ' ' || *l == '\t') l++;
        i = strlen(l);
        while (i > 0 && (l[i - 1] == '\n' || l[i - 1] == '\r' || l[i - 1] == ' ')) l[--i] = 0;
        if (l[0] == 0 || l[0] == '#') continue;
        at = strchr(l, '@');
        if (!at || at == l) continue;
        i = (size_t)(at - l);
        if (i >= sizeof nm) i = sizeof nm - 1;
        memcpy(nm, l, i);
        nm[i] = 0;
        bar = strchr(at + 1, '|');
        if (bar) *bar = 0;
        snprintf(vr, sizeof vr, "%s", at + 1);
        if (bar) *bar = '|';
        if (vr[0] == 0 || n >= 64) continue;
        snprintf(names[n], 200, "%s", nm);
        snprintf(vers[n], 64, "%s", vr);
        n++;
    }
    fclose(f);
    return n;
}

/* Versione pinnata dal lock per 'name', o NULL. */
static const char* pkg_locked_version(char names[64][200], char vers[64][64], int n, const char* name)
{
    int i;
    for (i = 0; i < n; i++) if (strcmp(names[i], name) == 0) return vers[i];
    return NULL;
}

/* Esito dell'ultima risoluzione REQUIRE: 1 se almeno un pacchetto non e' stato risolto.
 * NECESSARIO perche' gmb_pkg_collect_requires ritorna NULL sia quando non c'e' nulla da iniettare
 * sia quando la risoluzione FALLISCE: senza questo flag il chiamante non puo' distinguere
 * "nessun REQUIRE" da "REQUIRE non risolto" e continuerebbe in SILENZIO (il fallimento arriverebbe
 * dopo, come fuorviante "funzione non definita"). */
static int gmb_pkg_failed_flag = 0;

int gmb_pkg_require_failed(void)
{
    return gmb_pkg_failed_flag;
}

/* Copia di s con i COMMENTI (`'...` a fine riga) sostituiti da spazi, rispettando le stringhe
 * "..." — così un REQUIRE dentro un commento NON viene intercettato (parità col C# token-based:
 * ScanRequirementsFromTokens ignora i commenti). */
static char* gmb_mask_comments(const char* s)
{
    char* m = (char*)malloc(strlen(s) + 1);
    if (!m) return NULL;
    strcpy(m, s);
    int in_str = 0;
    for (char* q = m; *q; q++) {
        if (in_str) { if (*q == '"') in_str = 0; continue; }
        if (*q == '"') { in_str = 1; continue; }
        if (*q == '\'') { while (*q && *q != '\n') { *q = ' '; q++; } if (!*q) break; }
    }
    return m;
}

/* Cache dei pacchetti impostata dal chiamante (driver/CLI): <install_root>/packages, cioè la
 * cartella `packages/` del progetto. Nessun default su ~/.gmb/packages: quel percorso è
 * eliminato (i pacchetti stanno solo nel progetto). */
static const char* g_pkg_cache_dir = NULL;
void gmb_pkg_set_cache_dir(const char* dir) { g_pkg_cache_dir = (dir && *dir) ? dir : NULL; }

char* gmb_pkg_collect_requires(const char* main_src, const char* project_dir)
{
    const char* cache = getenv("GMB_PKG_CACHE");
    char cachebuf[4096];
    char* b = NULL;
    size_t n = 0, cap = 0;
    const char* p;
    char seen[64][200];  int nseen = 0;
    char queue[64][256]; int qn = 0; int qi = 0;
    int errors = 0;

    gmb_pkg_failed_flag = 0;

    if (main_src == NULL) return NULL;
    char* scan = gmb_mask_comments(main_src);
    const char* base = scan ? scan : main_src;
    if (!cache || !*cache) {
        /* CACHE DEI PACCHETTI: la cartella `packages/` del PROGETTO (GMBasic/packages e
         * GMBasicMonoRepo/packages) — impostata dai driver con <install_root>/packages.
         * Il percorso `~/.gmb/packages/<nome>/<versione>/` è stato ELIMINATO: i pacchetti
         * stanno solo nel progetto (direttiva utente 2026-10-28).
         * `GMB_PKG_CACHE` resta come override esplicito. */
        if (g_pkg_cache_dir && *g_pkg_cache_dir) {
            snprintf(cachebuf, sizeof(cachebuf), "%s", g_pkg_cache_dir);
            cache = cachebuf;
        } else {
            return NULL;   /* nessuna cache di progetto impostata: nessun pacchetto risolvibile */
        }
    }

    /* 1) scansione delle direttive a inizio statement (non le clausole dei comandi) */
    p = base;
    while ((p = cc_strcasestr(p, "REQUIRE")) != NULL) {
        char name[200], ver[64];
        int at_stmt_start = 1;
        const char* ls = p;
        char q0[512];
        const char* e;

        /* guardia atStatementStart: dal carattere precedente fino a ':'/newline deve
         * esserci solo spazio bianco (equivale al C#: inizio file o dopo Newline/Colon). */
        while (ls > base && ls[-1] != '\n' && ls[-1] != ':') {
            if (ls[-1] != ' ' && ls[-1] != '\t' && ls[-1] != '\r') { at_stmt_start = 0; break; }
            ls--;
        }

        {
            const char* q = p + 7;
            while (*q == ' ' || *q == '\t') q++;
            if (!at_stmt_start || *q != '"') { p += 7; continue; }
            /* nome (stringa) */
            e = strchr(q + 1, '"');
            if (!e) break;
            { size_t sl = (size_t)(e - (q + 1)); if (sl >= sizeof(q0)) sl = sizeof(q0) - 1; memcpy(q0, q + 1, sl); q0[sl] = 0; }
            p = e + 1;
            pkg_split_at(q0, name, sizeof(name), ver, sizeof(ver));

            /* VERSION "vincolo" (forma canonica, stessa sintassi del C#:
             * ParseRequireStatement -> Match(TokenKind.Version)). */
            {
                const char* r = p;
                while (*r == ' ' || *r == '\t' || *r == '\r' || *r == '\n') r++;
                if (cc_starts_ci(r, "VERSION")
                    && !(isalnum((unsigned char)r[7]) || r[7] == '_')) {
                    const char* a = r + 7;
                    while (*a == ' ' || *a == '\t') a++;
                    if (*a == '"') {
                        char v2[128];
                        if (pkg_next_string(a, v2, sizeof v2)) {
                            snprintf(ver, sizeof(ver), "%s", v2);
                            /* avanza p oltre la stringa del vincolo */
                            { const char* q1 = strchr(a, '"');
                              if (q1) { const char* q2 = strchr(q1 + 1, '"'); if (q2) p = q2 + 1; } }
                        } else {
                            fprintf(stderr, "REQUIRE: %s — manca la stringa dopo VERSION.\n", name);
                            errors++;
                            continue;
                        }
                    } else {
                        fprintf(stderr, "REQUIRE: %s — VERSION senza stringa di versione.\n", name);
                        errors++;
                        continue;
                    }
                }
            }
            if (name[0] == 0) continue;
            if (qn < 64) {
                snprintf(queue[qn], 256, "%s%s%s", name, *ver ? "@" : "", ver);
                qn++;
            }
        }
    }

    /* 1bis) Fase 4: le dipendenze del MANIFESTO di progetto (gmb-package.json) entrano in coda
     * insieme ai REQUIRE. Il lock, se presente, ha la precedenza (stessa semantica del C#). */
    {
        int nd = pkg_read_manifest_deps(project_dir, queue, 64, &qn);
        if (nd > 0) fprintf(stderr, "REQUIRE: manifesto di progetto (%d dipendenze) da %s\n", nd, project_dir);
    }

    /* 2) risoluzione (coda: include le transitive).
     * Se c'e' un gmb-package.lock nel progetto, il pacchetto e' PINNATO alla versione registrata:
     * e' cio' che rende la build riproducibile (stessa versione su ogni backend/macchina). */
    {
        char ln[64][200], lv[64][64];
        int nlock = pkg_read_lock(project_dir, ln, lv);
        if (nlock > 0) fprintf(stderr, "REQUIRE: lock %s (%d pacchetti)\n", project_dir, nlock);

        while (qi < qn) {
            char qspec[256], name[200], ver[128], pkgdir[4096], best[160], pkgver[4096], mf[8192];
            char err[1024] = "";
            snprintf(qspec, sizeof(qspec), "%s", queue[qi++]);
            pkg_split_at(qspec, name, sizeof(name), ver, sizeof(ver));
            if (name[0] == 0) continue;
            if (pkg_seen(seen, &nseen, name)) continue;

            /* ORDINE DI RICERCA (direttiva utente): 1) VENDOR di progetto (pin esplicito,
             * `<dir del .bas>/vendor`), 2) INSTALLAZIONE LOCALE dell'app
             * (`<dir dell'eseguibile>/packages`, modello node_modules), 3) CACHE condivisa.
             * A ogni livello si applica il vincolo SemVer: se lì nessuna versione lo soddisfa
             * si passa al livello successivo (un'installazione locale vecchia non blocca un
             * REQUIRE "^2"). Path FISSI, nessuna risalita di parent. */
            {
                char roots[3][4096];
                int nr = 0;
                if (project_dir && *project_dir) {
                    char vbuf[4096];
                    snprintf(vbuf, sizeof(vbuf), "%s/vendor/%s", project_dir, name);
                    DIR* vd = opendir(vbuf);
                    if (vd) { closedir(vd); snprintf(roots[nr++], sizeof(roots[0]), "%s", vbuf); }
                }
                {
                    char exe[4096], lbuf[4096];
                    if (gmb_exe_dir(exe, sizeof exe)) {
                        snprintf(lbuf, sizeof(lbuf), "%s/packages/%s", exe, name);
                        DIR* ld = opendir(lbuf);
                        if (ld) { closedir(ld); snprintf(roots[nr++], sizeof(roots[0]), "%s", lbuf); }
                    }
                }
                snprintf(roots[nr++], sizeof(roots[0]), "%s/%s", cache, name);

                int picked = 0;
                for (int ri = 0; ri < nr && !picked; ri++) {
                    err[0] = 0;
                    if (pkg_pick_version(roots[ri], ver, best, sizeof(best), err, sizeof(err))) {
                        snprintf(pkgdir, sizeof(pkgdir), "%s", roots[ri]);
                        picked = 1;
                    }
                }
                if (!picked) {
                    /* ERRORE ESPLICITO: prima era un `continue` silenzioso e il programma
                     * compilava senza il pacchetto (fallo tardivo: "funzione non definita"). */
                    if (!err[0]) snprintf(err, sizeof(err), "Nessuna versione del pacchetto '%s' soddisfa il vincolo.", name);
                    fprintf(stderr, "REQUIRE: %s\n", err);
                    errors++;
                    continue;
                }
            }
            snprintf(pkgver, sizeof(pkgver), "%s/%s", pkgdir, best);
            snprintf(mf, sizeof(mf), "%s/gmb-package.json", pkgver);
            const char* locked = pkg_locked_version(ln, lv, nlock, name);
            if (locked && strcmp(locked, best) != 0) {
                /* Il lock ha la precedenza sul vincolo: e' il punto del lock. Cerca la versione
                 * pinnata in TUTTI i livelli (vendor/local/cache), non solo in quello scelto. */
                int pinned = 0;
                char roots2[3][4096];
                int nr2 = 0;
                if (project_dir && *project_dir) {
                    char vbuf[4096];
                    snprintf(vbuf, sizeof(vbuf), "%s/vendor/%s", project_dir, name);
                    DIR* vd = opendir(vbuf);
                    if (vd) { closedir(vd); snprintf(roots2[nr2++], sizeof(roots2[0]), "%s", vbuf); }
                }
                {
                    char exe[4096], lbuf[4096];
                    if (gmb_exe_dir(exe, sizeof exe)) {
                        snprintf(lbuf, sizeof(lbuf), "%s/packages/%s", exe, name);
                        DIR* ld = opendir(lbuf);
                        if (ld) { closedir(ld); snprintf(roots2[nr2++], sizeof(roots2[0]), "%s", lbuf); }
                    }
                }
                snprintf(roots2[nr2++], sizeof(roots2[0]), "%s/%s", cache, name);
                for (int ri = 0; ri < nr2 && !pinned; ri++) {
                    char b2[160];
                    if (pkg_pick_version(roots2[ri], locked, b2, sizeof(b2), err, sizeof(err))
                        && strcmp(b2, locked) == 0) {
                        snprintf(pkgdir, sizeof(pkgdir), "%s", roots2[ri]);
                        snprintf(best, sizeof best, "%s", locked);
                        pinned = 1;
                    }
                }
                if (!pinned) {
                    fprintf(stderr, "REQUIRE: Lock pin %s@%s non in cache (serve gmb pkg install).\n", name, locked);
                    errors++;
                    continue;
                }
                snprintf(pkgver, sizeof(pkgver), "%s/%s", pkgdir, best);
                snprintf(mf, sizeof(mf), "%s/gmb-package.json", pkgver);
            }
            fprintf(stderr, "REQUIRE: %s -> %s\n", name, best);
            pkg_inject_exports(pkgver, mf, &b, &n, &cap, queue, &qn);
        }
    }

    if (errors > 0) {
        fprintf(stderr, "REQUIRE: %d pacchetto/i non risolto/i — compilazione annullata.\n", errors);
        gmb_pkg_failed_flag = 1;   /* il chiamante deve poterlo rilevare: vedi gmb_pkg_require_failed */
        free(b);
        free(scan);
        return NULL;
    }
    free(scan);
    return b;
}

/* ---- Fase 1 CodeGen C (FaseC-CodegenLoop-C): engine compile di un .bas ---- */
/* Compila un file .bas in chunk A1/GMBC (pipeline 100% C: lexer -> macro -> parser ->
   gmb_compile). Ritorna 0 ok (out_data/out_len heap) o non-zero (diag riempito, heap).
   Mai no-op: ogni stato errore ha un messaggio esplicito. */
int gmb_compile_bas_file(const char* path, uint8_t** out_data, size_t* out_len, char** diag) {
    if (out_data) *out_data = NULL;
    if (out_len) *out_len = 0;
    if (diag) *diag = NULL;
    if (!path) { if (diag) *diag = strdup("gmb_compile_bas_file: path NULL"); return -1; }
    FILE* f = fopen(path, "rb");
    if (!f) { if (diag) *diag = strdup("file non trovato"); return -1; }
    fseek(f, 0, SEEK_END); long alen = ftell(f); fseek(f, 0, SEEK_SET);
    char* src = (char*)malloc((size_t)alen + 1);
    if (!src) { fclose(f); if (diag) *diag = strdup("memoria"); return -1; }
    if (alen > 0 && fread(src, 1, (size_t)alen, f) != (size_t)alen) { fclose(f); free(src); if (diag) *diag = strdup("lettura fallita"); return -1; }
    src[alen] = 0; fclose(f);

    /* REQUIRE: inietta gli export (.inc/.bas) dei pacchetti risolti PRIMA del sorgente.
     * project_dir = directory del .bas: se contiene gmb-package.lock, le versioni sono pinnate. */
    {
        char pdir[4096];
        const char* slash = strrchr(path, '/');
        char* reqsrc;
        if (slash) { size_t pl = (size_t)(slash - path); if (pl >= sizeof pdir) pl = sizeof pdir - 1;
                     memcpy(pdir, path, pl); pdir[pl] = 0; }
        else snprintf(pdir, sizeof pdir, ".");
        reqsrc = gmb_pkg_collect_requires(src, pdir);
        char* combined = NULL;
        if (reqsrc != NULL) {
            size_t rl = strlen(reqsrc), sl = strlen(src);
            combined = (char*)malloc(rl + sl + 2);
            if (combined) { memcpy(combined, reqsrc, rl); combined[rl] = '\n'; memcpy(combined + rl + 1, src, sl + 1); }
            free(reqsrc);
        }
        if (combined) { free(src); src = combined; }
    }

    gmb_lex_result_t lx = gmb_lexer_scan(src, (long)strlen(src));
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    lx.owned_src = NULL;
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    int rc = -1;
    if (perr) {
        if (diag) *diag = strdup(perr);
        free(perr);
    } else {
        uint8_t* data = NULL; size_t len = 0; char* cerr = NULL;
        if (gmb_compile(&prog, &data, &len, &cerr) == 0) {
            if (out_data) *out_data = data; else if (data) free(data);
            if (out_len) *out_len = len;
            rc = 0;
        } else {
            if (diag) *diag = cerr ? cerr : strdup("compilazione fallita");
            else if (cerr) free(cerr);
            rc = -1;
        }
    }
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx);
    free(src);
    return rc;
}

/* ── gmb_compile_snippet: frammento BASIC compilato contro l'OSPITE ────────────
 * Usato da EVAL/& (VM/C) a runtime: il frammento è avvolto in una procedura
 * (`__GMB_EVAL__`) e compilato con il compilatore VERO, con globali/locali/proc
 * risolti sulle tabelle del chunk ospite. I nomi ignoti (campi DBF, variabili
 * nuove) NON creano slot globali: vengono dichiarati locali (`DIM`) e il driver
 * li pre/post-carica dal record attivo. Nessun interprete parallelo. */
/* Costruisce il testo del wrapper: `stmt_mode` = sequenza di statement (SUB),
 * altrimenti espressione (FUNCTION + RETURN). */
static char* snip_wrap_text(const char* src, int mode, const char* ret_target,
                            char** params, int nparams)
{
    size_t need = strlen(src) + 256 + (ret_target ? strlen(ret_target) + 16 : 0);
    for (int i = 0; i < nparams; i++) need += strlen(params[i]) + 8;
    char* w0 = (char*)malloc(need);
    if (!w0) return NULL;
    char* w = w0;
    if (mode == 1) w += sprintf(w, "SUB __GMB_EVAL__(");
    else           w += sprintf(w, "FUNCTION __GMB_EVAL__(");
    for (int i = 0; i < nparams; i++) w += sprintf(w, "%sBYREF %s", i ? ", " : "", params[i]);
    w += sprintf(w, ")\n");
    if (mode == 0) {
        w += sprintf(w, "RETURN %s\nEND FUNCTION\n", src);
    } else if (mode == 2) {
        w += sprintf(w, "%s\nRETURN %s\nEND FUNCTION\n", src, ret_target ? ret_target : "");
    } else {
        w += sprintf(w, "%s\nEND SUB\n", src);
    }
    return w0;
}

/* S_eval: `LET nome = …` dentro un frammento DICHIARA un locale transiente che
 * fa shadowing sulla locale/globale del chiamante (il valore esterno non cambia).
 * Raccolgo i target di LET per dichiararli locali nel wrapper (DIM) e NON passarli
 * come parametri BYREF (che sarebbero write-through sul chiamante). */
static void snip_collect_let_targets(const char* src, char*** out, int* n)
{
    *out = NULL; *n = 0;
    char* wrapped = snip_wrap_text(src, 1, NULL, NULL, 0);
    if (!wrapped) return;
    gmb_lex_result_t lx = gmb_lexer_scan(wrapped, (long)strlen(wrapped));
    lx.owned_src = NULL;
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    if (!perr) {
        for (int i = 0; i < prog.n; i++) {
            const gmb_stmt_t* fn = prog.stmts[i];
            if (!fn || fn->kind != GMB_STMT_FUNCTION) continue;
            for (int j = 0; j < fn->n_funcbody; j++) {
                const gmb_stmt_t* st = fn->func_body[j];
                if (!st || !st->is_let || !st->target) continue;
                int dup = 0;
                for (int k = 0; k < *n; k++) if (strcasecmp((*out)[k], st->target) == 0) dup = 1;
                if (dup) continue;
                *out = (char**)realloc(*out, (size_t)(*n + 1) * sizeof(char*));
                (*out)[(*n)++] = strdup(st->target);
            }
        }
    }
    free(perr);
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2);
    gmb_lex_result_free(&lx);
    free(wrapped);
}

/* Il testo del frammento è un'ESPRESSIONE da restituire o una sequenza di
 * statement? Parità Clipper/C#: si prova PRIMA come corpo di SUB (il testo è
 * codice); se il corpo è un solo statement-espressione il valore si restituisce.
 * Se la forma-statement non è parsabile (es. `2+3` nudo) si prova come
 * espressione (`RETURN 2+3`). Ritorna 1 espressione, 0 statement, -1 errore. */
static int snip_classify(const char* src, char** params, int nparams, char** out_err,
                         char** out_assign_target)
{
    /* 1) forma statement */
    char* wrapped = snip_wrap_text(src, 1, NULL, params, nparams);
    if (wrapped) {
        gmb_lex_result_t lx = gmb_lexer_scan(wrapped, (long)strlen(wrapped));
        lx.owned_src = NULL;
        gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
        char* perr = NULL;
        gmb_program_t prog = gmb_parse(&lx2, &perr);
        int verdict = -1;
        if (!perr) {
            const gmb_stmt_t* fn = (prog.n > 0) ? prog.stmts[0] : NULL;
            verdict = (fn && fn->kind == GMB_STMT_FUNCTION && fn->n_funcbody == 1
                       && fn->func_body && fn->func_body[0]
                       && fn->func_body[0]->kind == GMB_STMT_EXPR) ? 1 : 0;
            /* assegnazione singola in posizione di espressione: il valore è il
             * target assegnato (`EVAL("Y = 42")` → 42, come Clipper/C#). */
            if (verdict == 0 && out_assign_target && fn && fn->n_funcbody == 1
                && fn->func_body && fn->func_body[0]
                && (fn->func_body[0]->kind == GMB_STMT_ASSIGN || fn->func_body[0]->kind == GMB_STMT_LET)
                && fn->func_body[0]->target)
                *out_assign_target = strdup(fn->func_body[0]->target);
        }
        free(perr);
        gmb_program_free(&prog);
        gmb_lex_result_free(&lx2);
        gmb_lex_result_free(&lx);
        free(wrapped);
        if (verdict >= 0) return verdict;
    }
    /* 2) forma espressione (testo non parsabile come statement: `2+3`) */
    wrapped = snip_wrap_text(src, 0, NULL, params, nparams);
    if (!wrapped) { if (out_err) *out_err = strdup("frammento: memoria esaurita"); return -1; }
    gmb_lex_result_t lx = gmb_lexer_scan(wrapped, (long)strlen(wrapped));
    lx.owned_src = NULL;
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    int verdict = perr ? -1 : 1;
    if (perr && out_err) *out_err = perr; else free(perr);
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2);
    gmb_lex_result_free(&lx);
    free(wrapped);
    return verdict;
}

int gmb_compile_snippet(const char* src, int as_stmt, const gmb_snippet_ext_t* ext,
                        gmb_compiled_snippet_t* out, char** error)
{
    if (error) *error = NULL;
    if (!src || !out) { if (error) *error = strdup("gmb_compile_snippet: argomenti nulli"); return -1; }
    memset(out, 0, sizeof(*out));

    char** params = NULL;
    int* pslots = NULL;
    int nparams = 0, pcap = 0;
    int rc = -1;

    pthread_mutex_lock(&g_snip_mu);
    snip_closure_clear();

    /* LET dichiara un locale transiente (S_eval): non è un parametro BYREF. */
    char** let_targets = NULL;
    int n_let = 0;
    snip_collect_let_targets(src, &let_targets, &n_let);

    /* Parametri = LOCALI dell'ospite (BYREF write-through sul frame del chiamante). */
    for (int i = 0; i < ext->local_count; i++) {
        const char* nm = ext->local_names ? ext->local_names[i] : NULL;
        if (!nm || !nm[0]) continue;
        int dup = 0;
        for (int k = 0; k < nparams; k++) if (strcasecmp(params[k], nm) == 0) dup = 1;
        for (int k = 0; k < n_let; k++) if (strcasecmp(let_targets[k], nm) == 0) dup = 1;
        if (dup) continue;
        if (nparams >= pcap) {
            pcap = pcap ? pcap * 2 : 8;
            params = (char**)realloc(params, (size_t)pcap * sizeof(char*));
            pslots = (int*)realloc(pslots, (size_t)pcap * sizeof(int));
        }
        params[nparams] = strdup(nm);
        pslots[nparams] = i;
        nparams++;
    }

    /* espressione solo se il frammento È un'espressione (v. snip_classify) */
    int wrap_mode = 1;              /* 0 espressione, 1 statement (SUB), 2 corpo+RETURN target */
    char* assign_target = NULL;
    if (as_stmt == 2) {
        /* SOLO espressione (REQUIRE/filtri): niente classificazione, `=` è uguaglianza. */
        wrap_mode = 0;
    } else if (!as_stmt) {
        char* cls_err = NULL;
        int cls = snip_classify(src, params, nparams, &cls_err, &assign_target);
        if (cls < 0) {
            if (error) *error = cls_err ? cls_err : strdup("frammento BASIC non valido");
            else free(cls_err);
            pthread_mutex_unlock(&g_snip_mu);
            return -1;
        }
        if (cls == 1) wrap_mode = 0;
        else if (assign_target) wrap_mode = 2;
        else wrap_mode = 1;
    }
    int stmt_mode = (wrap_mode != 0);

    for (int attempt = 0; attempt < 32; attempt++) {
        char* wrapped;
        if (n_let > 0) {
            size_t need = strlen(src) + 512;
            for (int i = 0; i < nparams; i++) need += strlen(params[i]) + 8;
            for (int i = 0; i < n_let; i++) need += strlen(let_targets[i]) + 8;
            wrapped = (char*)malloc(need);
            if (wrapped) {
                char* w = wrapped;
                if (wrap_mode == 1) w += sprintf(w, "SUB __GMB_EVAL__(");
                else                w += sprintf(w, "FUNCTION __GMB_EVAL__(");
                for (int i = 0; i < nparams; i++) w += sprintf(w, "%sBYREF %s", i ? ", " : "", params[i]);
                w += sprintf(w, ")\n");
                for (int i = 0; i < n_let; i++) w += sprintf(w, "DIM %s\n", let_targets[i]);
                if (wrap_mode == 0)      w += sprintf(w, "RETURN %s\nEND FUNCTION\n", src);
                else if (wrap_mode == 2) w += sprintf(w, "%s\nRETURN %s\nEND FUNCTION\n", src, assign_target ? assign_target : "");
                else                     w += sprintf(w, "%s\nEND SUB\n", src);
            }
        } else {
            wrapped = snip_wrap_text(src, wrap_mode, assign_target, params, nparams);
        }
        if (!wrapped) break;

        gmb_lex_result_t lx = gmb_lexer_scan(wrapped, (long)strlen(wrapped));
        lx.owned_src = NULL;
        gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
        char* perr = NULL;
        gmb_program_t prog = gmb_parse(&lx2, &perr);
        free(wrapped);

        if (perr) {
            if (error) *error = perr; else free(perr);
            gmb_program_free(&prog);
            gmb_lex_result_free(&lx2);
            gmb_lex_result_free(&lx);
            break;
        }

        gmb_compiled_chunk_t ch;
        memset(&ch, 0, sizeof ch);
        uint8_t* dummy = NULL; size_t dlen = 0;
        char* cerr = NULL;
        g_snip_ext = ext;
        g_snip_out = &ch;
        int crc = gmb_compile(&prog, &dummy, &dlen, &cerr);
        g_snip_ext = NULL;
        g_snip_out = NULL;

        gmb_program_free(&prog);
        gmb_lex_result_free(&lx2);
        gmb_lex_result_free(&lx);

        if (crc == 0) {
            uint32_t entry = 0;
            int found = 0;
            /* Solo le procedure NUOVE (dopo quelle seminate dall'ospite): un EVAL
             * annidato ha già un `__GMB_EVAL__` dell'ospite e non va riusato. */
            for (uint32_t i = (uint32_t)(ext->proc_count > 0 ? ext->proc_count : 0); i < ch.proc_count; i++)
                if (ch.proc_names[i] && strcasecmp(ch.proc_names[i], "__GMB_EVAL__") == 0)
                    { entry = ch.proc_addrs[i]; found = 1; break; }
            if (!found) {
                if (error) *error = strdup("gmb_compile_snippet: wrapper non compilato");
                gmb_compiled_chunk_free(&ch);
                break;
            }
            out->chunk = ch;
            out->entry = entry;
            out->param_names = params;
            out->param_slots = pslots;
            out->n_params = nparams;
            out->is_stmt = stmt_mode;
            params = NULL; pslots = NULL;
            rc = 0;
            break;
        }

        int retried = 0;
        if (cerr && strncmp(cerr, "SNIP_GLOBAL:", 12) == 0) {
            const char* name = cerr + 12;
            int dup = 0;
            for (int i = 0; i < nparams; i++) if (strcasecmp(params[i], name) == 0) dup = 1;
            if (!dup && name[0]) {
                if (nparams >= pcap) {
                    pcap = pcap ? pcap * 2 : 8;
                    params = (char**)realloc(params, (size_t)pcap * sizeof(char*));
                    pslots = (int*)realloc(pslots, (size_t)pcap * sizeof(int));
                }
                params[nparams] = strdup(name);
                pslots[nparams] = -1;
                nparams++;
                retried = 1;
            }
        }
        free(cerr);
        gmb_compiled_chunk_free(&ch);
        if (!retried) {
            if (error) *error = strdup("compilazione del frammento BASIC fallita");
            break;
        }
    }
    pthread_mutex_unlock(&g_snip_mu);

    free(assign_target);
    for (int i = 0; i < n_let; i++) free(let_targets[i]);
    free(let_targets);
    if (rc != 0 && params) {
        for (int i = 0; i < nparams; i++) free(params[i]);
        free(params);
        free(pslots);
    }
    return rc;
}

void gmb_compiled_snippet_free(gmb_compiled_snippet_t* s)
{
    if (!s) return;
    gmb_compiled_chunk_free(&s->chunk);
    for (int i = 0; i < s->n_params; i++) free(s->param_names[i]);
    free(s->param_names);
    free(s->param_slots);
    memset(s, 0, sizeof(*s));
}
