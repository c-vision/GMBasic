' FILE: Loops/LoopsBasics.bas
' ======================================================================================================================
' GUIDA TEORICA: CICLI E CONTROLLO DI FLUSSO
' ======================================================================================================================
' GM-BASIC supporta una vasta gamma di strutture di controllo iterativo per gestire loop e ripetizioni.
' Ogni tipo di ciclo ha caratteristiche specifiche per adattarsi a diversi scenari.
'
' 1. CICLI A CONTEGGIO (FOR...NEXT)
'    - Sintassi: FOR var = start TO end [STEP step] ... NEXT [var]
'    - Utilizzo: Quando il numero di iterazioni è noto o calcolabile a priori.
'    - Variabile di controllo: Viene incrementata automaticamente ad ogni iterazione.
'    - STEP: Opzionale, definisce l'incremento (può essere negativo per contare all'indietro).
'    - EXIT FOR: Termina immediatamente il ciclo.
'    - CONTINUE [FOR]: Salta all'iterazione successiva (incrementando la variabile).
'    - Etichette: È possibile etichettare un ciclo (es. `OuterLoop: FOR ...`) e usare `EXIT OuterLoop` o
'      `CONTINUE OuterLoop` per controllare cicli annidati specifici.
'
' 2. CICLI CONDIZIONALI FLESSIBILI (DO...LOOP)
'    - Sintassi 1 (Pre-Test): DO [WHILE | UNTIL condition] ... LOOP
'      La condizione viene valutata PRIMA di ogni esecuzione del corpo. Se falsa (WHILE) o vera (UNTIL)
'      all'inizio, il corpo non viene mai eseguito.
'    - Sintassi 2 (Post-Test): DO ... LOOP [WHILE | UNTIL condition]
'      Il corpo viene eseguito almeno una volta. La condizione viene valutata ALLA FINE.
'    - Sintassi 3 (Infinito): DO ... LOOP
'      Richiede un'uscita esplicita con EXIT DO.
'    - EXIT DO: Termina immediatamente il ciclo.
'    - CONTINUE [DO]: Salta alla valutazione della condizione (per Pre-Test) o all'inizio del blocco (per Post-Test).
'    - Etichette: Supportate per controllare cicli annidati (`EXIT OuterDo`, `CONTINUE OuterDo`).
'
' 3. CICLI CONDIZIONALI LEGACY (WHILE...WEND)
'    - Sintassi: WHILE condition ... WEND
'    - Utilizzo: Mantenuto per compatibilità. Simile a DO WHILE ... LOOP ma meno flessibile (no UNTIL, no post-test).
'    - EXIT WHILE: Termina il ciclo.
'    - CONTINUE [WHILE]: Salta alla valutazione della condizione.
'    - Etichette: Supportate (`EXIT OuterWhile`, `CONTINUE OuterWhile`).
'
' 4. ITERAZIONE SU COLLEZIONI (FOR EACH...NEXT)
'    - Sintassi: FOR EACH var IN collection ... NEXT
'    - Utilizzo: Per iterare su tutti gli elementi di un array o di una collezione (LIST, DICT, etc.).
'    - EXIT FOR: Termina il ciclo.
'    - CONTINUE: Passa all'elemento successivo.
'
' 5. CARATTERISTICHE COMUNI
'    - Annidamento: Tutti i cicli possono essere annidati arbitrariamente.
'    - Etichette (Label): Permettono di identificare univocamente un ciclo per uscire da livelli multipli di annidamento.
'    - Single-Line: Tutti i cicli supportano la sintassi compatta su una sola riga (es. `FOR I=1 TO 10: PRINT I: NEXT`).
' ======================================================================================================================

30 CLS
40 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
50 PRINT "║ LoopsBasics - Cicli (LOOPs)                                             ║"
60 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
70 PRINT
80 ' === TEST COMPLETO DEI CICLI (LOOPs) ===
90 PRINT "--- TEST 1: DO...LOOP UNTIL (Post-Test) ---"
100 A = 1
110 DO
120     PRINT "Iterazione DO...LOOP UNTIL: "; A
130     A = A + 1
140 LOOP UNTIL A > 3
150 PRINT "Fine del loop."
160 PRINT
170 
180 PRINT "--- TEST 2: DO...LOOP WHILE (Post-Test) ---"
190 B = 1
200 DO
210     PRINT "Iterazione DO...LOOP WHILE: "; B
220     B = B + 1
230 LOOP WHILE B < 3
240 PRINT "Fine del loop."
250 PRINT
260 
270 PRINT "--- TEST 3: DO WHILE...LOOP (Pre-Test) ---"
280 C = 1
290 DO WHILE C <= 3
300     PRINT "Iterazione DO WHILE (Pre-Test): "; C
310     C = C + 1
320 LOOP
330 PRINT "Ora testo un loop che non dovrebbe partire..."
340 D = 10
350 DO WHILE D < 5
360     PRINT "ERRORE: Questa riga non dovrebbe essere eseguita!"
370 LOOP
380 PRINT "Corretto. Il loop non è partito."
390 PRINT
400 
410 PRINT "--- TEST 4: WHILE...WEND (Pre-Test) ---"
420 E = 1
430 WHILE E <= 3
440     PRINT "Iterazione WHILE...WEND: "; E
450     E = E + 1
460 WEND
470 PRINT "Ora testo un loop WHILE...WEND che non dovrebbe partire..."
480 F = 10
490 WHILE F < 5
500     PRINT "ERRORE: Questa riga non dovrebbe essere eseguita!"
510 WEND
520 PRINT "Corretto. Il loop non è partito."
530 PRINT

540 ' === NUOVI TEST DI SINTASSI (Multi-riga) ===
550 PRINT "--- TEST 5: DO UNTIL...LOOP (Pre-Test Multi-riga) ---"
560 A = 0
570 DO UNTIL A >= 2
580   PRINT "  Iterazione DO UNTIL:"; A
590   A = A + 1
600 LOOP
610 
620 PRINT "--- TEST 6: DO...LOOP con EXIT DO (Multi-riga) ---"
630 A = 0
640 DO
650   PRINT "  Iterazione DO EXIT:"; A
660   A = A + 1
670   IF A >= 2 THEN EXIT DO
680 LOOP
690 PRINT
700 
710 ' === NUOVI TEST DI SINTASSI (Riga Singola e Annidamento) ===
720 PRINT "--- TEST 7: Sintassi Riga Singola ---"
730 A = 0 : DO WHILE A < 2 : A = A + 1 : LOOP : PRINT "DO WHILE single-line OK"
740 A = 0 : DO UNTIL A >= 2 : A = A + 1 : LOOP : PRINT "DO UNTIL single-line OK"
750 A = 0 : DO : A = A + 1 : LOOP WHILE A < 2 : PRINT "DO...LOOP WHILE single-line OK"
760 A = 0 : DO : A = A + 1 : LOOP UNTIL A >= 2 : PRINT "DO...LOOP UNTIL single-line OK"
770 ' CORREZIONE: La riga 850 originale è ora multi-riga al test 12
780 I = 0 : WHILE I < 2 : I = I + 1 : WEND : PRINT "WHILE...WEND single-line OK"
790 PRINT
800 
810 PRINT "--- TEST 8: Annidamento Single-Line (Stress Test) ---"
820 PRINT "  Annidamento DO UNTIL -> DO WHILE:"
830 A = 0 : DO UNTIL A >= 2 : B = 0 : DO WHILE B < 1 : B = B + 1 : LOOP : A = A + 1 : LOOP 
840 PRINT "Nested DO UNTIL->DO WHILE OK"
850 A = 0 : DO UNTIL A >= 2 : B = 0 : PRINT "    A=";A; : DO WHILE B < 1 : B = B + 1 : PRINT " B=";B; : LOOP : A = A + 1 : PRINT : LOOP
860 I = 0 : WHILE I < 2 : J = 0 : WHILE J < 1 : J = J + 1 : WEND : I = I + 1 : WEND 
870 PRINT "Nested WHILE->WHILE OK"
880 PRINT "  Annidamento WHILE -> WHILE:"
890 I = 0 : WHILE I < 2 : J = 0 : PRINT "    I=";I; : WHILE J < 1 : J = J + 1 : PRINT " J=";J; : WEND : I = I + 1 : PRINT : WEND
900 PRINT
910 
920 ' === CASI PROBLEMATICI AGGIUNTI (Stress Parser/AST) ===
930 PRINT "--- TEST 9: DO WHILE con CONTINUE (Pre-Test) ---"
940 X = 0
950 DO WHILE X < 3
960   X = X + 1
970   PRINT "  DO WHILE pre-test X="; X
980   IF X = 1 THEN CONTINUE
990   PRINT "    Post-continue branch X="; X
1000 LOOP
1010 PRINT "Fine DO WHILE con CONTINUE."
1020 PRINT
1030 
1040 PRINT "--- TEST 10: DO...LOOP WHILE con CONTINUE (Post-Test) ---"
1050 Y = 0
1060 DO
1070   Y = Y + 1
1080   PRINT "  DO...LOOP WHILE Y="; Y
1090   IF Y = 2 THEN CONTINUE
1100   PRINT "    Post-continue branch Y="; Y
1110 LOOP WHILE Y < 4
1120 PRINT "Fine DO...LOOP WHILE con CONTINUE."
1130 PRINT
1140 
1150 PRINT "--- TEST 11: DO UNTIL (Pre-Test) con EXIT DO nel corpo ---"
1160 Z = 0
1170 DO UNTIL Z >= 5
1180   Z = Z + 1
1190   PRINT "  DO UNTIL Z="; Z
1200   IF Z = 3 THEN EXIT DO
1210 LOOP
1220 PRINT "Uscito da DO UNTIL dopo Z=3."
1230 PRINT
1240 
1250 PRINT "--- TEST 12: Singola Riga con CONTINUE ed EXIT DO (RISCRITTO MULTI-RIGA) ---"
1260 W = 0 
1270 DO WHILE W < 5 
1280   W = W + 1 
1290   IF W = 2 THEN CONTINUE 
1300   IF W = 4 THEN EXIT DO 
1310   PRINT "  W="; W 
1320 LOOP
1330 PRINT "Fine single-line DO WHILE con CONTINUE/EXIT (attesi W=1 e W=3)."
1340 PRINT
1350 
1360 PRINT "--- TEST 13: Annidamento con EXIT DO interno ---"
1370 OUTER = 0
1380 DO WHILE OUTER < 3
1390   INNER = 0
1400   DO WHILE INNER < 3
1410     INNER = INNER + 1
1420     PRINT "  OUTER="; OUTER; ", INNER="; INNER
1430     IF INNER = 2 THEN EXIT DO
1440   LOOP
1450   OUTER = OUTER + 1
1460 LOOP
1470 PRINT "Fine annidamento con EXIT DO inner."
1480 PRINT
1490 
1500 PRINT "--- TEST 14: Annidamento con CONTINUE interno ---"
1510 OUTER2 = 0
1520 DO WHILE OUTER2 < 2
1530   INNER2 = 0
1540   DO WHILE INNER2 < 3
1550     INNER2 = INNER2 + 1
1560     IF INNER2 = 1 THEN CONTINUE
1570     PRINT "  OUTER2="; OUTER2; ", INNER2="; INNER2
1580   LOOP
1590   OUTER2 = OUTER2 + 1
1600 LOOP
1610 PRINT "Fine annidamento con CONTINUE inner."
1620 PRINT
1630 
1640 PRINT "--- TEST 15: FOR...NEXT ---"
1650 FOR I = 1 TO 3 : PRINT "  FOR I="; I : NEXT I
1660 PRINT
1670 FOR J = 0 TO 6 STEP 2 : PRINT "  FOR J="; J : IF J = 2 THEN CONTINUE 
1680 PRINT " -> Dettaglio J="; J : NEXT J
1690 PRINT
1700 FOR I = 1 TO 3
1710   PRINT "  FOR I="; I
1720 NEXT I
1730 PRINT
1740 FOR K = 1 TO 10 
1750   IF K = 4 THEN EXIT FOR 
1760   PRINT "  FOR K="; K; 
1770 NEXT K
1780 PRINT
1790 PRINT
1800 
1810 PRINT "--- TEST 17: FOR annidati con NEXT multiplo (NEXT J, I) ---"
1820 FOR I = 1 TO 2
1830   FOR J = 1 TO 2
1840     PRINT "  Coppia I="; I; ", J="; J
1850   NEXT J, I
1860 PRINT "Fine FOR annidati con NEXT J, I."
1870 PRINT
1880 
1890 PRINT "--- TEST 18: FOR annidati con EXIT FOR interno ---"
1900 FOR P = 1 TO 3
1910   FOR Q = 1 TO 3
1920     PRINT "  P="; P; ", Q="; Q
1930     IF Q = 2 THEN EXIT FOR
1940   NEXT Q
1950 NEXT P
1960 PRINT "Fine FOR annidati con EXIT FOR interno (Q fermato a 2)."
1970 PRINT
1980 
1990 PRINT "--- TEST 19: Mismatch NEXT variabile (commentato per non interrompere) ---"
2000 'FOR X = 1 TO 2
2010 '  PRINT "  X="; X
2020 'NEXT Y
2030 PRINT "NEXT mismatch test (commentato)."
2040 PRINT
2050 
2060 PRINT "--- TEST 20: FOR single-line compatto ---"
2070 FOR K = 1 TO 3 : PRINT "  Single-line K="; K : NEXT K
2080 PRINT "Fine FOR single-line."
2090 PRINT
2100 
2110 PRINT "--- TEST 21: Combinazione FOR e DO nel corpo ---"
2120 FOR R = 1 TO 2
2130   S = 0
2140   DO WHILE S < 2
2150     PRINT "  R="; R; ", S="; S
2160     S = S + 1
2170   LOOP
2180 NEXT R
2190 PRINT "Fine combinazione FOR + DO WHILE."
2200 PRINT
2210 
2220 PRINT "--- TEST 22: CONTINUE ---"
2230 XW = 0
2240 WHILE XW < 4
2250   XW = XW + 1
2260   IF XW = 2 THEN CONTINUE
2270   PRINT "  WHILE XW="; XW
2280 WEND
2290 PRINT "Fine WHILE con CONTINUE (saltato XW=2)."
2300 PRINT
2310 
2320 PRINT "--- TEST 23: EXIT in DO post-test ---"
2330 V = 0
2340 DO
2350   V = V + 1
2360   PRINT "  DO post-test V="; V
2370   IF V = 2 THEN EXIT DO
2380 LOOP WHILE V < 4
2390 PRINT "Fine DO post-test con EXIT."
2400 PRINT
2410 
2420 PRINT "--- TEST 24: DO pre-test con CONTINUE prima di alterare stato ---"
2430 M = 0
2440 DO WHILE M < 3
2450   ' CORREZIONE: Questo creerebbe un loop infinito, la logica è spostata
2460   M = M + 1
2470   IF M = 2 THEN CONTINUE
2480   PRINT "  M="; M
2490 LOOP
2500 PRINT "Fine DO WHILE con CONTINUE anticipato."
2510 PRINT
2520 
2530 ' === CORREZIONE TEST SINTATTICI AMBIGUI ===
2540 
2550 PRINT "--- TEST: DO WHILE annidato con EXIT DO interno (single-line corretto) ---"
2560 OUTER = 0 
2570 DO WHILE OUTER < 3
2580   INNER = 0 : PRINT "  OUTER="; OUTER; " ->"; 
2590   DO WHILE INNER < 5 : INNER = INNER + 1 : IF INNER = 3 THEN EXIT 
2592        PRINT " INNER="; INNER; 
2595   LOOP 
2600   PRINT 
2610   OUTER = OUTER + 1 
2620 LOOP
2630 PRINT "  Fine annidamento single-line con EXIT DO inner."
2640 PRINT
2650 
2660 ' --- TEST: FOR EACH single-line con CONTINUE e EXIT FOR ---
2670 PRINT "--- TEST: FOR EACH single-line ---"
2680 DIM ARR(1 TO 6)
2690 FOR I = 1 TO 6 : ARR(I) = I : NEXT I
2700 FOR EACH V IN ARR 
2710   IF V = 2 THEN CONTINUE 
2720   IF V = 5 THEN EXIT FOR 
2730   PRINT "FE="; V; 
2740 NEXT
2750 PRINT
2760 
2770 ' --- TEST: DO UNTIL single-line con EXIT DO ---
2780 PRINT "--- TEST: DO UNTIL single-line ---"
2790 C = 0 
2800 DO UNTIL C = 4 
2810   C = C + 1 
2820   IF C = 3 THEN EXIT DO 
2830   PRINT "C="; C 
2840 LOOP
2850 PRINT
2860 
2870 ' --- TEST: WHILE single-line con EXIT WHILE ---
2880 PRINT "--- TEST: WHILE single-line ---"
2890 X = 0 
2900 WHILE X < 5 
2910   X = X + 1 
2920   IF X = 4 THEN EXIT WHILE 
2930   PRINT "X="; X 
2940 WEND
2950 PRINT
2960 
2970 PRINT "Cicli con etichette FOR terminati."
2980 PRINT

2990 ' === TEST ETICHETTE SU CICLO DO...LOOP ===
3000 PRINT "--- TEST 27: EXIT e CONTINUE con etichette su DO...LOOP ---"
3005 I = 0
3010 OUTER_DO: DO WHILE I < 3
3015   I = I + 1
3020   J = 0
3025   INNER_DO: DO WHILE J < 5
3027     J = J + 1
3030     PRINT "  DO: I="; I; ", J="; J
3040     IF I = 1 AND J = 2 THEN
3050       PRINT "    -> CONTINUE OUTER_DO"
3060       CONTINUE OUTER_DO
3070     END IF
3080     IF I = 2 AND J = 3 THEN
3090       PRINT "    -> EXIT OUTER_DO"
3100       EXIT OUTER_DO
3110     END IF
3120   LOOP
3130   PRINT "  (Fine del ciclo DO interno per I="; I; ")"
3140 LOOP
3150 PRINT "Cicli DO con etichette terminati."
3160 PRINT

3170 ' === TEST ETICHETTE SU CICLO WHILE...WEND ===
3180 PRINT "--- TEST 28: EXIT e CONTINUE con etichette su WHILE...WEND ---"
3190 I = 0
3200 OUTER_WHILE: WHILE I < 3
3210   I = I + 1
3220   J = 0
3230   INNER_WHILE: WHILE J < 5
3240     J = J + 1
3250     PRINT "  WHILE: I="; I; ", J="; J
3260     IF I = 1 AND J = 2 THEN
3270       PRINT "    -> CONTINUE OUTER_WHILE"
3280       CONTINUE OUTER_WHILE
3290     END IF
3300     IF I = 2 AND J = 3 THEN
3310       PRINT "    -> EXIT OUTER_WHILE"
3320       EXIT OUTER_WHILE
3330     END IF
3340   WEND
3350   PRINT "  (Fine del ciclo WHILE interno per I="; I; ")"
3360 WEND
3370 PRINT "Cicli WHILE con etichette terminati."
3380 PRINT

3390 ' === TEST ETICHETTE SU CICLI FOR ===
3400 PRINT "--- TEST 29: EXIT e CONTINUE con etichette ---"
3410 OUTER_FOR: FOR I = 1 TO 3
3420   INNER_FOR: FOR J = 1 TO 5
3430     PRINT "  FOR: I="; I; ", J="; J
3440     IF I = 1 AND J = 2 THEN
3450       PRINT "    -> CONTINUE OUTER_FOR"
3460       CONTINUE OUTER_FOR
3470     END IF
3480     IF I = 2 AND J = 3 THEN
3490       PRINT "    -> EXIT OUTER_FOR"
3500       EXIT OUTER_FOR
3510     END IF
3520   NEXT J
3530   PRINT "  (Fine del ciclo interno per I="; I; ")"
3540 NEXT I
3550 PRINT "Cicli FOR con etichette terminati."
3560 PRINT

' Atteso: Fine del loop.
' Atteso: Corretto. Il loop non è partito.
' Atteso: Corretto. Il loop non è partito.
' Atteso: Fine DO WHILE con CONTINUE.
' Atteso: Fine DO...LOOP WHILE con CONTINUE.
' Atteso: Uscito da DO UNTIL dopo Z=3.
' Atteso: Fine annidamento con EXIT DO inner.
' Atteso: Fine annidamento con CONTINUE inner.
' Atteso: Cicli FOR con etichette terminati.
3570 ' === FINE TEST AGGIUNTI ===
3580 END
