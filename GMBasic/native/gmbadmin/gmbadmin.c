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
    printf("gmbadmin — CLI amministrativo di GMBASIC (C). Il front per l'utente è `gmb`:\n"
           "quando il primo argomento è un comando admin, `gmb` esegue `gmbadmin` con lo stesso argv.\n"
           "\n"
           "  pkg  init|add|remove|bump|list|info|install|uninstall|restore|update|rollback\n"
           "       trust|verify|cache <list|verify|gc|dedup|clean>\n"
           "  pack <dir> [--out f.gmbpkg] [--dry-run] [--key-file priv|--key segreto]\n"
           "  publish <f.gmbpkg> | publish --repo <git|path> <nome@tag> [--registry u] [--token t]\n"
           "  login <token> | logout | register <nome> | admin reserve|assign|revoke|password|user …\n"
           "  keygen [dir] [--pem]\n"
           "  fixtures list|ensure|pull <nome>\n"
           "  libs status|ensure|check|prune|index|publish <gruppo> <rid>\n"
           "  setup status | setup <LIBS|OCR|TORCH|BROWSER|OBSCURA|ALL> [--force] [--yes] | setup rm\n"
           "  brain list|info|use|detach|forget|resolve|install|uninstall\n"
           "        stats|verify|create|register|buildvault|optimize|rename|backup|export|sign\n"
           "        dedupe|extract|merge|convert|diff|doctor\n"
           "  rag build|update|status|query · graph build|update|status|watch|export-obsidian\n"
           "  daemon install|start|stop|restart|status|logs|list|uninstall   (macOS/launchd)\n"
           "  hf <repo> [--file F] [--dest D] · mlx <repo> [--dest D]\n"
           "  grep <pattern> [path] [-i] [--include G] [--depth N]\n"
           "  convert <file> [-o out.(md|html|txt)] · convfmt · web2md <url> · batch <dir> [-r]\n"
           "\n"
           "Dettaglio di ogni comando: `gmb brain help`, `gmb libs`, `gmb setup`, `gmb pkg help`.\n"
           "Download: `gmb libs ensure` (GMBasicModules), `gmb brain install <nome>` (GMBasicBrains),\n"
           "`gmb fixtures ensure` (fixtures-v1), `gmb setup <comp>` (ML/Chromium/Obscura).\n");
}

/* gmbadminsys.c: PRIMO USO (librerie di tutti i gruppi + toolchain zig). */
int gmbadmin_first_use(int argc, char** argv);

int main(int argc, char** argv)
{
    if (argc < 2) { usage(); return 2; }
    const char* cmd = argv[1];
    if (!strcmp(cmd, "-h") || !strcmp(cmd, "--help") || !strcmp(cmd, "-help") || !strcmp(cmd, "help")) { usage(); return 0; }

    if (!strcmp(cmd, "first-use") || !strcmp(cmd, "firstuse")) return gmbadmin_first_use(argc, argv);
    if (!strcmp(cmd, "pkg"))      return gmbadmin_pkg(argc, argv);
    if (!strcmp(cmd, "fixtures")) return gmbadmin_fixtures(argc, argv);
    if (!strcmp(cmd, "setup"))    return gmbadmin_setup(argc, argv);
    if (!strcmp(cmd, "libs"))     return gmbadmin_libs(argc, argv);
    if (!strcmp(cmd, "brain"))    return gmbadmin_brain(argc, argv);
    if (!strcmp(cmd, "grep") || !strcmp(cmd, "rg")) return gmbadmin_grep(argc, argv);
    if (!strcmp(cmd, "convert") || !strcmp(cmd, "convfmt")) return gmbadmin_doc(argc, argv);
    if (!strcmp(cmd, "web2md"))   return gmbadmin_web2md(argc, argv);
    if (!strcmp(cmd, "batch"))    return gmbadmin_batch(argc, argv);
    if (!strcmp(cmd, "keygen"))   return gmbadmin_keygen(argc, argv);
    if (!strcmp(cmd, "verify"))   return gmbadmin_verify(argc, argv);    /* `gmb doctor --verify` */
    if (!strcmp(cmd, "pack"))     return gmbadmin_pack(argc, argv);
    if (!strcmp(cmd, "publish")) {
        for (int i = 2; i < argc; i++)
            if (!strcmp(argv[i], "--repo") || !strncmp(argv[i], "--repo=", 7))
                return gmbadmin_publish_repo(argc, argv);
        return gmbadmin_publish(argc, argv);
    }
    if (!strcmp(cmd, "login"))    return gmbadmin_login(argc, argv, 0);
    if (!strcmp(cmd, "logout"))   return gmbadmin_login(argc, argv, 1);
    if (!strcmp(cmd, "register")) return gmbadmin_register(argc, argv);
    if (!strcmp(cmd, "admin"))    return gmbadmin_admin(argc, argv);
    if (!strcmp(cmd, "rag"))      return gmbadmin_rag(argc, argv);
    if (!strcmp(cmd, "graph"))    return gmbadmin_graph(argc, argv);
    if (!strcmp(cmd, "daemon"))   return gmbadmin_daemon(argc, argv);
    if (!strcmp(cmd, "hf"))       return gmbadmin_hf(argc, argv, 0);
    if (!strcmp(cmd, "mlx"))      return gmbadmin_hf(argc, argv, 1);

    fprintf(stderr, "?gmbadmin: comando sconosciuto '%s'\n", cmd);
    usage();
    return 2;
}
