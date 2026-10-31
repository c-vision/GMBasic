' FILE: Types/TypesConversions.bas
' ======================================================================================================================
' GUIDA TEORICA: TIPI DI DATO NUMERICI, PROMOZIONE E LIBRERIA MATEMATICA
' ======================================================================================================================
' Questo programma testa in modo approfondito la gestione dei tipi numerici in GM-BASIC, la promozione automatica
' dei tipi nelle espressioni e l'uso delle librerie matematiche e di conversione.
'
' 1. TIPI DI DATO ESPLICITI
'    - `SINGLE`: Virgola mobile a precisione singola (32-bit). Veloce ma meno preciso.
'    - `DOUBLE`: Virgola mobile a doppia precisione (64-bit). È il tipo numerico di default.
'    - `CURRENCY`: Tipo a virgola fissa (64-bit) con 4 cifre decimali fisse. Ideale per calcoli finanziari
'      per evitare gli errori di arrotondamento tipici dei tipi a virgola mobile.
'
' 2. FUNZIONI DI CONVERSIONE (Namespace CONVERSIONS)
'    - Le funzioni `Cxxx` (es. `CSNG`, `CDBL`, `CINT`, `CLNG`) forzano la conversione di un'espressione a un
'      tipo di dato specifico, eseguendo un arrotondamento standard (arrotondamento del banchiere o "round half to even").
'    - `FIX(n)`: Tronca la parte decimale di un numero, restituendo la parte intera.
'    - `FLOOR(n)`: Restituisce l'intero più grande minore o uguale a `n` (arrotonda verso -infinito).
'
' 3. PROMOZIONE AUTOMATICA DEI TIPI
'    - Quando un'espressione aritmetica coinvolge tipi numerici diversi, GM-BASIC promuove automaticamente
'      il risultato al tipo più "ampio" o "preciso" per evitare perdita di dati.
'    - Gerarchia di Promozione (dalla più bassa alla più alta):
'      `BIT -> BYTE -> INTEGER -> LONG -> QUAD -> SINGLE -> DOUBLE -> CURRENCY`
'    - Esempio: `SINGLE + DOUBLE` risulta in un `DOUBLE`. `DOUBLE + CURRENCY` risulta in `CURRENCY`.
'
' 4. LIBRERIA MATEMATICA (Namespace MATH)
'    - GM-BASIC fornisce una libreria matematica completa accessibile tramite il namespace `MATH` (importato di default).
'    - Funzioni Trigonometriche: `SIN`, `COS`, `TAN`, `ATN` (arcotangente), `ASIN`, `ACOS`.
'    - Costanti: `PI()`.
'    - Funzioni Logaritmiche/Esponenziali: `LOG` (logaritmo naturale), `LOG10`, `EXP`.
'    - Funzioni di Utilità: `MIN`, `MAX`, `CLAMP` (limita un valore a un intervallo).
'    - Funzioni Geometriche: `HYPOT`, `DIST2D`, `DIST3D` per calcoli di distanze.
'    - Funzioni Combinatorie: `FACT` (fattoriale), `PERM` (permutazioni), `COMB` (combinazioni).
'
' 5. NUMERI CASUALI
'    - `RND()`: Restituisce un numero casuale tra 0 (incluso) e 1 (escluso).
'    - `RANDOMIZE <seed>`: Inizializza il generatore di numeri casuali con un "seme". Usando lo stesso seme,
'      la sequenza di numeri generata da `RND()` sarà sempre la stessa, rendendo i test riproducibili.
' ======================================================================================================================

' Importa esplicitamente i namespace, anche se MATH è di default.
IMPORT CONVERSIONS
IMPORT MATH

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ TypesConversions - Conversioni e Funzioni Aritmetiche                   ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
20 PRINT "--- 1. Dichiarazione Tipi Espliciti (SINGLE, DOUBLE, CURRENCY) ---"
30 DIM S AS SINGLE
40 DIM D AS DOUBLE
50 DIM C AS CURRENCY
60 PRINT "Valori di default: S="; S; ", D="; D; ", C="; C; " (Atteso: 0, 0, 0)"
70 S = 1.234567 : D = 1.23456789012345 : C = 1234.5678
80 PRINT "Valori assegnati: S="; S; ", D="; D; ", C="; C
90 PRINT "(Nota: la precisione di SINGLE è limitata e il valore viene arrotondato)"
100 PRINT

110 PRINT "--- 2. Test Funzioni di Conversione (Cxxx) ---"
120 PRINT "CSNG(1.23456789) =", CSNG(1.23456789)
130 PRINT "CDBL(S) =", CDBL(S)
140 PRINT "CCUR(987.654321) =", CCUR(987.654321)
150 PRINT "CSTR(S) = '"; CSTR(S); "'"
160 PRINT
170 PRINT "--- Arrotondamento e Conversione a Interi ---"
180 PRINT "CINT(12345.6) =", CINT(12345.6); "(Atteso: 12346, arrotondamento standard)"
190 PRINT "CLNG(987654.3) =", CLNG(987654.3); "(Atteso: 987654, arrotondamento standard)"
200 PRINT

210 PRINT "--- 3. Test VAL, FIX, FLOOR (INT) ---"
220 DIM S$ AS STRING
230 S$ = "  -123.45xyz"
240 PRINT "VAL('"; S$; "') =", VAL(S$); "(Atteso: -123.45)"
250 PRINT
260 PRINT "Confronto su 3.7:  FIX="; FIX(3.7); " | FLOOR="; FLOOR(3.7)
270 PRINT "Confronto su -3.7: FIX="; FIX(-3.7); "| FLOOR="; FLOOR(-3.7); "(FIX tronca, FLOOR va verso -inf)"
280 PRINT

290 PRINT "--- 4. Test Promozione dei Tipi in Espressioni ---"
300 DIM S_PROMO AS SINGLE : S_PROMO = 100.5
310 DIM D_PROMO AS DOUBLE : D_PROMO = 200.00000005
320 DIM C_PROMO AS CURRENCY : C_PROMO = 300.1234
330 PRINT "Single + Double =", S_PROMO + D_PROMO; "(Risultato promosso a Double)"
340 PRINT "Double + Currency =", D_PROMO + C_PROMO; "(Risultato promosso a Currency)"
350 PRINT

400 PRINT "--- 5. Test Funzioni Trigonometriche e Costanti ---"
410 PRINT "PI =", PI()
420 VAL_PI = PI()
430 PRINT "SIN(PI()/2) =", SIN(VAL_PI / 2); "(Atteso: 1)"
440 PRINT "COS(PI()) =", COS(VAL_PI); "(Atteso: -1)"
450 PRINT "TAN(PI()/4) =", TAN(VAL_PI / 4); "(Atteso: 1)"
460 PRINT "ATN(1) =", ATN(1); "(Atteso: ~0.785)"
470 PRINT "ASIN(1) =", ASIN(1); "(Atteso: ~1.57)"
480 PRINT "ACOS(0) =", ACOS(0); "(Atteso: ~1.57)"
490 PRINT
500 PRINT "--- Conversione Gradi/Radianti ---"
510 PRINT "DEG2RAD(180) =", DEG2RAD(180); "(Atteso: ~3.14159)"
520 PRINT "RAD2DEG(PI()) =", RAD2DEG(VAL_PI); "(Atteso: 180)"
530 PRINT

600 PRINT "--- 6. Test Funzioni Logaritmiche ed Esponenziali ---"
610 PRINT "EXP(1) =", EXP(1); "(Atteso: ~2.718)"
620 PRINT "LOG(10) =", LOG(10); "(Atteso: ~2.302)"
630 PRINT "LOG10(100) =", LOG10(100); "(Atteso: 2)"
640 PRINT

700 PRINT "--- 7. Test Funzioni di Utilità (MIN, MAX, CLAMP) ---"
710 PRINT "MIN(10, 5, 20) =", MIN(10, 5, 20); "(Atteso: 5)"
720 PRINT "MAX(-5, -1, -10) =", MAX(-5, -1, -10); "(Atteso: -1)"
730 PRINT "CLAMP(15, 0, 10) =", CLAMP(15, 0, 10); "(Atteso: 10)"
740 PRINT "CLAMP(-5, 0, 10) =", CLAMP(-5, 0, 10); "(Atteso: 0)"
750 PRINT "CLAMP(5, 0, 10) =", CLAMP(5, 0, 10); "(Atteso: 5)"
760 PRINT

800 PRINT "--- 8. Test Funzioni Geometriche (Distanze) ---"
810 PRINT "HYPOT(3, 4) =", HYPOT(3, 4); "(Atteso: 5)"
820 PRINT "PYTHAGORAS(3, 4) =", PYTHAGORAS(3, 4); "(Atteso: 5)"
830 PRINT "DIST2D(0, 0, 3, 4) =", DIST2D(0, 0, 3, 4); "(Atteso: 5)"
840 PRINT "DIST3D(0, 0, 0, 1, 2, 2) =", DIST3D(0, 0, 0, 1, 2, 2); "(Atteso: 3)"
850 PRINT

900 PRINT "--- 9. Test Funzioni Polari e Angoli ---"
910 PRINT "POLAR_X(5, 60) =", POLAR_X(5, 60); "(Atteso: ~2.5)"
920 PRINT "POLAR_Y(5, 60) =", POLAR_Y(5, 60); "(Atteso: ~4.33)"
930 PRINT "ANGLE2D(0, 0, 1, 1) =", ANGLE2D(0, 0, 1, 1); "(Atteso: 45)"
940 PRINT

1000 PRINT "--- 10. Test Funzioni Combinatorie ---"
1010 PRINT "FACT(5) =", FACT(5); "(Atteso: 120)"
1020 PRINT "PERM(5, 2) =", PERM(5, 2); "(Atteso: 20)"
1030 PRINT "COMB(5, 2) =", COMB(5, 2); "(Atteso: 10)"
1040 PRINT

1100 PRINT "--- 11. Test Numeri Casuali ---"
1110 PRINT "Primo RND():", RND()
1120 RANDOMIZE 12345
1130 PRINT "RND() dopo RANDOMIZE 12345:", RND()
1140 PRINT "RND() successivo:", RND()
1150 RANDOMIZE 12345
1160 PRINT "RND() dopo un altro RANDOMIZE 12345:", RND(); "(Atteso stesso valore di prima)"
1170 PRINT

1200 PRINT "--- Test completato ---"
1210 END
