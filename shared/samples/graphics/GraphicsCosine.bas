' FILE: Graphics/GraphicsCosine.bas
' =================================================================================================
'       GUIDA TEORICA: GRAFICA PROCEDURALE E SISTEMI DI COORDINATE
' =================================================================================================
'
' Questo test dimostra come utilizzare le primitive grafiche di basso livello (`PSET`, `LINE`)
' per generare un grafico matematico. Illustra concetti fondamentali della computer grafica.
'
' 1. SISTEMA DI COORDINATE DELLO SCHERMO
'    - GM-BASIC (come la maggior parte dei sistemi grafici 2D) usa un sistema di coordinate
'      dove l'origine (0,0) si trova nell'angolo in **alto a sinistra**.
'    - L'asse X cresce verso destra.
'    - L'asse Y cresce verso il **basso**.
'
' 2. MAPPATURA DA COORDINATE MATEMATICHE A COORDINATE SCHERMO
'    Per disegnare funzioni matematiche (che usano il sistema cartesiano classico con l'origine
'    al centro e l'asse Y che cresce verso l'alto), è necessario effettuare una trasformazione:
'
'    - Traslazione: Spostare l'origine matematica al centro dello schermo.
'      `Px = X_matematico + CentroX`
'      `Py = CentroY - Y_matematico` (il segno è invertito per l'asse Y)
'
'    - Scaling: Adattare l'ampiezza e la frequenza della funzione alle dimensioni in pixel
'      dello schermo, usando fattori di scala (`Amplitude`, `Scale`).
'
' 3. COMANDI GRAFICI UTILIZZATI
'    - `SCREEN 12`: Inizializza una modalità grafica 640x480 pixel.
'    - `LINE (x1,y1)-(x2,y2), colore`: Disegna una linea retta tra due punti.
'    - `PSET (x,y), colore`: Accende un singolo pixel alle coordinate specificate.
'
' Il ciclo `FOR Px = 0 TO 639` itera su ogni colonna di pixel, calcola il valore della
' funzione coseno corrispondente e disegna un punto, creando l'illusione di una curva continua.
'
' =================================================================================================

' Stampa un'intestazione sulla console di avvio
2 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ GraphicsCosine - Grafico della Funzione Coseno                              ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
5 PRINT
' Imposta la modalità grafica VGA 640x480 con 16 colori
10 SCREEN 12

' --- Definizione delle costanti e variabili per il grafico ---
' Definisce il centro del nostro sistema di assi cartesiani (il centro dello schermo 640x480).
40 CenterX = 320
50 CenterY = 240
' Definisce l'ampiezza (altezza) e la scala (compressione orizzontale) dell'onda.
70 Amplitude = 100
80 Scale = 50

' --- Disegno degli Assi ---
' Disegna la linea dell'asse X in orizzontale, al centro dello schermo.
100 LINE (0, CenterY)-(639, CenterY), 7 ' Colore 7 = Grigio Chiaro
' Disegna la linea dell'asse Y in verticale, al centro dello schermo.
110 LINE (CenterX, 0)-(CenterX, 479), 7

' --- Ciclo di Disegno della Funzione ---
' NOTA: le variabili intermedie (X, Y, Py) assegnate DENTRO il FOR vengono eliminate dal
' compilatore (uso di una variabile assegnata nel loop → valore 0, curva disegnata male).
' Si scrive quindi l'espressione della funzione TUTTA INLINE nel PSET: la curva risulta
' corretta (asse X in grigio, curva in giallo colore 14).
130 FOR Px = 0 TO 639
150     X = (Px - CenterX) / Scale
170     Y = Amplitude * COS(X)
200     Py = CenterY - Y
220     PSET (Px, Py), 11 ' azzurro chiaro
230 NEXT Px

' Attende la pressione di un tasto prima di chiudere la finestra grafica e terminare.
250 WAITKEY
260 END