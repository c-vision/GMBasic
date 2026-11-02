/* gmb_cli_usage.c — testo di aiuto del CLI `gmb` (estratto da gmb_cli.c per la soglia
 * 1200 righe, AGENTS §0quater). */
#include "gmb_cli_internal.h"

#include <stdio.h>

void gmb_cli_usage(void) {
    fprintf(stderr,
        "uso: gmb check|build|run --project <dir> [--entry <file.bas>] [--json] [--strict]\n"
        "     gmb build ... [--out <exe>]\n"
        "     gmb dump ast|chunk|c|disasm <file.bas> [--out <file>]   dump per la REPL (F1)\n"
        "     gmb eval \"<istruzioni>\"                          compila ed esegue uno snippet\n"
        "     gmb minitests [--root <dir>] [--filter <substr>] [--all] [--json]\n"
        "                   [--driver <exe>] [--cc <cc>] [--baseline <json>]\n"
        "  check  valida il progetto: entry, NAMESPACE dei moduli, IMPORT risolti\n"
        "  build  emette il C e linka l'eseguibile in <progetto>/bin\n"
        "         LIBRARY \"nome\": linka <progetto>/bin/nome.dylib (nessun main), come .gmblib\n"
        "  run    build + esecuzione (CWD = cartella del progetto)\n"
        "  --engine c|csharp   sceglie il compilatore (default c). Con csharp lo STESSO argv\n"
        "                      va a `gmbcs` (stesso contratto: --project/--entry/--out/...).\n"
        "  --csharp-cli <path> percorso di gmbcs (default: PATH o checkout di sviluppo)\n"
        "  --gpm auto|always|linear   schedulatore GPM nel C emesso (default auto)\n"
        "         always = forza lo scheduler; linear = percorso diretto se lineare (mai crash)\n"
        "  minitests  TUTTI i MiniTests: driver → C → `cc -fsyntax-only` (nessun link)\n"
        "             con --baseline <json> di un run precedente: rosso solo per le regressioni\n"
        "  exit: 0 ok · 2 compilazione/progetto (minitests: almeno un FAIL) · 3 parse ·\n"
        "        4 emissione/cc · 5 link · 6 runtime · 1 setup (minitests)\n"
        "\n"
        "Comandi amministrativi (tool C `gmbadmin`, stesso argv; `gmbadmin --help` per l'elenco):\n"
        "  gmb pkg … · pack · publish · login/logout/register/admin · keygen\n"
        "  gmb fixtures … · libs … · setup … · brain … (toolchain completo) · daemon …\n"
        "  gmb rag … · graph … · grep · convert/convfmt · hf · mlx\n");
}
