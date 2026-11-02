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
           "  convert <file> [-o out.(md|html|txt)] · convfmt\n"
           "  keygen [dir] [--pem]\n"
           "  pack <dir> [--out f.gmbpkg] [--key-file priv|--key segreto]\n"
           "  publish <f.gmbpkg> [--registry u] [--token t] · login <token> · logout\n"
           "  register <nome> [--email e] · admin <reserve|assign|revoke|password|user …>\n"
           "  rag <build|update|status|query> …  ·  graph <build|update|status> <wiki>\n"
           "  daemon <install|start|stop|restart|status|logs|list|uninstall> …\n"
           "  setup <TORCH|BROWSER|OBSCURA|OCR|LIBS|ALL> [--force] [--yes] [--torch-local dir]\n"
           "  hf <repo> [--file F] [--dest D] · mlx <repo> [--dest D]\n"
           "  libs publish <support|engine|core> <rid> [--yes]\n"
           "\n"
           "Solo macOS per `daemon` (launchd). Residuo F9c: graph export-obsidian/watch,\n"
           "web2md, batch (rispondono con un errore esplicito, nessuna dipendenza C#).\n");
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
    if (!strcmp(cmd, "register")) return gmbadmin_register(argc, argv);
    if (!strcmp(cmd, "admin"))    return gmbadmin_admin(argc, argv);
    if (!strcmp(cmd, "rag"))      return gmbadmin_rag(argc, argv);
    if (!strcmp(cmd, "graph"))    return gmbadmin_graph(argc, argv);
    if (!strcmp(cmd, "daemon"))   return gmbadmin_daemon(argc, argv);
    if (!strcmp(cmd, "hf"))       return gmbadmin_hf(argc, argv, 0);
    if (!strcmp(cmd, "mlx"))      return gmbadmin_hf(argc, argv, 1);

    static const char* pending[] = {
        "web2md", "batch", NULL
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
