' FILE: Strings/StringsWrap.bas
' ======================================================================================================================
' GUIDA TEORICA: FUNZIONE DI WRAPPING AVANZATO DEL TESTO (`WRAP`)
' ======================================================================================================================
' La funzione `WRAP` del namespace `STRINGS` è uno strumento potente per la formattazione di testi lunghi,
' consentendo di dividere una stringa in più righe secondo una larghezza massima e con diverse modalità di
' allineamento.
'
' 1. SINTASSI
'    - `WRAP(testo$, larghezza, [allineamento])`
'
' 2. PARAMETRI
'    - `testo$`: La stringa di input da formattare.
'    - `larghezza`: La larghezza massima (in caratteri) di ogni riga di output.
'    - `allineamento` (Opzionale): Un numero che specifica la modalità di allineamento del testo:
'      - `0` (Default): Allineamento a sinistra (testo "a bandiera").
'      - `1`: Allineamento a destra.
'      - `2`: Allineamento giustificato (il testo viene distribuito per toccare sia il margine sinistro
'        che quello destro, aggiungendo spazi extra tra le parole).
'
' 3. COMPORTAMENTO
'    - Divisione delle Parole: La funzione cerca di non spezzare le parole. Va a capo allo spazio più vicino
'      che non superi la larghezza massima.
'    - Gestione Parole Lunghe: Se una singola parola è più lunga della larghezza specificata, la funzione la
'      spezza forzatamente per rispettare il limite.
'
' 4. UTILIZZO
'    - È ideale per formattare paragrafi di testo, creare output per la console o per file di testo che
'      siano leggibili e ben impaginati.
' ======================================================================================================================

' Importa il namespace STRINGS per accedere alla funzione WRAP.
IMPORT STRINGS

10 CLS
20 PRINT "╔══════════════════════════════════════════════════════════════╗"
30 PRINT "║ StringsWrap - Avanzato Funzione WRAP                         ║"
40 PRINT "╚══════════════════════════════════════════════════════════════╝"
50 PRINT
100 REM --- SETUP DEL TEST ---
' Definisce il testo di prova lungo.
110 LOREM$ = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
' Definisce la larghezza massima per il wrapping.
120 W = 50
' Definisce un "righello" per visualizzare la larghezza.
130 RULER$ = "|<---- 10 ---->|<---- 20 ---->|<---- 30 ---->|<---- 40 ---->|<---- 50 ---->|"
140 PRINT "Testo originale:"
150 PRINT LOREM$
160 PRINT "Larghezza di wrapping:"; W
170 PRINT

200 REM --- TEST ALLINEAMENTO SINISTRA ---
210 PRINT "--- WRAP con Allineamento a Sinistra (0, default) ---"
220 PRINT RULER$
' Chiama WRAP con allineamento 0 (sinistra).
230 PRINT WRAP(LOREM$, W, 0)
240 PRINT RULER$
250 PRINT

300 REM --- TEST ALLINEAMENTO DESTRA ---
310 PRINT "--- WRAP con Allineamento a Destra (1) ---"
320 PRINT RULER$
' Chiama WRAP con allineamento 1 (destra).
330 PRINT WRAP(LOREM$, W, 1)
340 PRINT RULER$
350 PRINT

400 REM --- TEST ALLINEAMENTO GIUSTIFICATO ---
410 PRINT "--- WRAP con Allineamento Giustificato (2) ---"
420 PRINT RULER$
' Chiama WRAP con allineamento 2 (giustificato).
430 PRINT WRAP(LOREM$, W, 2)
440 PRINT RULER$
450 PRINT

500 REM --- TEST CON PAROLA LUNGA ---
510 PRINT "--- WRAP con Parola Più Lunga della Larghezza ---"
' Definisce un testo con una parola che supera la larghezza di 50 caratteri.
520 LONGWORD$ = "Questo test include una parola Anticonstituzionalissimamente che è volutamente e significativamente più lunga della larghezza massima di 50 caratteri."
530 PRINT RULER$
' Verifica che la funzione spezzi correttamente la parola lunga.
540 PRINT WRAP(LONGWORD$, W, 0)
550 PRINT RULER$
560 PRINT

600 PRINT "--- FINE TEST ---"
610 END