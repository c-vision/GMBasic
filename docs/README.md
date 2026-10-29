# docs/ — documentazione del monorepo

**Unica cartella di documentazione, alla radice del monorepo.** Dal 2026-10-29 non esistono
più cartelle `docs/` annidate: quelle che c'erano in `GMBasic/`, `GMBasicSharp/common/ClipperLib/`
e `shared/` sono state consolidate qui (vedi "Provenienza").

Lo strumento (script, gate, generatore) **non** vive qui: sta in [`tools/`](../tools/README.md).
In `docs/` restano solo documenti e artefatti di report generati.

## Struttura

| Percorso | Contenuto | Prodotto da |
|---|---|---|
| [`Sync-GMBasic-Monorepo.md`](Sync-GMBasic-Monorepo.md) | log del sync col progetto originale GMBasic, per data; base commit e inventario delle modifiche | a mano (log) |
| [`gmb_env_audit.tsv`](gmb_env_audit.tsv) | audit delle variabili d'ambiente `GMB_*` (categoria + occorrenze + file:riga). Riferito da AGENTS.md §"VIETATE LE VARIABILI D'AMBIENTE" | `tools/basic/audit_gmb_env.bas` |
| [`comparison/`](comparison/) | confronto superficie canonica ↔ backend: `audit-backends.md`, `audit-stdlib.md`, `recon-stdlib.{md,json}`, `unimplemented-canonical.md`, CSV di dispatch TokenKind, TODO del refactor TokenKind | `tools/audit` (`--all`), `tools/basic/gen_command_desc.bas`, `recon_stdlib.py` |
| [`comparison/manifests/`](comparison/manifests/) | un manifest JSON per backend (`IL`, `PCodeVM`, `VMC`, `C99Native`) con copertura e lista dei gap | `tools/audit` (`--all`) |
| [`comparison/command_desc.tsv`](comparison/command_desc.tsv) | indice nome → sintassi/descrizione usato per generare i blocchi di `COMMANDS.TXT` | `tools/basic/gen_command_desc.bas` |
| [`implemented/audit/COMMANDS.txt`](implemented/audit/COMMANDS.txt) | **snapshot storico** del catalogo comandi. Il catalogo vivo è [`COMMANDS.TXT`](../COMMANDS.TXT) alla radice (più recente e completo) | — |
| [`ClipperLib/clipper53.md`](ClipperLib/clipper53.md) | riferimento tecnico dell'emulazione Clipper 5.3 (`ClipperLib.Clipper`: SAY/GET/READ/BOX/INKEY/ACHOICE/MEMOEDIT/PICTURE, SET …) | a mano |
| [`ClipperLib/controls.md`](ClipperLib/controls.md) | documentazione di `ClipperLib.Controls` (78+ controlli TUI) e `ClipperLib.TBrowse` | a mano |

## Provenienza (consolidamento 2026-10-29)

| Prima | Ora |
|---|---|
| `shared/docs/comparison/**` | `docs/comparison/**` |
| `shared/docs/implemented/audit/COMMANDS.txt` | `docs/implemented/audit/COMMANDS.txt` |
| `GMBasic/docs/gmb_env_audit.tsv` | `docs/gmb_env_audit.tsv` (sostituisce la copia **stale**, con path pre-split `GMBasic/`…`GMBasicSharp/`) |
| `GMBasicSharp/common/ClipperLib/docs/{clipper53,controls}.md` | `docs/ClipperLib/{clipper53,controls}.md` |
| `docs/Sync-GMBasic-Monorepo.md` | invariato (era già alla radice) |

Le cartelle `shared/docs/`, `GMBasic/docs/` e `ClipperLib/docs/` **non esistono più**.
Backup del pre-consolidamento: `/tmp/GMBasicMonoRepo-backup-<data>/`.

## Cosa resta da decidere

- `docs/implemented/audit/COMMANDS.txt` è una **copia superata** di `COMMANDS.TXT` (stesso numero
  di righe, descrizioni più vecchie). Va **cancellata** appena confermato: nessuno la legge.
- `tools/basic/audit_gmb_env.bas` ricava la radice da `IO.PARENTDIR(IO.CWD())` (presuppone di girare
  da `GMBasic/tools/`): nel monorepo finisce sul root solo grazie al controllo sul fratello
  `GMBasicSharp/`. Da rendere esplicito (radice del monorepo) quando si tocca quel tool.
