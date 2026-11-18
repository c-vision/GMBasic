' MiniTest CONSTRUCTS — EMBEDFILE: incorporare un file a COMPILE-TIME (equivalente di
' `@embedFile` di Zig). Il contenuto diventa un letterale stringa: nessun I/O a runtime,
' il programma non ha bisogno del file accanto.
'
' Risorsa: shared/samples/Fixtures/embed_sample.txt = "embed ok\nsecond line\n" (21 byte /
' 21 caratteri). Il percorso è relativo alla cartella del programma (quella del .bas),
' quindi da shared/minitests/constructs/ si sale di due livelli.
'
' Verifica: contenuto integrale (LEN), accesso per posizione (LEFT$/MID$/INSTR), e
' normalizzazione (UCASE$) — cioè che sia davvero una stringa come tutte le altre.
' EXPECT: len= 21 |left=embed ok|mid=second|idx= 10 |up= 21 |last=line|eof=-1
EMBEDFILE "../../samples/Fixtures/embed_sample.txt" AS t$

SUB Main()
    PRINT "len="; LEN(t$);
    PRINT "|left="; LEFT$(t$, 8);
    PRINT "|mid="; MID$(t$, 10, 6);
    PRINT "|idx="; INSTR(t$, "second");
    PRINT "|up="; LEN(UCASE$(t$));
    PRINT "|last="; MID$(t$, LEN(t$) - 4, 4);
    ' l'ultimo carattere è il newline: RIGHT$(t$,1) deve essere CHR$(10)
    PRINT "|eof="; (RIGHT$(t$, 1) = CHR$(10))
END SUB
