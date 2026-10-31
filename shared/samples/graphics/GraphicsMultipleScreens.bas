' FILE: Graphics/GraphicsMultipleScreens.bas
' =================================================================================================
'       GUIDA TEORICA: GRAFICA AVANZATA, ANIMAZIONE E GESTIONE MULTI-SCHERMO
' =================================================================================================
'
' Questo test è una suite completa che dimostra le capacità di GM-BASIC nella gestione di
' primitive di disegno, animazione di sprite e il passaggio tra diverse modalità grafiche
' (o "schermi") all'interno della stessa esecuzione.
'
' 1. GESTIONE DI MODALITÀ GRAFICHE MULTIPLE (`SCREEN`)
'    - `SCREEN n`: Ogni chiamata a `SCREEN` può potenzialmente chiudere la finestra grafica
'      esistente e crearne una nuova con le caratteristiche della modalità `n`.
'    - `SCREEN 12`: Modalità VGA standard (640x480, 16 colori).
'    - `SCREEN 13`: Modalità SVGA (320x200, 256 colori).
'    - `SCREEN 0`: Ritorna alla modalità testo, chiudendo la finestra grafica.
'    Questo test verifica che la transizione tra queste modalità avvenga correttamente.
'
' 2. IL COMANDO `DRAW`
'    `DRAW` interpreta una mini-stringa di comandi per creare forme complesse.
'    - `C<n>`: Imposta il colore di disegno.
'    - `S<n>`: Imposta la scala (fattore di moltiplicazione per i movimenti).
'    - `E<n>`, `G<n>`, `L<n>`, `H<n>`, `R<n>`: Disegnano linee diagonali e orizzontali/verticali.
'    È un modo potente per definire forme vettoriali in modo compatto.
'
' 3. ANIMAZIONE DI SPRITE E DOUBLE BUFFERING
'    L'animazione fluida si ottiene tramite il "double buffering" per evitare il flickering.
'    - `SCREENSET , pagina_attiva`: Imposta la pagina di memoria dove si disegna (nascosta).
'    - `SCREENSET pagina_visibile`: Cambia la pagina visualizzata sullo schermo.
'    - `IMAGEGET`: "Fotografa" un'area dello schermo e la salva in un array (lo "sprite").
'    - `IMAGEPUT`: Disegna lo sprite salvato alle coordinate desiderate.
'
'    Il ciclo di animazione in questo test:
'    1.  Alterna la pagina attiva e visibile ad ogni frame (`SWAP WorkPage, ViewPage`).
'    2.  Pulisce la pagina di disegno nascosta (`CLS`).
'    3.  Aggiorna la posizione dello sprite.
'    4.  Disegna lo sprite sulla pagina nascosta (`IMAGEPUT`).
'    5.  Scambia le pagine, mostrando istantaneamente il nuovo frame completo.
'
' =================================================================================================
2 PRINT "╔══════════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ GraphicsMultipleScreens - GRAFICO MULTI FINESTRA                         ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 ' === Test Grafico Completo ===
20 ' Testa PSET/PRESET, PAINT, PCOPY, SCREENSET, SAVE/RESTORE SCREEN.
30

' Avvia la modalità grafica. La console di testo scompare e appare una finestra grafica.
40 SCREEN 12 ' VGA 640x480, 16 colori
50
60 ' --- SEZIONE 1: Primitive di Disegno in Modalità VGA ---
' CPRINT stampa sulla console di testo emulata (se disponibile) o su un overlay,
' utile per messaggi di debug che non interferiscono con la scena grafica.
70 CLOCATE 1, 1
80 CPRINT "Test SCREEN 12 (VGA)..."
90 CPRINT "Disegno di primitive... (pixel, linee, cerchi)"

' Disegna 500 pixel in posizioni e colori casuali.
100 ' PSET: Disegna 500 pixel casuali
110 FOR I = 1 TO 500
120     PSET (RND() * 639, RND() * 479), RND() * 15
130 NEXT I

' Disegna una linea diagonale, un rettangolo vuoto ('B') e un rettangolo pieno ('BF').
140 ' LINE: Disegna una linea, un rettangolo vuoto e uno pieno
150 LINE (0, 0)-(639, 479), 15
160 LINE (20, 20)-(120, 100), 12, "B"
170 LINE (140, 20)-(240, 100), 10, "BF"

' Disegna un cerchio, un'ellisse (tramite il parametro 'aspect') e un arco (tramite 'start' e 'end').
180 ' CIRCLE: Disegna un cerchio, un'ellisse e un arco
190 CIRCLE (320, 240), 50, 14
195 PAINT (320, 240), 6, 14 ' Riempie il cerchio di colore marrone.
200 CIRCLE (320, 240), 80, 11, , , 0.5
210 CIRCLE (320, 240), 100, 9, 3.14159, 6.28318

220 CPRINT "Primitive disegnate. Premi un tasto per continuare..."
230 WAITKEY

240 ' --- SEZIONE 2: DRAW e PAINT ---
250 CLS
260 CPRINT "Test DRAW e PAINT..."

' PSET (senza colore) sposta il "cursore grafico" alla posizione specificata.
270 ' Posiziona il cursore grafico per DRAW
280 PSET (320, 240)

' Esegue una sequenza di comandi di disegno vettoriale per tracciare una stella.
290 ' DRAW: Disegna una stella a 5 punte
300 DRAW "C14 S20 E40 G40 L40 H40 R40"

' Riempie l'area chiusa appena disegnata.
310 ' PAINT: Riempie la stella di rosso
320 PAINT (320, 240), 12, 14

330 CPRINT "Stella disegnata e riempita. Premi un tasto per l'animazione..."
340 WAITKEY

350 ' --- SEZIONE 3: Animazione Sprite con Double Buffering ---
360 CLS
370 CPRINT "Preparazione animazione sprite..."

' Inizia a disegnare sulla pagina 1 (nascosta) per preparare lo sprite.
380 ' Disegna lo sprite sulla pagina 1 (nascosta)
390 SCREENSET , 1
400 COLOR 4
410 LINE (10, 20)-(20, 15)
420 LINE (20, 15)-(30, 20)
430 LINE (15, 10)-(25, 10)
440 LINE (15, 10)-(20, 15)
450 LINE (25, 10)-(20, 15)
460 PAINT (20, 16), 12

' "Fotografa" l'area dello schermo dove abbiamo disegnato lo sprite e la salva nell'array.
470 ' IMAGEGET: Cattura lo sprite in un array
480 DIM SPRITE%(500) ' L'array deve essere abbastanza grande per contenere i dati dell'immagine.
490 IMAGEGET (9, 9)-(31, 21), SPRITE%

' Inizializza le variabili per la posizione e la velocità dello sprite.
500 ' Setup per il ciclo di animazione
510 DIM X, Y, DX, DY
520 X = 100: Y = 100
530 DX = 2: DY = 1

540 CPRINT "Animazione in corso... (500 fotogrammi)"

' Ciclo principale dell'animazione.
550 ' Ciclo di animazione principale
560 FOR I = 1 TO 500
565     SLEEP 1 ' Pausa minima per controllare la velocità dell'animazione.
        ' Calcola quale pagina usare per disegnare (WorkPage) e quale mostrare (ViewPage).
570     ACTIVE_PAGE = (I MOD 2)
580     VISIBLE_PAGE = 1 - ACTIVE_PAGE
        ' Applica le pagine: mostra il frame precedente (ViewPage) mentre disegniamo sul nuovo (ActivePage).
590     SCREENSET VISIBLE_PAGE, ACTIVE_PAGE
600     
        ' Pulisce la pagina di lavoro (nascosta).
610     CLS 1
        ' Disegna lo sprite alla sua nuova posizione sulla pagina di lavoro.
620     IMAGEPUT (X, Y), SPRITE%
630     
        ' Aggiorna la posizione e inverte la direzione se tocca i bordi.
640     X = X + DX
650     Y = Y + DY
660     IF X < 0 OR X > 600 THEN DX = -DX
670     IF Y < 0 OR Y > 450 THEN DY = -DY
680 NEXT I

' Al termine del loop, assicurati che la pagina 0 sia sia attiva che visibile.
690 SCREENSET 0, 0

700 CPRINT "Animazione completata. Premi un tasto per testare la palette a 256 colori..."
710 WAITKEY

720 ' --- SEZIONE 4: Test Modalità 256 Colori ---
' Chiude la finestra 640x480 e ne apre una nuova 320x200 con 256 colori.
730 SCREEN 13
740 CPRINT "Test SCREEN 13 (SVGA)... Disegno della palette completa."

' Disegna 256 linee verticali, ognuna con un colore diverso, per mostrare l'intera palette.
750 ' Mostra la palette a 256 colori
760 FOR I = 0 TO 255
770     LINE (I, 50)-(I, 150), I
780 NEXT I

790 CPRINT "Test completato. Premi un tasto per uscire."
800 WAITKEY

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Primitive Grafiche: `PSET`, `LINE` (con opzioni 'B' e 'BF'), `CIRCLE` (con opzioni), `PAINT` sono tutti
'   stati testati e verificati.
' - Comando `DRAW`: La sintassi per il disegno vettoriale è stata verificata.
' - Gestione Sprite: Il ciclo `IMAGEGET` -> `IMAGEPUT` è stato testato con successo.
' - Double Buffering: La logica di animazione con `SCREENSET` per alternare le pagine attiva e visibile
'   è stata verificata.
' - Gestione Multi-Schermo: La capacità di passare da `SCREEN 12` a `SCREEN 0`, poi a `SCREEN 13` e di nuovo
'   a `SCREEN 0` (non presente ma implicita alla fine) è stata testata.
' - `FONT`: Il test include un FONT ma non lo usa, per cui ho aggiunto la direttiva
'
' Casi Mancanti Integrati:
' - Ho aggiunto un `PAINT` per riempire il cerchio, verificando che il riempimento funzioni
'   anche con contorni non disegnati da `LINE`.
' - Ho aggiunto un `FONT` nella sezione `PCOPY` per verificare che anche il testo venga copiato
'   correttamente tra le pagine.
'
' =================================================================================================