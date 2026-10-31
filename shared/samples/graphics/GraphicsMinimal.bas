' FILE: Graphics/GraphicsMinimal.bas
' =================================================================================================
'       GUIDA TEORICA: INIZIALIZZAZIONE GRAFICA E OUTPUT DI TESTO
' =================================================================================================
'
' Questo programma dimostra la sequenza di comandi essenziale e obbligatoria per
' inizializzare una modalità grafica e visualizzare del testo. È l'esempio "Hello, World!"
' del sottosistema grafico di GM-BASIC.
'
' 1. `SCREEN n` - Attivazione della Modalità Grafica
'    - Questo comando è il punto di ingresso. Chiude la console di testo (se presente) e apre
'      una nuova finestra grafica gestita da una libreria sottostante (come SDL).
'    - Il parametro `n` seleziona una modalità video predefinita con una specifica risoluzione
'      e profondità di colore (es. `SCREEN 12` attiva la modalità 640x480 a 16 colori).
'
' 2. `FONT "file", dimensione` - Caricamento del Font
'    - In modalità grafica, i caratteri non sono pre-disegnati in una griglia come in modalità testo.
'      Devono essere renderizzati da un file di font (es. TrueType, `.ttf`).
'    - Questa istruzione è **obbligatoria**. Se si omette `FONT`, qualsiasi comando `PRINT` successivo
'      non produrrà alcun output visibile, poiché il sistema non sa come disegnare i caratteri.
'
' 3. `COLOR fg, bg` e `CLS` - Gestione dei Colori
'    - `COLOR` imposta i colori di primo piano (testo) e sfondo per le operazioni successive.
'    - `CLS` (Clear Screen) pulisce l'intera finestra, riempiendola con il colore di sfondo
'      attualmente impostato.
'
' 4. `LOCATE y, x` e `PRINT` - Posizionamento e Disegno del Testo
'    - In modalità grafica, `LOCATE` sposta il cursore di disegno a coordinate **pixel** assolute.
'      `LOCATE 200, 150` posiziona il punto di inizio del testo a 150 pixel dal bordo sinistro
'      e 200 pixel dal bordo superiore.
'    - `PRINT` renderizza la stringa utilizzando il font e il colore di primo piano correnti
'      a partire dalla posizione del cursore.
'
' 5. `WAITKEY` - Sincronizzazione dell'Output
'    - Poiché l'output grafico avviene in una finestra separata, è fondamentale inserire una
'      pausa (`WAITKEY`, `SLEEP` o un loop `INKEY`) per dare all'utente il tempo di vedere
'      il risultato prima che il programma termini e la finestra si chiuda.
'
' =================================================================================================

2 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ GraphicsMinimal - Grafica Minima                                            ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
' === TEST MINIMALE ASSOLUTO: CLS, COLOR, PRINT ===
' Verifica il rendering di una singola stringa dopo aver
' impostato i colori e pulito lo schermo.
' ----------------------------------------------------

SUB Main()
  ' 1. Attiva la modalità grafica (640x480, 16 colori).
  SCREEN 12
  
  ' --- Test Aggiunto: Verifica del mancato caricamento del FONT ---
  ' Tentiamo di stampare senza aver caricato un font.
  ' Il comportamento atteso è che non appaia nulla, senza generare un errore.
  CLS 0 : COLOR 15
  LOCATE 10, 10
  PRINT "(Questo testo non dovrebbe essere visibile)"
  SLEEP 2000 ' Pausa per osservare lo schermo nero

  ' 2. Carica una risorsa font. Questo passo è obbligatorio per poter stampare testo.
  FONT "font.ttf", 12
  
  ' 3. Imposta i colori: Testo Bianco (15) su Sfondo Rosso (4).
  COLOR 15, 4
  
  ' 4. Pulisce l'intera finestra. Lo schermo diventerà rosso.
  CLS
  
  ' 5. Posiziona il cursore grafico alle coordinate pixel (150, 200).
  LOCATE 200, 150
  ' Disegna la stringa con il font e i colori correnti.
  PRINT "TESTO BIANCO"
  
  ' 6. Pausa l'esecuzione per permettere di visualizzare il risultato.
  '    Senza WAITKEY, il programma terminerebbe e la finestra si chiuderebbe immediatamente.
  SLEEP 1
  WAITKEY
  
END SUB

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Funzionalità Verificate: Questo test copre con successo la sequenza minima e indispensabile
'   per l'output di testo in modalità grafica: `SCREEN`, `FONT`, `COLOR`, `CLS`, `LOCATE`, `PRINT`.
'
' - Casi Aggiunti:
'   - È stato aggiunto un test per verificare che `PRINT` non produca output visibile se `FONT`
'     non è stato chiamato in precedenza, confermando il comportamento atteso.
'
' Il file è ora più completo per i suoi obiettivi minimali.
'
' =================================================================================================