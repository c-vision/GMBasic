/* gmb_first_use.c — PRIMO USO: scarica TUTTO ciò che manca, una volta sola.
 *
 * PERCHÉ ESISTE (direttiva 2026-11-10). Un compilatore appena installato non ha né le librerie
 * native né il compilatore C (zig): senza, `gmb build`/`gmbnative`/`gmbrepl`/`gmbcs` falliscono
 * con errori che sembrano bug. Qui c'è l'UNICA implementazione di "procura quello che manca":
 * la chiamano `gmbadmin first-use` (usato anche da `gmb`, `gmbnative` e `gmbcs`, che delegano al
 * tool admin) e la REPL C (che linka già il runtime). Nessuna copia della logica altrove.
 *
 * SCELTA DELL'AMBITO (settings, MAI variabili d'ambiente — AGENTS §Regola-primaria):
 *   shared/settings/system.settings.json → "bootstrapScope":
 *     "host" (default) → solo il RID di questa macchina
 *     "all"            → tutti e 6 i RID (una copia completa della cache per ciascuno)
 *     "off"            → nessun download automatico
 *
 * COSA SCARICA, per ogni RID dell'ambito:
 *   1. l'indice piatto `index-<rid>.txt` (release stabile `libs-index`);
 *   2. TUTTI i file dell'indice (`gmb_bootstrap_rid`, scope ALL: runtime + dev + gli altri
 *      gruppi pubblicati). `torch` resta escluso, come da politica esistente: è external
 *      (download.pytorch.org), pesa centinaia di MB e si scarica on-demand quando una
 *      funzione lo chiede;
 *   3. la TOOLCHAIN (`toolchain`): l'archivio `zig-<ver>.tar.xz` viene estratto in
 *      `~/.gmbasic/toolchains/zig/<ver>-<rid>/`, che è la cartella che legge `zigcc.sh`.
 *      Il `zig` è eseguibile e la versione è quella dichiarata dall'archivio stesso.
 *
 * MARKER. `<home>/.gmbasic/.first-use-<rid>` con l'ambito usato dentro: a cache completa il
 * primo uso non costa nulla (una `stat`, nessuna rete). Se la rete fallisce il marker viene
 * scritto lo stesso con `state=degraded` e il motivo: senza, ogni avvio pagherebbe il timeout
 * (e una suite di test diventerebbe lentissima). Per riprovare: `gmbadmin first-use --force`
 * (oppure `gmb libs ensure`).
 *
 * NIENTE STUB: se qualcosa non si scarica lo si dice con il nome del file e il motivo; il
 * chiamante NON blocca l'avvio del compilatore (l'utente può lavorare con ciò che ha).
 */

#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define FU_MAX_RIDS 6
static const char* FU_RIDS[FU_MAX_RIDS] = {
    "osx-arm64", "osx-x64", "linux-x64", "linux-arm64", "win-x64", "win-arm64"
};

/* ── percorsi ─────────────────────────────────────────────────────────────────────────────── */

/* `<home>/.gmbasic` — la cache librerie è `<home>/.gmbasic/libs` (gmb_cache): si toglie
 * l'ultimo componente invece di riscrivere `getenv("HOME")`, così un override della cache
 * resta coerente. */
static int fu_root(char* out, size_t cap)
{
    char cache[2048];
    if (!gmb_cache(cache, sizeof cache) || !cache[0]) return 0;
    snprintf(out, cap, "%s", cache);
    char* sl = strrchr(out, '/');
    if (!sl) { out[0] = '\0'; return 0; }
    *sl = '\0';
    return out[0] != '\0';
}

static int fu_marker(char* out, size_t cap, const char* rid)
{
    char root[2048];
    if (!fu_root(root, sizeof root)) return 0;
    snprintf(out, cap, "%s/.first-use-%s", root, rid);
    return 1;
}

static int fu_exists(const char* p)
{
    struct stat st;
    return p && *p && stat(p, &st) == 0;
}

static void fu_mkdir_p(const char* path)
{
    char buf[2600];
    snprintf(buf, sizeof buf, "%s", path ? path : "");
    for (char* p = buf + 1; *p; p++)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0755); *p = '/'; }
    mkdir(buf, 0755);
}

/* ── ambito ───────────────────────────────────────────────────────────────────────────────── */

/* "off" | "host" (default) | "all": da `bootstrapScope` di system.settings.json.
 * Lettura con `gmb_paths_settings_value` (gmb_paths.c): è il parser che vive già in questo
 * target leggero, dove `gmb_settings_str` (gmb_settings.c, che tira dentro gfx) non c'è. */
static int fu_scope(char* out, size_t cap)
{
    char val[64] = "";
    (void)gmb_paths_settings_value("bootstrapScope", val, sizeof val);
    if (!val[0]) snprintf(val, sizeof val, "host");
    for (char* p = val; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    if (!strcmp(val, "off") || !strcmp(val, "none") || !strcmp(val, "no")) { snprintf(out, cap, "off"); return 1; }
    if (!strcmp(val, "all") || !strcmp(val, "tutto") || !strcmp(val, "tutti")) { snprintf(out, cap, "all"); return 1; }
    snprintf(out, cap, "host");
    return 1;
}

/* ── toolchain (zig) ──────────────────────────────────────────────────────────────────────── */

/* Trova `<cachedir>/zig-<ver>.tar.xz` (nome piatto dell'indice; l'asset reale si chiama
 * `zig-<ver>-<rid>.tar.xz`, ma il manifest è già per RID e usa il nome senza RID). */
static int fu_find_zig(const char* cache_rid_dir, char* name, size_t namecap)
{
    DIR* d = opendir(cache_rid_dir);
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    while ((e = readdir(d)) != NULL)
    {
        size_t L = strlen(e->d_name);
        if (L < 12) continue;
        if (strncmp(e->d_name, "zig-", 4) != 0) continue;
        if (strcmp(e->d_name + L - 7, ".tar.xz") != 0) continue;
        if (!found || strcmp(e->d_name, name) < 0) { snprintf(name, namecap, "%s", e->d_name); found = 1; }
    }
    closedir(d);
    return found;
}

/* Toolchain già installata per questo RID, di QUALUNQUE versione? Scrive il binario `zig`
 * trovato. Serve perché l'archivio in cache può mancare (indice vecchio, cache ripulita) mentre
 * il compilatore è già a posto: senza questo controllo una macchina perfettamente attrezzata
 * veniva marcata `degraded`. */
static int fu_zig_installed(const char* rid, char* out, size_t cap)
{
    char root[2048], base[2400];
    if (!fu_root(root, sizeof root)) return 0;
    snprintf(base, sizeof base, "%s/toolchains/zig", root);
    DIR* d = opendir(base);
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    size_t rl = strlen(rid);
    while ((e = readdir(d)) != NULL)
    {
        size_t L = strlen(e->d_name);
        if (L < rl + 2) continue;                     /* serve "<versione>-<rid>" */
        if (e->d_name[L - rl - 1] != '-') continue;
        if (strcmp(e->d_name + L - rl, rid) != 0) continue;
        char z[2800];
#if defined(_WIN32)
        snprintf(z, sizeof z, "%s/%s/zig.exe", base, e->d_name);
#else
        snprintf(z, sizeof z, "%s/%s/zig", base, e->d_name);
#endif
        if (fu_exists(z)) { snprintf(out, cap, "%s", z); found = 1; break; }
    }
    closedir(d);
    return found;
}

/* Installa la toolchain del RID a partire dall'archivio già verificato nella cache.
 * Destinazione: `<root>/toolchains/zig/<versione>-<rid>/` (la cartella che legge zigcc.sh),
 * con `--strip-components=1` perché l'archivio ha una radice `zig-<ver>-<rid>/`. */
static int fu_install_toolchain(const char* rid, int dry)
{
    char root[2048], cache[2048], cachedir[2560], archive[512];
    if (!fu_root(root, sizeof root)) return 0;
    /* macOS: la toolchain zig per `osx-*` NON si scarica e non si installa — DECISIONE
     * (2026-11-16): su macOS il compilatore di riferimento è `cc` (Apple clang), e lo zig
     * sull'host è la parte problematica (misurato: 4 dylib invece di 14 e `Segmentation
     * fault: 11` con il linker zig; AGENTS §"DUE COMPILATORI": il toolchain si evita
     * sull'host). Gli archivi `osx-*` del gruppo `toolchain` saranno ELIMINATI dagli asset;
     * per il cross (linux/win) resta `zig` dal PATH o dalla cache, non questa cartella. */
    if (strncmp(rid, "osx", 3) == 0)
    {
        fprintf(stderr, "[first-use] toolchain %s: non serve (su macOS si usa `cc`; gli archivi "
                        "zig per osx-* sono in dismissione).\n", rid);
        return 1;
    }
    char have[2800];
    if (fu_zig_installed(rid, have, sizeof have))
    {
        fprintf(stderr, "[first-use] toolchain %s: già installata (%s)\n", rid, have);
        return 1;
    }
    if (!gmb_cache(cache, sizeof cache)) return 0;
    snprintf(cachedir, sizeof cachedir, "%s/%s", cache, rid);
    archive[0] = '\0';
    if (!fu_find_zig(cachedir, archive, sizeof archive))
    {
        /* NIENTE STUB E NIENTE FINZIONI: l'indice pubblicato (`libs-index`) non contiene ancora
         * le righe del gruppo `toolchain` (gli archivi `zig-<ver>-<rid>.tar.xz` esistono nella
         * release `toolchain-vN`, ma senza riga d'indice non c'è né versione né sha da usare).
         * Qui non si inventa una versione: l'assenza nell'indice NON è un guasto del download —
         * il compilatore C ha già il suo percorso ormai collaudato (`cc` di sistema, oppure un
         * `zig` nel PATH, vedi tools/sh/zigcc.sh). Quando l'indice porterà la riga, l'archivio
         * verrà verificato e estratto qui, senza altre modifiche. */
        fprintf(stderr, "[first-use] toolchain %s: nessun archivio zig nell'indice pubblicato — "
                        "non è un errore (si usa cc/zig di sistema).\n", rid);
        return 1;
    }
    /* versione = parte fra "zig-" e ".tar.xz" */
    char ver[64];
    snprintf(ver, sizeof ver, "%s", archive + 4);
    char* dot = strstr(ver, ".tar.xz");
    if (dot) *dot = '\0';

    char dst[2560], zigbin[2700];
    snprintf(dst, sizeof dst, "%s/toolchains/zig/%s-%s", root, ver, rid);
    snprintf(zigbin, sizeof zigbin, "%s/zig", dst);
#if defined(_WIN32)
    snprintf(zigbin, sizeof zigbin, "%s/zig.exe", dst);
#endif
    if (fu_exists(zigbin))
    {
        fprintf(stderr, "[first-use] toolchain %s-%s: già installata\n", ver, rid);
        return 1;
    }
    if (dry)
    {
        fprintf(stderr, "[first-use] (dry) estrarrei %s/%s → %s\n", cachedir, archive, dst);
        return 1;
    }
    fu_mkdir_p(dst);
    char cmd[8192];
    snprintf(cmd, sizeof cmd, "tar -xJf \"%s/%s\" -C \"%s\" --strip-components=1", cachedir, archive, dst);
    int rc = system(cmd);
    if (rc != 0)
    {
        fprintf(stderr, "[first-use] ERRORE: estrazione fallita (%s/%s → %s)\n", cachedir, archive, dst);
        return 0;
    }
    if (!fu_exists(zigbin))
    {
        fprintf(stderr, "[first-use] ERRORE: archivio estratto ma '%s' non esiste\n", zigbin);
        return 0;
    }
#if !defined(_WIN32)
    chmod(zigbin, 0755);   /* un tar che non conserva i modi lascerebbe un compilatore non eseguibile */
#endif
    fprintf(stderr, "[first-use] toolchain %s-%s: installata in %s\n", ver, rid, dst);
    return 1;
}

/* ── per-RID ──────────────────────────────────────────────────────────────────────────────── */

static int fu_ensure_rid(const char* rid, int dry)
{
    char cache[2048], dir[2560], index[2700], url[2800];
    if (!gmb_cache(cache, sizeof cache)) return 0;
    snprintf(dir, sizeof dir, "%s/%s", cache, rid);
    snprintf(index, sizeof index, "%s/index.txt", dir);
    snprintf(url, sizeof url,
             "https://github.com/c-vision/GMBasicModules/releases/download/libs-index/index-%s.txt", rid);

    if (dry)
    {
        fprintf(stderr, "[first-use] (dry) %s: indice %s → %s, poi tutti i file + toolchain\n",
                rid, url, index);
        return 1;
    }
    fu_mkdir_p(dir);
    /* INDICE SEMPRE AGGIORNATO: è il solo "manifest" del RID e pesa ~20 KB. Usare quello in
     * cache senza guardare la data lasciava fuori i gruppi aggiunti dopo (es. `toolchain`: zig),
     * e il primo uso non sapeva che esistevano. Si scarica su un file TEMPORANEO e si sostituisce
     * l'indice solo in caso di successo: un timeout non deve distruggere quello buono in cache. */
    char tmp[2800];
    snprintf(tmp, sizeof tmp, "%s.new", index);
    fprintf(stderr, "[first-use] %s: aggiorno l'indice delle librerie…\n", rid);
    if (gmb_fetch_url(url, tmp)) rename(tmp, index);
    else
    {
        remove(tmp);
        if (!fu_exists(index))
        {
            fprintf(stderr, "[first-use] %s: indice non scaricabile (%s) — rete assente?\n", rid, url);
            return 0;
        }
        fprintf(stderr, "[first-use] %s: rete assente, uso l'indice in cache (può essere vecchio)\n", rid);
    }
    int ok = gmb_bootstrap_rid(rid, GMB_SCOPE_ALL);
    int tc = fu_install_toolchain(rid, dry);
    return ok && tc;
}

/* ── cartella ABI di LLMEnv (descrittori *.gmbarch) ────────────────────────────────────── */

/* Cartella valida = contiene almeno un `*.gmbarch`. Il descrittore non si apre: il formato è
 * di LLMEnv (`llmdesc`), qui serve solo sapere se la cartella è quella giusta. */
static int fu_abi_desc(const char* dir, int* count)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL)
    {
        size_t L = strlen(e->d_name);
        if (L > 8 && strcmp(e->d_name + L - 8, ".gmbarch") == 0) n++;
    }
    closedir(d);
    if (count) *count = n;
    return n > 0;
}

/* Scoperta SENZA percorsi cablati: `$LLMDESC_ABI_DIR` (la PRIMA fonte di `llmdesc`) e poi i
 * layout RELATIVI a `gmb_paths_llm_root` (env `LLM_ROOT` → `paths.llmRoot` → accanto
 * all'install → risalendo da eseguibile e cwd): `ABI/`, `lib/llmenv/ABI/`, `share/llmenv/ABI/`
 * (pkg e Homebrew non mettono l'ABI nella radice del prefisso). 1 = trovata, con fonte. */
/* Percorso NORMALIZZATO: `gmb_paths_llm_root` può contenere `../..` (`<root>/GMBasic/../../LLMEnv`)
 * e una stringa del genere non è confrontabile con il percorso reale (né confronti nel codice
 * né messaggi). Su Windows non c'è `realpath`: si lascia com'è. */
static void fu_abi_normalize(char* p, size_t cap)
{
#if !defined(_WIN32)
    char real[4096];
    if (p && cap && p[0] && realpath(p, real)) snprintf(p, cap, "%s", real);
#else
    (void)p; (void)cap;
#endif
}

int gmb_first_use_abi_dir(char* out, size_t cap, char* how, size_t howcap)
{
    if (out && cap) out[0] = '\0';
    if (how && howcap) how[0] = '\0';
    const char* env = getenv("LLMDESC_ABI_DIR");
    if (env && *env && fu_abi_desc(env, NULL))
    {
        snprintf(out, cap, "%s", env);
        fu_abi_normalize(out, cap);
        if (how) snprintf(how, howcap, "%s", "LLMDESC_ABI_DIR");
        return 1;
    }
    char llm[2048];
    if (gmb_paths_llm_root(llm, sizeof llm) && llm[0])
    {
        static const char* rel[] = { "ABI", "lib/llmenv/ABI", "share/llmenv/ABI" };
        for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++)
        {
            char p[2600];
            snprintf(p, sizeof p, "%s/%s", llm, rel[i]);
            if (fu_abi_desc(p, NULL))
            {
                snprintf(out, cap, "%s", p);
                fu_abi_normalize(out, cap);
                if (how) snprintf(how, howcap, "LLM_ROOT/%s", rel[i]);
                return 1;
            }
        }
    }
    return 0;
}

/* ── API ──────────────────────────────────────────────────────────────────────────────────── */

/* 1 = cache completa (o già completa), 0 = qualcosa manca (l'avvio NON viene bloccato).
 * `scope_override` (NULL = dai settings): "host" | "all" | "off". Serve a `first-use --all`,
 * che deve poter prendere le toolchain degli ALTRI RID senza toccare system.settings.json. */
int gmb_first_use_ensure_scope(int dry, const char* scope_override)
{
    char scope[16];
    if (scope_override && *scope_override) snprintf(scope, sizeof scope, "%s", scope_override);
    else fu_scope(scope, sizeof scope);
    if (!strcmp(scope, "off"))
    {
        fprintf(stderr, "[first-use] disattivato (bootstrapScope=\"off\" nei settings)\n");
        return 1;
    }

    int all = !strcmp(scope, "all");
    int nrids = all ? FU_MAX_RIDS : 1;
    int ok = 1;

    for (int i = 0; i < nrids; i++)
    {
        const char* rid = all ? FU_RIDS[i] : gmb_rid();
        char marker[2400];
        if (!fu_marker(marker, sizeof marker, rid)) { ok = 0; break; }
        if (fu_exists(marker))
        {
            if (!dry) continue;   /* già fatto: nessuna rete, nessun costo */
            fprintf(stderr, "[first-use] (dry) %s: marker presente (%s)\n", rid, marker);
            continue;
        }
        fprintf(stderr, "[first-use] %s: completo la cache (ambito \"%s\")…\n", rid, scope);
        int r = fu_ensure_rid(rid, dry);
        if (dry) { ok = ok && r; continue; }
        /* marker SEMPRE, anche in caso di fallimento: il motivo è dentro; senza, ogni avvio
         * ripagherebbe il timeout di rete (una suite di test diventerebbe lentissima). */
        FILE* f = fopen(marker, "w");
        if (f)
        {
            time_t now = time(NULL);
            fprintf(f, "scope=%s\nrid=%s\nstate=%s\nat=%ld\n", scope, rid, r ? "complete" : "degraded", (long)now);
            fclose(f);
        }
        if (!r)
        {
            ok = 0;
            fprintf(stderr, "[first-use] %s: INCOMPLETO — riprova con: gmbadmin first-use --force\n", rid);
        }
    }

    /* CARTELLA ABI di LLMEnv: si risolve QUI una volta (env → LLM_ROOT → layout installati),
     * così il primo uso la DICE e `first-use --abi-dir` la può stampare per la shell senza
     * cablare niente. NON si scrive nei settings: un percorso macchina in un file versionato
     * è sbagliato (D16), e `llmdesc` legge `LLMDESC_ABI_DIR` come prima fonte. */
    {
        char abi[2048], how[64];
        if (gmb_first_use_abi_dir(abi, sizeof abi, how, sizeof how))
            fprintf(stderr, "[first-use] ABI LLMEnv: %s (%s)\n", abi, how);
        else
            fprintf(stderr, "[first-use] ABI LLMEnv: non trovata (imposta LLMDESC_ABI_DIR, o installa LLMEnv)\n");
    }
    return ok;
}

/* `--force`: dimentica i marker (tutti i RID, così vale anche per l'ambito "all"). */
void gmb_first_use_forget(void)
{
    for (int i = 0; i < FU_MAX_RIDS; i++)
    {
        char marker[2400];
        if (fu_marker(marker, sizeof marker, FU_RIDS[i])) remove(marker);
    }
}

/* Compatibilità: senza override lo scope è quello dei settings (`bootstrapScope`). */
int gmb_first_use_ensure(int dry) { return gmb_first_use_ensure_scope(dry, NULL); }
