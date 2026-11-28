' ============================================================================
' TextViewer — vista testuale virtuale in una LISTBOX (Markdown, diff, log).
' ============================================================================
' Tutto passa da chiavi STYLE e da CONTROL SET/GET (nessun comando nuovo):
'
'     LISTBOX r, c, w, h, "" NAME "doc" STYLE "mode=markdown"
'     CONTROL SET "doc", "text", testo$
'     CONTROL GET "doc", "listcount" INTO n
'     CONTROL GETS "doc", "text" INTO riga$        ' riga DELLA VISTA
'
' Il sample simula un piccolo visualizzatore di file versionati: a sinistra il
' README in Markdown (titolo, citazione, lista, tabella, codice), a destra il diff
' dell'ultima modifica, in basso i log virtualizzati. Nessun disegno manuale: la
' LISTBOX impagina da sola alla larghezza che ha (anche quando cambia).
'
' Esecuzione:  tools/sh/gmb_c99native_run.sh ../shared/samples/Controls/TextViewer.bas
' ============================================================================
IMPORT CONTROLS


DIM readme AS STRING
DIM patch  AS STRING
DIM logbuf AS STRING
DIM tot    AS DOUBLE
DIM n      AS DOUBLE
DIM i      AS DOUBLE
DIM riga   AS STRING

SUB Main()
    SCREEN 0

    ' --- 1) README in Markdown ---------------------------------------------
    readme = "# Progetto Aurora" + CHR$(10)  + CHR$(10) _
           + "> Documentazione generata dalla build." + CHR$(10)  + CHR$(10) _
           + "## Requisiti" + CHR$(10) + "- GMBasic 1.0" + CHR$(10) + "- macOS o Linux" + CHR$(10)  + CHR$(10) _
           + "| modulo | stato |" + CHR$(10) + "|--------|-------|"  + CHR$(10) _
           + "| core   | stabile |" + CHR$(10) + "| ui     | beta |" + CHR$(10)  + CHR$(10) _
           + "```" + CHR$(10) + "gmb run main.bas" + CHR$(10) + "```"

    LISTBOX 1, 2, 58, 20, "" NAME "readme" STYLE "mode=markdown"
    CONTROL SET "readme", "text", readme
    CONTROL GET "readme", "listcount" INTO tot

    ' --- 2) diff dell'ultima modifica --------------------------------------
    patch = "--- a/core.c" + CHR$(10) + "+++ b/core.c"  + CHR$(10) _
          + "@@ -10,3 +10,3 @@" + CHR$(10) + " int cache = 0;"  + CHR$(10) _
          + "-int limit = 16;" + CHR$(10) + "+int limit = 64;" + CHR$(10) + " int hits;"  + CHR$(10)

    LISTBOX 61, 2, 50, 20, "" NAME "patch" STYLE "mode=diff"
    CONTROL SET "patch", "text", patch
    CONTROL GET "patch", "listcount" INTO n

    ' --- 3) log virtualizzato (liste grandi) -------------------------------
    FOR i = 1 TO 500
        logbuf = logbuf + "linea " + STR$(i) + " ok"  + CHR$(10)
    NEXT i

    LISTBOX 1, 24, 110, 8, "" NAME "log" STYLE "virtual=1"
    CONTROL SET "log", "text", logbuf
    CONTROL GET "log", "listcount" INTO i

    ' --- 4) letture: la vista mostra testo impaginato, non la sorgente -------
    CONTROL SET "readme", "index", 0
    CONTROL GETS "readme", "text" INTO riga
    PRINT "titolo della vista: ["; TRIM$(riga); "]"

    ' ultima riga di log come «indietro di una riga», con il selettore per nome
    CONTROL SET "log", "index", i - 1
    CONTROL GETS "log", "text" INTO riga
    PRINT "ultima riga log: ["; TRIM$(riga); "]"

    ' riga del diff con il marcatore: si vede quale lato è cambiato
    CONTROL SET "patch", "index", 4
    CONTROL GETS "patch", "text" INTO riga
    PRINT "diff riga 4: ["; TRIM$(riga); "]"

    PRINT "readme righe ="; tot; " patch righe ="; n; " log righe ="; i
    PRINT "=== fine ==="
END SUB