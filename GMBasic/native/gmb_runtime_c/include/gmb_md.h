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

#endif /* GMB_MD_H */
