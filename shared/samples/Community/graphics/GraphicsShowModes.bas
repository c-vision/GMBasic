' FILE: Graphics/GraphicsShowModes.bas
' =================================================================================================
'       GUIDA TEORICA: INTROSPEZIONE DELLE MODALITÀ GRAFICHE (`SCREENS`)
' =================================================================================================
'
' Questo test dimostra l'uso della funzione `SCREENS()`, una potente funzionalità di
' introspezione che permette a un programma GM-BASIC di scoprire quali modalità grafiche
' sono supportate dal sistema host su cui sta girando.
'
' 1. LA FUNZIONE `SCREENS()`
'    - Sintassi: `SCREENS()`
'    - Comportamento: Interroga il sottosistema grafico (es. SDL) per ottenere un elenco di
'      tutte le risoluzioni, profondità di colore e modalità a schermo intero/finestra disponibili.
'    - Valore di Ritorno: Restituisce un **array di `DISPLAYMODE`**.
'
' 2. IL TIPO DI DATO `DISPLAYMODE`
'    `DISPLAYMODE` è un tipo di dato strutturato (UDT) predefinito in GM-BASIC, progettato
'    specificamente per contenere le informazioni di una singola modalità video. I suoi campi includono:
'    - `ModeID`: L'identificatore numerico da passare al comando `SCREEN` per attivare questa modalità.
'    - `Width`, `Height`: La risoluzione in pixel.
'    - `BPP` (Bits Per Pixel): La profondità di colore.
'    - `Colors`: Il numero di colori (es. 16, 256, o un valore grande per True Color).
'    - `Description`: Una stringa descrittiva della modalità.
'    - `Active`: Un flag booleano (`BIT`) che indica se questa è la modalità attualmente in uso.
'
' 3. UTILIZZO PRATICO
'    Questa funzionalità è fondamentale per creare applicazioni grafiche portabili e robuste.
'    Invece di assumere che una certa modalità (es. `SCREEN 12`) sia sempre disponibile, un
'    programma può:
'    1. Chiamare `SCREENS()` per ottenere il NUMERO di schermi/modalità supportati.
'    2. Usare `SCREENINFO()` per leggere i campi della modalità corrente (ModeID, Width,
'       Height, BPP, Colors) e verificare che sia quella desiderata.
'    3. Utilizzare il `ModeID` per chiamare `SCREEN` in modo sicuro, garantendo che il
'       programma funzioni su hardware diversi senza errori.
'
' =================================================================================================
1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ GraphicsShowModes - INFO MODALITA' GRAFICHE SCREENS/SCREENINFO               ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
' La funzione SCREENS() restituisce il NUMERO di schermi disponibili (1.0 in headless/VM).
10 ' Questo valore è coerente su PCodeVM, VM/C, C99 e ILBackend.
20 NUM_SCREENS = SCREENS()
50 PRINT "Schermi disponibili: "; NUM_SCREENS
60 PRINT
' SCREENINFO() espone i campi della modalità corrente: ModeID/Width/Height/BPP/Colors.
70 PRINT "ModeID    : "; SCREENINFO().ModeID
80 PRINT "Width     : "; SCREENINFO().Width
90 PRINT "Height    : "; SCREENINFO().Height
100 PRINT "BPP       : "; SCREENINFO().BPP
110 PRINT "Colors    : "; SCREENINFO().Colors
120 PRINT "Active    : "; SCREENINFO().Active
130 PRINT "--------------------------------------------------------------------------------"
140 END
