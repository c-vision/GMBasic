' FILE: Graphics/GraphicsScreenSet.bas
' =================================================================================================
'       GUIDA TEORICA: DOUBLE BUFFERING E GESTIONE DELLE PAGINE VIDEO (`SCREENSET`)
' =================================================================================================
'
' Questo test dimostra una tecnica fondamentale per l'animazione e la grafica fluida:
' il "Double Buffering", implementato in GM-BASIC tramite il comando `SCREENSET`.
'
' 1. IL PROBLEMA DEL "FLICKERING"
'    Quando si disegna una scena complessa direttamente sullo schermo visibile, l'utente può
'    percepire uno sfarfallio ("flickering"). Questo accade perché vede lo schermo mentre
'    viene prima cancellato e poi ridisegnato pezzo per pezzo.
'
' 2. LA SOLUZIONE: DOUBLE BUFFERING
'    Invece di disegnare sulla schermata visibile, si utilizzano due "buffer" di memoria video,
'    chiamati "pagine":
'    - Una Pagina Visibile (Front Buffer): Quella che l'utente sta attualmente vedendo.
'    - Una Pagina Attiva o di Lavoro (Back Buffer): Una copia nascosta della schermata, su cui
'      vengono eseguiti tutti i comandi di disegno.
'
'    Il ciclo di animazione diventa:
'    1. Disegna l'intero nuovo frame sulla pagina *nascosta*.
'    2. Quando il disegno è completo, "scambia" le pagine: la pagina nascosta diventa
'       istantaneamente visibile, e quella vecchia diventa il nuovo back buffer.
'    Questo scambio atomico elimina completamente il flickering.
'
' 3. IL COMANDO `SCREENSET`
'    `SCREENSET` è il comando che controlla quale pagina è visibile e su quale si disegna.
'    - Sintassi: `SCREENSET [pagina_visibile], [pagina_attiva]`
'    - `SCREENSET 1`: Imposta la pagina 1 come *visibile*.
'    - `SCREENSET , 1`: Imposta la pagina 1 come *attiva* (per il disegno), senza cambiare
'      quella visibile. La virgola iniziale è fondamentale.
'    - `SWAP a, b`: Sebbene non usato qui nel ciclo principale, `SWAP` è un modo comune
'      per alternare i valori delle variabili che tengono traccia della pagina attiva e visibile.
'
' =================================================================================================
' === TEST MINIMALE DI DOUBLE BUFFERING ===
' Scrive su due pagine grafiche distinte e le scambia
' per verificare il funzionamento di SCREENSET.
' ----------------------------------------------------

SUB Main()
  ' 1. Attiva la modalità grafica e carica un font
  SCREEN 12
  FONT "font.ttf", 12 ' Font 12: niente raddoppio retina, testo leggibile

  ' 2. Disegna sulla PAGINA 0 (che è attiva e visibile di default)
  CLS 0 ' Pulisce la pagina 0 con il nero
  COLOR 14, 0 ' Giallo su nero
  LOCATE 200, 150
  PRINT "PAGINA 0"

  ' 3. Disegna sulla PAGINA 1 (nascosta)
  ' Imposta la PAGINA ATTIVA a 1. La pagina 0 rimane visibile. Stiamo disegnando "dietro le quinte".
  SCREENSET , 1
  CLS 4 ' Pulisce la pagina 1 (nascosta) con il rosso
  COLOR 15, 4 ' Bianco su rosso
  LOCATE 200, 150
  PRINT "PAGINA 1"

  ' 4. Mostra la pagina 0 e attendi
  ' Assicura che la PAGINA VISIBILE sia la 0.
  SCREENSET 0
  ' SLEEP 1 è un workaround per garantire che il renderer abbia il tempo di aggiornare
  ' lo schermo prima che WAITKEY blocchi l'esecuzione.
  SLEEP 1
  WAITKEY

  ' 5. Mostra la pagina 1 e attendi
  ' Scambia la pagina visibile, mostrando istantaneamente il contenuto che abbiamo disegnato sulla pagina 1.
  SCREENSET 1
  SLEEP 1
  WAITKEY
  
  ' 6. Loop di scambio pagine
  ' Dimostra un ciclo di animazione base in cui le pagine vengono alternate ad ogni pressione di un tasto.
  DIM CurrentPage AS INTEGER
  CurrentPage = 0
  ' Loop di animazione: esce con ESC (INKEY=CHR(27)), come SpritesExamples.
  DIM Key$ AS STRING
  DO
    ' Rende visibile la pagina calcolata nell'iterazione precedente.
    SCREENSET CurrentPage
    ' Inverte il valore per la prossima iterazione.
    IF CurrentPage = 0 THEN
      CurrentPage = 1
    ELSE
      CurrentPage = 0
    END IF
    Key$ = INKEY$
    IF Key$ = CHR$(27) THEN EXIT DO
    SLEEP 50
  LOOP
END SUB
