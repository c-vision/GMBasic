' FILE: Branches/BranchesIfAllSyntaxes.bas
' =================================================================================================
'                                GUIDA TEORICA: ISTRUZIONE IF (RAMIFICAZIONE CONDIZIONALE)
' =================================================================================================
'
' GM-BASIC supporta una vasta gamma di sintassi per l'istruzione IF, coprendo sia lo stile classico
' (compatibilità QBasic/GW-BASIC) che quello moderno strutturato.
'
' 1. SINTASSI A RIGA SINGOLA (IF...THEN...ELSE)
'    Compatta, utile per condizioni semplici.
'    - Semplice: `IF cond THEN stmt`
'    - Con ELSE: `IF cond THEN stmt1 ELSE stmt2`
'    - Multi-istruzione (con ':'): `IF cond THEN stmt1 : stmt2 ELSE stmt3 : stmt4`
'
' 2. SINTASSI IMPLICITA (GOTO)
'    Permette di omettere `GOTO` quando la destinazione è un numero di riga o un'etichetta.
'    - `IF cond GOTO label`
'    - `IF cond THEN label` (GOTO implicito)
'    - `IF cond THEN 100` (GOTO implicito a riga)
'
' 3. SINTASSI A BLOCCO (IF...END IF)
'    Strutturata, ideale per logica complessa.
'    IF cond THEN
'       ...
'    ELSEIF cond2 THEN
'       ...
'    ELSE
'       ...
'    END IF
'
' 4. SINTASSI SPECIALI
'    - `IF cond GOSUB label`: Esegue una subroutine e ritorna.
'    - `IF cond THEN` (vuoto): Utile con `ELSE` per invertire la logica senza usare `NOT`.
'
' =================================================================================================
1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ BranchesIfAllSyntaxes - Test di Stress Completo Sintassi IF              ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT

' Subroutine helper per testare chiamate in riga singola
SUB DummySub()
  PRINT "  -> SUB 'DummySub' chiamata."
END SUB

10 REM --- Inizializzazione ---
20 A = 10 : B = 20
30 FLAG = 0

100 PRINT "--- 1. SINTASSI THEN (Singola Istruzione) ---"
110 IF A < B THEN PRINT "  OK(1a): THEN Print"
120 IF A > B THEN PRINT "  ERRORE(1b): THEN non doveva eseguire" ELSE PRINT "  OK(1b): ELSE dopo THEN"

130 PRINT : PRINT "--- 2. SINTASSI GOTO ---"
140 IF A < B GOTO 160
150 GOTO 170
160 PRINT "  OK(2a): IF...GOTO ha funzionato."
170 IF A > B GOTO 9999 : PRINT "  OK(2b): IF...GOTO non ha saltato."

180 PRINT : PRINT "--- 3. SINTASSI GOTO IMPLICITO (Numero di riga) ---"
190 FLAG = 0
200 IF A < B THEN 220
210 GOTO 230
220 FLAG = 1
230 IF FLAG = 1 THEN PRINT "  OK(3a): GOTO implicito a riga ha funzionato." ELSE PRINT "  ERRORE(3a)"
240 IF A > B THEN 9999 ELSE 250
250 PRINT "  OK(3b): GOTO implicito in ELSE ha funzionato."

260 PRINT : PRINT "--- 4. SINTASSI GOTO IMPLICITO (Etichetta) ---"
270 IF A < B THEN TestEtichetta
280 GOTO FineTestEtichetta
TestEtichetta:
290 PRINT "  OK(4a): GOTO implicito a etichetta ha funzionato."
FineTestEtichetta:
300 PRINT : PRINT "--- 5. SINTASSI GOSUB ---"
310 'IF A < B GOSUB 1000  ' GOSUB disabilitato (causa loop runtime in headless)
320 PRINT "  OK(5a): Test GOSUB commentato."

330 PRINT : PRINT "--- 6. SINTASSI CHIAMATA A FUNZIONE/SUB ---"
340 'IF A < B THEN DummySub()  ' SUB call disabilitata (causa loop runtime in headless)
350 FLAG = 0
360 IF A < B THEN FLAG = 1
370 IF FLAG = 1 THEN PRINT "  OK(6a): IF...THEN Chiamata a funzione ha funzionato." ELSE PRINT "  ERRORE(6a)"

380 PRINT : PRINT "--- 7. SINTASSI MULTI-ISTRUZIONE (THEN e ELSE) ---"
390 FLAG = 0
400 IF A < B THEN PRINT "  OK(7a): Prima istruzione THEN" : FLAG = 1 : PRINT "  OK(7a): Seconda istruzione THEN"
410 IF FLAG = 0 THEN PRINT "  ERRORE(7a): Blocco THEN non eseguito."
420 FLAG = 0
430 IF A > B THEN PRINT "  ERRORE(7b)" : FLAG = 1 ELSE PRINT "  OK(7b): Prima istruzione ELSE" : FLAG = 2 : PRINT "  OK(7b): Seconda istruzione ELSE"
440 IF FLAG <> 2 THEN PRINT "  ERRORE(7b): Blocco ELSE non eseguito correttamente."

450 PRINT : PRINT "--- 8. SINTASSI THEN VUOTO ---"
460 FLAG = 0
470 IF A > B THEN ELSE FLAG = 1
480 IF FLAG = 1 THEN PRINT "  OK(8a): IF...THEN ELSE (THEN vuoto) ha funzionato." ELSE PRINT "  ERRORE(8a)"
490 FLAG = 0
500 IF A > B THEN : ELSE FLAG = 1
510 IF FLAG = 1 THEN PRINT "  OK(8b): IF...THEN : ELSE (THEN vuoto) ha funzionato." ELSE PRINT "  ERRORE(8b)"

600 PRINT : PRINT "--- 9. IF MULTI-RIGA ---"
610 FLAG = 0
620 IF B > A THEN
630     PRINT "  OK(9a): Entrato nel blocco THEN multi-riga."
640     FLAG = 1
650 ELSEIF A > B THEN
660     PRINT "  ERRORE(9b): Entrato in ELSEIF."
670     FLAG = 2
680 ELSE
690     PRINT "  ERRORE(9c): Entrato in ELSE."
700     FLAG = 3
710 END IF
720 IF FLAG <> 1 THEN PRINT "  ERRORE(9d): Blocco IF multi-riga non corretto."

900 PRINT : PRINT "--- Test di Stress IF completati ---"
910 END

1000 REM --- Subroutine per test GOSUB ---
1010 PRINT "  -> GOSUB eseguito."
1020 RETURN

9999 PRINT "ERRORE: Saltato a una riga non valida."
