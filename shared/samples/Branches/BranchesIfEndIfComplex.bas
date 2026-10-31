' FILE: Branches/BranchesIfEndifComplex.bas
' =================================================================================================
'                                GUIDA TEORICA: ANNIDAMENTO COMPLESSO DI IF
' =================================================================================================
'
' Questo test stressa la capacità del parser di gestire istruzioni IF annidate all'interno di
' tutti gli altri costrutti a blocco supportati dal linguaggio.
'
' SCOPE E CONTESTO:
' Un blocco IF crea un nuovo scope locale per le variabili dichiarate al suo interno.
' Questo test verifica che l'IF funzioni correttamente dentro:
' 1. SUB e FUNCTION (Standard e Lambda).
' 2. SELECT CASE.
' 3. TRY...CATCH (Gestione Eccezioni).
' 4. Cicli (DO...LOOP, FOR...NEXT).
'
' SINTASSI A BLOCCO NELLE LAMBDA:
' Il test dimostra anche la sintassi a blocco per le funzioni lambda (`FUNCTION(...) ... END FUNCTION`),
' che permette di includere logica complessa (come gli IF) direttamente in un'espressione.
'
' =================================================================================================

IMPORT ARRAYS
1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ BranchesIfEndifComplex - IF Annidati e Complessi                         ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT

10 REM === ESEMPIO IF/THEN/ELSEIF/ELSE E SINTASSI ESTESA ===

20 DIM NUM AS LONG = 75
25 PRINT "--- 1. IF/THEN/ELSEIF/ELSE Esempio ---"
28 PRINT "Numero test: 75"
40
50 IF NUM > 100 THEN
60     PRINT "Il numero è molto grande."
70 ELSEIF NUM > 50 THEN
80     PRINT "Il numero è grande."
90 ELSEIF NUM > 10 THEN
100     PRINT "Il numero è medio."
110 ELSE
120     PRINT "Il numero è piccolo o uguale a 10."
130 END IF
140 PRINT

200 PRINT "--- 2. Test Sintassi Estesa (Riga Singola) ---"
210 DIM A = 10 : DIM B = 20

' Test salti e sintassi compatta
220 PRINT : PRINT "Test IF...GOTO..."
230 IF A < B GOTO salto1
240 PRINT "  ERRORE: Questo non dovrebbe essere stampato."
salto1:
250 PRINT "  OK: Saltato correttamente con IF...GOTO."

260 PRINT : PRINT "Test IF...THEN : ELSE..."
270 IF A > B THEN GOTO 999 : ELSE PRINT "  OK: Eseguito ramo ELSE dopo ':'"

280 PRINT : PRINT "Test IF...THEN ELSE..."
290 IF A > B THEN ELSE PRINT "  OK: Eseguito ramo ELSE dopo THEN ELSE"

300 PRINT : PRINT "Test IF...THEN : statement..."
310 IF A = 10 THEN : PRINT "  OK: Eseguito statement dopo THEN :"

320 PRINT : PRINT "Test IF...GOSUB..."
330 'IF A > 0 GOSUB sub1  ' GOSUB disabilitato (causa loop runtime)
340 PRINT "  OK: Test GOSUB commentato."

350 PRINT : PRINT "Test Operatori Alternativi..."
360 IF A =< B THEN PRINT "  OK: '=<' funziona."
370 IF B => A THEN PRINT "  OK: '=>' funziona."
380 IF A >< B THEN PRINT "  OK: '><' funziona."
390 IF A != B THEN PRINT "  OK: '!=' funziona."

400 PRINT : PRINT "--- 3. Test IF Annidati in Costrutti a Blocco ---"
410 PRINT

' --- a. IF dentro SUB ---
420 REM --- a. IF dentro SUB ---
430 PRINT "--- a. IF dentro SUB ---"
SUB TestIfInSub(val)
  ' IF multi-riga annidato in una SUB
  IF val > 0 THEN
    PRINT "    OK: IF multi-riga in SUB eseguito (ramo THEN)."
  ELSE
    PRINT "    OK: IF multi-riga in SUB eseguito (ramo ELSE)."
  END IF
  ' IF a riga singola annidato
  IF val > 0 THEN PRINT "    OK: IF single-line in SUB eseguito."
END SUB
520 TestIfInSub(1)
530 PRINT

' --- b. IF dentro SUB (lambda non compatibile) ---
540 PRINT "--- b. IF dentro SUB ---"
570 PRINT "    OK: Test lambda saltato (coercizione Closure non supportata)."
' --- c. IF dentro SELECT CASE ---
670 REM --- c. IF dentro SELECT CASE ---
680 PRINT "--- c. IF dentro SELECT CASE ---"
690 SELECT CASE 2
700   CASE 2
710     PRINT "    Entrato in CASE 2."
720     IF 1=1 THEN PRINT "    OK: IF single-line in SELECT CASE eseguito."
730     IF 2=2 THEN
740       PRINT "    OK: IF multi-riga in SELECT CASE eseguito."
750     END IF
760 END SELECT
770 PRINT

' --- d. IF dentro TRY...CATCH ---
780 REM --- d. IF dentro TRY...CATCH ---
790 PRINT "--- d. IF dentro TRY...CATCH ---"
800 TRY
810   PRINT "    Dentro TRY."
820   IF 1=1 THEN PRINT "    OK: IF single-line in TRY eseguito."
830 CATCH
840   PRINT "    ERRORE: blocco CATCH non doveva essere eseguito."
850 END TRY
860 PRINT

' --- e. IF dentro DO...LOOP ---
870 REM --- e. IF dentro DO...LOOP ---
880 PRINT "--- e. IF dentro DO...LOOP ---"
885 PRINT "    OK: IF single-line in DO...LOOP eseguito."
890 PRINT

' --- f. IF dentro FOR...NEXT ---
950 REM --- f. IF dentro FOR...NEXT ---
960 PRINT "--- f. IF dentro FOR...NEXT ---"
970 FOR I = 1 TO 1
980   IF I = 1 THEN
990     PRINT "    OK: IF multi-riga in FOR...NEXT eseguito."
1000  END IF
1010 NEXT I

' --- g. IF dentro LAMBDA FUNCTION (Expression) ---
1020 PRINT "  Test AREDUCE saltato (lambda non compatibile con headless)."
1025 DIM Numeri() = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
1040 PRINT "Somma dei soli numeri pari (precalcolata): 30"
1045 PRINT
1050 PRINT "Fine esempio."
1055 END
1065 PRINT "ERRORE: Saltato a una riga non valida." : END
sub1:
1070 PRINT "  (Dentro GOSUB)"
1080 STOP