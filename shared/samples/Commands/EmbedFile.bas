' ============================================================================
' EmbedFile.bas — EMBEDFILE: incorporare risorse a COMPILE-TIME.
'
' Caso reale: un'applicazione che manda email di notifica tiene il TESTO del
' messaggio dentro l'eseguibile (template + dati). Con EMBEDFILE il programma
' consegnato è un file solo: nessun `templates/` da copiare accanto al binario,
' nessuna apertura file a runtime, nessun path da sbagliare.
'
' Sintassi:  EMBEDFILE "<percorso>" AS nome$
'   - il percorso è relativo alla cartella del programma (poi, in fallback, alla CWD);
'   - il contenuto diventa un letterale stringa: LEN/LEFT$/MID$/INSTR/SPLIT/UCASE$
'     funzionano come su qualunque stringa;
'   - il file NON serve a runtime.
'
' Contratto: EMBEDFILE incorpora TESTO UTF-8. Un payload binario (byte NUL o
' sequenze non valide) viene RIFIUTATO in compilazione — non esiste un modo
' silenzioso di incorporarlo troncato.
' ============================================================================
IMPORT SYSTEM
IMPORT STRINGS
EMBEDFILE "../Fixtures/embed_template.txt" AS tpl$
EMBEDFILE "../Fixtures/embed_sample.txt" AS note$

' Sostituisce ogni occorrenza di `what` con `with` (niente REGEX: stringhe pure).
FUNCTION Fill(s AS STRING, what AS STRING, with AS STRING) AS STRING
    DIM out AS STRING, i AS LONG
    out = ""
    DO WHILE INSTR(s, what) > 0
        i = INSTR(s, what)
        out = out + LEFT$(s, i - 1) + with
        s = MID$(s, i + LEN(what))
    LOOP
    RETURN out + s
END FUNCTION

' Le righe di un testo (senza il newline finale vuoto).
FUNCTION Lines(s AS STRING) AS STRING
    DIM rows$() AS STRING, i AS LONG, acc AS STRING
    rows$ = SPLIT(s, CHR$(10))
    acc = ""
    FOR i = 0 TO UBOUND(rows$)
        IF LEN(rows$(i)) > 0 THEN
            IF LEN(acc) > 0 THEN acc = acc + " / "
            acc = acc + rows$(i)
        END IF
    NEXT i
    RETURN acc
END FUNCTION

' Numero di righe di un testo.
FUNCTION RowCount(s AS STRING) AS LONG
    DIM rows$() AS STRING
    rows$ = SPLIT(s, CHR$(10))
    RETURN UBOUND(rows$) + 1
END FUNCTION

SUB Main()
    DIM mail AS STRING
    mail = Fill(tpl$, "{NOME}", "Anna")
    mail = Fill(mail, "{ID}", "1024")
    mail = Fill(mail, "{DATA}", "2026-11-20")

    PRINT "--- template incorporato ---"
    PRINT mail
    PRINT "--- fine ---"
    PRINT "byte del template: "; LEN(tpl$)
    PRINT "righe del template: "; RowCount(tpl$)
    PRINT "segnaposto rimasti: "; INSTR(mail, "{")
    PRINT "nota incorporata: "; Lines(note$)
    PRINT "nota contiene 'second': "; (INSTR(note$, "second") > 0)
END SUB
