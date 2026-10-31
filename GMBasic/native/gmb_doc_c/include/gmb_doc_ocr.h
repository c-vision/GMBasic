/* gmb_doc_ocr.h — OCR per documenti.
 * Livello 1: Apple Vision (macOS) — libllm_vision_apple via dlopen, zero modelli.
 * Livello 2: Tesseract (cross-OS) — libtesseract + libleptonica + tessdata/eng dal
 * bundle LIBS (Resources/libs/<os>/), fornito da `gmb setup LIBS`. Mai link statico.
 * Nessun modello LLM è dato per scontato sull'host. Se nessun motore è disponibile
 * le funzioni ritornano NULL e il chiamante emette '> [OCR REQUIRED — page N]'.
 */
#ifndef GMB_DOC_OCR_H
#define GMB_DOC_OCR_H

#include <stdint.h>

/* Tesseract (cross-OS, Livello 2): OCR su pixel RGBA (row-major, 4 byte/px).
 * Il modello eng è in Resources/libs/<os>/tessdata/eng.traineddata (bundle LIBS).
 * Ritorna stringa heap-allocata (free()) o NULL. */
char* gmb_doc_ocr_rgba(const uint8_t* rgba, int w, int h);

/* Apple Vision (macOS, Livello 1): OCR su file PNG. NULL se non disponibile. */
char* gmb_doc_ocr_image(const char* png_path);

#endif
