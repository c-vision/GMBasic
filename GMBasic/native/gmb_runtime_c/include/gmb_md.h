/* gmb_md.h — Markdown AST in C (Fase 5, parity MdSupport C#). Boxed dict/list gmb_val_t. */
#ifndef GMB_MD_H
#define GMB_MD_H

#include "gmb_runtime.h"

gmb_val_t gmb_md_parse(const char* text);
gmb_val_t gmb_md_headings(gmb_val_t doc, int level);
gmb_val_t gmb_md_links(gmb_val_t doc);
gmb_val_t gmb_md_images(gmb_val_t doc);
gmb_val_t gmb_md_tables(gmb_val_t doc);
gmb_val_t gmb_md_codeblocks(gmb_val_t doc);
char* gmb_md_tree(gmb_val_t doc);
gmb_val_t gmb_md_query(gmb_val_t doc, const char* selector);
char* gmb_md_to_html(gmb_val_t doc);
char* gmb_md_to_text(gmb_val_t doc);
char* gmb_md_validate(const char* text);
char* gmb_md_strip(const char* text);
char* gmb_md_format(gmb_val_t doc);

/* ── iterazione dei blocchi, in ORDINE di documento ──
 * Serve ai view (gmb_textview): l'AST resta l'unica fonte del parsing, il
 * renderer legge soltanto. Fuori range: "" / 0. */
int         gmb_md_block_count(gmb_val_t doc);
const char* gmb_md_block_type(gmb_val_t doc, int index);      /* "heading", "paragraph", … */
const char* gmb_md_block_text(gmb_val_t doc, int index);
int         gmb_md_block_level(gmb_val_t doc, int index);     /* titoli: 1..6 */
int         gmb_md_block_ordered(gmb_val_t doc, int index);   /* liste: 1 = numerata */
int         gmb_md_block_item_count(gmb_val_t doc, int index);
const char* gmb_md_block_item(gmb_val_t doc, int index, int item);
int         gmb_md_block_cols(gmb_val_t doc, int index);      /* tabelle: colonne */
int         gmb_md_block_row_count(gmb_val_t doc, int index);
const char* gmb_md_block_cell(gmb_val_t doc, int index, int row, int col);  /* row < 0 = header */

#endif /* GMB_MD_H */
