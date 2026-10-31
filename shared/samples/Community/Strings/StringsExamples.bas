' FILE: Strings/StringsExamples.bas
' ======================================================================================================================
' GUIDA TEORICA: MANIPOLAZIONE AVANZATA DI STRINGHE
' ======================================================================================================================
' Questo programma è un test di regressione completo per le funzioni di manipolazione di stringhe in GM-BASIC.
' Copre sia le funzioni classiche (ereditate da GW-BASIC/QBasic) sia le estensioni moderne fornite dal
' namespace `STRINGS`.
'
' 1. FUNZIONI FONDAMENTALI (Namespace SYSTEM, Globali)
'    - `LEN(s$)`: Restituisce la lunghezza di una stringa.
'    - `ASC(s$)`: Restituisce il codice ASCII del primo carattere.
'    - `VAL(s$)`: Converte l'inizio numerico di una stringa in un valore `DOUBLE`.
'    - `STR$(n)`: Converte un numero in una stringa, aggiungendo uno spazio iniziale per i numeri positivi.
'    - `CHR$(n)`: Restituisce il carattere corrispondente a un codice ASCII.
'    - `LEFT$(s$, n)`, `RIGHT$(s$, n)`, `MID$(s$, start, [len])`: Funzioni classiche per l'estrazione di sottostringhe.
'    - `INSTR([start,] s$, sub$)`: Cerca una sottostringa, restituendo la posizione (1-based) o 0.
'    - `SPACE$(n)`: Crea una stringa di `n` spazi.
'
' 2. FUNZIONI MODERNE (Namespace STRINGS)
'    Queste funzioni offrono una maggiore potenza e coerenza rispetto alle loro controparti classiche.
'
'    - Conversione di Case:
'      - `LOWER`, `UPPER`: Conversione standard.
'      - `PASCAL`, `CAMEL`, `SNAKE`, `KEBAB`, `TITLE`, `SENTENCE`: Utilità per la formattazione del codice e del testo.
'
'    - Pulizia e Trasformazione:
'      - `TRIM`, `LTRIM`, `RTRIM`: Rimozione degli spazi bianchi.
'      - `REVERSE(s$)`: Inverte l'ordine dei caratteri.
'      - `REPLACE(s$, find$, replace$)`: Sostituisce tutte le occorrenze di una sottostringa.
'      - `REMOVE(s$, remove$)`: Rimuove tutte le occorrenze di una sottostringa.
'
'    - Generazione e Formattazione:
'      - `REPEAT(s$, count, [sep$])`: Ripete una stringa più volte, opzionalmente con un separatore.
'      - `PADL`, `PADR`, `PADC`: Aggiungono padding a una stringa per raggiungere una lunghezza fissa.
'      - `WRAP(s$, width, [align])`: Suddivide una stringa lunga in più righe con allineamento.
'
'    - Analisi e Decomposizione:
'      - `SEARCH(s$, sub$)`: Versione booleana di `INSTR` (restituisce TRUE/FALSE).
'      - `COUNT(s$, sub$)`: Conta le occorrenze di una sottostringa.
'      - `SPLIT(s$, sep$)`: Suddivide una stringa in un array di stringhe.
'      - `JOIN(array$(), sep$)`: Unisce un array di stringhe in una singola stringa.
'
' 3. FUNZIONI DI I/O (Namespace CONSOLE)
'    - `TAB(n)`: Utilizzata all'interno di un'istruzione `PRINT`, sposta il cursore alla colonna specificata.
' ======================================================================================================================

' Importa le funzioni di stringa moderne (LOWER, UPPER, REPLACE, etc.)
IMPORT STRINGS

' Importa le funzioni di conversione (HEX, OCT, BIN)
IMPORT CONVERSIONS

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ StringsExamples - Funzioni Stringa                                       ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 REM === TEST COMPLETO FUNZIONI STRINGA (CORRETTO ED ESTESO) ===
30 PRINT "--- INIZIO TEST FUNZIONI STRINGA ---"
40 PRINT
50 
60 REM --- TEST: LEN ---
70 PRINT "1. TEST LEN(stringa)"
80 A$ = "CIAO MONDO"
90 PRINT "La stringa è: '"; A$; "'"
100 PRINT "LEN(A$) = "; LEN(A$); " (Atteso: 10)"
110 PRINT

130 REM --- TEST: ASC ---
140 PRINT "2. TEST ASC(stringa)"
150 B$ = "Apple"
160 PRINT "La stringa è: '"; B$; "'"
170 PRINT "ASC(B$) = "; ASC(B$); " (Atteso: 65)"
180 PRINT

200 REM --- TEST: VAL ---
210 PRINT "3. TEST VAL(stringa)"
220 D$ = "-123.45 Prova"
230 PRINT "La stringa è: '"; D$; "'"
240 PRINT "VAL(D$) = "; VAL(D$); " (Atteso: -123.45)"
250 PRINT

270 REM --- TEST: STR ---
280 PRINT "4. TEST STR(numero)"
290 N1 = -42.5
300 N2 = 123
310 PRINT "STR(-42.5) = '"; STR(N1); "'"; " (Atteso: '-42.5')"
320 PRINT "STR(123) = '"; STR(N2); "'"; " (Atteso: ' 123')"
330 PRINT

350 REM --- TEST: CHR ---
360 PRINT "5. TEST CHR(codice_ascii)"
370 PRINT "CHR(66) = '"; CHR(66); "'"; " (Atteso: 'B')"
380 PRINT

400 REM --- TEST: LEFT, RIGHT, MID ---
410 S$ = "PROGRAMMAZIONE"
420 PRINT "S$ = '"; S$; "'"
430 PRINT "LEFT(S$, 7) = '"; LEFT(S$, 7); "'"; " (Atteso: 'PROGRAM')"
440 PRINT "RIGHT(S$, 5) = '"; RIGHT(S$, 5); "'"; " (Atteso: 'ZIONE')"
450 PRINT "MID(S$, 8, 4) = '"; MID(S$, 8, 4); "'"; " (Atteso: 'MAZI')"
460 PRINT "MID(S$, 3) = '"; MID(S$, 3); "'"; " (Atteso: 'OGRAMMAZIONE')"
470 PRINT

490 REM --- TEST: INSTR ---
500 PRINT "6. TEST INSTR(stringa, sottostringa)"
510 PRINT "S$ = '"; S$; "'"
520 PRINT "INSTR(S$, ""MMA"") = "; INSTR(S$, "MMA"); " (Atteso: 7)"
530 PRINT "INSTR(S$, ""XYZ"") = "; INSTR(S$, "XYZ"); " (Atteso: 0)"
540 PRINT "INSTR(S$, ""A"", 7) = "; INSTR(S$, "A", 7); " (Atteso: 9)"
550 PRINT

570 REM --- TEST: DUP, SPACE ---
580 PRINT "7. TEST DUP E SPACE"
590 PRINT "DUP(10, 42) = '"; DUP(10, 42); "'"; " (Atteso: '**********')"
600 PRINT "SPACE(15) = '"; SPACE(15); "'"; " (Atteso: 15 spazi)"
610 PRINT

630 REM --- TEST: CONVERSIONE DI BASE ---
640 PRINT "8. TEST HEX, OCT, BIN"
650 NUM_CONV = 255
660 PRINT "NUM_CONV ="; NUM_CONV
670 PRINT "HEX(NUM_CONV) = '"; HEX(NUM_CONV); "'"; " (Atteso: 'FF')"
680 PRINT "OCT(NUM_CONV) = '"; OCT(NUM_CONV); "'"; " (Atteso: '377')"
690 PRINT "BIN(NUM_CONV) = '"; BIN(NUM_CONV); "'"; " (Atteso: '11111111')"
700 PRINT

720 REM --- TEST: CONVERSIONE CASE (Namespace STRINGS) ---
730 PRINT "9. TEST CONVERSIONE CASE"
740 T$ = "ciao MONDO from Basic"
750 PRINT "T$ = '"; T$; "'"
760 PRINT "LOWER(T$) = '"; LOWER(T$); "'"
770 PRINT "UPPER(T$) = '"; UPPER(T$); "'"
780 PRINT "PASCAL(T$) = '"; PASCAL(T$); "'"; " (Atteso: CiaoMondoFromBasic)"
790 PRINT "CAMEL(T$) = '"; CAMEL(T$); "'"; " (Atteso: ciaoMondoFromBasic)"
800 PRINT "SNAKE(T$) = '"; SNAKE(T$); "'"; " (Atteso: ciao_mondo_from_basic_v10)"
810 PRINT "KEBAB(T$) = '"; KEBAB(T$); "'"; " (Atteso: ciao-mondo-from-basic-v10)"
820 PRINT "TITLE(T$) = '"; TITLE(T$); "'"; " (Atteso: Ciao Mondo From Basic)"
830 PRINT "SENTENCE(T$) = '"; SENTENCE(T$); "'"; " (Atteso: Ciao mondo from basic v10)"
840 PRINT

860 REM --- TEST: TRIM, LTRIM, RTRIM, REVERSE (Namespace STRINGS) ---
870 PRINT "10. TEST TRIM, LTRIM, RTRIM, REVERSE"
880 SP$ = "  SPAZI  "
890 PRINT "SP$ = '"; SP$; "'"
900 PRINT "TRIM(SP$) = '"; TRIM(SP$); "'"
910 PRINT "LTRIM(SP$) = '"; LTRIM(SP$); "'"
920 PRINT "RTRIM(SP$) = '"; RTRIM(SP$); "'"
930 REV$ = "12345"
940 PRINT "REV$ = '"; REV$; "'"
950 PRINT "REVERSE(REV$) = '"; REVERSE(REV$); "'"
960 PRINT

980 REM --- TEST: REPLACE, REMOVE, REPEAT (Namespace STRINGS) ---
990 PRINT "11. TEST REPLACE, REMOVE, REPEAT"
1000 REP$ = "banana"
1010 PRINT "REP$ = '"; REP$; "'"
1020 PRINT "REPLACE(REP$, ""an"", ""XX"") = '"; REPLACE(REP$, "an", "XX"); "'"; " (Atteso: 'bXXXXa')"
1030 PRINT "REMOVE(REP$, ""a"") = '"; REMOVE(REP$, "a"); "'"; " (Atteso: 'bnn')"
1040 PRINT "REPEAT(""-*"", 5) = '"; REPEAT("-*", 5); "'"; " (Atteso: '-*-*-*-*-*')"
1050 PRINT "REPEAT(""A"", 4, "","") = '"; REPEAT("A", 4, ","); "'"; " (Atteso: 'A,A,A,A')"
1060 PRINT

1080 REM --- TEST: PADDING (Namespace STRINGS) ---
1090 PRINT "12. TEST PADL, PADR, PADC"
1100 PAD$ = "CIAO"
1110 PRINT "PAD$ = '"; PAD$; "'"
1120 PRINT "PADL(PAD$, 10, ""-"") = '"; PADL(PAD$, 10, "-"); "'"; " (Atteso: '------CIAO')"
1130 PRINT "PADR(PAD$, 10, ""-"") = '"; PADR(PAD$, 10, "-"); "'"; " (Atteso: 'CIAO------')"
1140 PRINT "PADC(PAD$, 10, ""-"") = '"; PADC(PAD$, 10, "-"); "'"; " (Atteso: '---CIAO---')"
1150 PRINT

1170 REM --- TEST: SEARCH, COUNT (Namespace STRINGS) ---
1180 PRINT "13. TEST SEARCH, COUNT"
1190 SC$ = "La casa bella è bella assai"
1200 PRINT "SC$ = '"; SC$; "'"
1210 PRINT "SEARCH(SC$, ""bella"") = "; SEARCH(SC$, "bella"); " (Atteso: TRUE)"
1220 PRINT "SEARCH(SC$, ""brutta"") = "; SEARCH(SC$, "brutta"); " (Atteso: FALSE)"
1230 PRINT "COUNT(SC$, ""a"") = "; COUNT(SC$, "a"); " (Atteso: 7)"
1240 PRINT

1260 REM --- TEST: SPLIT, JOIN (Namespace STRINGS) ---
1270 PRINT "14. TEST SPLIT, JOIN"
1280 DIM LIST$(5)
1290 S_IN$ = "UNO,DUE,TRE"
1300 PRINT "S_IN$ = '"; S_IN$; "'"
' SPLIT restituisce un array, che viene assegnato a LIST$
1310 LIST$() = SPLIT(S_IN$, ",")
1315 LIST$ = SPLIT(S_IN$, ",") 'anche così dovrebbe funzionare
1320 PRINT "Elementi splittati:"
' UBOUND restituisce l'indice massimo dell'array
1330 FOR I = 0 TO UBOUND(LIST$)
1340     PRINT "LIST$(";I;") = '"; LIST$(I); "'"
1350 NEXT I
' JOIN unisce gli elementi dell'array con un separatore
1360 S_OUT$ = JOIN(LIST$, "->")
1370 PRINT "JOIN(LIST$, ""->"") = '"; S_OUT$; "'"; " (Atteso: 'UNO->DUE->TRE')"
1380 PRINT

1400 REM --- TEST: WRAP (Namespace STRINGS) ---
1410 PRINT "15. TEST WRAP"
1420 WRAP_S$ = "Questa è una stringa molto lunga che verrà divisa in piu' righe."
1430 PRINT "WRAP_S$ = '"; WRAP_S$; "'"
1440 PRINT "WRAP(WRAP_S$, 20):"
' WRAP formatta il testo per andare a capo
1450 PRINT WRAP(WRAP_S$, 20)
1460 PRINT

1480 REM --- TEST: CONTAINS, STARTSWITH, ENDSWITH (Globali) ---
1490 PRINT "16. TEST CONTAINS, STARTSWITH, ENDSWITH"
1500 S1$ = "Ciao Mondo"
1510 PRINT "S1$ = '"; S1$; "'"
1520 PRINT "CONTAINS(S1$, 'Mondo') = "; CONTAINS(S1$, "Mondo"); " (Atteso: -1)"
1530 PRINT "CONTAINS(S1$, 'mondo') = "; CONTAINS(S1$, "mondo"); " (Atteso: 0)"
1540 PRINT "CONTAINS(S1$, 'Ciao') = "; CONTAINS(S1$, "Ciao"); " (Atteso: -1)"
1550 PRINT "STARTSWITH(S1$, 'Ciao') = "; STARTSWITH(S1$, "Ciao"); " (Atteso: -1)"
1560 PRINT "STARTSWITH(S1$, 'Mondo') = "; STARTSWITH(S1$, "Mondo"); " (Atteso: 0)"
1570 PRINT "STARTSWITH(S1$, '') = "; STARTSWITH(S1$, ""); " (Atteso: -1)"
1580 PRINT "ENDSWITH(S1$, 'Mondo') = "; ENDSWITH(S1$, "Mondo"); " (Atteso: -1)"
1590 PRINT "ENDSWITH(S1$, 'Ciao') = "; ENDSWITH(S1$, "Ciao"); " (Atteso: 0)"
1600 PRINT "ENDSWITH(S1$, '') = "; ENDSWITH(S1$, ""); " (Atteso: -1)"
1610 PRINT

1620 REM --- TEST: TAB in PRINT ---
1630 PRINT "17. TEST TAB"
1640 PRINT "Col:     12345678901234567890"
' TAB sposta il cursore alla colonna specificata
1650 PRINT "Output:  A"; TAB(5); "B"; TAB(10); "C"
1660 PRINT "(Atteso in colonna 1, 5, 10)"
1670 PRINT
1680
1690 PRINT "--- 18. TEST TRANSFORM, EXTRACT, STRINGIFY ---"
1700 PRINT "STRINGIFY test:"; LEN(STRINGIFY) >= 0
1710 PRINT "PETSCII('HELLO')="; PETSCII("HELLO")
1730 PRINT
1740 PRINT "--- FINE TEST ---"
1750 END
