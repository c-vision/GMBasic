' FILE: graphics/TerrainWalkLive.bas
' =================================================================================================
'       TERRENO E PERSONAGGIO, IN MOVIMENTO — finestra grafica
' =================================================================================================
'
' `SCREEN 12` apre una finestra 640x480; il ciclo ridisegna il mondo a ogni fotogramma e `SYNC`
' lo mostra. Il personaggio cammina sempre al centro (x = 300): a scorrere è il terreno, come in
' un platform a scorrimento orizzontale. Si esce con ESC o chiudendo la finestra.
'
' Il mondo non esiste da nessuna parte: ogni quota è calcolata da CAM + x, dove CAM è il pixel di
' mondo sotto il bordo sinistro della finestra. Si può quindi scorrere all'infinito senza memoria
' e senza fine del terreno.
'
' Le forme sono disegnate con `LINE ... BF` (un rettangolo pieno per colonna o per fascia): una
' chiamata per forma, non un `PSET` per pixel. Su schermi Retina il renderer lavora su una pagina
' grande il doppio (qui 1280x960) e scala le coordinate: con linee da 1 pixel resterebbe scoperto
' il pixel nativo accanto, e il disegno apparirebbe a righe sottili. `BF` riempie l'area, quindi
' il risultato è pieno a qualunque renderScale.
'
' Per ricavarne un video: un PNG per fotogramma con `CANVAS SAVE 0, <file>` (handle 0 = pagina
' visibile) e montaggio con `swift tools/swift/pngs2mp4.swift <cartella> <file.mp4> 30`.
'
' Uso:  tools/sh/gmb_c99native_run.sh "$PWD/shared/samples/graphics/TerrainWalkLive.bas" chk ""
IMPORT GRAPHICS

CONST W = 640                 ' larghezza della finestra (SCREEN 12 = 640x480)
CONST H = 480
CONST VELOCE = 5              ' pixel di mondo percorsi a ogni fotogramma
CONST EROE_X = 300            ' il personaggio resta a questa colonna
CONST RITARDO = 33            ' millisecondi fra un fotogramma e l'altro (30 fps)
CONST ORIZZONTE = 300         ' ultima riga di cielo puro

' --- colori: RGB impacchettato, non indici di tavolozza ---
CONST C_SOLE = 0x40FFE7A3
CONST C_NUVOLA = 0x40F1F6FA
CONST C_COLLINA_LONTANA = 0x40769AA8
CONST C_COLLINA_VICINA = 0x404F7A86
CONST C_ERBA = 0x404E8C3A
CONST C_ERBA_SCURA = 0x40396228
CONST C_TERRA = 0x40584232
CONST C_OMBRA = 0x40304A22
CONST C_CAPELLI = 0x40503A2E
CONST C_TESTA = 0x40E9C08C
CONST C_MAGLIA = 0x40C1503C
CONST C_GAMBE = 0x4028323C

FUNCTION RGBc(r AS INTEGER, g AS INTEGER, b AS INTEGER) AS LONG
  RGBc = 0x40000000 + r * 65536 + g * 256 + b
END FUNCTION

' --- i profili del mondo, in coordinate di MONDO: ricevono wx, non la colonna dello schermo ---
FUNCTION Suolo(wx AS LONG) AS INTEGER
  Suolo = 340 - INT(22 * SIN(wx / 97.0) + 12 * SIN(wx / 37.0 + 1.7))
END FUNCTION

FUNCTION CollinaLontana(wx AS LONG) AS INTEGER
  CollinaLontana = 250 - INT(16 * SIN(wx / 141.0 + 0.6) + 8 * SIN(wx / 57.0))
END FUNCTION

FUNCTION CollinaVicina(wx AS LONG) AS INTEGER
  CollinaVicina = 285 - INT(18 * SIN(wx / 83.0 + 2.2) + 10 * SIN(wx / 41.0 + 0.4))
END FUNCTION

' --- un rettangolo pieno: `BF` riempie in una sola chiamata, e copre anche il pixel
' nativo che il renderer aggiunge su schermi Retina (renderScale 2) ---
SUB Rett(x0 AS INTEGER, y0 AS INTEGER, x1 AS INTEGER, y1 AS INTEGER, col AS LONG)
  LINE (x0, y0)-(x1 + 1, y1 + 1), col, BF
END SUB

' --- il personaggio: 30x52 pixel, il doppio della versione su canvas, per una finestra 640x480 ---
SUB Personaggio(px AS INTEGER, py AS INTEGER, passo AS INTEGER)
  CALL Rett(px + 4, py, px + 24, py + 4, C_OMBRA)

  ' testa: capelli e viso
  CALL Rett(px + 10, py - 52, px + 19, py - 50, C_CAPELLI)
  CALL Rett(px + 10, py - 49, px + 19, py - 44, C_TESTA)

  ' busto e cintura
  CALL Rett(px + 8, py - 42, px + 21, py - 26, C_MAGLIA)
  CALL Rett(px + 8, py - 25, px + 21, py - 24, C_GAMBE)

  ' braccia
  CALL Rett(px + 6, py - 40, px + 7, py - 28, C_GAMBE)
  CALL Rett(px + 22, py - 40, px + 23, py - 28, C_GAMBE)

  ' gambe e piedi: si alternano ogni quattro fotogrammi
  IF passo MOD 2 = 0 THEN
    CALL Rett(px + 8, py - 23, px + 11, py - 6, C_GAMBE)
    CALL Rett(px + 8, py - 5, px + 13, py - 1, C_GAMBE)
    CALL Rett(px + 18, py - 23, px + 21, py - 6, C_GAMBE)
    CALL Rett(px + 16, py - 5, px + 21, py - 1, C_GAMBE)
  ELSE
    CALL Rett(px + 6, py - 23, px + 9, py - 8, C_GAMBE)
    CALL Rett(px + 3, py - 7, px + 9, py - 1, C_GAMBE)
    CALL Rett(px + 18, py - 23, px + 21, py - 10, C_GAMBE)
    CALL Rett(px + 18, py - 9, px + 23, py - 1, C_GAMBE)
  END IF
END SUB

' --- un fotogramma: cielo, sole, nuvole, colline, terreno, personaggio ---
SUB DisegnaFrame(cam AS LONG, frame AS INTEGER)
  DIM x AS INTEGER, y AS INTEGER, dy AS INTEGER, dx AS INTEGER, y0 AS INTEGER
  DIM t AS DOUBLE, col AS LONG, passo AS INTEGER, py AS INTEGER

  ' 1. cielo a fasce orizzontali, colore interpolato (due righe per fascia)
  ' Le fasce sono alte due righe logiche ma la `BF` ne copre una in più: quella riga
  ' extra è quella che il renderer Retina userebbe altrimenti per lo sfondo scuro.
  FOR y = 0 TO ORIZZONTE STEP 2
    t = y / ORIZZONTE
    col = RGBc(INT(79 + 89 * t), INT(126 + 79 * t), INT(178 + 54 * t))
    LINE (0, y)-(W - 1, y + 2), col, BF
  NEXT y

  ' 2. sole: disco per corde orizzontali
  FOR dy = -28 TO 28
    dx = INT(SQR(784 - dy * dy))
    LINE (500 - dx, 90 + dy)-(500 + dx, 90 + dy + 1), C_SOLE, BF
  NEXT dy

  ' 3. nuvole con parallasse: si spostano a un quarto della velocità del terreno
  dx = 120 - INT(cam / 4) MOD 900
  CALL Rett(dx, 70, dx + 120, 82, C_NUVOLA)
  CALL Rett(dx + 30, 56, dx + 90, 69, C_NUVOLA)
  dx = 520 - INT(cam / 6) MOD 1100
  CALL Rett(dx, 140, dx + 96, 150, C_NUVOLA)
  CALL Rett(dx + 24, 128, dx + 72, 139, C_NUVOLA)

  ' 4. colline: una colonna per volta, dal profilo fino in fondo; il terreno le copre
  FOR x = 0 TO W - 1
    LINE (x, CollinaLontana(cam + x))-(x + 1, H - 1), C_COLLINA_LONTANA, BF
  NEXT x
  FOR x = 0 TO W - 1
    LINE (x, CollinaVicina(cam + x))-(x + 1, H - 1), C_COLLINA_VICINA, BF
  NEXT x

  ' 5. terreno: sei righe di erba (la prima più scura, fa da contorno) e terra sotto
  FOR x = 0 TO W - 1
    y0 = Suolo(cam + x)
    IF x MOD 9 = 0 THEN
      LINE (x, y0)-(x + 1, y0 + 1), C_ERBA_SCURA, BF
    ELSE
      LINE (x, y0)-(x + 1, y0 + 1), C_ERBA, BF
    END IF
    LINE (x, y0 + 1)-(x + 1, y0 + 6), C_ERBA, BF
    LINE (x, y0 + 7)-(x + 1, H - 1), C_TERRA, BF
  NEXT x

  ' 6. il personaggio, sempre al centro, con i piedi sulla quota del mondo sotto di lui
  py = Suolo(cam + EROE_X)
  passo = frame \ 4
  CALL Personaggio(EROE_X, py, passo)
END SUB

SUB Main()
  DIM f AS INTEGER, cam AS LONG
  DIM Key$ AS STRING

  SCREEN 12
  COLOR 15, 0
  CLS

  ' Una sola via d'uscita, la stessa per ESC e per la chiusura della finestra: quando l'utente
  ' chiude la finestra il runtime lo segnala come ESC, quindi `INKEY$` basta e non serve altro.
  f = 0
  DO
    CALL DisegnaFrame(cam, f)
    SYNC
    Key$ = INKEY$
    IF Key$ = CHR$(27) THEN EXIT DO
    SLEEP RITARDO
    cam = cam + VELOCE
    f = f + 1
  LOOP

  SCREEN 0
END SUB
