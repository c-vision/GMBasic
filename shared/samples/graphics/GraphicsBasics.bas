' FILE: Graphics/GraphicsBasics.bas
' =================================================================================================
'       GUIDA TEORICA: GRAFICA DI BASE (SCREEN, FONT, PRINT, LOCATE)
' =================================================================================================
'
' Questo test dimostra le operazioni fondamentali per passare a una modalità grafica e
' stampare del testo su di essa.
'
' 1. INIZIALIZZAZIONE DELLA MODALITÀ GRAFICA (`SCREEN`)
'    - `SCREEN n`: È il comando che inizializza il sottosistema grafico. `n` è un codice
'      che corrisponde a una risoluzione e profondità di colore predefinite (es. 12 = 640x480).
'    - GM-BASIC utilizza una libreria come SDL per creare la finestra di output.
'
' 2. CARICAMENTO DEI FONT (`FONT`)
'    - Sintassi: `FONT "percorso_file_font.ttf", dimensione`
'    - A differenza della modalità testo, in una modalità grafica i caratteri non sono predefiniti
'      ma devono essere renderizzati da un file di font (TrueType Font, .ttf).
'    - È OBBLIGATORIO caricare un font con `FONT` prima di poter usare `PRINT`, altrimenti
'      il testo non sarà visibile.
'
' 3. STAMPA SU SCHERMO GRAFICO (`PRINT`)
'    - In modalità grafica, `PRINT` si comporta come un comando di disegno. Renderizza la stringa
'      fornita come una texture e la "disegna" sulla finestra alla posizione corrente del cursore grafico.
'    - Il cursore avanza automaticamente a destra dopo ogni stampa e va a capo quando raggiunge
'      il bordo della finestra o quando incontra un `PRINT` vuoto.
'
' 4. POSIZIONAMENTO (`LOCATE`)
'    - Sintassi: `LOCATE y, x`
'    - In modalità grafica, `LOCATE` sposta il cursore di testo a coordinate *pixel* assolute.
'      - `y`: La coordinata verticale (pixel dall'alto).
'      - `x`: La coordinata orizzontale (pixel da sinistra).
'    - Questo permette un posizionamento preciso del testo sullo schermo.
'
' 5. COLORI (`COLOR` E `CLS`)
'    - `COLOR fg, bg`: Imposta i colori di primo piano (testo) e sfondo.
'    - `CLS`: Pulisce la finestra grafica riempiendola con il colore di sfondo impostato.
'
' =================================================================================================

SUB Main()
  ' Pulisce la console di avvio (se presente)
  CLS
  PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ GraphicsBasics - Grafica Minima                                             ║"
  PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
  
  ' Imposta la modalità grafica a 640x480. Questo crea la finestra di output.
  SCREEN 12
  ' Carica il font che verrà usato per tutte le successive operazioni di PRINT.
  FONT "font.ttf", 12
  ' Imposta i colori: testo bianco (15) su sfondo blu (1), poi pulisce lo schermo con il blu.
  COLOR 15, 1 : CLS

  ' --- Blocco test A: PRINT di base (newline a colonna 0) ---
  PRINT "Riga 1"
  PRINT "Riga 2"
  PRINT "Riga 3"
  PRINT "Riga 4"

  ' --- Blocco test B: ';' accoda senza a capo ---
  PRINT "A";
  PRINT "B";
  PRINT "C";
  PRINT "D"

  ' PRINT vuoto: lascia una riga vuota
  PRINT

  ' --- Blocco test C: ',' (tab-zone) accoda alle zone 0/14/28/42 ---
  PRINT "E",
  PRINT "F",
  PRINT "G",
  PRINT "H"

  ' --- Test 1: Stampa in una posizione specifica (coordinate pixel, LOCATE y, x) ---
  COLOR 14 ' Giallo
  LOCATE 200, 50
  PRINT "Testo a (50, 200)"

  ' --- Test 2: Stampa sulla riga successiva, stessa colonna ---
  ' Il PRINT da solo va a capo a COLONNA 0 (parità console). Per mantenere la colonna X
  ' si riposiziona esplicitamente con LOCATE, oppure si usa ';' per accodare sulla stessa riga.
  COLOR 10 ' Verde
  LOCATE 228, 50
  PRINT "Sempre a X=50"

  ' --- Test 3: Sposta a destra e stampa ---
  COLOR 11 ' Ciano
  LOCATE 200, 300
  PRINT "Testo a (300, 200)"

  ' --- Test 4: Stampa sotto il testo precedente (stessa colonna) ---
  LOCATE 228, 300
  PRINT "Sotto il testo a (300, 200)"

  ' Attende la pressione di un tasto prima di chiudere la finestra grafica.
  WAITKEY
  
END SUB
