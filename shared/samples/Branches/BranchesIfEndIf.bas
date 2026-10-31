' FILE: Branches/BranchesIfEndif.bas
' =================================================================================================
'                                GUIDA TEORICA: IF STRUTTURATO E SINTASSI CLASSICA
' =================================================================================================
'
' Questo test copre sia la forma strutturata (multi-linea) dell'IF che le varianti
' a linea singola tipiche dei dialetti BASIC classici.
'
' 1. IF STRUTTURATO (MULTI-RIGA)
'    È la forma preferita per la leggibilità.
'    - Deve terminare con `END IF` (o `ENDIF`).
'    - Supporta un numero arbitrario di clausole `ELSEIF`.
'    - Supporta una clausola `ELSE` finale opzionale.
'
' 2. IF A RIGA SINGOLA (CLASSICO)
'    Compatto, utile per controlli rapidi.
'    - `IF cond THEN stmt1 [ELSE stmt2]`
'    - `IF cond GOTO label` (GOTO esplicito)
'    - `IF cond THEN label` (GOTO implicito)
'    - `IF cond THEN 100` (GOTO implicito a numero di riga)
'
' 3. OPERATORI DI CONFRONTO
'    GM-BASIC supporta sia gli operatori classici che quelli C-style:
'    - Uguale: `=`
'    - Diverso: `<>`, `!=`, `><`
'    - Minore/Maggiore o uguale: `<=`, `=<`, `>=`, `=>`
'
' =================================================================================================
1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ BranchesIfEndif - IF/THEN/ELSEIF/ELSE/END IF                             ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 REM ESEMPIO IF/THEN/ELSEIF/ELSE E SINTASSI ESTESA
20 DIM NUM AS LONG = 75
30 PRINT "--- 1. IF/THEN/ELSEIF/ELSE Esempio ---"
35 PRINT "Numero test: 75"
40
' Esempio di blocco IF completo con ELSEIF multipli
50 IF NUM > 100 THEN
60     PRINT "Il numero è molto grande."
70 ELSEIF NUM > 50 THEN
80     PRINT "Il numero è grande."
90 ELSEIF NUM > 10 THEN
100     PRINT "Il numero è medio."
110 ELSE
120     PRINT "Il numero è piccolo o uguale a 10."
130 ENDIF
140 PRINT

200 PRINT "--- 2. Test Sintassi Estesa ---"
210 DIM A = 10 : DIM B = 20

' Test della sintassi "IF...GOTO" classica
220 PRINT : PRINT "Test IF...GOTO..."
230 IF A < B GOTO salto1
240 PRINT "  ERRORE: Questo non dovrebbe essere stampato."
salto1:
250 PRINT "  OK: Saltato correttamente con IF...GOTO."

' Test della sintassi con ':' prima di ELSE (usato in alcuni dialetti per separare statement)
260 PRINT : PRINT "Test IF...THEN : ELSE..."
270 IF A > B THEN 999 : ELSE PRINT "  OK: Eseguito ramo ELSE dopo ':'"

' Test della sintassi con THEN vuoto seguito da ELSE
280 PRINT : PRINT "Test IF...THEN ELSE..."
290 IF A > B THEN ELSE PRINT "  OK: Eseguito ramo ELSE dopo THEN ELSE"

' Test del ':' opzionale dopo THEN prima di uno statement
300 PRINT : PRINT "Test IF...THEN : statement..."
310 IF A = 10 THEN : PRINT "  OK: Eseguito statement dopo THEN :"

' Test di IF con GOSUB (chiamata condizionale a subroutine)
320 PRINT : PRINT "Test IF...GOSUB..."
330 IF A > 0 GOSUB sub1
340 PRINT "  OK: Tornato correttamente da IF...GOSUB."

' Test degli operatori di confronto alternativi
350 PRINT : PRINT "Test Operatori Alternativi..."
360 IF A =< B THEN PRINT "  OK: '=<' funziona."
370 IF B => A THEN PRINT "  OK: '=>' funziona."
380 IF A >< B THEN PRINT "  OK: '><' funziona."
390 IF A != B THEN PRINT "  OK: '!=' funziona."

900 PRINT : PRINT "Fine esempio."
910 END
' Trap per salti errati
999 PRINT "ERRORE: Saltato a una riga non valida." : END

' Subroutine di test
sub1:
1000 PRINT "  (Dentro GOSUB)"
1010 RETURN
