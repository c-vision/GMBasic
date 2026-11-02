/* GMBasic/native/gmbadmin/gmbadmin.h — CLI amministrativo in C (front unico `gmb`).
 *
 * Regola del progetto: tutto ciò che esiste nel mondo C# deve esistere in C; il C# chiama
 * queste funzioni via P/Invoke. `gmbadmin` è il backend C dei sottocomandi admin di `gmb`
 * (pkg, fixtures, setup, libs, brain, grep, convert, keygen).
 */
#ifndef GMBADMIN_H
#define GMBADMIN_H

#include <stddef.h>

int gmbadmin_pkg(int argc, char** argv);       /* argv[0]="pkg" */
int gmbadmin_fixtures(int argc, char** argv);  /* argv[0]="fixtures" */
int gmbadmin_setup(int argc, char** argv);     /* argv[0]="setup" */
int gmbadmin_libs(int argc, char** argv);      /* argv[0]="libs" */
int gmbadmin_brain(int argc, char** argv);     /* argv[0]="brain" */
int gmbadmin_grep(int argc, char** argv);      /* argv[0]="grep" */
int gmbadmin_doc(int argc, char** argv);       /* argv[0]="convert"|"convfmt" */
int gmbadmin_keygen(int argc, char** argv);    /* argv[0]="keygen" */

/* Percorsi/utility condivisi. */
void gmbadmin_cache_root(char* out, size_t cap);   /* ~/.gmbasic/packages */
void gmbadmin_keyring(char* out, size_t cap);      /* ~/.gmbasic/trusted-keys.json */
int  gmbadmin_registry_url(int argc, char** argv, char* out, size_t cap);

#endif /* GMBADMIN_H */
