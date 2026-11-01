' vault_rapid_banner.bas — banner di disambiguazione Rapid-MLX nei documenti del vault.
'
' Port BASIC di tools/vault-rapid-banner.py. Inserisce in testa (dopo il frontmatter YAML,
' o dopo il primo H1) un banner che distingue le TRE identità di Rapid-MLX:
'   1. repo REMOTO ufficiale
'   2. copia LOCALE del repo ufficiale
'   3. FORK personale
' Nota canonica: Knowledge/Rapid-MLX-Repo-Remoto-Copia-Fork-Percorsi.md
'
' IDEMPOTENTE: i file che contengono gia' il marker vengono saltati.
' DRY-RUN per default: senza --apply NON scrive nulla.
'
' Uso:
'   tools/basic/vault_rapid_banner.bas [--root <dir>] [--apply]
'   --root <dir> e' OBBLIGATORIO (nessun default: il vault dipende dalla macchina).
'
' Il criterio "cita rapid" replica la regex del .py (case-insensitive):
'   Rapid-?MLX | rapid[-_]mlx | RapidMLX | rapidmlx | rapim-mlx
IMPORT IO
IMPORT GREP

DIM g_marker AS STRING
DIM g_banner AS STRING
DIM g_lines(60000) AS STRING
DIM g_nl AS LONG

FUNCTION BaseName(p AS STRING) AS STRING
  DIM i AS LONG, c AS LONG
  c = 0
  FOR i = 1 TO LEN(p)
    IF MID$(p, i, 1) = "/" THEN c = i
  NEXT i
  IF c = 0 THEN RETURN p
  RETURN MID$(p, c + 1, LEN(p) - c)
END FUNCTION

FUNCTION RelOf(p AS STRING, root AS STRING) AS STRING
  IF LEFT$(p, LEN(root) + 1) = root + "/" THEN RETURN MID$(p, LEN(root) + 2, LEN(p) - LEN(root) - 1)
  RETURN p
END FUNCTION

' "RapidMLX" / "Rapid-MLX" / "rapid_mlx" / "rapim-mlx" (il testo e' gia' in maiuscolo)
FUNCTION CitesRapid(t AS STRING) AS INTEGER
  DIM u AS STRING, p AS LONG, c AS STRING
  u = UPPER$(t)
  p = INSTR(1, u, "RAPID")
  DO WHILE p > 0
    c = MID$(u, p + 5, 1)
    IF c = "M" AND MID$(u, p + 5, 3) = "MLX" THEN RETURN -1
    IF (c = "-" OR c = "_") AND MID$(u, p + 6, 3) = "MLX" THEN RETURN -1
    p = INSTR(p + 5, u, "RAPID")
  LOOP
  IF INSTR(1, u, "RAPIM-MLX") > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

' Testo con il banner inserito dopo la riga <idx>; "" se il marker c'e' gia'.
FUNCTION SpliceAfter(t AS STRING, idx AS LONG) AS STRING
  DIM out AS STRING, j AS LONG
  out = ""
  FOR j = 0 TO idx
    IF j > 0 THEN out = out + CHR$(10)
    out = out + g_lines(j)
  NEXT j
  out = out + CHR$(10) + CHR$(10)
  j = idx + 1
  DO WHILE j <= g_nl AND TRIM$(g_lines(j)) = ""
    j = j + 1
  LOOP
  out = out + g_banner + CHR$(10)
  DO WHILE j <= g_nl
    out = out + g_lines(j)
    IF j < g_nl THEN out = out + CHR$(10)
    j = j + 1
  LOOP
  RETURN out
END FUNCTION

FUNCTION InsertBanner(t AS STRING) AS STRING
  DIM ln$() AS STRING
  DIM i AS LONG
  IF INSTR(1, t, g_marker) > 0 THEN RETURN ""
  ln$ = SPLIT(t, CHR$(10))
  g_nl = UBOUND(ln$)
  FOR i = 0 TO g_nl
    g_lines(i) = ln$(i)
  NEXT i
  ' 1) dopo il frontmatter YAML
  IF g_nl >= 1 AND TRIM$(g_lines(0)) = "---" THEN
    FOR i = 1 TO g_nl
      IF TRIM$(g_lines(i)) = "---" THEN RETURN SpliceAfter(t, i)
    NEXT i
  END IF
  ' 2) dopo il primo H1, se e' nelle prime 3 righe
  FOR i = 0 TO 2
    IF i > g_nl THEN EXIT FOR
    IF LEFT$(g_lines(i), 2) = "# " THEN RETURN SpliceAfter(t, i)
  NEXT i
  ' 3) in testa
  RETURN g_banner + CHR$(10) + t
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM root AS STRING, argText AS STRING
  DIM i AS INTEGER, argc AS INTEGER
  DIM doApply AS INTEGER
  DIM files$() AS STRING
  DIM tot AS LONG, changed AS LONG
  DIM txt AS STRING, nw AS STRING, bname AS STRING
  DIM h AS INTEGER

  g_marker = "Rapid-MLX-Repo-Remoto-Copia-Fork-Percorsi"
  g_banner = "> [!info] Rapid-MLX — tre identità distinte (remoto / copia ufficiale / fork) · agg. 2026-09-17" + CHR$(10)
  g_banner = g_banner + "> - **Remoto ufficiale**: https://github.com/raullenchai/Rapid-MLX (tag **v0.14.2**)" + CHR$(10)
  g_banner = g_banner + "> - **Copia locale ufficiale**: `$AI_ROOT/Rapid-MLX-official` (v0.14.2) + venv `$AI_ROOT/env-rapid-official` (`rapid-mlx==0.14.2`); usata da `tools/bench4_compare.sh`" + CHR$(10)
  g_banner = g_banner + "> - **Fork personale**: `<checkout>/Rapid-MLX-NextGen` (branch `dev`, HEAD `fa7f291d`, ~0.13.1; editable in `env312`, `--version` stale 0.10.3)" + CHR$(10)
  g_banner = g_banner + "> Canonico: `Knowledge/Rapid-MLX-Repo-Remoto-Copia-Fork-Percorsi.md`" + CHR$(10)

  argc = COMMANDCOUNT()
  FOR i = 1 TO argc
    argText = COMMAND$(i)
    IF argText = "--root" AND i < argc THEN
      i = i + 1 : root = COMMAND$(i)
    ELSEIF argText = "--apply" THEN
      doApply = -1
    END IF
  NEXT i
  IF LEN(root) = 0 THEN
    PRINT "ERRORE: --root <dir> obbligatorio (cartella del vault)."
    RETURN 2
  END IF
  IF NOT DIREXISTS(root) THEN
    PRINT "ERRORE: root non trovato ("; root; ")"
    RETURN 2
  END IF

  files$ = GREP.FILES(" ", root)
  tot = 0 : changed = 0
  IF UBOUND(files$) >= 0 THEN
    FOR i = 0 TO UBOUND(files$)
      IF RIGHT$(UPPER$(files$(i)), 3) = ".MD" THEN
        bname = UPPER$(BaseName(files$(i)))
        IF INSTR(1, bname, "RAPID-MLX-REPO-REMOTO-COPIA-FORK") <> 1 THEN   ' nota canonica: esclusa
          txt = IO.READFILE(files$(i))
          IF CitesRapid(txt) THEN
            tot = tot + 1
            nw = InsertBanner(txt)
            IF LEN(nw) > 0 THEN
              changed = changed + 1
              IF doApply THEN
                h = IO.FCREATE(files$(i))
                IF h <> 0 THEN
                  IO.FWRITE(h, nw)
                  IO.FCLOSE(h)
                END IF
                PRINT "  APPLY "; RelOf(files$(i), root)
              ELSE
                PRINT "  DRY   "; RelOf(files$(i), root)
              END IF
            END IF
          END IF
        END IF
      END IF
    NEXT i
  END IF
  PRINT ""
  IF doApply THEN
    PRINT changed; " file da modificare su "; tot; " che citano rapid (APPLICATO)"
  ELSE
    PRINT changed; " file da modificare su "; tot; " che citano rapid (DRY-RUN)"
  END IF
  RETURN 0
END FUNCTION
