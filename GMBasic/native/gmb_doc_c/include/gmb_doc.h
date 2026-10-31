/* gmb_doc.h — Conversione documentale in C (namespace CONVERT, parity C# ConversionLib).
 * Formati: txt, csv, md, html, rtf, docx, epub, odt, xlsx, pptx (ZIP+XML via gmb_compress),
 * pdf (PDFOxide C ABI via dlsym, gmb_doc_pdf.c), url/web (via gmb_net_http_get).
 * Render: markdown / html / text.
 * Tutte le funzioni ritornano stringhe allocate (da liberare con free()). */
#ifndef GMB_DOC_H
#define GMB_DOC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convert un file in fmt ∈ {"md","html","text"}. Ritorna NULL su errore. */
char* gmb_doc_convert(const char* path, const char* fmt);

/* Convert un buffer in memoria (ext = estensione senza punto, es. "docx"). */
char* gmb_doc_convert_buffer(const unsigned char* data, size_t len, const char* ext, const char* fmt);

/* Convert una cartella: tutti i file convertiti nel fmt, separati da "--- FILE: <name> ---". */
char* gmb_doc_batch(const char* dir, const char* fmt, int recursive);

/* Web: scarica l'URL (gmb_net_http_get) e converte l'HTML in fmt. */
char* gmb_doc_web(const char* url, const char* fmt);

/* Elenco formati supportati ("txt,csv,md,html,rtf,docx,epub,odt,xlsx,pptx,pdf"). */
void gmb_doc_formats(char* out, int cap);

/* Formati come stringa allocata ("txt,csv,..."). */
char* gmb_doc_formats_s(void);

/* Selfcheck (ritorna 0 ok). */
int gmb_doc_selfcheck(void);

/* ── PDF (PDFOxide C ABI, caricata via dlsym da Resources/libs/<os>/libpdf_oxide.*) ── */
/* Apre un PDF; ritorna l'handle (NULL su errore). Chiudere con gmb_doc_pdf_close. */
void* gmb_doc_pdf_open(const char* path);
void gmb_doc_pdf_close(void* h);
int  gmb_doc_pdf_page_count(void* h);
char* gmb_doc_pdf_extract_markdown(void* h, int page);
char* gmb_doc_pdf_extract_text(void* h, int page);
char* gmb_doc_pdf_extract_html(void* h, int page);

/* Search: una riga "p<page>|<testo>" per ogni match (case-insensitive). */
char* gmb_doc_pdf_search(void* h, const char* pattern);

/* Metadata: "pages=N; version=M.m" (allocata, free()). */
char* gmb_doc_pdf_metadata(void* h);

/* Estrae le immagini incorporate della pagina in out_dir (img_<page>_<n>.<fmt>);
 * ritorna il numero di file scritti. */
int gmb_doc_pdf_extract_images(void* h, int page, const char* out_dir);

/* Render pagina → buffer RGBA8888 premultiplied (malloc, free()). Ritorna NULL su errore. */
uint8_t* gmb_doc_pdf_render_rgba(void* h, int page, int dpi, int* out_w, int* out_h);

/* Render pagina → file PNG (RGBA8). Ritorna 0 ok, -1 errore. */
int gmb_doc_pdf_render_png(void* h, int page, int dpi, const char* out_path);

/* Convert un file PDF in fmt {"md","html","text"} (page=0 = tutto il documento). */
char* gmb_doc_pdf_convert(const char* path, const char* fmt);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DOC_H */

/* ── Namespace DOCUMENT/PDF — API a handle intero (registry) ── */
int  gmb_doc_open(const char* path);      /* ritorna handle >=1, 0 su errore */
void gmb_doc_close(int h);
int  gmb_doc_page_count(int h);
char* gmb_doc_extract_markdown(int h, int page);  /* page=-1 = intero documento */
char* gmb_doc_extract_text(int h, int page);
char* gmb_doc_extract_html(int h, int page);
char* gmb_doc_metadata(int h);
char* gmb_doc_search(int h, const char* pattern);
int  gmb_doc_extract_images(int h, int page, const char* out_dir);
int  gmb_doc_render_png(int h, int page, int dpi, const char* out_path);

/* DOCUMENT.EXTRACT: text-layer markdown (PDF via PDFOxide; altri formati via gmb_doc_convert). */
char* gmb_doc_pdf_extract_hybrid(void* h);
char* gmb_doc_extract(const char* path);

/* Comandi legacy CONVERT EXTRACT TO / RENDER TO. */
int gmb_doc_extract_to(const char* path, const char* dest);
int gmb_doc_render_to(const char* path, const char* dest);

char* gmb_doc_extract_engine(const char* path);
