/* gmb_doc_selfcheck.c — verifica conversione txt/csv/html/rtf/docx */
#include "gmb_doc.h"
#include "gmb_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int check(const char* name, int ok)
{
    printf("%s: %s\n", name, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

int main(void)
{
    int fail = 0;
    char tmp[] = "/tmp/gmbdoc_sc_XXXXXX";
    if (!mkdtemp(tmp)) return 1;

    /* txt → md */
    { char p[512]; snprintf(p, sizeof(p), "%s/a.txt", tmp);
      FILE* f = fopen(p, "w"); fprintf(f, "Hello World\nSecond line\n"); fclose(f);
      char* r = gmb_doc_convert(p, "md");
      fail |= check("txt->md", r && strstr(r, "Hello World") != NULL);
      free(r); }

    /* csv → md (tabella) */
    { char p[512]; snprintf(p, sizeof(p), "%s/b.csv", tmp);
      FILE* f = fopen(p, "w"); fprintf(f, "Name,Age\nAnn,30\nBob,25\n"); fclose(f);
      char* r = gmb_doc_convert(p, "md");
      fail |= check("csv->md", r && strstr(r, "Ann") != NULL);
      free(r); }

    /* html → md (strip tag) */
    { char p[512]; snprintf(p, sizeof(p), "%s/c.html", tmp);
      FILE* f = fopen(p, "w"); fprintf(f, "<html><body><h1>Title</h1><p>Body &amp; more</p></body></html>"); fclose(f);
      char* r = gmb_doc_convert(p, "md");
      fail |= check("html->md", r && strstr(r, "Body & more") != NULL);
      free(r); }

    /* rtf → text */
    { char p[512]; snprintf(p, sizeof(p), "%s/d.rtf", tmp);
      FILE* f = fopen(p, "w"); fprintf(f, "{\\rtf1 Hello \\b bold \\par world}"); fclose(f);
      char* r = gmb_doc_convert(p, "text");
      fail |= check("rtf->text", r && strstr(r, "Hello") != NULL);
      free(r); }

    /* formats */
    { char buf[128]; gmb_doc_formats(buf, sizeof(buf));
      fail |= check("formats", strstr(buf, "docx") != NULL && strstr(buf, "pdf") != NULL); }

    /* docx → md (ZIP+XML, zip reale incorporato) */
    {
        static const unsigned char g_docx_zip[] = {
          80,75,3,4,20,0,0,0,0,0,2,13,27,93,245,100,
          125,34,123,0,0,0,123,0,0,0,17,0,0,0,119,111,
          114,100,47,100,111,99,117,109,101,110,116,46,120,109,108,60,
          63,120,109,108,32,118,101,114,115,105,111,110,61,34,49,46,
          48,34,63,62,60,119,58,100,111,99,117,109,101,110,116,32,
          120,109,108,110,115,58,119,61,34,120,34,62,60,119,58,98,
          111,100,121,62,60,119,58,112,62,60,119,58,114,62,60,119,
          58,116,62,72,101,108,108,111,32,82,69,65,76,32,68,79,
          67,88,60,47,119,58,116,62,60,47,119,58,114,62,60,47,
          119,58,112,62,60,47,119,58,98,111,100,121,62,60,47,119,
          58,100,111,99,117,109,101,110,116,62,80,75,1,2,20,3,
          20,0,0,0,0,0,2,13,27,93,245,100,125,34,123,0,
          0,0,123,0,0,0,17,0,0,0,0,0,0,0,0,0,
          0,0,128,1,0,0,0,0,119,111,114,100,47,100,111,99,
          117,109,101,110,116,46,120,109,108,80,75,5,6,0,0,0,
          0,1,0,1,0,63,0,0,0,170,0,0,0,0,0
        };
        char docxp[512]; snprintf(docxp, sizeof(docxp), "%s/real.docx", tmp);
        FILE* f = fopen(docxp, "wb");
        fwrite(g_docx_zip, 1, sizeof(g_docx_zip), f);
        fclose(f);
        char* r = gmb_doc_convert(docxp, "md");
        fail |= check("docx->md", r && strstr(r, "Hello REAL DOCX") != NULL);
        free(r);
    }

    /* web (headless: può fallire senza rete → non fail) */
    { char* r = gmb_doc_web("https://example.com", "text");
      if (r) { check("web->text", strstr(r, "Example") != NULL); free(r); }
      else printf("web->text: SKIP (no net)\n"); }

    /* pdf → md (PDFOxide via dlsym; SKIP se la lib non è caricabile) */
    {
        static const unsigned char g_mini_pdf[] = {
          37, 80, 68, 70, 45, 49, 46, 52, 10, 49, 32, 48,
          32, 111, 98, 106, 10, 60, 60, 32, 47, 84, 121, 112,
          101, 32, 47, 67, 97, 116, 97, 108, 111, 103, 32, 47,
          80, 97, 103, 101, 115, 32, 50, 32, 48, 32, 82, 32,
          62, 62, 10, 101, 110, 100, 111, 98, 106, 10, 50, 32,
          48, 32, 111, 98, 106, 10, 60, 60, 32, 47, 84, 121,
          112, 101, 32, 47, 80, 97, 103, 101, 115, 32, 47, 75,
          105, 100, 115, 32, 91, 51, 32, 48, 32, 82, 93, 32,
          47, 67, 111, 117, 110, 116, 32, 49, 32, 62, 62, 10,
          101, 110, 100, 111, 98, 106, 10, 51, 32, 48, 32, 111,
          98, 106, 10, 60, 60, 32, 47, 84, 121, 112, 101, 32,
          47, 80, 97, 103, 101, 32, 47, 80, 97, 114, 101, 110,
          116, 32, 50, 32, 48, 32, 82, 32, 47, 77, 101, 100,
          105, 97, 66, 111, 120, 32, 91, 48, 32, 48, 32, 54,
          49, 50, 32, 55, 57, 50, 93, 32, 47, 67, 111, 110,
          116, 101, 110, 116, 115, 32, 52, 32, 48, 32, 82, 32,
          47, 82, 101, 115, 111, 117, 114, 99, 101, 115, 32, 60,
          60, 32, 47, 70, 111, 110, 116, 32, 60, 60, 32, 47,
          70, 49, 32, 53, 32, 48, 32, 82, 32, 62, 62, 32,
          62, 62, 32, 62, 62, 10, 101, 110, 100, 111, 98, 106,
          10, 52, 32, 48, 32, 111, 98, 106, 10, 60, 60, 32,
          47, 76, 101, 110, 103, 116, 104, 32, 53, 48, 32, 62,
          62, 10, 115, 116, 114, 101, 97, 109, 10, 66, 84, 32,
          47, 70, 49, 32, 50, 52, 32, 84, 102, 32, 55, 50,
          32, 55, 48, 48, 32, 84, 100, 32, 40, 72, 101, 108,
          108, 111, 32, 83, 101, 108, 102, 99, 104, 101, 99, 107,
          32, 80, 68, 70, 41, 32, 84, 106, 32, 69, 84, 10,
          101, 110, 100, 115, 116, 114, 101, 97, 109, 10, 101, 110,
          100, 111, 98, 106, 10, 53, 32, 48, 32, 111, 98, 106,
          10, 60, 60, 32, 47, 84, 121, 112, 101, 32, 47, 70,
          111, 110, 116, 32, 47, 83, 117, 98, 116, 121, 112, 101,
          32, 47, 84, 121, 112, 101, 49, 32, 47, 66, 97, 115,
          101, 70, 111, 110, 116, 32, 47, 72, 101, 108, 118, 101,
          116, 105, 99, 97, 32, 62, 62, 10, 101, 110, 100, 111,
          98, 106, 10, 120, 114, 101, 102, 10, 48, 32, 54, 10,
          48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 32, 54,
          53, 53, 51, 53, 32, 102, 32, 10, 48, 48, 48, 48,
          48, 48, 48, 48, 48, 57, 32, 48, 48, 48, 48, 48,
          32, 110, 32, 10, 48, 48, 48, 48, 48, 48, 48, 48,
          53, 56, 32, 48, 48, 48, 48, 48, 32, 110, 32, 10,
          48, 48, 48, 48, 48, 48, 48, 49, 49, 53, 32, 48,
          48, 48, 48, 48, 32, 110, 32, 10, 48, 48, 48, 48,
          48, 48, 48, 50, 52, 49, 32, 48, 48, 48, 48, 48,
          32, 110, 32, 10, 48, 48, 48, 48, 48, 48, 48, 51,
          52, 49, 32, 48, 48, 48, 48, 48, 32, 110, 32, 10,
          116, 114, 97, 105, 108, 101, 114, 10, 60, 60, 32, 47,
          83, 105, 122, 101, 32, 54, 32, 47, 82, 111, 111, 116,
          32, 49, 32, 48, 32, 82, 32, 62, 62, 10, 115, 116,
          97, 114, 116, 120, 114, 101, 102, 10, 52, 49, 49, 10,
          37, 37, 69, 79, 70, 10
        };
        char pdfp[512]; snprintf(pdfp, sizeof(pdfp), "%s/mini.pdf", tmp);
        FILE* f = fopen(pdfp, "wb");
        fwrite(g_mini_pdf, 1, sizeof(g_mini_pdf), f);
        fclose(f);
        void* h = gmb_doc_pdf_open(pdfp);
        if (!h) { printf("pdf->md: SKIP (libpdf_oxide non caricata)\n"); }
        else {
            int n = gmb_doc_pdf_page_count(h);
            char* r = gmb_doc_pdf_extract_markdown(h, 0);
            fail |= check("pdf->md", n >= 1 && r && strstr(r, "Hello Selfcheck PDF") != NULL);
            free(r);
            char* meta = gmb_doc_pdf_metadata(h);
            fail |= check("pdf->metadata", meta && strstr(meta, "pages=1") != NULL);
            free(meta);
            char* s = gmb_doc_pdf_search(h, "selfcheck");
            fail |= check("pdf->search", s && strstr(s, "p0|") != NULL);
            free(s);
            int w = 0, hgt = 0;
            uint8_t* rgba = gmb_doc_pdf_render_rgba(h, 0, 72, &w, &hgt);
            fail |= check("pdf->render_rgba", rgba != NULL && w > 0 && hgt > 0);
            free(rgba);
            gmb_doc_pdf_close(h);
        }
    }

    return fail;
}
