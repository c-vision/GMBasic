' FILE: LexerLists.bas
' ======================================================================================================================
' GUIDA TEORICA: IL LEXER E LE STRUTTURE MULTI-RIGA
' ======================================================================================================================
' Il Lexer di GM-BASIC è progettato per gestire strutture dati complesse e formattazione flessibile del codice.
'
' 1. LETTERALI MULTI-RIGA
'    - Dizionari e Array: Possono essere definiti su più righe usando le parentesi graffe `{ ... }`.
'    - Liste: Possono essere definite su più righe usando le parentesi quadre `[ ... ]`.
'    - Il Lexer gestisce automaticamente i ritorni a capo all'interno di questi blocchi, permettendo una formattazione
'      pulita e leggibile (simile a JSON).
'
' 2. CONTINUAZIONE DI RIGA
'    - Carattere Underscore (_): Usato alla fine di una riga per indicare che l'istruzione continua sulla successiva.
'      È fondamentale per spezzare stringhe lunghe o espressioni complesse senza interrompere il flusso logico.
'    - Concatenazione Implicita: Quando usato con stringhe letterali, l'underscore permette di concatenare visivamente
'      il testo preservando la leggibilità.
' ======================================================================================================================

10 PRINT "Inizio" : DIM A = 1

' Definizione di un Dizionario su più righe per leggibilità
20 REM Dizionario multi-riga
30 DIM D AS DICT<STRING, INTEGER> = {
    "a": 1,
    "b": 2
}

' Accesso a un elemento del dizionario
40 PRINT D("a"); ' Semicolon

' Definizione di un Array (o collezione generica) su più righe
50 REM Array multi-riga con virgola finale
60 LET B = {
    10,
    20,
} : REM Commento su stessa riga

' Accesso a un elemento dell'array
80 PRINT B(1) ' Accesso all'array

' Definizione di una Lista su più righe con parentesi quadre
REM Lista multi-riga con parentesi quadre e numeri di riga interni
90 DIM C AS LIST<INTEGER> = [
   100,
   200
]

' Test di continuazione di riga esplicita con underscore
REM --- NUOVO TEST: Concatenazione di 3 stringhe con _ ---
100 DIM TreStringhe$ = "Prima parte" _
   " Seconda parte" _
   " Terza parte"
110 PRINT TreStringhe$ ' Atteso: Prima parte Seconda parte Terza parte

' Test complesso combinando liste e continuazione di riga in un'espressione
115 DIM resultList AS LIST<STRING> = ["ElementoZero", "ElementoUno"]
120 PRINT "Concatenazione " + "con " + _
         "funzioni: " + resultList[0] + _
         ", e con " : PRINT " continuazione (:)"