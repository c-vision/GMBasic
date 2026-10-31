' FILE: Sprites/SpritesExamples.bas
' ======================================================================================================================
' GUIDA TEORICA: RENDERING ACCELERATO DI SPRITE IN BATCH
' ======================================================================================================================
' Questo programma dimostra l'architettura ad alte prestazioni di GM-BASIC per la gestione degli sprite,
' progettata per renderizzare migliaia di oggetti in modo efficiente.
'
' 1. IL MODELLO A DUE FASI (UPDATE E DRAW)
'    - La logica del programma è separata in due fasi distinte all'interno del "Game Loop" principale (`DO...LOOP`):
'      - Fase di Update (Logica di Gioco): Un ciclo `FOR` aggiorna lo *stato* di ogni sprite (posizione, velocità).
'        Questo viene eseguito sulla CPU.
'      - Fase di Draw (Rendering): Un singolo comando `SYNC` delega il rendering effettivo alla GPU.
'
' 2. IL RUOLO DEL COMANDO `SPRITE`
'    - `SPRITE LOAD <file$>, <handleVar>`: Carica un'immagine dalla memoria di massa e la carica nella VRAM (memoria
'      della GPU), associandola a un handle (un ID numerico). Questa operazione è relativamente lenta e va
'      eseguita una sola volta all'inizio.
'    - `SPRITE SET <subcomando> <handle>, ...`: Modifica le proprietà (stato) di uno sprite già caricato.
'      Esempio: `SPRITE SET POSITION <handle>, x, y`.
'      - Queste operazioni sono estremamente veloci perché modificano solo dati in un buffer di stato. NON
'        eseguono alcun disegno.
'
' 3. IL COMANDO `SYNC`
'    - `SYNC` è il comando chiave per il rendering in batch. Quando viene chiamato:
'      1. L'interprete invia l'intero buffer di stato degli sprite (contenente posizione, rotazione, scala, etc.
'         per *tutti* gli sprite) alla scheda grafica.
'      2. La GPU esegue il rendering di tutti gli sprite in un'unica operazione ottimizzata ("batch").
'      3. L'immagine finale viene presentata sullo schermo.
'
' 4. VANTAGGIO DI PRESTAZIONI
'    - Questa architettura disaccoppia la logica di gioco dal rendering. La CPU può calcolare il frame successivo
'      mentre la GPU sta ancora disegnando quello precedente.
'    - Il rendering in batch è esponenzialmente più veloce che disegnare ogni sprite individualmente, in quanto
'      riduce drasticamente la comunicazione tra CPU e GPU.
' ======================================================================================================================

' Importa il namespace GRAPHICS per accedere ai comandi SPRITE, SYNC, etc.
IMPORT GRAPHICS

SUB Main()
  CLS
  PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ SpritesExamples - Test di Rendering in Batch                                ║"
  PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
  
  ' Imposta la modalità grafica e carica un font
  SCREEN 12 ' 640x480, 16 colori
  FONT "font.ttf", 16
  
  ' Carica l'immagine dello sprite.
  ' Carica 60 sprite distinti (un handle per sprite).
  CONST NumSprites = 150
  DIM SprID(NumSprites) AS LONG
  FOR I = 0 TO NumSprites - 1
    SPRITE LOAD "player.png", SprID(I)
    IF SprID(I) = 0 THEN
      COLOR 12 : PRINT "Errore: Impossibile caricare player.png"
      END
    END IF
  NEXT I
  ' Sprite esplosione (nascosta all'inizio): compare per pochi frame quando due sprite collidono
  GLOBAL ExplosionID AS LONG
  SPRITE LOAD "explosion.png", ExplosionID
  IF ExplosionID = 0 THEN
    COLOR 12 : PRINT "Errore: Impossibile caricare explosion.png"
    END
  END IF
  SPRITE OFF ExplosionID
  GLOBAL ExplosionTimer = 0
  DIM ExplosionX = 0
  DIM ExplosionY = 0
  GLOBAL Alive = NumSprites
  
  ' --- EVENTI COLLISIONE (rilevate dal motore nel SYNC) ---
  ' ON SPRITE COLLISION: la callback riceve (x, y) della collisione
  ON SPRITE COLLISION OnCollision
  ' ON SPRITE COLLISIONS: la callback riceve l'array degli id collidenti
  ON SPRITE COLLISIONS OnCollisions
  
  ' Crea gli array per contenere lo stato degli sprite
  DIM PosX(NumSprites), PosY(NumSprites)
  DIM VelX(NumSprites), VelY(NumSprites)
  
  ' Inizializza ogni sprite su una GRIGLIA (non sovrapposti: collisioni graduali) con
  ' velocità casuali
  FOR I = 0 TO NumSprites - 1
    PosX(I) = 24 + (I MOD 15) * 40
    PosY(I) = 24 + (I \ 15) * 40
    VelX(I) = (RND() * 2) - 1
    VelY(I) = (RND() * 2) - 1
  NEXT I
  
  ' --- GAME LOOP PRINCIPALE ---
  DO
    ' 1. Pulisce lo schermo a ogni frame
    CLS 1 ' Sfondo blu
    
    ' 2. Fase di UPDATE (mosso dalla CPU)
    ' Questo loop aggiorna la logica di gioco (le posizioni).
    FOR I = 0 TO NumSprites - 1
      ' Aggiorna posizione
      PosX(I) = PosX(I) + VelX(I)
      PosY(I) = PosY(I) + VelY(I)
      
      ' Controllo dei bordi (wrapping)
      IF PosX(I) < 0 THEN PosX(I) = 640
      IF PosX(I) > 640 THEN PosX(I) = 0
      IF PosY(I) < 0 THEN PosY(I) = 480
      IF PosY(I) > 480 THEN PosY(I) = 0
      
      ' Imposta la posizione dello sprite nel buffer di stato.
      ' Questa è un'operazione molto veloce che non disegna nulla.
      SPRITE SET POSITION SprID(I), PosX(I), PosY(I)
    NEXT I
    
    ' 3. Fase di DRAW (mosso dalla GPU)
    ' Il comando SYNC dice al motore grafico di disegnare tutti gli sprite
    ' le cui proprietà sono state modificate in un unico batch.
    SYNC
    
    ' Esplosione: visibile per 3 frame dopo una collisione, poi OFF
    IF ExplosionTimer > 0 THEN
      ExplosionTimer = ExplosionTimer - 1
      IF ExplosionTimer = 0 THEN SPRITE OFF ExplosionID
    END IF
    
    ' Controlla se l'utente preme ESC per uscire
    INKEY K$
    IF K$ <> "" THEN EXIT DO
    IF Alive <= 10 THEN EXIT DO ' ferma quando restano 10 o meno sprite attivi
  LOOP
  ' Chiude la finestra grafica e torna alla modalità testo
  ' Attende un tasto REALE prima di chiudere (WAITKEY in headless non blocca:
  ' il keyboard service restituisce subito e la finestra si chiude dopo 1s)
  PRINT "Animazione terminata: sprite rimasti "; Alive
  PRINT "Premi un tasto per chiudere..."
  SLEEP 800
  SCREEN 0
END SUB

' ================================================================
' Handler evento ON SPRITE COLLISION: riceve (x, y) della collisione
' e mostra l'esplosione per pochi frame.
' ================================================================
SUB OnCollision(x AS DOUBLE, y AS DOUBLE)
  SPRITE SET POSITION ExplosionID, x, y
  SPRITE ON ExplosionID
  ExplosionTimer = 20
END SUB

' ================================================================
' Handler evento ON SPRITE COLLISIONS: riceve l'array degli id degli
' sprite in collisione e li elimina (SPRITE OFF).
' ================================================================
SUB OnCollisions(ids() AS DOUBLE)
  ' Elimina UNO sprite per collisione (le eliminazioni massicce svuotavano la
  ' demo in pochi secondi). Gli altri della coppia restano per il prossimo evento.
  DIM N = UBOUND(ids)
  IF N > 0 THEN
    SPRITE OFF ids(0)
    Alive = Alive - 1
  END IF
END SUB
