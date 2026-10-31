' ==============================================================================
' Sample: DATA / READ / RESTORE — tabella dati e lettura con RESTORE a riga/etichetta
' ==============================================================================
' DATA/READ/RESTORE sono le istruzioni BASIC per una tabella di costanti letta in
' sequenza:
'
'   DATA <v1>, <v2>, ...        dichiara valori (possono stare anche più DATA)
'   READ <var1>, <var2>, ...    legge il prossimo valore del pool in ordine
'   RESTORE                     riporta il puntatore all'inizio del pool
'   RESTORE <riga>              riporta il puntatore alla prima DATA su/oltre <riga>
'   RESTORE <etichetta>         come sopra, usando la riga dell'etichetta
'
' Il pool è costruito in ORDINE DI RIGA; un READ oltre la fine del pool solleva
' "OUT OF DATA". NOTA: il RESTORE con NUMERO di riga richiede che il programma usi
' numeri di riga espliciti (le righe non numerate hanno chiavi sintetiche >= 1_000_000,
' non raggiungibili da RESTORE <n>). Con righe implicite usare sempre l'etichetta.
' ==============================================================================

10 DATA 10, 20, 30
20 DATA 40, 50

' --- lettura sequenziale ---
30 READ a, b, c
40 PRINT "A:"; a; "B:"; b; "C:"; c        ' 10 20 30
50 READ d, e
60 PRINT "D:"; d; "E:"; e                  ' 40 50

' --- RESTORE <riga>: riparte dalla DATA a riga >= 20 (40, 50) ---
70 RESTORE 20
80 READ x, y
90 PRINT "X:"; x; "Y:"; y                  ' 40 50

' --- RESTORE nudo: riparte dall'inizio del pool (10, 20) ---
100 RESTORE
110 READ p, q
120 PRINT "P:"; p; "Q:"; q                  ' 10 20

' --- RESTORE <etichetta>: riparte dalla DATA successiva all'etichetta (99) ---
130 ReStart:
140 DATA 99
150 RESTORE ReStart
160 READ w
170 PRINT "W:"; w                           ' 99
