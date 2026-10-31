' FILE: Databases/BareCommandFallback.bas
' =================================================================================================
'   NAMESPACE BARE COMMAND / FUNCTION FALLBACK
' =================================================================================================
'
' Regola: un comando o una funzione possono essere usati SENZA il prefisso del namespace
' SOLO se il namespace e' attivo (IMPORT esplicito o di default/settings). L'IMPORT e'
' un limite invalicabile: senza IMPORT il bare comando fallisce.
'
' Il fallback prova i namespace attivi e vince il PRIMO parser che consuma l'istruzione
' PER INTERO (backtracking sugli altri). Es: con DBF+RDD attivi, la clausola 'VIA'
' (specifica di RDD) fa vincere RDD perche' DBF si fermerebbe a 'VIA'.
'
' Funzioni: quelle dei namespace di default (SYSTEM, CONVERSIONS, MATH, ...) sono usabili
' bare (es. IIF); le altre restano NAMESPACE.FUNZIONE(...).
'
' NOTA: i comandi gia' globali (SET, CLOSE, GET, ...) hanno priorita' sul fallback:
' 'SET DEFAULT' non fallisce su RDD, serve 'RDD SET DEFAULT'.

IMPORT RDD
IMPORT CONSOLE

SUB Main()
    PRINT "=== Bare Command Fallback ==="
    PRINT

    ' Apre un'area ARRAY in memoria (RDD attivo via IMPORT RDD).
    USE ":memory:" VIA "ARRAY" ALIAS T1
    PRINT "1. USE bare (VIA -> RDD) -> OK"

    ' REPLACE bare con scope ALL
    REPLACE A WITH 10 ALL
    PRINT "2. REPLACE bare ALL -> OK"

    ' DELETE / RECALL bare
    DELETE ALL
    RECALL ALL
    PRINT "3. DELETE/RECALL bare -> OK"

    ' DELETE FOR (condizione)
    DELETE FOR A > 0
    PRINT "4. DELETE FOR bare -> OK"

    ' Funzione bare da namespace di default (SYSTEM): IIF
    DIM s AS STRING
    s = IIF(1 = 1, "si", "no")
    PRINT "5. funzione bare IIF(SYSTEM) -> "; s

    ' Funzione qualificata (resta NAMESPACE.FUNZIONE)
    PRINT "6. qualificata CONVERSIONS.CSTR(42) -> "; CONVERSIONS.CSTR(42)

    PRINT
    PRINT "--- TEST COMPLETATO ---"
END SUB
