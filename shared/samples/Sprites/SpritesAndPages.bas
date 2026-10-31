' FILE: Sprites/SpritesAndPages.bas
' ======================================================================================================================
' GUIDA TEORICA: GESTIONE DELLA GRAFICA MULTI-PAGINA E SPRITE
' ======================================================================================================================
' Questo programma dimostra tecniche grafiche avanzate, inclusa la gestione delle "pagine" di memoria video
' (video pages) per creare effetti di "double buffering" e la manipolazione di sprite.
'
' 1. MODALITÀ GRAFICHE (`SCREEN`)
'    - `SCREEN <mode>`: Imposta la modalità grafica del programma. Questo comando apre una finestra e
'      inizializza la memoria video con una risoluzione e profondità di colore specifiche.
'      - `SCREEN 12`: Modalità VGA standard (640x480 pixel, 16 colori).
'      - `SCREEN 13`: Modalità SVGA (800x600 pixel, 256 colori).
'      - `SCREEN 0`: Ritorna alla modalità testo e chiude la finestra grafica.
'    - Il passaggio da una modalità all'altra è completamente supportato e permette di gestire diverse finestre
'      grafiche in sequenza.
'
' 2. GESTIONE DELLE PAGINE VIDEO (`SCREENSET`)
'    - Molte modalità grafiche supportano più "pagine" di memoria. Si può disegnare su una pagina nascosta
'      mentre un'altra è visibile, e poi scambiarle per ottenere animazioni fluide senza flickering.
'    - `SCREENSET [visiblePage], [activePage]`
'      - `visiblePage`: La pagina che l'utente sta vedendo.
'      - `activePage`: La pagina su cui i comandi di disegno (LINE, PSET, CLS, etc.) avranno effetto.
'
' 3. CREAZIONE E MANIPOLAZIONE DI SPRITE
'    - Uno "sprite" è semplicemente un'immagine che può essere disegnata ripetutamente sullo schermo.
'    - In questo esempio, lo sprite viene creato disegnando una forma (una X) su una pagina video nascosta.
'    - `IMAGEGET (x1,y1)-(x2,y2), array%`: "Fotografa" un'area rettangolare dello schermo (dalla pagina attiva)
'      e salva i dati dei pixel in un array di interi. Il primo elemento dell'array memorizza le dimensioni
'      dell'immagine.
'    - `IMAGEPUT (x,y), array%`: Disegna il contenuto dell'array (lo sprite catturato) in una nuova posizione
'      sullo schermo (sulla pagina attiva).
'
' 4. FLUSSO DI ESECUZIONE DEL TEST
'    1.  Apre una finestra VGA (SCREEN 12) e disegna una linea.
'    2.  Chiude la finestra (SCREEN 0) e ne apre una nuova in SVGA (SCREEN 13).
'    3.  Passa alla pagina attiva 1 (nascosta) e disegna una X.
'    4.  Cattura la X in un array `SPRITE%` usando `IMAGEGET`.
'    5.  Torna alla pagina visibile e attiva 0, cambia lo sfondo e disegna lo sprite catturato con `IMAGEPUT`.
'    6.  Chiude e riapre una terza finestra in VGA per verificare la corretta gestione delle risorse.
' ======================================================================================================================

' Test avanzato con manipolazione di pagine e sprite
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ SpritesAndPages - Manipolazione Grafica con Sprite e Pagine              ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 REM Test avanzato con manipolazione di pagine e sprite
' 1. Apre la prima finestra in modalità grafica VGA (640x480, 16 colori).
20 SCREEN 12
' Pulisce lo schermo con il colore 1 (blu).
30 CLS 1
' Disegna una linea bianca (colore 15) da un angolo all'altro.
40 LINE (0,0)-(639,479), 15
' Stampa testo colorato sulla finestra grafica.
50 CPRINT "FASE 1: VGA. Premi un tasto..."
' Attende la pressione di un tasto.
60 WAITKEY
70 
' Chiude la finestra grafica e torna alla modalità testo.
80 SCREEN 0
85 CPRINT "Tornato a SCREEN 0. Ora apro SVGA e preparo lo sprite..."
90 
' 2. Apre una nuova finestra in modalità SVGA (800x600, 256 colori).
100 SCREEN 13
110 
120 REM --- Preparazione Sprite su pagina nascosta ---
' Imposta la pagina attiva a 1 (nascosta) mentre la pagina visibile rimane 0.
130 SCREENSET , 1
' Pulisce la pagina 1 con il colore 4 (rosso).
140 CLS 4
' Disegna una "X" bianca sulla pagina 1.
150 LINE (10,10)-(30,30), 15
160 LINE (10,30)-(30,10), 15
170 
' Dichiara un array di INTEGER per memorizzare i dati dello sprite.
180 DIM SPRITE%(600)
' "Fotografa" l'area della "X" dalla pagina 1 e la salva nell'array.
190 IMAGEGET (9,9)-(31,31), SPRITE%
200 
210 REM --- Animazione minimale sulla pagina 0 ---
' Torna a rendere la pagina 0 sia visibile che attiva.
220 SCREENSET 0, 0
' Pulisce la pagina 0 con il colore 2 (verde).
230 CLS 2
' Disegna lo sprite catturato sulla pagina 0 alle coordinate (100, 100).
240 IMAGEPUT (100, 100), SPRITE%
250 
260 CPRINT "FASE 2: SVGA con sprite. Premi un tasto..."
270 WAITKEY
280 
' Chiude la finestra SVGA.
290 SCREEN 0
295 CPRINT "Tornato a SCREEN 0. Ora riapro VGA..."
300 
' 3. Riapre una finestra VGA per verificare la corretta gestione delle risorse.
310 SCREEN 12
' Pulisce con colore 6 (ciano).
320 CLS 6
' Disegna una linea azzurra.
330 LINE (0,0)-(639,479), 14
340 CPRINT "FASE 3: Di nuovo VGA. Se la REPL non è nera, è ok."
350 WAITKEY
360 
' Chiude l'ultima finestra.
370 SCREEN 0
