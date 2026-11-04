/* ============================================================================
 * gmb_file.c — file system (KILL/MKDIR/RMDIR/FILEEXISTS) e file I/O
 *              (OPEN / WRITE # / INPUT # / CLOSE).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"
#include "gmb_strbuf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

gmb_bool_t gmb_file_exists(const char* path) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    if (!path) return 0;
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

// DIREXISTS(path): true se path è una directory esistente.
gmb_bool_t gmb_dir_exists(gmb_string_t path) {
    if (!path.data) return 0;
#if defined(_WIN32) || defined(_WIN64)
    DWORD attr = GetFileAttributesA(path.data);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return stat(path.data, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

gmb_int_t gmb_file_kill(gmb_string_t path) {
    if (!path.data) return -1;
    return remove(path.data) == 0 ? 0 : -1;
}

gmb_int_t gmb_file_mkdir(gmb_string_t path) {
    if (!path.data || !*path.data) return -1;
    /* C# Directory.CreateDirectory: crea ricorsivamente i padri e tollera l'esistente. */
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path.data);
    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len-1] == '/' || tmp[len-1] == '\\')) tmp[len-1] = 0;
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
#if defined(_WIN32) || defined(_WIN64)
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }
#if defined(_WIN32) || defined(_WIN64)
    return _mkdir(tmp) == 0 ? 0 : 0; /* tollera EEXIST */
#else
    mkdir(tmp, 0755);
    return 0;
#endif
}

gmb_int_t gmb_file_rmdir(gmb_string_t path) {
    if (!path.data) return -1;
    return rmdir(path.data) == 0 ? 0 : -1;
}

gmb_int_t gmb_file_rename(gmb_string_t old_path, gmb_string_t new_path) {
    if (!old_path.data || !new_path.data) return -1;
    return rename(old_path.data, new_path.data) == 0 ? 0 : -1;
}

/* MOVE src TO dst (parity File.Move C#): rename; se fallisce per filesystem
 * diverso (EXDEV) o perché dst è una directory esistente, fallback copia+cancella.
 * Ritorna 0 su successo, -1 su errore. */
gmb_int_t gmb_file_move(gmb_string_t src, gmb_string_t dst) {
    if (!src.data || !*src.data || !dst.data || !*dst.data) return -1;
    if (rename(src.data, dst.data) == 0) return 0;
    if (gmb_file_copy(src.data, dst.data) != 0) return -1;
    if (remove(src.data) != 0) return -1;
    return 0;
}

/* --- File I/O (OPEN / WRITE # / INPUT # / CLOSE) ---
 * Canone IL (§0bis): la variabile `#h` È il file number (0..GMB_MAX_FILES-1) e indicizza
 * DIRETTAMENTE la tabella. gmb_file_open restituisce 0 su successo, -1 errore; il file
 * number effettivo è in *handle_out. (Prima la tabella era 1-based: `*handle_out = i+1`,
 * e il numero del chiamante veniva ignorato → `JSON OPEN ... AS #h` con h=0 dava 1 su
 * C99Native invece di 0, divergenza dal canone IL — bug C99Native-JSON-OPEN-Handle-Diverge.) */
#define GMB_MAX_FILES 64
static FILE* gmb_file_handles[GMB_MAX_FILES] = {0};
static int gmb_file_reclen[GMB_MAX_FILES] = {0};   /* LEN dei file aperti FOR RANDOM (0 = non RANDOM) */

// EOF(handle): -1 (true) se il cursore è alla fine del file, 0 (false) altrimenti.
gmb_int_t gmb_file_eof(gmb_int_t handle) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    int c = fgetc(gmb_file_handles[handle]);
    if (c == EOF) return -1;
    ungetc(c, gmb_file_handles[handle]);
    return 0;
}

/* --- FERROR: stato dell'ultima operazione sui file, con la stessa semantica di
 * IlRuntime.FileReadWriteUtils.FileError (0=ok, 2=file non trovato, 1=altro errore).
 * Prima FERROR era uno stub `(gmb_double_t)0` nell'emitter C# (§0bis: vietato). --- */
static int g_s_file_error = 0;
int gmb_file_error(void) { return g_s_file_error; }
void gmb_file_error_set(int e) { g_s_file_error = e; }

/* Apre il FILE* (mode BASIC-normalizzato) o NULL, impostando g_s_file_error.
 * "w+" (Random/Binary) = OpenOrCreate SENZA troncamento (se esiste → "r+b", altrimenti "w+b"). */
static FILE* gmb_file_fopen_mode(const char* path, const char* mode) {
    FILE* f = NULL;
    if (mode && strcmp(mode, "w+") == 0) {
        FILE* probe = fopen(path, "rb");
        if (probe) { fclose(probe); f = fopen(path, "r+b"); }
        else f = fopen(path, "w+b");
    } else {
        f = fopen(path, mode ? mode : "r");
    }
    if (!f) {
        /* parità IlRuntime.FOpen/FCreate: 2 se il file non esiste, 1 altrimenti */
        FILE* probe = path ? fopen(path, "rb") : NULL;
        if (probe) fclose(probe);
        g_s_file_error = probe ? 1 : 2;
    }
    return f;
}

/* FCREATE/FOPEN: apre un file SENZA un file number utente → alloca il PRIMO LIBERO ≥ 1
 * (canone IL: FREEFILE/FCREATE/FOPEN 1-based). */
gmb_int_t gmb_file_open_auto(gmb_string_t path, const char* mode, gmb_int_t* handle_out) {
    FILE* f = gmb_file_fopen_mode(path.data, mode);
    if (!f) return -1;
    for (int i = 1; i < GMB_MAX_FILES; i++)
        if (!gmb_file_handles[i]) { gmb_file_handles[i] = f; *handle_out = i; g_s_file_error = 0; return 0; }
    fclose(f);
    g_s_file_error = 1;
    return -1;
}

gmb_int_t gmb_file_open(gmb_string_t path, const char* mode, gmb_int_t* handle_out) {
    FILE* f = gmb_file_fopen_mode(path.data, mode);
    if (!f) return -1;
    /* Onora il file number RICHIESTO dal chiamante (la variabile `#h`, 0..GMB_MAX_FILES-1)
     * se valido e libero; altrimenti alloca il primo libero ≥ 1. Così `DIM h` + `OPEN AS #h`
     * lascia h=0 (canone IL) mentre `#N`/FREEFILE usano il numero 1-based. */
    if (*handle_out >= 0 && *handle_out < GMB_MAX_FILES && !gmb_file_handles[*handle_out]) {
        gmb_file_handles[*handle_out] = f; g_s_file_error = 0; return 0;
    }
    for (int i = 1; i < GMB_MAX_FILES; i++)
        if (!gmb_file_handles[i]) { gmb_file_handles[i] = f; *handle_out = i; g_s_file_error = 0; return 0; }
    fclose(f);
    g_s_file_error = 1;
    return -1;
}

/* FREEFILE: primo numero di file LIBERO ≥ 1 (canone IL, 1-based; il numero 0 è riservato
 * a chi non passa un numero esplicito). Prima era uno STUB costante (`1`). */
gmb_int_t gmb_file_free_file(void) {
    for (int i = 1; i < GMB_MAX_FILES; i++)
        if (!gmb_file_handles[i]) return (gmb_int_t)i;
    return -1;  /* nessuno slot libero */
}

/* FINPUT(count, handle): legge `count` byte dall'handle e li ritorna come stringa
 * (parità IlRuntime.FileInput: legge count byte e ne restituisce il testo). */
gmb_string_t gmb_file_read_n(gmb_int_t handle, gmb_int_t count) {
    if (count < 0) count = 0;
    if (count > (1 << 20)) count = 1 << 20;
    char* buf = (char*)malloc((size_t)count + 1);
    if (!buf) return gmb_string_alloc("");
    /* fread diretto sul handle: gmb_file_read_bytes ritorna 0=ok / 1=EOF, NON il numero di
     * byte letti — usarlo come conteggio azzerava la stringa (FREAD con count vuoto). */
    size_t got = 0;
    if (handle >= 0 && handle < GMB_MAX_FILES && gmb_file_handles[handle])
        got = fread(buf, 1, (size_t)count, gmb_file_handles[handle]);
    buf[got] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* READ: una riga come gmb_string_t (per FREAD(handle) senza count, funzione pura). */
gmb_string_t gmb_file_read_line_str(gmb_int_t handle) {
    gmb_string_t out = {0};
    if (gmb_file_read_line(handle, &out) != 0) return gmb_string_alloc("");
    if (!out.data) return gmb_string_alloc("");
    gmb_string_t r = gmb_string_alloc(out.data);
    free(out.data);
    return r;
}

/* READLINES(path): lista di stringhe, una per riga del file.
 * §0bis: la costruzione della lista vive nel runtime (il backend la CHIAMA). */
gmb_list_t gmb_read_all_lines_list(const char* path) {
    gmb_list_t l = gmb_list_new();
    int n = 0;
    char** lines = gmb_read_all_lines(path, &n);
    if (!lines) return l;
    for (int i = 0; i < n; i++)
        gmb_list_add(&l, gmb_make_val_string(lines[i] ? lines[i] : ""));
    return l;
}

/* READBYTES(path): lista di byte (0..255), come IlRuntime.ReadAllBytes byte[]. */
gmb_list_t gmb_read_all_bytes_list(const char* path) {
    gmb_list_t l = gmb_list_new();
    size_t n = 0;
    uint8_t* b = gmb_read_all_bytes(path, &n);
    if (!b) return l;
    for (size_t i = 0; i < n; i++)
        gmb_list_add(&l, gmb_make_val_double((double)b[i]));
    free(b);
    return l;
}

/* READFILE(path): intero file di testo -> stringa (utf-8, BOM iniziale rimosso).
 * File assente -> stringa vuota (stessa semantica di ReadBytes/ReadLines). */
gmb_string_t gmb_read_file_text(const char* path) {
    size_t n = 0;
    uint8_t* b = gmb_read_all_bytes(path, &n);
    if (!b) return gmb_string_alloc("");
    size_t off = (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) ? 3 : 0;
    char* s = (char*)malloc(n - off + 1);
    if (!s) { free(b); return gmb_string_alloc(""); }
    if (n > off) memcpy(s, b + off, n - off);
    s[n - off] = 0;
    free(b);
    gmb_string_t r = gmb_string_alloc(s);
    free(s);
    return r;
}

/* CHDIR: cambia la working directory di processo (per i sample che impostano work/). */
int gmb_fs_chdir(gmb_string_t path) {
    if (!path.data || !*path.data) return -1;
    return chdir(path.data);
}

// Scrive una singola stringa sul file (senza newline): PRINT # n, "testo"
/* Ritorna il numero di byte scritti (parita' IlRuntime.FWrite, che ritorna bytes.Length);
 * prima ritornava sempre 0 → IO.FWRITE dava 0 su VM/C e nel C99. -1 su handle invalido.
 * Nessun chiamante usava lo 0 come "successo": i soli consumatori sono i path FWRITE. */
gmb_int_t gmb_file_write_string(gmb_int_t handle, gmb_string_t s) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    const char* d = s.data ? s.data : "";
    fputs(d, gmb_file_handles[handle]);
    return (gmb_int_t)strlen(d);
}

gmb_int_t gmb_file_write_newline(gmb_int_t handle) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    fputc('\n', gmb_file_handles[handle]);
    return 0;
}

// Legge una riga: restituisce 0 con stringa letta, 1 su EOF.
gmb_int_t gmb_file_read_line(gmb_int_t handle, gmb_string_t* out) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    char buf[4096];
    if (!fgets(buf, sizeof(buf), gmb_file_handles[handle])) return 1;
    size_t ln = strlen(buf);
    while (ln > 0 && (buf[ln-1] == '\n' || buf[ln-1] == '\r')) buf[--ln] = 0;
    *out = gmb_string_alloc(buf);
    return 0;
}

// INPUT # su una variabile JSON/val: legge la riga e la conserva come stringa nel val.
gmb_int_t gmb_file_read_line_val(gmb_int_t handle, gmb_val_t* out) {
    gmb_string_t s = {NULL, 0, 0};
    int rc = gmb_file_read_line(handle, &s);
    if (rc != 0) return rc;
    *out = gmb_make_val_string(s.data);
    return 0;
}

// INPUT # su un canale JSON (`JSON OPEN ... AS #n`): legge il RESTO del file come UN documento JSON e lo
// parsa (dizionario/lista). File vuoto o JSON non valido = dizionario vuoto. Parità IL FileReadJsonNumber /
// PCode INPUT_FILE (content type 1) / VM-C: prima C99/C99Native leggevano una riga CSV in una stringa e la
// variabile JSON diventava una stringa (LEN=18, `g["k"].campo` vuoto).
gmb_val_t gmb_file_read_json(gmb_int_t handle) {
    gmb_strbuf_t sb = gmb_strbuf_new();
    gmb_string_t line = { 0 };
    while (gmb_file_read_line(handle, &line) == 0 && line.data) {
        gmb_strbuf_append(&sb, line.data, line.length);
        gmb_strbuf_append(&sb, "\n", 1);
        free(line.data);
        line.data = NULL;
    }
    free(line.data);
    gmb_val_t out = gmb_val_nil();
    if (sb.length > 0) out = gmb_json_parse(sb.data);
    gmb_strbuf_free(&sb);
    if (out.kind != GMB_TYPE_OBJECT) {
        gmb_val_t empty = gmb_json_parse("{}");
        return empty;
    }
    return out;
}

// INPUT # "a,b,c": legge una riga e la splitta per virgola in un array di gmb_string_t
// (allocato). *count_out = numero di campi. Il chiamante libera con free().
gmb_string_t* gmb_file_read_csv_line(gmb_int_t handle, gmb_int_t* count_out) {
    *count_out = 0;
    gmb_string_t line = {NULL, 0, 0};
    if (gmb_file_read_line(handle, &line) != 0) return NULL;
    if (!line.data) return NULL;
    /* Parser CSV canonico (parità WriteField C#/PCode): le virgole DENTRO un campo
     * tra doppi apici sono letterali, `""` è un apice escapato; il campo risultante
     * NON contiene gli apici. Prima: split secco su ogni virgola → `"Mario, Rossi"`
     * spezzato in due campi e apici conservati. */
    size_t n = 1;
    int inq = 0;
    for (size_t i = 0; i < line.length; i++) {
        if (line.data[i] == '"') {
            if (inq && i + 1 < line.length && line.data[i + 1] == '"') i++;
            else inq = !inq;
        } else if (line.data[i] == ',' && !inq) n++;
    }
    gmb_string_t* fields = (gmb_string_t*)calloc(n ? n : 1, sizeof(gmb_string_t));
    if (!fields) { free(line.data); return NULL; }
    char* out = (char*)malloc(line.length + 1);
    if (!out) { free(fields); free(line.data); return NULL; }
    size_t fi = 0, o = 0;
    inq = 0;
    for (size_t i = 0; i <= line.length; i++) {
        char ch = (i < line.length) ? line.data[i] : '\0';
        if (ch == '"') {
            if (inq && i + 1 < line.length && line.data[i + 1] == '"') { out[o++] = '"'; i++; continue; }
            inq = !inq;
            continue;
        }
        if ((ch == ',' && !inq) || ch == '\0') {
            out[o] = '\0';
            fields[fi++] = gmb_string_alloc(out);
            o = 0;
            if (ch == '\0') break;
            continue;
        }
        out[o++] = ch;
    }
    free(out);
    free(line.data);
    *count_out = (gmb_int_t)fi;
    return fields;
}

/* Formattazione canonica dei campi WRITE # (parità IlRuntime.WriteField/PCode
 * WRITE_FILE): stringhe tra virgolette con `""` escape, double -1/0 → TRUE/FALSE. */
gmb_string_t gmb_csv_quote(gmb_string_t s) {
    const char* d = s.data ? s.data : "";
    size_t extra = 2;
    for (size_t i = 0; d[i]; i++) if (d[i] == '"') extra++;
    char* b = (char*)malloc(strlen(d) + extra + 1);
    if (!b) return gmb_string_alloc("");
    size_t o = 0;
    b[o++] = '"';
    for (size_t i = 0; d[i]; i++) {
        if (d[i] == '"') b[o++] = '"';
        b[o++] = d[i];
    }
    b[o++] = '"';
    b[o] = '\0';
    gmb_string_t r = gmb_string_alloc(b);
    free(b);
    return r;
}

gmb_string_t gmb_csv_double(gmb_double_t v) {
    if (v == -1.0) return gmb_string_alloc("TRUE");
    if (v == 0.0) return gmb_string_alloc("FALSE");
    char buf[64];
    if (isnan(v)) snprintf(buf, sizeof(buf), "NaN");
    else if (isinf(v)) snprintf(buf, sizeof(buf), v > 0 ? "Infinity" : "-Infinity");
    else {
        if (v == 0.0) v = 0.0;   /* -0.0 -> "0" (parita con .NET/sample) */
snprintf(buf, sizeof(buf), "%.15g", v);
        char* e = strchr(buf, 'e');
        if (e) { *e = 'E'; if (e[1] != '-' && e[1] != '+') { memmove(e + 2, e + 1, strlen(e + 1) + 1); e[1] = '+'; } }
    }
    return gmb_string_alloc(buf);
}

gmb_int_t gmb_file_close(gmb_int_t handle) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    fclose(gmb_file_handles[handle]);
    gmb_file_handles[handle] = NULL;
    gmb_file_reclen[handle] = 0;
    return 0;
}

/* --- File binario / accesso casuale (SEEK / PUT # / GET #) ---
 * Formati allineati alla serializzazione .NET (BinaryWriter/BinaryReader):
 *   int16/32/64 little-endian, float/double LE, bool 1 byte, stringa 7-bit
 *   length-prefixed UTF-8, UDT = campi in sequenza.
 * SEEK posizione: 1-based (offset = pos - 1). */

gmb_int_t gmb_file_seek(gmb_int_t handle, long offset, int origin) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    int whence = origin == 2 ? SEEK_END : origin == 1 ? SEEK_CUR : SEEK_SET;
    if (fseek(gmb_file_handles[handle], offset, whence) != 0) return -1;
    return (gmb_int_t)ftell(gmb_file_handles[handle]);
}

long gmb_file_tell(gmb_int_t handle) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    return ftell(gmb_file_handles[handle]);
}

long gmb_file_length(gmb_int_t handle) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    long cur = ftell(gmb_file_handles[handle]);
    fseek(gmb_file_handles[handle], 0, SEEK_END);
    long len = ftell(gmb_file_handles[handle]);
    fseek(gmb_file_handles[handle], cur, SEEK_SET);
    return len;
}

gmb_int_t gmb_file_read_bytes(gmb_int_t handle, void* out, size_t n) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    size_t got = fread(out, 1, n, gmb_file_handles[handle]);
    return got == n ? 0 : 1; // 0 ok, 1 EOF/parziale
}

gmb_int_t gmb_file_write_bytes(gmb_int_t handle, const void* src, size_t n) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    return fwrite(src, 1, n, gmb_file_handles[handle]) == n ? 0 : 1;
}

gmb_int_t gmb_file_write_int16(gmb_int_t handle, short v) { return gmb_file_write_bytes(handle, &v, 2); }
gmb_int_t gmb_file_write_int32(gmb_int_t handle, int v)   { return gmb_file_write_bytes(handle, &v, 4); }
gmb_int_t gmb_file_write_int64(gmb_int_t handle, long long v) { return gmb_file_write_bytes(handle, &v, 8); }
gmb_int_t gmb_file_write_float(gmb_int_t handle, float v) { return gmb_file_write_bytes(handle, &v, 4); }
gmb_int_t gmb_file_write_double(gmb_int_t handle, double v) { return gmb_file_write_bytes(handle, &v, 8); }
gmb_int_t gmb_file_write_bool(gmb_int_t handle, gmb_bool_t v) {
    unsigned char b = v ? 1 : 0;
    return gmb_file_write_bytes(handle, &b, 1);
}

gmb_int_t gmb_file_read_int16(gmb_int_t handle, short* v) { return gmb_file_read_bytes(handle, v, 2); }
gmb_int_t gmb_file_read_int32(gmb_int_t handle, int* v)   { return gmb_file_read_bytes(handle, v, 4); }
gmb_int_t gmb_file_read_int64(gmb_int_t handle, long long* v) { return gmb_file_read_bytes(handle, v, 8); }
gmb_int_t gmb_file_read_float(gmb_int_t handle, float* v) { return gmb_file_read_bytes(handle, v, 4); }
gmb_int_t gmb_file_read_double(gmb_int_t handle, double* v) { return gmb_file_read_bytes(handle, v, 8); }
gmb_int_t gmb_file_read_bool(gmb_int_t handle, gmb_bool_t* v) {
    unsigned char b; int rc = gmb_file_read_bytes(handle, &b, 1); *v = b ? 1 : 0; return rc;
}

// Stringa 7-bit length-prefixed (BinaryWriter.Write(string)).
gmb_int_t gmb_file_write_string_bin(gmb_int_t handle, gmb_string_t s) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    size_t len = s.data ? s.length : 0;
    // Encoding 7-bit della lunghezza (max 4 byte per size_t pratiche).
    unsigned long long l = (unsigned long long)len;
    unsigned char buf[10]; int nb = 0;
    while (l >= 0x80) { buf[nb++] = (unsigned char)(l & 0x7F) | 0x80; l >>= 7; }
    buf[nb++] = (unsigned char)l;
    if (gmb_file_write_bytes(handle, buf, (size_t)nb) != 0) return -1;
    if (len > 0) return gmb_file_write_bytes(handle, s.data, len);
    return 0;
}

/* PUT #n, , value — semantica canonica byte (parità IlRuntime.PutFile e PCode PUT_FILE):
 * STRING → byte grezzi; LISTA → byte degli elementi in sequenza (elemento stringa → i suoi
 * byte, numero → ToString); altro → byte di ToString. UNICA sorgente: la usano il gestore
 * PUT_FILE della VM/C e l'emitter C99Native (st_put), così le 4 gambe scrivono gli stessi
 * byte (sample HttpMultiPart: file salvato = contenuto grezzo, 28 byte). */
gmb_int_t gmb_file_put_val(gmb_int_t handle, gmb_val_t v) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    if (v.kind == GMB_TYPE_STRING)
        return gmb_file_write_bytes(handle, v.v_string.data ? v.v_string.data : "", v.v_string.data ? v.v_string.length : 0);
    if (v.kind == GMB_TYPE_OBJECT) {
        gmb_list_t l = gmb_val_as_list(v);
        if (l.items && l.count > 0) {
            gmb_int_t rc = 0;
            for (size_t i = 0; i < l.count; i++)
                rc |= gmb_file_put_val(handle, l.items[i]);
            return rc;
        }
    }
    gmb_string_t s = gmb_val_to_string(v);
    gmb_int_t rc = gmb_file_write_bytes(handle, s.data ? s.data : "", s.data ? s.length : 0);
    gmb_string_free(&s);
    return rc;
}

/* --- Record I/O su file RANDOM (PUT/GET #n, rec, <UDT>) -------------------------------------
 * LAYOUT CANONICO (stesso canone di SIZEOF/PACKED TYPE, common/Ast/TypeLayout.cs): campi in
 * ordine di dichiarazione, SENZA padding, little-endian. Un token per campo nel descrittore:
 *   t  BIT                 1 byte (0/1; riletto -1/0)
 *   y  BYTE                1 byte senza segno        Y  SBYTE   1 byte con segno
 *   i  INTEGER             2 byte con segno          I  UINT    2 byte senza segno
 *   f  SINGLE              4 byte (float)
 *   l  LONG/QUAD/ULONG...  8 byte (int64)
 *   d  DOUBLE              8 byte (IEEE 754)
 *   c  CURRENCY            16 byte: int64 (valore * 10000, arrotondato) + 8 byte a zero
 *   sN STRING * N          N+1 byte: testo troncato/riempito a N con spazi + terminatore NUL
 * Il record occupa LEN byte (OPEN ... LEN = n, default 128); il layout dev'essere <= LEN, il resto
 * e' riempito di zeri. PUT #n, rec: offset = (rec-1)*LEN; rec assente (<=0) = posizione corrente.
 * GET oltre la fine del file (o su record parziale) restituisce zeri/stringhe vuote.
 * Unica sorgente per VM/C, C99Native; IL e PCode usano l'implementazione gemella
 * in C# (common/CommonLib/RandomRecordCodec.cs): i byte devono coincidere. */
#define GMB_RANDOM_DEFAULT_LEN 128

gmb_int_t gmb_file_set_random(gmb_int_t handle, gmb_int_t reclen) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    gmb_file_reclen[handle] = reclen > 0 ? (int)reclen : GMB_RANDOM_DEFAULT_LEN;
    return 0;
}

gmb_int_t gmb_file_is_random(gmb_int_t handle) {
    return (handle >= 0 && handle < GMB_MAX_FILES && gmb_file_handles[handle] && gmb_file_reclen[handle] > 0) ? -1 : 0;
}

/* Parse di un token: ritorna la larghezza in byte del campo (0 = descrittore non valido) e
 * avanza *p. *n_out = N per sN. */
static int gmb_rec_tok(const char** p, int* n_out) {
    char c = **p;
    if (!c) return 0;
    (*p)++;
    *n_out = 0;
    switch (c) {
        case 't': case 'y': case 'Y': return 1;
        case 'i': case 'I': return 2;
        case 'f': return 4;
        case 'l': case 'd': return 8;
        case 'c': return 16;
        case 's': {
            int n = 0;
            while (**p >= '0' && **p <= '9') { n = n * 10 + (**p - '0'); (*p)++; }
            *n_out = n;
            return n + 1;
        }
        default: return 0;
    }
}

gmb_int_t gmb_file_record_size(const char* desc) {
    gmb_int_t total = 0;
    int n;
    const char* p = desc ? desc : "";
    for (;;) {
        int w = gmb_rec_tok(&p, &n);
        if (w == 0) break;
        total += w;
    }
    return total;
}

static void gmb_rec_put_le(unsigned char* b, unsigned long long v, int bytes) {
    for (int i = 0; i < bytes; i++) b[i] = (unsigned char)((v >> (8 * i)) & 0xFF);
}
static unsigned long long gmb_rec_get_le(const unsigned char* b, int bytes) {
    unsigned long long v = 0;
    for (int i = 0; i < bytes; i++) v |= (unsigned long long)b[i] << (8 * i);
    return v;
}

/* Ritorna 0 ok; -1 handle non RANDOM/non valido; -2 layout > LEN (errore "record troppo grande"). */
gmb_int_t gmb_file_put_record(gmb_int_t handle, gmb_int_t rec, const char* desc, const gmb_val_t* f, int nf) {
    if (!gmb_file_is_random(handle)) return -1;
    int reclen = gmb_file_reclen[handle];
    gmb_int_t need = gmb_file_record_size(desc);
    if (need > reclen) return -2;
    unsigned char* buf = (unsigned char*)calloc((size_t)reclen, 1);
    if (!buf) return -1;
    size_t off = 0;
    int n, idx = 0;
    const char* p = desc ? desc : "";
    char c;
    while ((c = *p) != 0) {
        int w = gmb_rec_tok(&p, &n);
        if (w == 0) break;
        gmb_val_t v = idx < nf ? f[idx] : gmb_make_val_double(0.0);
        idx++;
        switch (c) {
            case 't': buf[off] = gmb_val_to_double(v) != 0.0 ? 1 : 0; break;
            case 'y': case 'Y': buf[off] = (unsigned char)((long long)gmb_val_to_double(v) & 0xFF); break;
            case 'i': case 'I': gmb_rec_put_le(buf + off, (unsigned long long)(long long)gmb_val_to_double(v), 2); break;
            case 'f': { float fv = (float)gmb_val_to_double(v); unsigned int u; memcpy(&u, &fv, 4); gmb_rec_put_le(buf + off, u, 4); break; }
            case 'l': gmb_rec_put_le(buf + off, (unsigned long long)(long long)gmb_val_to_double(v), 8); break;
            case 'd': { double dv = gmb_val_to_double(v); unsigned long long u; memcpy(&u, &dv, 8); gmb_rec_put_le(buf + off, u, 8); break; }
            case 'c': gmb_rec_put_le(buf + off, (unsigned long long)llround(gmb_val_to_double(v) * 10000.0), 8); break;
            case 's': {
                /* gmb_val_to_string su un valore STRING ritorna la STESSA stringa (nessuna copia): si libera
                 * solo la conversione di un valore non-stringa. */
                int owned = v.kind != GMB_TYPE_STRING;
                gmb_string_t s = owned ? gmb_val_to_string(v) : v.v_string;
                size_t len = s.data ? s.length : 0;
                if (len > (size_t)n) len = (size_t)n;
                if (len) memcpy(buf + off, s.data, len);
                for (size_t k = len; k < (size_t)n; k++) buf[off + k] = ' ';
                if (owned) gmb_string_free(&s);
                break;
            }
        }
        off += (size_t)w;
    }
    if (rec > 0 && fseek(gmb_file_handles[handle], (long)(rec - 1) * reclen, SEEK_SET) != 0) { free(buf); return -1; }
    else if (rec <= 0) fseek(gmb_file_handles[handle], 0, SEEK_CUR);   /* stdio: cambio read/write richiede un seek */
    size_t wr = fwrite(buf, 1, (size_t)reclen, gmb_file_handles[handle]);
    free(buf);
    return wr == (size_t)reclen ? 0 : -1;
}

gmb_int_t gmb_file_get_record(gmb_int_t handle, gmb_int_t rec, const char* desc, gmb_val_t* f, int nf) {
    if (!gmb_file_is_random(handle)) return -1;
    int reclen = gmb_file_reclen[handle];
    gmb_int_t need = gmb_file_record_size(desc);
    if (need > reclen) return -2;
    unsigned char* buf = (unsigned char*)calloc((size_t)reclen, 1);
    if (!buf) return -1;
    if (rec > 0 && fseek(gmb_file_handles[handle], (long)(rec - 1) * reclen, SEEK_SET) != 0) { free(buf); return -1; }
    else if (rec <= 0) fseek(gmb_file_handles[handle], 0, SEEK_CUR);
    size_t got = fread(buf, 1, (size_t)reclen, gmb_file_handles[handle]);
    if (got < (size_t)reclen) clearerr(gmb_file_handles[handle]);   /* record oltre EOF: resto a zero */
    size_t off = 0;
    int n, idx = 0;
    const char* p = desc ? desc : "";
    char c;
    while ((c = *p) != 0 && idx < nf) {
        int w = gmb_rec_tok(&p, &n);
        if (w == 0) break;
        const unsigned char* b = buf + off;
        switch (c) {
            case 't': f[idx] = gmb_make_val_double(b[0] ? -1.0 : 0.0); break;
            case 'y': f[idx] = gmb_make_val_double((double)b[0]); break;
            case 'Y': f[idx] = gmb_make_val_double((double)(signed char)b[0]); break;
            case 'i': f[idx] = gmb_make_val_double((double)(short)gmb_rec_get_le(b, 2)); break;
            case 'I': f[idx] = gmb_make_val_double((double)(unsigned short)gmb_rec_get_le(b, 2)); break;
            case 'f': { unsigned int u = (unsigned int)gmb_rec_get_le(b, 4); float fv; memcpy(&fv, &u, 4); f[idx] = gmb_make_val_double((double)fv); break; }
            case 'l': f[idx] = gmb_make_val_double((double)(long long)gmb_rec_get_le(b, 8)); break;
            case 'd': { unsigned long long u = gmb_rec_get_le(b, 8); double dv; memcpy(&dv, &u, 8); f[idx] = gmb_make_val_double(dv); break; }
            case 'c': f[idx] = gmb_make_val_double((double)(long long)gmb_rec_get_le(b, 8) / 10000.0); break;
            case 's': {
                /* STRING * N: testo fino al primo NUL, riempito a N con spazi (semantica del campo a
                 * lunghezza fissa): il chiamante assegna direttamente v_string al campo. */
                size_t len = 0;
                while (len < (size_t)n && b[len] != 0) len++;
                char* pad = (char*)malloc((size_t)n + 1);
                if (pad) {
                    if (len) memcpy(pad, b, len);
                    for (size_t k = len; k < (size_t)n; k++) pad[k] = ' ';
                    pad[n] = 0;
                    f[idx] = gmb_make_val_string_counted_n(pad, (size_t)n);
                    free(pad);
                } else f[idx] = gmb_make_val_string_counted_n("", 0);
                break;
            }
        }
        idx++;
        off += (size_t)w;
    }
    free(buf);
    return 0;
}

/* PUT/GET #n, [pos], <array di BYTE> su file BINARY: ogni elemento e' UN byte grezzo (array di
 * gmb_double_t nel C99Native). pos > 0 = offset di byte (pos-1); assente = posizione corrente.
 * GET legge fino a n byte: quelli mancanti (EOF) restano 0. Ritorno 0 ok, -1 handle non valido. */
gmb_int_t gmb_file_put_bytes_d(gmb_int_t handle, gmb_int_t pos, const gmb_double_t* a, gmb_int_t n) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    if (n <= 0) return 0;
    unsigned char* buf = (unsigned char*)malloc((size_t)n);
    if (!buf) return -1;
    for (gmb_int_t i = 0; i < n; i++) buf[i] = (unsigned char)((long long)a[i] & 0xFF);
    if (pos > 0 && fseek(gmb_file_handles[handle], (long)(pos - 1), SEEK_SET) != 0) { free(buf); return -1; }
    size_t wr = fwrite(buf, 1, (size_t)n, gmb_file_handles[handle]);
    free(buf);
    return wr == (size_t)n ? 0 : -1;
}

gmb_int_t gmb_file_get_bytes_d(gmb_int_t handle, gmb_int_t pos, gmb_double_t* a, gmb_int_t n) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    if (n <= 0) return 0;
    unsigned char* buf = (unsigned char*)calloc((size_t)n, 1);
    if (!buf) return -1;
    if (pos > 0 && fseek(gmb_file_handles[handle], (long)(pos - 1), SEEK_SET) != 0) { free(buf); return -1; }
    size_t got = fread(buf, 1, (size_t)n, gmb_file_handles[handle]);
    if (got < (size_t)n) clearerr(gmb_file_handles[handle]);
    for (gmb_int_t i = 0; i < n; i++) a[i] = (gmb_double_t)buf[i];
    free(buf);
    return 0;
}

gmb_int_t gmb_file_read_string_bin(gmb_int_t handle, gmb_string_t* out) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    unsigned long long l = 0; int shift = 0; unsigned char b;
    do {
        if (gmb_file_read_bytes(handle, &b, 1) != 0) return 1;
        l |= (unsigned long long)(b & 0x7F) << shift;
        shift += 7;
    } while (b & 0x80);
    char* buf = (char*)malloc((size_t)l + 1);
    if (!buf) return -1;
    if (l > 0 && gmb_file_read_bytes(handle, buf, (size_t)l) != 0) { free(buf); return 1; }
    buf[l] = 0;
    *out = gmb_string_alloc(buf);
    free(buf);
    return 0;
}

/* DIR(path): elenco file in una riga (DIRFUNC). Ritorna malloc'd. */
char* gmb_dir_list(const char* path) {
#if defined(_WIN32) || defined(_WIN64)
    (void)path;
    return strdup("");
#else
    char* buf = (char*)malloc(16384);
    if (!buf) return NULL;
    buf[0] = 0;
    if (!path || !*path) path = ".";
    DIR* d = opendir(path);
    if (d) {
        size_t o = 0;
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            /* esclude . e .. */
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            int n = snprintf(buf + o, 16384 - o, "%s/%s\n", path, e->d_name);
            if (n > 0) o += (size_t)n;
            if (o >= 16376) break;
        }
        closedir(d);
    }
    return buf;
#endif
}

/* --- Parity IL (IlRuntime): DIR/FOLDERSIZE/NORMALIZEPATH/GETFOLDERPATH/RMDIR ricorsivo.
 * Aggiunti 2026-08-25 per i builtin C99 (B2/B4 residui). Implementazione POSIX; su
 * Windows fallback minimi (coerente con gmb_dir_list che ritorna ""). --- */

/* Glob semplice BASIC: solo '*' e '?' ("*.*" equivale a "*"). */
static int gmb_glob_match(const char* pat, const char* name) {
    while (*pat) {
        if (*pat == '*') {
            while (*pat == '*') pat++;
            if (!*pat) return 1;
            for (; *name; name++) if (gmb_glob_match(pat, name)) return 1;
            return 0;
        }
        if (!*name) return 0;
        if (*pat != '?' && *pat != *name) return 0;
        pat++; name++;
    }
    return !*name;
}

/* DIR(mask): nomi (basename) di directory POI file che matchano la maschera.
 * Parity IlRuntime.Dir: dirname(mask) (o cwd), pattern basename (o "*"). */
gmb_list_t gmb_fs_dir(gmb_string_t mask) {
    gmb_list_t out = gmb_list_new();
#if !defined(_WIN32) && !defined(_WIN64)
    if (!mask.data || !*mask.data) return out;
    char dir[1024]; const char* pat;
    const char* slash = strrchr(mask.data, '/');
    if (slash) {
        size_t n = (size_t)(slash - mask.data);
        if (n >= sizeof(dir)) n = sizeof(dir) - 1;
        memcpy(dir, mask.data, n); dir[n] = 0;
        pat = slash + 1;
    } else { dir[0] = 0; pat = mask.data; }
    const char* d = dir[0] ? dir : ".";
    if (!*pat) pat = "*";
    DIR* dp = opendir(d);
    if (!dp) return out;
    for (int pass = 0; pass < 2; pass++) {
        struct dirent* e;
        while ((e = readdir(dp)) != NULL) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char full[2048];
            snprintf(full, sizeof(full), "%s/%s", d, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0) continue;
            int is_dir = S_ISDIR(st.st_mode);
            if ((pass == 0) != (is_dir != 0)) continue;
            if (!gmb_glob_match(pat, e->d_name)) continue;
            gmb_list_add(&out, gmb_make_val_string(e->d_name));
        }
        rewinddir(dp);
    }
    closedir(dp);
#else
    (void)mask;
#endif
    return out;
}

#if !defined(_WIN32) && !defined(_WIN64)
static gmb_double_t gmb_fs_dir_size_rec(const char* path) {
    gmb_double_t total = 0;
    DIR* dp = opendir(path);
    if (!dp) return 0;
    struct dirent* e;
    while ((e = readdir(dp)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) total += gmb_fs_dir_size_rec(full);
        else total += (gmb_double_t)st.st_size;
    }
    closedir(dp);
    return total;
}
#endif

/* FOLDERSIZE(path): file -> dimensione; directory -> somma ricorsiva dei file. */
gmb_double_t gmb_fs_folder_size(gmb_string_t path) {
#if !defined(_WIN32) && !defined(_WIN64)
    if (!path.data || !*path.data) return 0;
    struct stat st;
    if (stat(path.data, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return (gmb_double_t)st.st_size;
    return gmb_fs_dir_size_rec(path.data);
#else
    (void)path; return 0;
#endif
}

/* NORMALIZEPATH(path): assoluto + segmenti . /.. risolti (parity Path.GetFullPath,
 * NON richiede che il path esista). */
gmb_string_t gmb_fs_normalize_path(gmb_string_t path) {
    if (!path.data || !*path.data) return gmb_string_alloc("");
#if !defined(_WIN32) && !defined(_WIN64)
    char abs[2048];
    if (path.data[0] == '/') snprintf(abs, sizeof(abs), "%s", path.data);
    else {
        char cwd[1024];
        if (!getcwd(cwd, sizeof(cwd))) cwd[0] = 0;
        snprintf(abs, sizeof(abs), "%s/%s", cwd, path.data);
    }
    char* parts[256]; int n = 0;
    char* save = NULL;
    for (char* tok = strtok_r(abs, "/", &save); tok; tok = strtok_r(NULL, "/", &save)) {
        if (!strcmp(tok, ".") || !*tok) continue;
        if (!strcmp(tok, "..")) { if (n > 0) n--; continue; }
        if (n < 256) parts[n++] = tok;
    }
    char out[2048]; size_t o = 0;
    out[o++] = '/';
    for (int i = 0; i < n; i++) {
        size_t l = strlen(parts[i]);
        if (o + l + 2 >= sizeof(out)) break;
        memcpy(out + o, parts[i], l); o += l; out[o++] = '/';
    }
    if (n > 0) o--; /* trailing slash solo per la root */
    out[o] = 0;
    return gmb_string_alloc(out);
#else
    return gmb_string_alloc(path.data);
#endif
}

/* GETFOLDERPATH(path): parity Path.GetDirectoryName — directory del path (strip dei
 * separatori finali, poi fino all'ultimo separatore); root/vuoto -> "". */
/* DIRNAME del path (semantica canonica PARENTDIR / GETFOLDERPATH).
 * Allineata 1:1 a System.IO.Path.GetDirectoryName (oracolo del progetto, verificato con
 * dotnet fsi):
 *   "/a/b/c"→"/a/b"  "/a/b/"→"/a/b"  "/a/b//"→"/a/b"  "/a/b/c/"→"/a/b/c"
 *   "/a"→"/"  "/"→""  ""→""  "a/b"→"a"  "a"→""  "/a//b"→"/a"  "//a"→"/"
 * Regola: se il path TERMINA con un separatore l'ultimo segmento è una directory, quindi il
 * risultato è il path stesso senza i separatori finali; altrimenti è tutto ciò che precede
 * l'ultimo separatore (senza separatori finali, con la radice "/" preservata).
 * Prima qui c'era la semantica "ultimo slash", che divergeva su "/a/b/" ("/a") e "/" ("/")
 * da IL/PCode/VMC → tre implementazioni diverse dello stesso builtin. */
gmb_string_t gmb_fs_dir_name(gmb_string_t path) {
    if (!path.data || !*path.data) return gmb_string_alloc("");
    size_t n = strlen(path.data);
    if (path.data[n - 1] == '/' || path.data[n - 1] == '\\') {
        while (n > 0 && (path.data[n - 1] == '/' || path.data[n - 1] == '\\')) n--;
        char tmp[2048];
        if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
        memcpy(tmp, path.data, n); tmp[n] = 0;
        return gmb_string_alloc(tmp);
    }
    size_t i = n;
    while (i > 0 && path.data[i - 1] != '/' && path.data[i - 1] != '\\') i--;
    if (i == 0) return gmb_string_alloc("");              /* nessun separatore: nessuna dir */
    size_t cut = i - 1;
    while (cut > 1 && (path.data[cut - 1] == '/' || path.data[cut - 1] == '\\')) cut--;
    if (cut == 0) return gmb_string_alloc("/");           /* radice ("/a" → "/") */
    char out[2048];
    if (cut >= sizeof(out)) cut = sizeof(out) - 1;
    memcpy(out, path.data, cut); out[cut] = 0;
    return gmb_string_alloc(out);
}

#if !defined(_WIN32) && !defined(_WIN64)
static int gmb_fs_rmdir_rec(const char* path) {
    DIR* dp = opendir(path);
    if (!dp) return -1;
    struct dirent* e;
    int ok = 0;
    while ((e = readdir(dp)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) { ok = -1; continue; }
        if (S_ISDIR(st.st_mode)) { if (gmb_fs_rmdir_rec(full) != 0) ok = -1; }
        else if (remove(full) != 0) ok = -1;
    }
    closedir(dp);
    return rmdir(path) != 0 ? -1 : ok;
}
#endif

/* RMDIR path, RECURSIVE: cancella l'albero (parity .NET Directory.Delete(recursive)). */
gmb_int_t gmb_file_rmdir_recursive(gmb_string_t path) {
#if !defined(_WIN32) && !defined(_WIN64)
    if (!path.data || !*path.data) return -1;
    return gmb_fs_rmdir_rec(path.data);
#else
    (void)path; return -1;
#endif
}

#if !defined(_WIN32) && !defined(_WIN64)
static int gmb_fs_copy_file(const char* src, const char* dst) {
    FILE* fi = fopen(src, "rb");
    if (!fi) return -1;
    FILE* fo = fopen(dst, "wb");
    if (!fo) { fclose(fi); return -1; }
    char buf[8192]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) fwrite(buf, 1, n, fo);
    fclose(fi); fclose(fo);
    return 0;
}

static int gmb_fs_copydir_rec(const char* src, const char* dst, int recursive, int overwrite) {
    mkdir(dst, 0755); /* Directory.CreateDirectory */
    DIR* dp = opendir(src);
    if (!dp) return -1;
    struct dirent* e;
    int ok = 0;
    while ((e = readdir(dp)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char fs[2048], fd[2048];
        snprintf(fs, sizeof(fs), "%s/%s", src, e->d_name);
        snprintf(fd, sizeof(fd), "%s/%s", dst, e->d_name);
        struct stat st;
        if (stat(fs, &st) != 0) { ok = -1; continue; }
        if (S_ISDIR(st.st_mode)) {
            if (recursive && gmb_fs_copydir_rec(fs, fd, recursive, overwrite) != 0) ok = -1;
        } else {
            struct stat dexist;
            if (!overwrite && stat(fd, &dexist) == 0) continue;
            if (gmb_fs_copy_file(fs, fd) != 0) ok = -1;
        }
    }
    closedir(dp);
    return ok;
}
#endif

/* XCOPY src TO dst: copia directory (parity .NET CoreDirectoryCopy: crea dst, copia i
 * file, ricorsione opzionale, flag overwrite). */
/* Copia un singolo file (parity COPY_FILE syscall VM/C / IlRuntime.Copy): byte per byte. */
gmb_int_t gmb_file_copy(const char* src, const char* dst) {
    if (!src || !*src || !dst || !*dst) return -1;
    FILE* fin = fopen(src, "rb");
    if (!fin) return -1;
    FILE* fout = fopen(dst, "wb");
    if (!fout) { fclose(fin); return -1; }
    char buf[8192]; size_t r;
    while ((r = fread(buf, 1, sizeof(buf), fin)) > 0) fwrite(buf, 1, r, fout);
    fclose(fout);
    fclose(fin);
    return 0;
}

gmb_int_t gmb_fs_copy_directory(gmb_string_t src, gmb_string_t dst, gmb_int_t recursive, gmb_int_t overwrite) {
#if !defined(_WIN32) && !defined(_WIN64)
    if (!src.data || !*src.data || !dst.data || !*dst.data) return -1;
    return gmb_fs_copydir_rec(src.data, dst.data, recursive ? 1 : 0, overwrite ? 1 : 0);
#else
    (void)src; (void)dst; (void)recursive; (void)overwrite; return -1;
#endif
}

/* ==================== INI/JSON read helpers (READVALUE / READSECTION) ====================
 * Format-aware: se il file (dopo BOM/spazi) inizia con '{' è JSON (JSONC: commenti '//' e
 * slash-stella rimossi) e la coppia (section, key) naviga l'oggetto; altrimenti INI classico.
 * Stessi file di C# (FileReadWriteUtils). --- */

/* Rimuove commenti // e /* *\/ rispettando le stringhe "...". Scrive in `out` (cap). */
static void frw_strip_json_comments(const char* src, char* out, size_t cap)
{
    size_t oi = 0;
    int in_str = 0;
    for (const char* p = src; *p && oi + 1 < cap; p++)
    {
        if (in_str)
        {
            if (*p == '\\' && p[1]) { if (oi + 2 < cap) { out[oi++] = *p; out[oi++] = p[1]; } p++; continue; }
            out[oi++] = *p;
            if (*p == '"') in_str = 0;
            continue;
        }
        if (*p == '"') { out[oi++] = *p; in_str = 1; continue; }
        if (*p == '/' && p[1] == '/') { while (*p && *p != '\n') p++; if (*p) p--; continue; }
        if (*p == '/' && p[1] == '*')
        {
            p += 2;
            while (*p && !(*p == '*' && p[1] == '/')) p++;
            if (*p) p++;
            continue;
        }
        out[oi++] = *p;
    }
    out[oi] = '\0';
}

/* Legge l'intero file. Ritorna buffer allocato (da free) o NULL. */
static char* frw_slurp(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    long n;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0 || n > 8 * 1024 * 1024) { fclose(f); return NULL; }
    rewind(f);
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

/* True se il contenuto inizia con '{' dopo BOM/spazi. */
static int frw_is_json(const char* s)
{
    if (!s) return 0;
    while (*s == '\xEF' || *s == '\xBB' || *s == '\xBF' || *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    return *s == '{';
}

char* gmb_ini_read_value(const char* path, const char* section, const char* key, const char* def) {
    char* content = frw_slurp(path);
    if (!content) { char* d = strdup(def ? def : ""); return d; }
    char clean[16384];
    frw_strip_json_comments(content, clean, sizeof(clean));
    free(content);
    if (frw_is_json(clean))
    {
        char* v = NULL;
        if (section && *section)
        {
            char dotpath[2048];
            snprintf(dotpath, sizeof(dotpath), "%s.%s", section, key);
            v = gmb_json_extract(clean, dotpath);
        }
        else
        {
            v = gmb_json_extract(clean, key);
        }
        if (v && *v && strcmp(v, "null") != 0) return v;
        if (v) free(v);
        char* d = strdup(def ? def : "");
        return d;
    }
    /* (content già liberato sopra: il secondo free era un DOUBLE FREE → SIGSEGV sul
     * ramo non-JSON, cioè su ogni file INI reale — trovato da
     * minitests/json/readvalue.bas, gamba VM/C exit=139.) */
    FILE* f = fopen(path, "r");
    if (!f) { char* d = strdup(def ? def : ""); return d; }
    char line[1024];
    int in_sec = (section == NULL || *section == '\0');
    char* result = NULL;
    while (fgets(line, sizeof(line), f)) {
        char* t = line;
        while (*t == ' ' || *t == '\t') t++;
        char* end = t + strlen(t);
        while (end > t && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) *--end = '\0';
        if (*t == '[') {
            char* close = strchr(t, ']');
            in_sec = 0;
            if (close) {
                *close = '\0';
                if (section && strcasecmp(t + 1, section) == 0) in_sec = 1;
            }
            continue;
        }
        if (!in_sec) continue;
        char* eq = strchr(t, '=');
        if (!eq) continue;
        *eq = '\0';
        char* k = t, * v = eq + 1;
        while (*k == ' ') k++;
        char* ke = k + strlen(k);
        while (ke > k && ke[-1] == ' ') *--ke = '\0';
        while (*v == ' ') v++;
        if (strcasecmp(k, key) == 0) { result = strdup(v); break; }
    }
    fclose(f);
    if (!result) result = strdup(def ? def : "");
    return result;
}

/* Coercizione del valore INI, identica al canone di parita' IL/PCode/VM-C
 * (ReadSectionObject): long -> double -> bool -> string. */
gmb_val_t gmb_ini_coerce_val(const char* v) {
    if (!v || !*v) return gmb_make_val_string("");
    if (!strcasecmp(v, "true")) return gmb_make_val_bool(1);
    if (!strcasecmp(v, "false")) return gmb_make_val_bool(0);
    char* end = NULL;
    long long l = strtoll(v, &end, 10);
    if (end && *end == '\0' && end != v) return gmb_make_val_int((gmb_int_t)l);
    end = NULL;
    double d = strtod(v, &end);
    if (end && *end == '\0' && end != v) return gmb_make_val_double(d);
    return gmb_make_val_string(v);
}

/* Sezione INI/JSON -> dict boxato (gmb_val_t): chiavi/valori dalle righe "k=v" o dalle
 * chiavi di primo livello della sezione JSON. Unica costruzione per i backend C99 (§0bis);
 * il parsing e' gmb_ini_read_section (stessa sorgente della VM/C). */
/* ── Scrittura INI del file di settings (JSON WRITEVALUE/WRITESECTION, §0bis).
 * Canone CommonLib.FileReadWriteUtils.WriteValue/WriteSection: file a righe,
 * sezione `[nome]`, chiavi `k=v`; la sezione viene sostituita/creata. ── */

static char** ini_load_lines(const char* path, int* n_out) {
    *n_out = 0;
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    char** lines = NULL; int n = 0, cap = 0;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        size_t l = strlen(buf);
        while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = '\0';
        if (n >= cap) { cap = cap ? cap * 2 : 16; lines = (char**)realloc(lines, (size_t)cap * sizeof(char*)); }
        lines[n++] = strdup(buf);
    }
    fclose(f);
    *n_out = n;
    return lines;
}

static void ini_free_lines(char** lines, int n) {
    for (int i = 0; i < n; i++) free(lines[i]);
    free(lines);
}

static void ini_write_lines(const char* path, char** lines, int n) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) if (lines[i]) fprintf(f, "%s\n", lines[i]);
    fclose(f);
}

/* Trova l'intervallo della sezione: start = indice della riga `[section]` (-1 se assente),
 * end = indice della riga della sezione successiva (n se è l'ultima). */
static void ini_find_section(char** lines, int n, const char* section, int* start, int* end) {
    *start = -1; *end = n;
    int in_sec = (section == NULL || *section == '\0');
    for (int i = 0; i < n; i++) {
        const char* t = lines[i];
        while (*t == ' ' || *t == '\t') t++;
        if (*t == '[') {
            if (in_sec) { *end = i; return; }
            const char* close = strchr(t, ']');
            if (close) {
                char name[256];
                size_t l = (size_t)(close - (t + 1));
                if (l >= sizeof(name)) l = sizeof(name) - 1;
                memcpy(name, t + 1, l); name[l] = 0;
                if (strcasecmp(name, section) == 0) { in_sec = 1; *start = i; }
            }
        }
    }
}

void gmb_ini_write_value(const char* path, const char* section, const char* key, const char* value) {
    if (!path) return;
    int n = 0;
    char** lines = ini_load_lines(path, &n);
    int sec_start = -1, sec_end = n, key_line = -1;
    ini_find_section(lines, n, section, &sec_start, &sec_end);
    for (int i = (sec_start >= 0 ? sec_start + 1 : 0); i < sec_end; i++) {
        const char* t = lines[i];
        while (*t == ' ' || *t == '\t') t++;
        const char* eq = strchr(t, '=');
        if (eq && eq > t) {
            char k[256];
            size_t l = (size_t)(eq - t);
            if (l >= sizeof(k)) l = sizeof(k) - 1;
            memcpy(k, t, l); k[l] = 0;
            if (strcasecmp(k, key) == 0) { key_line = i; break; }
        }
    }
    char* newline = (char*)malloc(strlen(key) + strlen(value ? value : "") + 2);
    snprintf(newline, strlen(key) + strlen(value ? value : "") + 2, "%s=%s", key, value ? value : "");
    if (key_line >= 0) {
        free(lines[key_line]); lines[key_line] = newline;
    } else {
        /* Riga nuova: dentro la sezione esistente (subito dopo l'header) oppure in coda
         * creando la sezione. BUG precedente: l'off-by-one su `at` metteva la riga k=v
         * PRIMA dell'header `[sezione]` → la chiave finiva fuori sezione e la lettura
         * dava sempre il default (variante `JSON WRITEVALUE key, val, sezione`). */
        int add_hdr = (sec_start < 0 && section && *section) ? 1 : 0;
        int add_blank = (add_hdr && n > 0 && lines[n-1][0]) ? 1 : 0;
        /* `at` = posizione d'inserimento: dopo l'header della sezione esistente, oppure in
         * CODA (n) quando la sezione va creata. BUG precedente: `n + add_blank` sforava
         * l'array (le righe finite oltre il fondo + una riga NULL → "(null)" nel file). */
        /* Sezione assente: in coda (sec_end = n) se la sezione è NOMINATA, oppure PRIMA
         * della prima sezione (sec_end = indice del primo header) quando section è vuota —
         * canone C# WriteValue: la chiave "root" va sopra la prima `[sezione]`. */
        int at = (sec_start >= 0) ? (sec_start + 1) : sec_end;
        int nb = add_blank + add_hdr + 1;
        lines = (char**)realloc(lines, (size_t)(n + nb) * sizeof(char*));
        for (int i = n; i > at; i--) lines[i + nb - 1] = lines[i-1];
        int w = at;
        if (add_blank) lines[w++] = strdup("");
        if (add_hdr) { char hdr[300]; snprintf(hdr, sizeof(hdr), "[%s]", section); lines[w++] = strdup(hdr); }
        lines[w] = newline;
        n += nb;
    }
    ini_write_lines(path, lines, n);
    ini_free_lines(lines, n);
}

void gmb_ini_write_section(const char* path, const char* section, const gmb_dict_t* values) {
    if (!path || !values) return;
    int n = 0;
    char** lines = ini_load_lines(path, &n);
    int sec_start = -1, sec_end = n;
    ini_find_section(lines, n, section, &sec_start, &sec_end);

    /* blocco nuovo: [section] (se non vuota) + k=v per ogni voce (ordine del dict) */
    char** block = (char**)malloc((size_t)(values->count + 2) * sizeof(char*));
    int nb = 0;
    if (section && *section) {
        char hdr[300]; snprintf(hdr, sizeof(hdr), "[%s]", section);
        block[nb++] = strdup(hdr);
    }
    for (size_t i = 0; i < values->count; i++) {
        const char* k = values->items[i].key ? values->items[i].key : "";
        gmb_string_t vs = gmb_val_json_text(values->items[i].value);
        const char* vt = vs.data ? vs.data : "";
        char* ln = (char*)malloc(strlen(k) + strlen(vt) + 2);
        snprintf(ln, strlen(k) + strlen(vt) + 2, "%s=%s", k, vt);
        block[nb++] = ln;
    }

    /* rimuovi la vecchia sezione (se presente) e calcola il punto d'inserimento */
    int at;
    if (sec_start >= 0) {
        for (int i = sec_start; i < sec_end; i++) free(lines[i]);
        int removed = sec_end - sec_start;
        for (int i = sec_end; i < n; i++) lines[i - removed] = lines[i];
        n -= removed;
        at = sec_start;
    } else {
        at = n;
        /* canone C#: riga vuota di separazione se il file non termina con una vuota */
        if (n > 0 && lines[n-1][0]) {
            lines = (char**)realloc(lines, (size_t)(n + 1) * sizeof(char*));
            lines[n] = strdup(""); n++;
            at = n;
        }
    }

    lines = (char**)realloc(lines, (size_t)(n + nb) * sizeof(char*));
    for (int i = n; i > at; i--) lines[i + nb - 1] = lines[i - 1];
    for (int i = 0; i < nb; i++) lines[at + i] = block[i];
    n += nb;
    free(block);

    ini_write_lines(path, lines, n);
    ini_free_lines(lines, n);
}

gmb_val_t gmb_ini_section_to_dict_val(const char* path, const char* section) {
    int cnt = 0;
    char** lines = gmb_ini_read_section(path, section, &cnt);
    gmb_dict_t d = gmb_dict_new();
    for (int i = 0; i < cnt; i++) {
        char* eq = strchr(lines[i], '=');
        if (eq) { *eq = 0; gmb_dict_set(&d, lines[i], gmb_ini_coerce_val(eq + 1)); }
        else { gmb_dict_set(&d, lines[i], gmb_make_val_string("")); }
        free(lines[i]);
    }
    free(lines);
    return gmb_make_val_dict(d);
}

char** gmb_ini_read_section(const char* path, const char* section, int* count_out) {
    *count_out = 0;
    char* content = frw_slurp(path);
    if (content)
    {
        char clean[16384];
        frw_strip_json_comments(content, clean, sizeof(clean));
        free(content);
        if (!frw_is_json(clean)) goto ini_fallback;
        /* READSECTION su JSON: ritorna le chiavi di primo livello della sezione (oggetto),
         * o dell'oggetto radice se section è vuota. */
        char* obj = NULL;
        if (section && *section)
        {
            obj = gmb_json_extract(clean, section);
            if (!obj || !*obj || strcmp(obj, "null") == 0 || strstr(obj, "{") == NULL)
            { if (obj) free(obj); return NULL; }
        }
        const char* target = (obj && *obj) ? obj : clean;
        gmb_val_t v = gmb_json_parse(target);
        if (obj) free(obj);
        if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
        {
            gmb_dict_t* d = (gmb_dict_t*)((gmb_obj_t*)v.v_ptr)->data;
            if (d)
            {
                int n = (int)d->count;
                char** out = n > 0 ? (char**)malloc((size_t)n * sizeof(char*)) : NULL;
                /* BUG (§0bis, runtime condiviso): qui si ritornavano i SOLI NOMI chiave,
                 * senza il valore → gmb_ini_section_to_dict_val li leggeva come "k" senza '='
                 * → valore "" per ogni campo (READSECTION su file JSON: `host=` vuoto su
                 * VM/C e backend C99, mentre IL/PCode ritornano i valori). Ora ogni riga è
                 * "chiave=testo" col testo JSON del valore (stringa = contenuto, altro =
                 * serializzazione) — stessa forma del ramo INI. */
                for (int i = 0; i < n; i++)
                {
                    const char* k = d->items[i].key ? d->items[i].key : "";
                    gmb_string_t vs = gmb_val_json_text(d->items[i].value);
                    const char* vt = vs.data ? vs.data : "";
                    size_t need = strlen(k) + strlen(vt) + 2;
                    char* ln = (char*)malloc(need);
                    if (ln) snprintf(ln, need, "%s=%s", k, vt);
                    out[i] = ln;
                }
                *count_out = n;
                return out;
            }
        }
        return NULL;
    }
ini_fallback: ;
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    char line[1024];
    int in_sec = (section == NULL || *section == '\0');
    char** out = NULL;
    int n = 0, cap = 0;
    while (fgets(line, sizeof(line), f)) {
        char* t = line;
        while (*t == ' ' || *t == '\t') t++;
        char* end = t + strlen(t);
        while (end > t && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) *--end = '\0';
        if (*t == '[') {
            char* close = strchr(t, ']');
            in_sec = 0;
            if (close) { *close = '\0'; if (section && strcasecmp(t + 1, section) == 0) in_sec = 1; }
            continue;
        }
        if (!in_sec) continue;
        if (*t == '\0' || *t == ';' || *t == '#') continue;
        if (n >= cap) { cap = cap ? cap * 2 : 8; out = (char**)realloc(out, cap * sizeof(char*)); }
        out[n++] = strdup(t);
    }
    fclose(f);
    *count_out = n;
    return out;
}

/* ==================== Path helpers (APPPATH/TEMPFILE/READBYTES/COUNTLINES) ==================== */

/* APPPATH: directory del programma eseguibile (headless: cwd). */
char* gmb_path_app_dir(void) { char* c = getcwd(NULL, 0); return c ? c : strdup(""); }
/* PROJECTPATH/PROJECTFOLDER: stessa directory base. */
char* gmb_path_project_dir(void) { return gmb_path_app_dir(); }

/* PARENTDIR: directory del file/path dato. */
/* Variante `char*` della stessa semantica di gmb_fs_dir_name (sopra): usata da VM/C per
 * PARENTDIR (func_code 578). Tenere le due allineate: sono la STESSA funzione in due firme. */
char* gmb_path_parent(const char* path) {
    if (!path || !*path) return strdup("");
    size_t n = strlen(path);
    if (path[n - 1] == '/' || path[n - 1] == '\\') {
        while (n > 0 && (path[n - 1] == '/' || path[n - 1] == '\\')) n--;
        char* r = (char*)malloc(n + 1);
        if (!r) return strdup("");
        memcpy(r, path, n); r[n] = 0;
        return r;
    }
    size_t i = n;
    while (i > 0 && path[i - 1] != '/' && path[i - 1] != '\\') i--;
    if (i == 0) return strdup("");
    size_t cut = i - 1;
    while (cut > 1 && (path[cut - 1] == '/' || path[cut - 1] == '\\')) cut--;
    if (cut == 0) return strdup("/");
    char* r = (char*)malloc(cut + 1);
    if (!r) return strdup("");
    memcpy(r, path, cut); r[cut] = 0;
    return r;
}

/* TEMPFILE: nome univoco di file temporaneo — parità C# `Path.GetTempFileName()`
 * (2026-10-27, allineamento dei path: "i path di lunghezza corretta vanno allineati").
 * Prima il C scriveva SEMPRE "/tmp/gmb_tmp_XXXXXX" (19 caratteri) mentre IL/PCode davano
 * "<TEMPDIR()>tmpXXXXXX.tmp" (62 = 49 di TEMPDIR + 13 di nome): due divergenze — la
 * directory (/tmp fisso invece di $TMPDIR) e lo schema del nome. Ora: directory da
 * gmb_path_temp_dir() (la stessa di TEMPDIR(), $TMPDIR su Unix, %TEMP% su Windows) e nome
 * `tmp` + 6 caratteri casuali + `.tmp`, come `Path.GetTempFileName()`. Il file viene
 * CREATO e LASCIATO su disco (semantica .NET: file vuoto con quel nome), non cancellato. */
char* gmb_path_temp_file(void) {
    char* dir = gmb_path_temp_dir();
    char tmpl[4096];
    snprintf(tmpl, sizeof(tmpl), "%stmpXXXXXX.tmp", (dir && *dir) ? dir : "/tmp/");
    free(dir);
    /* mkstemps(template, 4): le sei X sono seguite dal suffisso ".tmp" (4 caratteri).
     * `mkstemp` NON va bene qui: pretende le X in CODA — con `tmpXXXXXX.tmp` falliva sempre
     * e la funzione ritornava il template con le X letterali (misurato sulla gamba VM/C:
     * ".../GmbTestRun-63715-5b0cade4/tmpXXXXXX.tmp"). */
    int fd = mkstemps(tmpl, 4);
    if (fd >= 0) close(fd);
    return strdup(tmpl);
}
/* TEMPDIR: directory temporanea di sistema — parità C# Path.GetTempPath() (2026-09-23,
 * Parita-5-Backend-Residui §2 — SkillsMarketplaceDemo): $TMPDIR (non /tmp fisso) e SEMPRE
 * con separatore finale. Prima il C dava "/tmp" senza slash → `TEMPDIR()+"x"` diventava
 * "/tmpx" mentre IL/PCode davano "<tmp>/x". Chiamanti: solo i builtin BASIC (573/574) e
 * l'emissione C99Native — nessun uso interno. */
char* gmb_path_temp_dir(void) {
#if defined(_WIN32) || defined(_WIN64)
    const char* t = getenv("TEMP");
    if (!t || !*t) t = "C:\\Temp";
    size_t n = strlen(t);
    char* out = (char*)malloc(n + 2);
    if (!out) return NULL;
    memcpy(out, t, n);
    if (n > 0 && out[n - 1] != '\\' && out[n - 1] != '/') out[n++] = '\\';
    out[n] = '\0';
    return out;
#else
    const char* t = getenv("TMPDIR");
    if (!t || !*t) t = "/tmp";
    size_t n = strlen(t);
    while (n > 1 && t[n - 1] == '/') n--;
    char* out = (char*)malloc(n + 2);
    if (!out) return NULL;
    memcpy(out, t, n);
    out[n++] = '/';
    out[n] = '\0';
    return out;
#endif
}

/* RESOLVEFILEPATH: percorso assoluto normalizzato. */
char* gmb_path_resolve(const char* path) {
#if defined(_WIN32) || defined(_WIN64)
    if (!path || !*path) return strdup(path ? path : "");
    char* rp = _fullpath(NULL, path, 0);
    return rp ? rp : strdup(path);
#else
    char* rp = realpath(path, NULL);
    return rp ? rp : strdup(path ? path : "");
#endif
}

/* ===== SET PATH / entry / current file (APPPATH/BINPATH/PROJECTFOLDER/GETFOLDERPATH) =====
 * SET PATH (Clipper-style) = lista dei percorsi di ricerca dei database per i motori
 * DBF/ISAM/SPEED/LEDGER. Default = cartella del file .bas entry-point. APPPATH() la
 * espone separata da ';'. PROJECTFOLDER() usa il file .bas CORRENTE (import-aware),
 * tracciato a ogni ingresso di procedura. GETFOLDERPATH(name[,depth]) cerca la cartella
 * per NOME salendo fino alla root, poi scendendo in BFS fino a `depth` livelli. */
static char* gmb_s_entry_path = NULL;    /* path assoluto del .bas entry-point */
static char** gmb_s_search = NULL;       /* lista SET PATH */
static int gmb_s_search_n = 0;
static char* gmb_s_current_file = NULL;  /* .bas corrente (import-aware) */
static char* gmb_s_default_dir = NULL;   /* SET DEFAULT: directory primaria (creazione + primo search) */
static int gmb_s_search_explicit = 0;   /* 1 = SET PATH eseguito esplicitamente (anche vuoto) */

static char* gmb_path_entry_dir(void) {
    if (gmb_s_entry_path && *gmb_s_entry_path) return gmb_path_parent(gmb_s_entry_path);
    return getcwd(NULL, 0);
}

/* SET DEFAULT TO dir: directory primaria (creazione + primo search). */
void gmb_path_set_default(const char* path) {
    free(gmb_s_default_dir);
    gmb_s_default_dir = NULL;
    if (path && *path) {
        if (path[0] == '/' || path[0] == '\\') gmb_s_default_dir = strdup(path);
        else { char* base = gmb_path_entry_dir(); char tmp[4096]; snprintf(tmp, sizeof(tmp), "%s/%s", base ? base : ".", path); gmb_string_t norm = gmb_fs_normalize_path(gmb_string_alloc(tmp)); gmb_s_default_dir = strdup(norm.data ? norm.data : tmp); free(norm.data); free(base); }
    }
}

/* SET PATH TO joined: lista aggiuntiva di ricerca (separatori ';' o ','). null o vuoto
 * → SVUOTA la lista (parità CA-Clipper: si cerca solo nella DEFAULT). Il default (cartella
 * entry) vale solo se SET PATH non è MAI stato eseguito (impostato da gmb_path_set_entry). */
void gmb_path_set_search(const char* joined) {
    int i;
    gmb_s_search_explicit = 1;
    for (i = 0; i < gmb_s_search_n; i++) free(gmb_s_search[i]);
    free(gmb_s_search);
    gmb_s_search = NULL;
    gmb_s_search_n = 0;
    if (joined == NULL || !*joined) return;
    {
        char* copy = strdup(joined);
        char* save = NULL;
        char* tok = strtok_r(copy, ";,", &save);
        int cap = 0;
        while (tok) {
            while (*tok == ' ' || *tok == '	') tok++;
            size_t l = strlen(tok);
            while (l > 0 && (tok[l-1] == ' ' || tok[l-1] == '	')) tok[--l] = '\0';
            if (*tok) {
                if (gmb_s_search_n >= cap) { cap = cap ? cap * 2 : 4; gmb_s_search = (char**)realloc(gmb_s_search, (size_t)cap * sizeof(char*)); }
                if (tok[0] == '/' || tok[0] == '\\') {
                    gmb_s_search[gmb_s_search_n++] = strdup(tok);
                } else {
                    char* base = gmb_path_entry_dir();
                    char tmp[4096];
                    snprintf(tmp, sizeof(tmp), "%s/%s", base ? base : ".", tok);
                    gmb_string_t norm = gmb_fs_normalize_path(gmb_string_alloc(tmp));
                    gmb_s_search[gmb_s_search_n++] = strdup(norm.data ? norm.data : tmp);
                    free(norm.data);
                    free(base);
                }
            }
            tok = strtok_r(NULL, ";,", &save);
        }
        free(copy);
    }
    if (gmb_s_search_n == 0) {
        char* base = gmb_path_entry_dir();
        gmb_s_search = (char**)malloc(sizeof(char*));
        gmb_s_search[gmb_s_search_n++] = base ? base : strdup("");
    }
}

/* Imposta il file entry-point; default SET PATH = la sua cartella. */
void gmb_path_set_entry(const char* path) {
    int i;
    free(gmb_s_entry_path);
    gmb_s_entry_path = NULL;
    if (path && *path) {
        char* r = gmb_path_resolve(path);
        gmb_s_entry_path = r;
    }
    /* default SET PATH = cartella entry (solo se mai eseguito SET PATH) */
    gmb_s_search_explicit = 0;
    for (i = 0; i < gmb_s_search_n; i++) free(gmb_s_search[i]);
    free(gmb_s_search);
    gmb_s_search = NULL;
    gmb_s_search_n = 0;
    {
        char* base = gmb_path_entry_dir();
        gmb_s_search = (char**)malloc(sizeof(char*));
        gmb_s_search[gmb_s_search_n++] = base ? base : strdup("");
    }
    gmb_path_set_current_file(gmb_s_entry_path);
}

/* Traccia il file .bas correntemente in esecuzione (PROJECTFOLDER import-aware). */
void gmb_path_set_current_file(const char* path) {
    free(gmb_s_current_file);
    gmb_s_current_file = path ? strdup(path) : NULL;
}

/* APPPATH(): lista SET PATH separata da ';'. */
char* gmb_path_search_paths(void) {
    if (gmb_s_search_n <= 0) return gmb_s_search_explicit ? strdup("") : gmb_path_app_dir();
    size_t need = 1;
    int i;
    for (i = 0; i < gmb_s_search_n; i++) need += strlen(gmb_s_search[i]) + 1;
    char* out = (char*)malloc(need);
    out[0] = '\0';
    for (i = 0; i < gmb_s_search_n; i++) {
        if (i) strcat(out, ";");
        strcat(out, gmb_s_search[i]);
    }
    return out;
}

/* BINPATH(): directory base dell'host (headless: cwd, come gmb_path_app_dir). */
char* gmb_path_bin_dir(void) { return gmb_path_app_dir(); }

/* PROJECTFOLDER(): cartella reale del file .bas corrente (import-aware). */
char* gmb_path_current_file_dir(void) {
    if (gmb_s_current_file && *gmb_s_current_file) return gmb_path_parent(gmb_s_current_file);
    return gmb_path_entry_dir();
}

/* Risolve un path di database (parità CA-Clipper): assoluto → DEFAULT dir (SET DEFAULT,
 * default = cartella entry) → lista SET PATH. Fallback alla DEFAULT per la creazione. */
char* gmb_path_resolve_db(const char* path) {
    if (!path || !*path || path[0] == '/' || path[0] == '\\') return gmb_path_resolve(path ? path : "");
    struct stat st;
    char* primary = (gmb_s_default_dir && *gmb_s_default_dir) ? strdup(gmb_s_default_dir) : gmb_path_entry_dir();
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s/%s", primary ? primary : ".", path);
    if (stat(tmp, &st) == 0) { free(primary); return strdup(tmp); }
    if (gmb_s_search_n > 0) {
        int i;
        for (i = 0; i < gmb_s_search_n; i++) {
            snprintf(tmp, sizeof(tmp), "%s/%s", gmb_s_search[i], path);
            if (stat(tmp, &st) == 0) { free(primary); return strdup(tmp); }
        }
    }
    free(primary);
    return strdup(path);
}

/* RESOLVEFILEPATH: risolve un path file/dir. Assoluto → così com'è (se esiste); relativo →
 * cartella entry e lista SET PATH. "" se non esiste da nessuna parte (parità C# ResolveFilePath). */
char* gmb_path_resolve_file(const char* path) {
    if (!path || !*path) return strdup("");
    struct stat st;
    if (path[0] == '/' || path[0] == '\\') {
        if (stat(path, &st) == 0) return strdup(path);
        return strdup("");
    }
    char* base = gmb_path_entry_dir();
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s/%s", base ? base : ".", path);
    if (stat(tmp, &st) == 0) { char* r = gmb_path_resolve(tmp); free(base); return r; }
    if (gmb_s_search_n > 0) {
        int i;
        for (i = 0; i < gmb_s_search_n; i++) {
            snprintf(tmp, sizeof(tmp), "%s/%s", gmb_s_search[i], path);
            if (stat(tmp, &st) == 0) { char* r = gmb_path_resolve(tmp); free(base); return r; }
        }
    }
    free(base);
    return strdup("");
}

/* GETFOLDERPATH(name, maxDepth): cerca la cartella per NOME salendo dalla cartella entry
 * fino alla root; se assente, scende in BFS fino a maxDepth livelli. Stringa vuota se no. */
char* gmb_path_find_folder(const char* name, int max_depth) {
    if (!name || !*name) return strdup("");
    char* start = gmb_path_entry_dir();
    if (!start || !*start) { free(start); return strdup(""); }
    char* cur = strdup(start);
    while (cur && *cur) {
        char tmp[4096];
        struct stat st;
        snprintf(tmp, sizeof(tmp), "%s/%s", cur, name);
        if (stat(tmp, &st) == 0 && S_ISDIR(st.st_mode)) {
            char* r = gmb_path_resolve(tmp);
            free(cur);
            free(start);
            return r;
        }
        char* parent = gmb_path_parent(cur);
        if (!parent || !*parent || strcmp(parent, cur) == 0) { free(parent); break; }
        free(cur);
        cur = parent;
    }
    free(cur);
    if (max_depth > 0) {
        char** frontier = (char**)malloc(sizeof(char*));
        frontier[0] = strdup(start);
        int f_n = 1;
        int depth;
        for (depth = 1; depth <= max_depth && f_n > 0; depth++) {
            char** next = NULL;
            int n_n = 0, cap = 0, i;
            for (i = 0; i < f_n; i++) {
                DIR* d = opendir(frontier[i]);
                if (!d) continue;
                struct dirent* e;
                while ((e = readdir(d)) != NULL) {
                    if (e->d_name[0] == '.') continue;
                    char sub[4096];
                    struct stat st;
                    snprintf(sub, sizeof(sub), "%s/%s", frontier[i], e->d_name);
                    if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
                    if (strcmp(e->d_name, name) == 0) {
                        char* r = gmb_path_resolve(sub);
                        closedir(d);
                        { int j; for (j = 0; j < f_n; j++) free(frontier[j]); free(frontier); }
                        free(start);
                        return r;
                    }
                    if (n_n >= cap) { cap = cap ? cap * 2 : 8; next = (char**)realloc(next, (size_t)cap * sizeof(char*)); }
                    next[n_n++] = strdup(sub);
                }
                closedir(d);
            }
            for (i = 0; i < f_n; i++) free(frontier[i]);
            free(frontier);
            frontier = next;
            f_n = n_n;
        }
        if (frontier) { int j; for (j = 0; j < f_n; j++) free(frontier[j]); free(frontier); }
    }
    free(start);
    return strdup("");
}

/* READBYTES: legge un intero file → byte. */
uint8_t* gmb_read_all_bytes(const char* path, size_t* len_out) {
    *len_out = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *len_out = rd;
    return buf;
}

/* READLINES: legge le righe di un file → array di stringhe. */
char** gmb_read_all_lines(const char* path, int* count_out) {
    *count_out = 0;
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    char** lines = NULL;
    int n = 0, cap = 0;
    char buf[8192];
    while (fgets(buf, sizeof(buf), f)) {
        size_t l = strlen(buf);
        while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = '\0';
        if (n >= cap) { cap = cap ? cap * 2 : 16; lines = (char**)realloc(lines, cap * sizeof(char*)); }
        lines[n++] = strdup(buf);
    }
    fclose(f);
    *count_out = n;
    return lines;
}

/* COUNTLINES: numero di righe di un file. */
long gmb_count_lines(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    long n = 0;
    int ch;
    while ((ch = fgetc(f)) != EOF) if (ch == '\n') n++;
    fclose(f);
    return n;
}

/* ── MEMORY-MAPPED FILES (POSIX mmap) — suite MEMORYMAP.* ─────────────────── */
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Handle VALIDATO tramite registry (mai puntatori nudi esposti al BASIC): un handle
 * corrotto/stale viene rifiutato (return -1/0) invece di dereferenziare memoria
 * arbitraria (prima h == puntatore mmap → segfault con handle fasullo). Nella pagina
 * HEADER del mapping salviamo la lunghezza: map[0] = length (8 byte). */
#define GMB_MMAP_MAX 32

typedef struct {
    int used;
    int64_t handle;
    void* map;
    size_t map_len;
} gmb_mmap_t;

static gmb_mmap_t g_mmaps[GMB_MMAP_MAX];
static int64_t g_mmap_next = 1;

static gmb_mmap_t* mmap_find(int64_t h)
{
    for (int i = 0; i < GMB_MMAP_MAX; i++)
        if (g_mmaps[i].used && g_mmaps[i].handle == h) return &g_mmaps[i];
    return NULL;
}

int64_t gmb_mmap_open(const char* path)
{
    if (!path) return 0;
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return 0;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < 0) { close(fd); return 0; }
    size_t len = (size_t)st.st_size;
    if (len < 4096) { if (ftruncate(fd, (off_t)4096) != 0) { close(fd); return 0; } len = 4096; }
    size_t map_len = len + 16;
    void* base = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (base == MAP_FAILED) return 0;
    int slot = -1;
    for (int i = 0; i < GMB_MMAP_MAX; i++)
        if (!g_mmaps[i].used) { slot = i; break; }
    if (slot < 0) { munmap(base, map_len); return 0; }
    memset(base, 0, 16);
    memcpy(base, &len, sizeof(size_t));
    g_mmaps[slot].used = 1;
    g_mmaps[slot].handle = g_mmap_next++;
    g_mmaps[slot].map = base;
    g_mmaps[slot].map_len = map_len;
    return g_mmaps[slot].handle;
}

int64_t gmb_mmap_peek(int64_t h, int64_t offset)
{
    gmb_mmap_t* m = mmap_find(h);
    if (!m || offset < 0) return -1;
    size_t len = 0;
    memcpy(&len, m->map, sizeof(size_t));
    if ((size_t)offset >= len) return -1;
    return ((unsigned char*)m->map)[16 + (size_t)offset];
}

int gmb_mmap_poke(int64_t h, int64_t offset, int value)
{
    gmb_mmap_t* m = mmap_find(h);
    if (!m || offset < 0) return -1;
    size_t len = 0;
    memcpy(&len, m->map, sizeof(size_t));
    if ((size_t)offset >= len) return -1;
    ((unsigned char*)m->map)[16 + (size_t)offset] = (unsigned char)(value & 0xFF);
    return 0;
}

int gmb_mmap_write_bytes(int64_t h, int64_t offset, const unsigned char* src, int count)
{
    gmb_mmap_t* m = mmap_find(h);
    if (!m || offset < 0 || !src || count < 0) return -1;
    size_t len = 0;
    memcpy(&len, m->map, sizeof(size_t));
    if ((size_t)offset + (size_t)count > len) count = (int)(len - (size_t)offset);
    if (count <= 0) return 0;
    memcpy((unsigned char*)m->map + 16 + (size_t)offset, src, (size_t)count);
    return count;
}

/* MMAPWRITEBYTES da array BYTE nativo C (gmb_int_t*): converte gli elementi in byte
 * (troncati a 8 bit) e delega a gmb_mmap_write_bytes. Serve ai due emitter C dove
 * `DIM x() AS BYTE` è gmb_int_t[] (un cast diretto leggerebbe i byte grezzi degli
 * interi a 8 byte). */
int gmb_mmap_write_bytes_i64(int64_t h, int64_t offset, const int64_t* src, int count)
{
    if (!src || count <= 0) return 0;
    if (count > 65536) count = 65536;
    unsigned char* tmp = (unsigned char*)malloc((size_t)count);
    if (!tmp) return -1;
    for (int i = 0; i < count; i++) tmp[i] = (unsigned char)(src[i] & 0xFF);
    int rc = gmb_mmap_write_bytes(h, offset, tmp, count);
    free(tmp);
    return rc;
}

/* Variante per l'emitter C99Native, dove `DIM x() AS BYTE` è gmb_double_t*
 * (array numerici double nel mondo C nativo). Stessa semantica della _i64. */
int gmb_mmap_write_bytes_f64(int64_t h, int64_t offset, const double* src, int count)
{
    if (!src || count <= 0) return 0;
    if (count > 65536) count = 65536;
    unsigned char* tmp = (unsigned char*)malloc((size_t)count);
    if (!tmp) return -1;
    for (int i = 0; i < count; i++) tmp[i] = (unsigned char)(((long long)src[i]) & 0xFF);
    int rc = gmb_mmap_write_bytes(h, offset, tmp, count);
    free(tmp);
    return rc;
}

int gmb_mmap_read_bytes(int64_t h, int64_t offset, unsigned char* out, int count)
{
    gmb_mmap_t* m = mmap_find(h);
    if (!m || offset < 0 || !out || count < 0) return 0;
    size_t len = 0;
    memcpy(&len, m->map, sizeof(size_t));
    if ((size_t)offset + (size_t)count > len) count = (int)(len - (size_t)offset);
    if (count <= 0) return 0;
    memcpy(out, (unsigned char*)m->map + 16 + (size_t)offset, (size_t)count);
    return count;
}

int gmb_mmap_close(int64_t h)
{
    gmb_mmap_t* m = mmap_find(h);
    if (!m) return -1;
    munmap(m->map, m->map_len);
    memset(m, 0, sizeof(*m));
    return 0;
}

/* gmb_mmap_read_bytes_list: legge count byte da offset → gmb_list_t di double. */
gmb_list_t gmb_mmap_read_bytes_list(int64_t h, int64_t offset, int count) {
    gmb_list_t l = gmb_list_new();
    unsigned char* buf = (unsigned char*)calloc(count > 0 ? (size_t)count : 1, 1);
    if (!buf) return l;
    int n = gmb_mmap_read_bytes(h, offset, buf, count);
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_double(buf[i]));
    free(buf);
    return l;
}

int gmb_file_get_fd(gmb_int_t handle) {
    if (handle < 0 || handle >= GMB_MAX_FILES || !gmb_file_handles[handle]) return -1;
    return fileno(gmb_file_handles[handle]);
}

int gmb_file_lock(gmb_int_t handle, long position, long length) {
    int fd = gmb_file_get_fd(handle);
    if (fd < 0) return 0;
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = position;
    fl.l_len = length;
    return fcntl(fd, F_SETLK, &fl) == 0;
}

int gmb_file_unlock(gmb_int_t handle, long position, long length) {
    int fd = gmb_file_get_fd(handle);
    if (fd < 0) return 0;
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = position;
    fl.l_len = length;
    return fcntl(fd, F_SETLK, &fl) == 0;
}

/* WRITEBYTES(path, bytes): scrive un blocco di byte su file in modo BYTE-ESATTO.
 * Perché serve: `gmb_file_write_string` usa fputs(), quindi si ferma al primo 0x00 e non
 * può produrre formati binari (BMP, PSF2, TTF, PNG). Qui si apre in "wb" e si usa fwrite.
 * Ritorna il numero di byte scritti, -1 se il file non si apre (path vuoto/non scrivibile). */
int gmb_write_all_bytes(const char* path, const uint8_t* data, size_t len) {
    if (!path || !*path) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t wr = 0;
    if (len > 0 && data) wr = fwrite(data, 1, len, f);
    if (fclose(f) != 0) return -1;
    return (int)wr;
}

/* Variante per gli array BASIC: la lista del runtime contiene gmb_val_t (numeri 0..255).
 * Ogni elemento è mascherato a 8 bit, così un BYTE negativo (= -1 per BIT) non sfonda. */
/* Variante per il backend C99Native: gli array `DIM b(n) AS BYTE` sono double[]. */
int gmb_write_all_bytes_f64(const char* path, const double* src, int count) {
    if (!path || !*path) return -1;
    int n = count > 0 ? count : 0;
    uint8_t* buf = n > 0 ? (uint8_t*)malloc((size_t)n) : NULL;
    if (n > 0 && !buf) return -1;
    for (int i = 0; i < n; i++) {
        long d = (long)src[i];
        buf[i] = (uint8_t)(d & 0xFF);
    }
    int rc = gmb_write_all_bytes(path, buf, (size_t)n);
    free(buf);
    return rc;
}

int gmb_write_all_bytes_list(const char* path, gmb_list_t list) {
    if (!path || !*path) return -1;
    size_t n = (size_t)(list.count > 0 ? list.count : 0);
    uint8_t* buf = n > 0 ? (uint8_t*)malloc(n) : NULL;
    if (n > 0 && !buf) return -1;
    for (size_t i = 0; i < n; i++) {
        gmb_val_t v = list.items[i];
        long d = 0;
        if (v.kind == GMB_TYPE_DOUBLE) d = (long)v.v_double;
        else if (v.kind == GMB_TYPE_INT) d = (long)v.v_int;
        buf[i] = (uint8_t)(d & 0xFF);
    }
    int rc = gmb_write_all_bytes(path, buf, n);
    free(buf);
    return rc;
}
