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
  
  ' Imposta la modalità grafica e carica un font.
  ' Finestra PIÙ GRANDE (1280×800, 32bpp). SW/SH sono le dimensioni LOGICHE usate dal
  ' programma (bordi di rimbalzo e griglia): se cambi la SCREEN, aggiorna anche questi due.
  CONST SW = 1280
  CONST SH = 800
  SCREEN 12, SW, SH, 32
  FONT "font.ttf", 16
  
  ' Carica l'immagine dello sprite.
  ' 50 sprite distanziati su una griglia 10×5 nella finestra 1280×800: player.png è 32×32,
  ' quindi c'è molto spazio fra loro e i contatti restano RADI e leggibili.
  CONST NumSprites = 50
  ' MANOPOLE: FrameDelay = pausa in MILLISECONDI a ogni frame (più alto = più lento);
  '           VelBase   = velocità massima in px/frame (più alto = più veloce).
  CONST FrameDelay = 6
  CONST VelBase = 1.6
  ' ACCELERAZIONE PROGRESSIVA: man mano che gli aerei esplodono gli altri accelerano.
  ' Boost va da 1× (tutti vivi) a 1+BoostMax (quasi finiti): BoostMax = 3 → fino a 4×.
  CONST BoostMax = 3.0
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
  ' Vivo(handle) = 1 finché lo sprite è in scena: serve a contare esattamente le rimozioni
  ' (un id può comparire in più eventi di collisione consecutivi finché non viene spento).
  GLOBAL Vivo(300) AS INTEGER
  FOR I = 0 TO NumSprites - 1
    Vivo(SprID(I)) = 1
  NEXT I
  
  ' --- EVENTI COLLISIONE (rilevate dal motore nel SYNC) ---
  ' ON SPRITE COLLISION: la callback riceve (x, y) della collisione
  ON SPRITE COLLISION OnCollision
  ' ON SPRITE COLLISIONS: la callback riceve l'array degli id collidenti
  ON SPRITE COLLISIONS OnCollisions
  
  ' Crea gli array per contenere lo stato degli sprite
  DIM PosX(NumSprites), PosY(NumSprites)
  DIM VelX(NumSprites), VelY(NumSprites)
  
  ' Griglia 10×5 distribuita su tutta la finestra SW×SH.
  FOR I = 0 TO NumSprites - 1
    PosX(I) = 60 + (I MOD 10) * ((SW - 120) / 9)
    PosY(I) = 80 + (I \ 10) * ((SH - 160) / 4)
    VelX(I) = ((RND() * 2) - 1) * VelBase
    VelY(I) = ((RND() * 2) - 1) * VelBase
  NEXT I
  
  ' --- GAME LOOP PRINCIPALE ---
  DIM Boost AS DOUBLE
  DO
    ' 1. Pulisce lo schermo a ogni frame
    CLS 1 ' Sfondo blu
    
    ' 2. Fase di UPDATE (mosso dalla CPU)
    ' ACCELERAZIONE PROGRESSIVA: ogni aereo esploso rende gli altri un po' più veloci
    ' (Alive scende → Boost sale: 1× all'inizio, 1+BoostMax quando ne restano pochi).
    Boost = 1 + (((NumSprites - Alive) / NumSprites) * BoostMax)
    ' Questo loop aggiorna la logica di gioco (le posizioni).
    FOR I = 0 TO NumSprites - 1
      ' Aggiorna posizione
      PosX(I) = PosX(I) + VelX(I) * Boost
      PosY(I) = PosY(I) + VelY(I) * Boost
      
      ' Bordi: RIMBALZO (non wrapping) con cambio di direzione e velocità casuale.
      ' Le coordinate dello sprite sono il CENTRO (il motore disegna a x-w/2): i bordi sono
      ' a 16 px (metà sprite) e a SW/SH-16.
      IF PosX(I) < 16 THEN
        PosX(I) = 16
        VelX(I) = ABS(VelX(I)) * (0.6 + RND() * 0.8)
      ELSEIF PosX(I) > SW - 16 THEN
        PosX(I) = SW - 16
        VelX(I) = -ABS(VelX(I)) * (0.6 + RND() * 0.8)
      END IF
      IF PosY(I) < 16 THEN
        PosY(I) = 16
        VelY(I) = ABS(VelY(I)) * (0.6 + RND() * 0.8)
      ELSEIF PosY(I) > SH - 16 THEN
        PosY(I) = SH - 16
        VelY(I) = -ABS(VelY(I)) * (0.6 + RND() * 0.8)
      END IF
      
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
    
    ' CADENZA: pausa breve per frame (FrameDelay ms, costante in testa al programma).
    ' Senza, il loop gira a centinaia di frame/s e le collisioni "volano": non si capisce
    ' chi si è toccato. 10 ms ≈ 60-80 frame/s, fluido e osservabile.
    SLEEP FrameDelay

    ' Controlla se l'utente preme ESC per uscire
    INKEY K$
    IF K$ <> "" THEN EXIT DO
    ' FINE: quando resta UN solo sprite non può più collidere (una collisione richiede due
    ' sprite). Prima si usciva lasciandolo a schermo ("il programma ha finito con ancora
    ' sprite sullo schermo"): qui si spengono gli ultimi rimasti e la fiamma, così la scena
    ' resta VUOTA e il ciclo non può diventare infinito.
    IF Alive <= 1 THEN
      FOR I = 0 TO NumSprites - 1
        IF Vivo(SprID(I)) = 1 THEN
          SPRITE OFF SprID(I)
          Vivo(SprID(I)) = 0
          Alive = Alive - 1
        END IF
      NEXT I
      SPRITE OFF ExplosionID
      EXIT DO
    END IF
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
' sprite in collisione ed elimina la COPPIA che si è toccata.
' ================================================================
SUB OnCollisions(ids() AS DOUBLE)
  ' Si eliminano ENTRAMBI gli sprite della prima coppia in collisione: si vede subito chi
  ' si è scontrato e chi resta (prima se ne toglieva uno solo, ogni 10 collisioni, e restava
  ' un ultimo sprite solitario che non poteva più collidere → ciclo INFINITO).
  ' `Vivo(handle)` rende `Alive` esatto: lo stesso id può ricomparire negli eventi
  ' successivi finché lo sprite non è spento, e non va contato due volte.
  '
  ' ATTENZIONE: l'ESPLOSIONE è a sua volta uno sprite (48×48). Mentre è visibile entra
  ' nell'elenco delle collisioni e "tocca" gli aerei vicini: se si ignorasse l'evento
  ' intero si perderebbero anche le collisioni VERE aerei-aerei, se si contasse
  ' l'esplosione si eliminerebbero aerei che non si sono scontrati. Quindi: si tiene
  ' l'elenco dei soli AEREI e si elimina la coppia SOLO se ce ne sono almeno due
  ' (un evento con l'esplosione contiene al più un aereo).
  DIM n AS INTEGER
  DIM k AS INTEGER
  DIM h AS INTEGER
  DIM rimossi AS INTEGER
  DIM na AS INTEGER
  DIM aerei(128) AS INTEGER
  n = UBOUND(ids)
  na = 0
  FOR k = 0 TO n
    h = ids(k)
    IF h > 0 AND h <= 300 AND h <> ExplosionID THEN
      aerei(na) = h
      na = na + 1
    END IF
  NEXT k
  IF na < 2 THEN EXIT SUB            ' solo un aereo (o nessuno): niente coppia reale
  rimossi = 0
  FOR k = 0 TO na - 1
    h = aerei(k)
    IF Vivo(h) = 1 THEN
      SPRITE OFF h
      Vivo(h) = 0
      Alive = Alive - 1
      rimossi = rimossi + 1
      ' Una COPPIA per evento: le rimozioni restano leggibili (una alla volta).
      IF rimossi >= 2 THEN EXIT FOR
    END IF
  NEXT k
END SUB
