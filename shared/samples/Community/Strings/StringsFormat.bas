' FILE: Strings/StringsFormat.bas
' ======================================================================================================================
' GUIDA TEORICA: FUNZIONE DI FORMATTAZIONE AVANZATA (`FORMAT`)
' ======================================================================================================================
' GM-BASIC include una potente funzione `FORMAT` per convertire numeri, date e valori booleani in stringhe
' personalizzate, sfruttando internamente le stringhe di formato standard di .NET per massima flessibilità.
'
' 1. SINTASSI
'    - `FORMAT(valore, formato$)`
'
' 2. FORMATTAZIONE NUMERICA
'    - `0`: Segnaposto per una cifra. Se la cifra non esiste, viene visualizzato uno `0`. (Padding con zeri)
'    - `#`: Segnaposto per una cifra. Se la cifra non esiste, non viene visualizzato nulla.
'    - `.`: Separatore decimale.
'    - `,`: Separatore delle migliaia.
'    - `%`: Moltiplica il numero per 100 e aggiunge il simbolo `%`.
'    - `E+00`, `E-00`: Formattazione scientifica (esponenziale).
'    - `\`: Carattere di escape. Permette di inserire un carattere speciale come letterale (es. `\#` per il simbolo `#`).
'    - `+`: Mostra sempre il segno (+ o -) del numero.
'
' 3. FORMATTAZIONE DATA/ORA
'    - `d`, `dd`: Giorno del mese (es. `5`, `05`).
'    - `ddd`, `dddd`: Nome del giorno abbreviato/completo (es. `Mon`, `Monday`).
'    - `m`, `mm`: Minuto.
'    - `M`, `MM`: Mese (es. `3`, `03`).
'    - `MMM`, `MMMM`: Nome del mese abbreviato/completo (es. `Mar`, `March`).
'    - I token sono CASE-SENSITIVE (`M` = mese, `m` = minuti). I nomi di giorni e mesi sono in lingua
'      INVARIANTE (inglese) su tutti i backend: l'output non dipende dalla lingua del sistema.
'    - `yy`, `yyyy`: Anno a 2/4 cifre.
'    - `h`, `hh`: Ora (formato 12 ore).
'    - `H`, `HH`: Ora (formato 24 ore).
'    - `n`, `nn`: Minuto (sinonimo di `m`).
'    - `s`, `ss`: Secondo.
'    - `AM/PM`: Designatore AM/PM.
'    - `'...'`, `"..."`: Qualsiasi testo tra apici singoli o doppi viene inserito come letterale. Per inserire un
'      apice, usare `''`.
'
' 4. FORMATTAZIONE BOOLEANA (TIPO `BIT`)
'    - La stringa di formato è divisa in due parti da un punto e virgola `;`.
'    - `FORMAT(valore_booleano, "parte_vera;parte_falsa")`
'    - Se il valore è TRUE (-1 o non-zero), viene usata la prima parte.
'    - Se il valore è FALSE (0), viene usata la seconda parte.
' ======================================================================================================================

' Importa le funzioni di data/ora (DATEVALUE, TIMESERIAL)
IMPORT DATETIME

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ StringsFormat - Funzione FORMAT                                         ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
20 PRINT "--- 1. Formattazione Numerica (Base) ---"
30 N = 1234 : D = -5678.9876 : C = 1234567.89
' Padding con zeri a sinistra
40 PRINT "Numero:"; N; " | Formato: '000000' | Risultato: '"; FORMAT(N, "000000"); "'"
' Segno, separatore migliaia, due decimali
50 PRINT "Numero:"; D; " | Formato: '+##,##0.00' | Risultato: '"; FORMAT(D, "+##,##0.00"); "'"
' Carattere letterale (€) e precisione a 4 decimali
60 PRINT "Numero:"; C; " | Formato: '\€ #,##0.0000' | Risultato: '"; FORMAT(C, "\€ #,##0.0000"); "'"
' Formattazione percentuale
70 PRINT "Numero:"; 0.123; " | Formato: '0.00%' | Risultato: '"; FORMAT(0.123, "0.00%"); "'"
80 PRINT

100 PRINT "--- 2. Formattazione Data/Ora (Base) ---"
110 GIORNO = DATEVALUE("2025-12-25") : ORA = TIMESERIAL(22, 5, 30)
' Giorno esteso, giorno, mese esteso, anno
120 PRINT "Data:"; GIORNO; " | Formato: 'dddd, d MMMM yyyy' | Risultato: '"; FORMAT(GIORNO, "dddd, d MMMM yyyy"); "'"
' Ora a 12 ore con AM/PM
130 PRINT "Ora:"; ORA; " | Formato: 'hh:mm:ss tt' | Risultato: '"; FORMAT(ORA, "hh:mm:ss tt"); "'"
140 PRINT

200 PRINT "--- 3. Formattazione Booleana (Base) ---"
210 VERO = -1 : FALSO = 0
220 PRINT "Valore:"; VERO; " | Formato: 'Yes;No' | Risultato: '"; FORMAT(VERO, "Yes;No"); "'"
230 PRINT "Valore:"; FALSO; " | Formato: 'ON;OFF' | Risultato: '"; FORMAT(FALSO, "ON;OFF"); "'"
240 PRINT "Valore:"; VERO; " | Formato: '✓;✗' | Risultato: '"; FORMAT(VERO, "✓;✗"); "'"
250 PRINT

300 PRINT "--- 4. Casi Numerici Complessi e Limite ---"
310 PRINT "Padding '0':"; 99; " | '00000' | '"; FORMAT(99, "00000"); "'"
320 PRINT "Padding '#':"; 99; " | '#####' | '"; FORMAT(99, "#####"); "'"
330 PRINT "Notazione Scientifica:"; 0.000123; " | '0.00E+00' | '"; FORMAT(0.000123, "0.00E+00"); "'"
340 PRINT "Notazione Scientifica:"; 12345678; " | '#.0E+0' | '"; FORMAT(12345678, "#.0E+0"); "'"
' Inserisce il simbolo # come carattere letterale usando \
350 PRINT "Literals & Escape:"; 42; " | 'Numero \##0' | '"; FORMAT(42, "Numero \##0"); "'"
360 PRINT "Segno Positivo:"; 123; " | '+0000' | '"; FORMAT(123, "+0000"); "'"
370 PRINT "Migliaia con numero corto:"; 1234; " | '#,##,##0' | '"; FORMAT(1234, "#,##,##0"); "'"
380 PRINT

400 PRINT "--- 5. Casi Data/Ora Complessi e Limite ---"
410 MEZZANOTTE = TIMESERIAL(0, 0, 1) : MEZZOGIORNO = TIMESERIAL(12, 0, 0)
420 PRINT "Mezzanotte:"; MEZZANOTTE; " | 'h:mm:ss tt' | '"; FORMAT(MEZZANOTTE, "h:mm:ss tt"); "'"
430 PRINT "Mezzogiorno:"; MEZZOGIORNO; " | 'h:mm tt' | '"; FORMAT(MEZZOGIORNO, "h:mm tt"); "'"
' Testo letterale tra apici singoli
440 PRINT "Literals in Data:"; GIORNO; " | 'ddd d MMM, yyyy ''(giorno dell''''anno)''' | '"; FORMAT(GIORNO, "ddd d MMM, yyyy '(giorno dell''anno)'"); "'"
450 PRINT

500 PRINT "--- 6. Casi Booleani Complessi ---"
' Se la parte 'false' manca, non viene stampato nulla
510 PRINT "Formato incompleto (Falso):"; FALSO; " | 'Vero;' | '"; FORMAT(FALSO, "Vero;"); "'"
' Gli spazi nel formato vengono rispettati
520 PRINT "Formato con spazi:"; VERO; " | // Status: OK ; Status: KO // | '"; FORMAT(VERO, " Status: OK ; Status: KO "); "'"
530 PRINT

600 PRINT "--- Test completato ---"
610 END
