' FILE: Console/ConsoleBasics.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE BASE DELLA CONSOLE (TESTO)
' =================================================================================================
'
' Questo test copre i comandi fondamentali per la manipolazione dell'output testuale su console.
' GM-BASIC emula un terminale standard (stile DOS/VGA) con supporto per colori e posizionamento.
'
' 1. PULIZIA SCHERMO (`CLS`)
'    Il comando `CLS` (Clear Screen) cancella tutto il testo visibile e reimposta il cursore
'    alla posizione (1, 1). Se è stato impostato un colore di sfondo, `CLS` riempie lo schermo
'    con quel colore.
'
' 2. POSIZIONAMENTO CURSORE (`LOCATE`)
'    - Sintassi: `LOCATE riga, colonna`
'    - Sposta il cursore di stampa alla posizione specificata (1-based).
'    - Le stampe successive (`PRINT`) inizieranno da quella posizione.
'
' 3. GESTIONE COLORI (`COLOR`)
'    GM-BASIC supporta i 16 colori standard (0-15).
'    - `COLOR fg`: Imposta solo il colore del testo (foreground).
'    - `COLOR fg, bg`: Imposta testo e sfondo.
'    - `COLOR , bg`: Imposta solo lo sfondo (mantiene il testo invariato).
'    - `COLOR`: (Senza argomenti) Ripristina i colori di default del sistema.
'
' =================================================================================================

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ ConsoleBasics - CONSOLE CLS/LOCATE/COLOR                                 ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 REM === TEST COMPLETO CLS, LOCATE, COLOR ===
30
40 REM Imposta colore testo a Giallo (14) e sfondo a Blu scuro (1)
50 COLOR 14, 1
60 LOCATE 5, 10
70 PRINT "Ciao, mondo colorato!"
80
90 REM Sposta il cursore e cambia solo il colore del testo a Rosso (12)
100 LOCATE 7, 5
110 COLOR 12
120 PRINT "Questo testo è rosso su sfondo blu scuro."
130
140 REM Sposta il cursore e cambia solo il colore dello sfondo a Grigio (7)
' Nota la virgola iniziale: salta il parametro foreground per cambiare solo il background.
150 LOCATE 9, 15
160 COLOR , 7
170 PRINT "Testo rosso su sfondo grigio."
180
190 REM Usa un colore non valido per testare la gestione errori (opzionale)
200 REM COLOR 20
210
220 REM Ripristino colori di default (nessun argomento) e fine
230 LOCATE 12, 1
240 COLOR
250 PRINT "Colori ripristinati."
260 PRINT "SCREENS="; SCREENS
270 PRINT "CELLSTR(1,1)='"; CELLSTR(1,1); "'"
300 PRINT "TEXTWIDTH('ABC')>0="; TEXTWIDTH("ABC") > 0
310 PRINT "TEXTHEIGHT>0="; TEXTHEIGHT > 0
320 PRINT "Test terminato."
330 END