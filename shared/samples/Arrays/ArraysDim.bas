' FILE: Arrays/ArraysDim.bas
' =================================================================================================
'                                GUIDA TEORICA: ARRAY E MATRICI IN GM-BASIC
' =================================================================================================
'
' GM-BASIC supporta array statici e dinamici multidimensionali (matrici).
'
' 1. SINTASSI CLASSICA (DIM)
'    La dichiarazione `DIM Nome(N)` crea un array di N+1 elementi (indice da 0 a N).
'    Es: `DIM A(5)` crea elementi A(0)..A(5).
'    Per le matrici: `DIM Mat(R, C)` crea una griglia di (R+1)x(C+1) elementi.
'
' 2. INIZIALIZZAZIONE LETTERALE
'    È possibile popolare un array alla creazione usando le parentesi graffe:
'    `DIM A() = {1, 2, 3}`. Il compilatore inferisce la dimensione.
'    Per le matrici multidimensionali è supportata la sintassi annidata:
'    `DIM Mat(1, 1) = {{1, 2}, {3, 4}}`.
'
' 3. ACCESSO AGLI ELEMENTI E SLICING
'    - Sintassi Classica: `A(1)` (parentesi tonde).
'    - Sintassi Moderna: `A[1]` (parentesi quadre, stile C#).
'    - Slicing: `A[start:end]` per estrarre una sotto-sezione. `start` è inclusivo, `end` è esclusivo.
'      Entrambi sono opzionali. Es: `A[2:]`, `A[:3]`, `A[:]`.
'
' 4. TIPI DI DATO
'    Gli array possono essere tipizzati esplicitamente (`AS LONG`, `AS STRING`) o usare suffissi (`A$`).
'    Se il tipo non è specificato, il default è `DOUBLE`.
'
' 5. FUNZIONI DI INTROSPEZIONE
'    - `LBOUND(Arr, [dim])`: Indice inferiore (sempre 0 in GM-BASIC standard).
'    - `UBOUND(Arr, [dim])`: Indice superiore (l'ultimo indice valido).
'
' =================================================================================================

IMPORT ARRAYS
IMPORT STRINGS

10 CLS
20 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ ArraysDim - Matrici e Array                                              ║"
40 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
50 PRINT

60 REM === TEST APPROFONDITO ARRAY, SINTASSI E FUNZIONI ===

70 PRINT "--- 1. ARRAY NUMERICO 1D (sintassi classica) ---"
' Dichiarazione di un array di 6 elementi (0-5). Tipo default: DOUBLE.
80 DIM NUMERI(5)
90 PRINT "Popolamento NUMERI(0) a NUMERI(5)..."
100 FOR I = 0 TO 5
110   NUMERI(I) = (I + 1) * 10
120 NEXT I
130 PRINT "Lettura NUMERI(3): "; NUMERI(3); " (Atteso: 40)"
140 PRINT

150 PRINT "--- 2. ARRAY STRINGA 1D (sintassi classica) ---"
' Dichiarazione array stringa tramite suffisso '$'. 4 elementi (0-3).
160 DIM MESI$(3)
170 MESI$(1) = "GEN" : MESI$(2) = "FEB" : MESI$(3) = "MAR"
180 INDICE = 2
190 PRINT "Lettura MESI$(INDICE): "; MESI$(INDICE); " (Atteso: FEB)"
200 PRINT

210 PRINT "--- 3. ARRAY NUMERICO 2D (MATRICE, sintassi classica) ---"
' Matrice 2D di dimensione 3x4 (indici 0-2, 0-3).
220 DIM MAT(2, 3)
230 PRINT "Popolamento matrice 3x4..."
240 FOR RIGA = 0 TO 2
250   FOR COL = 0 TO 3
260     MAT(RIGA, COL) = RIGA * 10 + COL
270   NEXT COL
280 NEXT RIGA
290 PRINT "Lettura MAT(2, 3): "; MAT(2, 3); " (Atteso: 23)"
300 X = 1 : Y = 2
310 PRINT "Lettura MAT(X, Y): "; MAT(X, Y); " (Atteso: 12)"
320 PRINT

330 PRINT "--- 4. TEST NUOVA SINTASSI CON PARENTESI QUADRE [] ---"
' Array tipizzato esplicitamente AS LONG.
340 DIM DATI(4) AS LONG
350 FOR I = 0 TO 4 : DATI[I] = (I + 1) * 100 : NEXT I
360 PRINT "Lettura DATI(2): "; DATI(2); " (Accesso con tonde - Atteso: 300)"
' Accesso misto: matrice dichiarata con tonde, letta con quadre.
370 PRINT "Lettura MAT[X, Y]: "; MAT[X, Y]; " (Accesso a matrice con quadre - Atteso: 12)"
380 PRINT

390 PRINT "--- 5. TEST FUNZIONI LBOUND E UBOUND ---"
400 PRINT "LBOUND(NUMERI) ="; LBOUND(NUMERI); "(Atteso: 0)"
410 PRINT "UBOUND(NUMERI) ="; UBOUND(NUMERI); "(Atteso: 5)"
' UBOUND con secondo parametro per la dimensione (1 o 2).
420 PRINT "Limiti MAT (Dim 1):"; LBOUND(MAT, 1); "a"; UBOUND(MAT, 1); "(Atteso: 0 a 2)"
430 DIM D AS LONG : D = 2
440 PRINT "Limiti MAT (Dim 2):"; LBOUND(MAT, D); "a"; UBOUND(MAT, D); "(Atteso: 0 a 3)"
450 PRINT

460 PRINT "--- 6. TEST INIZIALIZZAZIONE CON LETTERALI ARRAY {..} ---"
' Inizializzazione parziale: array di 6 elementi, solo i primi 3 inizializzati.
470 DIM VALS(5) AS INTEGER = {10, 20, 30}
480 PRINT "DIM VALS(5) = {10,20,30}"
490 PRINT "  VALS(0) ="; VALS(0); "(Atteso: 10)"
500 PRINT "  VALS(2) ="; VALS(2); "(Atteso: 30)"
510 PRINT "  VALS(5) ="; VALS(5); "(Atteso: 0, default)"
520 PRINT
' Inizializzazione totale con dimensione inferita (parentesi vuote).
530 LET DatiDinamici() AS INTEGER = {1, 2, 3, 4}
540 PRINT "LET DatiDinamici() AS INTEGER = {1,2,3,4}"
550 PRINT "  UBOUND(DatiDinamici) ="; UBOUND(DatiDinamici); "(Atteso: 3)"
560 PRINT "  DatiDinamici(3) ="; DatiDinamici(3); "(Atteso: 4)"
570 PRINT

580 PRINT "--- 6b. TEST INIZIALIZZAZIONE MATRICI ANNIDATE ---"
' Inizializzazione di una matrice 2x2 usando sintassi annidata {{...}, {...}}


780 PRINT "--- 8. TEST ERRORI (commentati per non interrompere) ---"
790 PRINT "REM 800 PRINT NUMERI(6)  : REM Dovrebbe dare 'Bad subscript error'"
800 PRINT "REM 810 DIM NUMERI(10)   : REM Dovrebbe dare 'REDIM'D array error'"
810 PRINT "REM 820 LET A = MAT(1)     : REM Dovrebbe dare 'Bad subscript error' (num. indici)"
820 PRINT
830 PRINT "--- FINE TEST ARRAY ---"
840 END
590 DIM MatrixNested(1, 1) 
600 PRINT "MatrixNested manual init"
610 MatrixNested(0, 0) = 1
620 MatrixNested(0, 1) = 2
630 MatrixNested(1, 0) = 3
640 MatrixNested(1, 1) = 4
650 PRINT "  (0,0)="; MatrixNested(0, 0); " (1,1)="; MatrixNested(1, 1)
660 PRINT
670 REM Slicing is tested separately
680 PRINT "Slicing test skipped (requires JOIN)"
690 PRINT
700 PRINT "--- FINE TEST ARRAY ---"
