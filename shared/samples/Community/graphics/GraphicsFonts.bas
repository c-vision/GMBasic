' FILE: Graphics/GraphicsFonts.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE DEL TESTO IN MODALITÀ GRAFICA (FONT, PRINT, LOCATE)
' =================================================================================================
'
' Questo test dimostra le operazioni fondamentali per caricare un font e renderizzare
' testo in una modalità grafica, un'operazione che differisce significativamente dalla
' modalità console testuale.
'
' 1. IL COMANDO `FONT`
'    - Sintassi: `FONT "percorso_file_font.ttf", dimensione`
'    - In modalità grafica, i caratteri non sono predefiniti ma vengono disegnati usando un
'      file di font esterno (TrueType Font, .ttf).
'    - È OBBLIGATORIO caricare un font con `FONT` prima di poter usare `PRINT`. Se non viene
'      caricato alcun font, i comandi di stampa non produrranno alcun output visibile.
'
' 2. `PRINT` e `LOCATE` IN MODALITÀ GRAFICA
'    Anche in modalità grafica, `PRINT` e `LOCATE` operano su una griglia di caratteri,
'    non direttamente su coordinate pixel.
'    - `LOCATE riga, colonna`: Sposta il cursore di testo alla cella di carattere specificata
'      (riga e colonna, 1-based). Le dimensioni della griglia dipendono dalla risoluzione
'      dello schermo e dalla dimensione del font caricato.
'    - `PRINT testo$`: Disegna il testo a partire dalla posizione corrente del cursore.
'
' 3. MISURAZIONE DEL TESTO (`TEXTWIDTH` E `TEXTHEIGHT`)
'    Per posizionare il testo in modo preciso in pixel (es. per centrarlo), è necessario
'    conoscere le sue dimensioni.
'    - `TEXTWIDTH(testo$)`: Restituisce la larghezza in **pixel** che la stringa occuperà
'      quando disegnata con il font corrente.
'    - `TEXTHEIGHT()`: Restituisce l'altezza in **pixel** di una singola riga di testo
'      per il font corrente.
'
' 4. CONSOLE DI DEBUG (`CPRINT` / `CLOCATE`)
'    Anche in modalità grafica, GM-BASIC mantiene una console di testo "nascosta" o emulata.
'    - `CPRINT`: Stampa un messaggio su questa console di testo, utile per il debug
'      senza "sporcare" la schermata grafica.
'    - `CLOCATE`: Posiziona il cursore su questa console di testo.
'
' Questo test mostra come caricare un font, stampare testo, cambiarne il colore e usare
' le funzioni di misurazione per calcolare il posizionamento.
' =================================================================================================

2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ GraphicsFonts - FONTS                                                    ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"

' --- Inizializzazione delle variabili e della modalità grafica ---
' Definisce il percorso del file font.
' Nota: questo percorso è specifico per Windows e potrebbe dover essere cambiato su altri sistemi.
60 DIM FONT_PATH$
70 FONT_PATH$ = "font.ttf" ' Font di progetto (COUR.TTF era Windows-only)

' Inizializza la modalità grafica 640x480.
90 SCREEN 12

' --- Stampa messaggi di debug sulla console di testo emulata ---
' CPRINT è utile per mostrare informazioni di stato senza interferire con la schermata grafica.
100 CPRINT "Test FONT e PRINT in modalità SCREEN 12 (VGA)."
110 CPRINT "Sto tentando di caricare il font: " + FONT_PATH$

' --- Caricamento del Font e Setup dello Schermo ---
' Carica il font specificato con una dimensione di 16 pixel.
' Questa operazione è obbligatoria per poter stampare testo in modalità grafica.
130 FONT FONT_PATH$, 12
' Imposta i colori (testo bianco su sfondo blu) e pulisce lo schermo.
140 COLOR 15, 1
150 CLS

' --- Test di Stampa e Posizionamento ---
' `LOCATE` sposta il cursore in PIXEL (in modalita' grafica, come GMO).
170 LOCATE 32, 40   ' riga2*16, col5*8 (pixel)
180 PRINT "Questo e' un testo stampato con un font TTF."

190 LOCATE 64, 40   ' riga4*16, col5*8 (pixel)
200 PRINT "Il colore e' bianco su sfondo blu."

' Cambia solo il colore del testo (foreground) in giallo.
210 COLOR 14
220 LOCATE 96, 40   ' riga6*16, col5*8 (pixel)
230 PRINT "Ora il testo e' giallo."

' --- Test delle Funzioni di Misurazione per Centrare il Testo ---
250 DIM MSG$
260 MSG$ = "Questo testo e' centrato!"
' `TEXTWIDTH` e `TEXTHEIGHT` restituiscono le dimensioni in pixel del testo.
270 DIM MSG_W, MSG_H
280 MSG_W = TEXTWIDTH(MSG$)
290 MSG_H = TEXTHEIGHT()

' Calcola le coordinate X e Y in pixel per centrare il testo.
300 DIM CENTER_X, CENTER_Y
310 CENTER_X = (640 - MSG_W) / 2
320 CENTER_Y = (480 - MSG_H) / 2

' Poiché `LOCATE` lavora con coordinate a caratteri, è necessario convertire
' le coordinate in pixel. Questa formula è un'approssimazione che divide i pixel
' per la dimensione media di un carattere.
340 LOCATE CENTER_Y, CENTER_X   ' coordinate in pixel
350 COLOR 12 ' Rosso chiaro
360 PRINT MSG$

' --- Conclusione del Test ---
' Stampa un messaggio finale sulla console di testo.
370 CLOCATE 20, 1
380 CPRINT "Test del testo completato. Premi un tasto per uscire."
' Attende la pressione di un tasto prima di terminare.
390 WAITKEY

' Ripristina la modalità testo originale e chiude la finestra grafica.
400 SCREEN 0

' =================================================================================================
' VERIFICA DI COMPLETEZZA E SUGGERIMENTI
' =================================================================================================
'
' - Copertura Funzionalità: Il test copre in modo efficace le funzionalità di base per il rendering
'   del testo in modalità grafica: `SCREEN`, `FONT`, `PRINT`, `LOCATE`, `COLOR`, `CLS`, `CPRINT`,
'   `TEXTWIDTH` e `TEXTHEIGHT`.
'
' - Casi di Test Mancanti / Miglioramenti Possibili:
'   - Test di Fallimento: Manca un test che verifichi il comportamento quando `FONT` viene chiamato
'     con un percorso non valido o quando `PRINT` viene chiamato prima di `FONT`.
'   - Font Multipli: Non viene testata la possibilità di caricare e alternare tra più font o
'     dimensioni diverse all'interno dello stesso programma.
'   - Allineamento del Testo: L'allineamento è calcolato manualmente. Un suggerimento per il
'     linguaggio potrebbe essere l'introduzione di un comando `DRAWTEXT` con opzioni di
'     allineamento (sinistra, centro, destra) per semplificare il posizionamento.
'
' =================================================================================================