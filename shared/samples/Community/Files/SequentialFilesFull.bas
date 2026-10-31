' ==============================================================================
' Sample COMPLETO — FILE SEQUENZIALI stile Clipper (4 backend)
' ==============================================================================
' Copre TUTTI i comandi/funzioni della catena sequenziale:
'   USING F = OPEN(path, modo) ... END USING   (chiusura automatica)
'   OPEN path FOR modo AS #n ... CLOSE #n      (forma classica)
'   PRINT #n, ...     scrittura raw (testo come appare a video)
'   WRITE #n, ...     scrittura CSV canonica (stringhe tra " ", booleani TRUE/FALSE)
'   INPUT #n, v1, v2, ...   lettura a campi CSV con conversione per tipo
'   LINE INPUT #n, s$  lettura della RIGA INTERA (nessuno split)
'   EOF(n)            -1 a fine file, 0 altrimenti
'   LOF(n)            lunghezza del file
'   SEEK(n, pos)      posizionamento (1-based)
' ==============================================================================

SUB Main()
    DIM path AS STRING = "work_seqfull_sample.txt"

    ' --- 1. Scrittura con USING + WRITE# (CSV canonico) ---
    PRINT "1) WRITE# con USING (OUTPUT)"
    USING F = OPEN(path, OUTPUT)
        WRITE #F, 101, "Mario, Rossi", 123.45, TRUE
        WRITE #F, 102, "Luigi Verdi", -50, FALSE
    END USING

    ' --- 2. Append con USING + PRINT# (raw) ---
    PRINT "2) APPEND con PRINT#"
    USING F = OPEN(path, APPEND)
        PRINT #F, "riga raw in append"
    END USING

    ' --- 3. Lettura a campi con INPUT# (tipi convertiti) ---
    PRINT "3) INPUT# multi-campo"
    USING F = OPEN(path, INPUT)
        DIM id AS INTEGER
        DIM nome AS STRING
        DIM saldo AS DOUBLE
        DIM attivo AS INTEGER
        INPUT #F, id, nome, saldo, attivo
        PRINT "   id="; id; " nome="; nome; " saldo="; saldo; " attivo="; attivo
        INPUT #F, id, nome, saldo, attivo
        PRINT "   id="; id; " nome="; nome; " saldo="; saldo; " attivo="; attivo

        ' --- 4. LINE INPUT# (riga intera) + EOF ---
        PRINT "4) LINE INPUT# + EOF"
        DIM riga AS STRING
        LINE INPUT #F, riga
        PRINT "   riga="; riga
        PRINT "   eof="; EOF(F)
        PRINT "   lof="; LOF(F)
    END USING

    ' --- 5. Forma classica OPEN ... AS #n + CLOSE #n ---
    PRINT "5) OPEN classico + CLOSE"
    OPEN path FOR INPUT AS #1
    DIM riga2 AS STRING
    LINE INPUT #1, riga2
    PRINT "   prima="; riga2
    CLOSE #1

    ' --- 6. Pulizia ---
    KILL path
    PRINT "SequentialFilesFull: completato."
END SUB
