/* gmbadmin.c — CLI amministrativo in C (backend dei sottocomandi admin di `gmb`).
 *
 * Un solo punto per: pkg, fixtures, setup, libs, brain, grep, convert/convfmt, keygen.
 * Tutto su runtime C: nessuna dipendenza dal mondo C# per queste funzioni.
 */
#include "gmbadmin.h"

#include <stdio.h>
#include <string.h>

static void usage(void)
{
    printf("gmbadmin — comandi amministrativi di GMBASIC (C)\n"
           "\n"
           "uso: gmbadmin <comando> [opzioni]\n"
           "\n"
           "  pkg <init|add|remove|bump|list|info|install|uninstall|restore|update|trust|cache|rollback>\n"
           "  fixtures <list|ensure|pull <nome>>\n"
           "  setup <status>\n"
           "  libs <status>\n"
           "  brain <list|info <nome>|use <nome>|detach <nome>>\n"
           "  grep <pattern> [path] [-i] [-n] [--include G] [--depth N]\n"
           "  convert <file> [-o out.(md|html|txt)]\n"
           "  convfmt\n"
           "  keygen\n"
           "  pack <dir> [--out f.gmbpkg] · publish <f.gmbpkg> [--registry u] [--token t]\n"
           "  login <token> [--registry u] · logout\n"
           "\n"
           "I comandi non ancora portati in C (publish, login/admin, pack, daemon, rag, graph,\n"
           "hf/mlx, setup TORCH|BROWSER|OCR) rispondono con un errore esplicito: porting F9c.\n");
}

int main(int argc, char** argv)
{
    if (argc < 2) { usage(); return 2; }
    const char* cmd = argv[1];
    if (!strcmp(cmd, "-h") || !strcmp(cmd, "--help") || !strcmp(cmd, "help")) { usage(); return 0; }

    if (!strcmp(cmd, "pkg"))      return gmbadmin_pkg(argc, argv);
    if (!strcmp(cmd, "fixtures")) return gmbadmin_fixtures(argc, argv);
    if (!strcmp(cmd, "setup"))    return gmbadmin_setup(argc, argv);
    if (!strcmp(cmd, "libs"))     return gmbadmin_libs(argc, argv);
    if (!strcmp(cmd, "brain"))    return gmbadmin_brain(argc, argv);
    if (!strcmp(cmd, "grep") || !strcmp(cmd, "rg")) return gmbadmin_grep(argc, argv);
    if (!strcmp(cmd, "convert") || !strcmp(cmd, "convfmt")) return gmbadmin_doc(argc, argv);
    if (!strcmp(cmd, "keygen"))   return gmbadmin_keygen(argc, argv);
    if (!strcmp(cmd, "pack"))     return gmbadmin_pack(argc, argv);
    if (!strcmp(cmd, "publish"))  return gmbadmin_publish(argc, argv);
    if (!strcmp(cmd, "login"))    return gmbadmin_login(argc, argv, 0);
    if (!strcmp(cmd, "logout"))   return gmbadmin_login(argc, argv, 1);

    static const char* pending[] = {
        "register", "admin", "daemon",
        "rag", "graph", "hf", "mlx", "web2md", "batch", NULL
    };
    for (int i = 0; pending[i]; i++) {
        if (!strcmp(cmd, pending[i])) {
            fprintf(stderr, "?gmb %s: non ancora in C (porting F9c tracciato in TODO §6);\n"
                            "  nessuna dipendenza C# verra' aggiunta: il comando sara' portato qui.\n", cmd);
            return 2;
        }
    }
    fprintf(stderr, "?gmbadmin: comando sconosciuto '%s'\n", cmd);
    usage();
    return 2;
}
