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
int gmbadmin_web2md(int argc, char** argv);    /* web2md <url> [-o out.md] */
int gmbadmin_batch(int argc, char** argv);     /* batch <dir> [-o outdir] [-r] */
int gmbadmin_keygen(int argc, char** argv);
int gmbadmin_verify(int argc, char** argv);    /* verify: confronta i file col release-manifest */
int gmbadmin_pack(int argc, char** argv);      /* pack <dir> [--out f.gmbpkg] */
int gmbadmin_publish(int argc, char** argv);   /* publish <f.gmbpkg> [--registry u] [--token t] */
int gmbadmin_publish_repo(int argc, char** argv); /* publish --repo <git|path> <nome@tag> */
int gmbadmin_login(int argc, char** argv, int logout);  /* login <token> | logout */
int gmbadmin_register(int argc, char** argv);  /* register <name> [--email e] [--registry u] */
int gmbadmin_admin(int argc, char** argv);     /* admin <reserve|assign|revoke|password|user ...> */
int gmbadmin_rag(int argc, char** argv);       /* rag <build|update|status|query> */
int gmbadmin_graph(int argc, char** argv);     /* graph <build|update|status> */
int gmbadmin_daemon(int argc, char** argv);    /* daemon <install|start|stop|status|logs|list|uninstall> */
/* Download on-demand (gmbadminfetch.c): componenti setup, libs publish, hf/mlx. */
int gmbadmin_setup_component(const char* name, int argc, char** argv, int quiet, int yes);
int gmbadmin_setup_rm(const char* comp, int force);
int gmbadmin_libs_prune(int argc, char** argv);
int gmbadmin_libs_publish(int argc, char** argv);
int gmbadmin_libs_index(int argc, char** argv);
int gmbadmin_hf(int argc, char** argv, int mlx);
int gmbadmin_verify_pkg(int argc, char** argv);   /* pkg verify <file.gmbpkg> */
int gmbadmin_cache_verify(int argc, char** argv); /* pkg cache verify [--lock file] */

/* Percorsi/utility condivisi. */
void gmbadmin_cache_root(char* out, size_t cap);   /* ~/.gmbasic/packages */
void gmbadmin_keyring(char* out, size_t cap);      /* ~/.gmbasic/trusted-keys.json */
int  gmbadmin_registry_url(int argc, char** argv, char* out, size_t cap);

#endif /* GMBADMIN_H */
