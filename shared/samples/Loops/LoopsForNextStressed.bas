' FILE: Loops/LoopsForNextStressed.bas
' ======================================================================================================================
' GUIDA TEORICA: STRESS TEST CICLI FOR ANNIDATI E OTTIMIZZAZIONE
' ======================================================================================================================
' Questo file mette alla prova la robustezza del parser e dell'ottimizzatore (Loop Flattening) in scenari complessi
' di annidamento dei cicli.
'
' 1. CICLI ANNIDATI E FLATTENING
'    - Il compilatore GM-BASIC utilizza una tecnica di "Loop Flattening" per trasformare cicli annidati ricorsivi
'      in una sequenza lineare di istruzioni e salti (macchina a stati).
'    - Questo approccio previene l'esaurimento dello stack di chiamate del sistema (Stack Overflow) anche con
'      livelli di annidamento molto profondi.
'
' 2. SINTASSI SINGLE-LINE (:)
'    - Il BASIC permette di scrivere interi cicli o parti di essi su una sola riga usando i due punti (:).
'    - Il parser deve essere in grado di scindere correttamente queste righe in istruzioni logiche sequenziali,
'      mantenendo la corretta gerarchia degli scope per le variabili di ciclo.
'
' 3. NEXT MULTIPLI
'    - La sintassi `NEXT J, I` è zucchero sintattico standard per `NEXT J : NEXT I`.
'    - Il compilatore deve espandere questa istruzione chiudendo i cicli nell'ordine specificato (dal più interno
'      al più esterno). Un ordine errato (es. chiudere I prima di J) causerebbe un errore logico o di runtime.
'
' 4. SALTI NON STRUTTURATI (GOTO) vs STRUTTURATI (EXIT FOR)
'    - Uscire da cicli annidati con `GOTO` (Caso 5) è supportato ma richiede che il runtime gestisca correttamente
'      il cambio di contesto. Nel codice appiattito, questo si traduce in un semplice salto a un'etichetta.
'    - `EXIT FOR` (Caso 7) è il metodo strutturato preferito, garantendo un'uscita pulita dallo scope del ciclo corrente.
' ======================================================================================================================

10 ' --- Test di Cicli FOR Annidati (Single-line e Multi-line) ---
20 CLS
30 PRINT "Inizio test cicli FOR annidati..."
40 PRINT

50 ' --- CASO 1: 4 cicli multi-riga standard ---
' Verifica la corretta gestione dello stack dei cicli per una profondità media (4 livelli).
60 PRINT "--- Caso 1: 4 Cicli Multi-Riga Annidati ---"
70 FOR I = 1 TO 2
80   PRINT "Esterno I ="; I
90   FOR J = 1 TO 2
100    PRINT "  Medio-1 J ="; J
110    FOR K = 1 TO 2
120      PRINT "    Medio-2 K ="; K
130      FOR L = 1 TO 2
140        PRINT "      Interno L ="; L
150      NEXT L
160    NEXT K
170  NEXT J
180 NEXT I
190 PRINT

200 ' --- CASO 2: Ciclo esterno multi-riga, interni su riga singola con ':' ---
' Verifica che il parser gestisca correttamente la transizione tra blocchi multilinea e single-line.
210 PRINT "--- Caso 2: Esterno Multi-Riga, Interni Single-Line ---"
220 FOR I = 1 TO 2
230   PRINT "Esterno I ="; I
240   FOR J = 1 TO 2 : PRINT "  Medio J ="; J : FOR K = 1 TO 2 : PRINT "    Interno K ="; K : NEXT K : NEXT J
250 NEXT I
260 PRINT

270 ' --- CASO 3: Tutti i cicli su una singola riga logica ---
' Stress test per il parser: un'intera gerarchia di tre cicli compressa in una singola riga logica.
280 PRINT "--- Caso 3: Tutti i Cicli Single-Line ---"
290 FOR I = 1 TO 2 : PRINT "I=";I; : FOR J = 1 TO 2 : PRINT " J=";J; : FOR K = 1 TO 2 : PRINT " K=";K; : NEXT K : NEXT J : NEXT I
300 PRINT
310 PRINT

320 ' --- CASO 4: Mix di multi-riga e single-line con NEXT multipli ---
' Verifica il supporto per la sintassi `NEXT J, I` che chiude due cicli in un colpo solo.
330 PRINT "--- Caso 4: Mix con NEXT Multipli ---"
340 FOR I = 1 TO 2
350   PRINT "I="; I
360   FOR J = 1 TO 2
370     PRINT "  J="; J
380     FOR K = 1 TO 2 : PRINT "    K="; K : NEXT K
390   NEXT J, I  ' Sintassi con NEXT multipli: chiude prima J, poi I
400 PRINT

410 ' --- CASO 5: Cicli profondi 5 livelli + escape multiplo ---
' Verifica il comportamento di un salto incondizionato (GOTO) fuori da cicli profondamente annidati.
' In un sistema appiattito, questo è un semplice salto a un'etichetta/linea (540).
415 PRINT "--- Caso 5: 5 Livelli, NEXT multipli dentro IF e salti ---"
420 FOR A = 1 TO 2
430   PRINT "A=";A
440   FOR B = 1 TO 2
450     PRINT " B=";B
460     FOR C = 1 TO 2
470       PRINT "  C=";C
480       FOR D = 1 TO 2
490         PRINT "   D=";D
500         FOR E = 1 TO 2
510           PRINT "    E=";E
520           IF E=2 AND D=2 THEN PRINT "    Salto D,C!": GOTO 540 ' Esce dai cicli E, D, C, B e va al NEXT A
530         NEXT E
535       NEXT D
537     NEXT C
539   NEXT B
540 NEXT A
550 PRINT

560 ' --- CASO 6: Cicli single-line e NEXT multipli inversi/anomali ---
' Verifica NEXT multipli su una riga singola.
570 PRINT "--- Caso 6: Single-line e NEXT multipli inversi ---"
580 FOR X = 1 TO 2: FOR Y = 1 TO 2: PRINT "X=";X;" Y=";Y;: NEXT Y, X
590 PRINT

600 ' --- CASO 7: CORRETTO - Usa EXIT FOR invece di GOTO ---
' Verifica l'uso strutturato di EXIT FOR per uscire da un ciclo specifico.
610 PRINT "--- Caso 7: Salto strutturato con EXIT FOR ---"
620 FOR M = 1 TO 2
630   FOR N = 1 TO 2
640     PRINT "M=";M;" N=";N
650     IF N=2 THEN
660       PRINT "  Uscita dal ciclo N con EXIT FOR"
670       EXIT FOR ' Esce solo dal ciclo interno (N)
680     END IF
690   NEXT N
700   PRINT "  Fine del corpo del ciclo M (dopo il ciclo N)"
710 NEXT M
720 PRINT

730 PRINT "Test completato."
