' Vista testuale virtuale — una LISTBOX che mostra un TESTO invece di voci fisse.
' Tutto passa dallo STYLE (dati, nessun comando nuovo): `mode=markdown|diff|plain`,
' `virtual=1` per una lista grande. Il testo si assegna con `CONTROL SET nome,"text"`.
' Le righe si leggono con `listcount` e con la selezione: qui si verifica che il
' documento venga IMPAGINATO (il Markdown non mostra il markup, il diff distingue
' le righe, la lista grande supera il limite delle voci).
' EXPECT: md-lines=yes|md-head=yes|md-quote=yes|md-plain=yes|diff-lines=yes|diff-last=yes|big-lines=yes|plain-lines=yes
IMPORT CONTROLS

DIM md AS STRING
DIM df AS STRING
DIM big AS STRING
DIM txt AS STRING
DIM buf AS STRING
DIM n AS DOUBLE
DIM i AS INTEGER

SUB Main()
    ' --- MARKDOWN: titolo, citazione, lista, codice.
    md = "# Titolo" + CHR$(10) + CHR$(10) _
       + "> citazione utile" + CHR$(10) + CHR$(10) _
       + "- primo" + CHR$(10) + "- secondo" + CHR$(10) + CHR$(10) _
       + "```" + CHR$(10) + "PRINT 1" + CHR$(10) + "```"

    LISTBOX 20, 6, 60, 14, "" NAME "doc" STYLE "mode=markdown"
    CONTROL SET "doc", "text", md

    CONTROL GET "doc", "listcount" INTO n
    IF n >= 8 THEN PRINT "md-lines=yes" ELSE PRINT "md-lines=no n="; n

    ' la prima riga è il titolo SENZA '#' (il markup non si mostra)
    CONTROL SET "doc", "index", 0
    CONTROL GETS "doc", "text" INTO txt
    IF LEFT$(txt, 1) <> "#" AND INSTR(txt, "Titolo") > 0 THEN PRINT "md-head=yes" ELSE PRINT "md-head=no ["; txt; "]"

    ' la citazione porta il rientro `│`
    CONTROL SET "doc", "index", 2
    CONTROL GETS "doc", "text" INTO txt
    IF INSTR(txt, "citazione") > 0 AND INSTR(txt, CHR$(226)) > 0 THEN PRINT "md-quote=yes" ELSE PRINT "md-quote=no ["; txt; "]"

    ' nessun '#' o '>' grezzo fuori dal blocco codice (riga 2..3 della lista)
    txt = ""
    FOR i = 1 TO 2
        CONTROL SET "doc", "index", i
        CONTROL GETS "doc", "text" INTO buf
        txt = txt + buf
    NEXT i
    IF INSTR(txt, "#") = 0 AND INSTR(txt, ">") = 0 THEN PRINT "md-plain=yes" ELSE PRINT "md-plain=no ["; txt; "]"

    ' --- DIFF: un diff unificato, 5 righe
    df = "--- a/x" + CHR$(10) + "+++ b/x" + CHR$(10) + "@@ -1 +1 @@" + CHR$(10) + "-vecchia" + CHR$(10) + "+nuova"
    LISTBOX 20, 24, 60, 6, "" NAME "dif" STYLE "mode=diff"
    CONTROL SET "dif", "text", df
    CONTROL GET "dif", "listcount" INTO n
    IF n = 5 THEN PRINT "diff-lines=yes" ELSE PRINT "diff-lines=no n="; n
    CONTROL SET "dif", "index", 4
    CONTROL GETS "dif", "text" INTO txt
    IF txt = "+nuova" THEN PRINT "diff-last=yes" ELSE PRINT "diff-last=no ["; txt; "]"

    ' --- LISTA GRANDE: 4000 righe, molto oltre le 32 voci di `items`
    big = ""
    FOR i = 1 TO 4000
        big = big + "riga " + STR$(i) + CHR$(10)
    NEXT i
    LISTBOX 100, 6, 40, 10, "" NAME "log" STYLE "virtual=1"
    CONTROL SET "log", "text", big
    CONTROL GET "log", "listcount" INTO n
    IF n = 4000 THEN PRINT "big-lines=yes" ELSE PRINT "big-lines=no n="; n

    ' --- PLAIN con STYLE "mode=plain": un '\n' finale non aggiunge una riga
    LISTBOX 100, 24, 40, 6, "" NAME "pl" STYLE "mode=plain"
    buf = "alfa" + CHR$(10) + "beta" + CHR$(10)
    CONTROL SET "pl", "text", buf
    CONTROL GET "pl", "listcount" INTO n
    IF n = 2 THEN PRINT "plain-lines=yes" ELSE PRINT "plain-lines=no n="; n
END SUB