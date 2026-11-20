' FILE: graphics/TerrainWalk.bas
' =================================================================================================
'       TERRENO E PERSONAGGIO — un disegno fatto di pixel, su un canvas offscreen
' =================================================================================================
'
' Il programma disegna quattro fotogrammi di una camminata su un terreno, li affianca in una
' striscia PNG e stampa a video, per ogni fotogramma, dove si trovano i piedi del personaggio.
'
' Perché il canvas e non la finestra. `CANVAS` è un buffer RGBA in memoria: non apre nulla, non
' dipende dal sistema grafico e produce lo stesso identico bitmap su tutte e quattro le gambe
' (C99Native, IL, PCodeVM, VM/C). Con `SCREEN` e `PSET`/`LINE` lo stesso terreno si vedrebbe su
' una finestra, ma solo con un display attaccato; qui serve un disegno che si possa guardare e
' confrontare anche in headless, e che si possa salvare in un file.
'
' Il codice è in tre pezzi, e conviene leggerli in quest'ordine:
'   1. il PROFILO del terreno e quello delle colline, due funzioni di x fatte di sinusoidi;
'   2. il RIEMPIMENTO per colonne: per ogni x si riempie dal profilo in giù, cambiando colore
'      (erba in cima, terra sotto);
'   3. il PERSONAGGIO, quindici pixel per ventisei, disegnato a rettangoli con i piedi sulla
'      quota del terreno e le gambe che si alternano a ogni fotogramma.
'
' I colori si scrivono `0x40RRGGBB`: il flag 0x40000000 dice al motore «questo è un colore RGB».
' Senza flag il numero sarebbe un indice della tavolozza a sedici colori, che qui non basta.
'
' Uso:  tools/sh/gmb_c99native_run.sh "$PWD/shared/samples/graphics/TerrainWalk.bas" chk ""
IMPORT GRAPHICS

CONST W = 240                 ' larghezza di un fotogramma
CONST H = 140                 ' altezza di un fotogramma
CONST NF = 4                  ' fotogrammi nella striscia
CONST X0 = 36                 ' dove parte il personaggio nel primo fotogramma
CONST PASSO_X = 56            ' quanti pixel avanza per fotogramma
CONST ORIZZONTE = 84          ' ultima riga di cielo puro
CONST SOLE_X = 198            ' centro del sole
CONST SOLE_Y = 26
CONST SOLE_R = 11

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
CONST C_SEPARATORE = 0x40141820

' -------------------------------------------------------------------------------------------------
' Colore RGB impacchettato. Tutto il disegno passa di qui: un solo posto da cambiare se un giorno
' il formato dei colori cambiasse.
' -------------------------------------------------------------------------------------------------
FUNCTION RGBc(r AS INTEGER, g AS INTEGER, b AS INTEGER) AS LONG
  RGBc = 0x40000000 + r * 65536 + g * 256 + b
END FUNCTION

' -------------------------------------------------------------------------------------------------
' I profili. Sono funzioni, non tabelle: il terreno non è un dato da leggere, è una curva da
' calcolare. Due sinusoidi con periodi diversi bastano perché il profilo non si ripeta a vista
' d'occhio, e perché il personaggio salga e scenda invece di camminare su una riga.
' -------------------------------------------------------------------------------------------------
FUNCTION Suolo(x AS INTEGER) AS INTEGER
  Suolo = 112 - INT(10 * SIN(x / 43.0) + 6 * SIN(x / 15.0 + 1.7))
END FUNCTION

FUNCTION CollinaLontana(x AS INTEGER) AS INTEGER
  CollinaLontana = 78 - INT(6 * SIN(x / 61.0 + 0.6) + 3 * SIN(x / 24.0))
END FUNCTION

FUNCTION CollinaVicina(x AS INTEGER) AS INTEGER
  CollinaVicina = 88 - INT(7 * SIN(x / 37.0 + 2.2) + 4 * SIN(x / 18.0 + 0.4))
END FUNCTION

' -------------------------------------------------------------------------------------------------
' Un rettangolo pieno. Le scritture fuori dal buffer non sono un problema: `CANVAS SET` le
' rifiuta e restituisce -1, quindi non serve proteggere i bordi a mano.
' -------------------------------------------------------------------------------------------------
SUB Rett(c AS LONG, x0 AS INTEGER, y0 AS INTEGER, x1 AS INTEGER, y1 AS INTEGER, col AS LONG)
  DIM x AS INTEGER, y AS INTEGER
  FOR y = y0 TO y1
    FOR x = x0 TO x1
      CANVAS SET c, x, y, col
    NEXT x
  NEXT y
END SUB

' -------------------------------------------------------------------------------------------------
' Il personaggio: testa, maglia, braccia, gambe. Il parametro `passo` alterna le gambe — è tutto
' ciò che serve perché una figura immobile sembri camminare.
' -------------------------------------------------------------------------------------------------
SUB Personaggio(c AS LONG, px AS INTEGER, py AS INTEGER, passo AS INTEGER)
  ' ombra a terra: due righe sotto i piedi, sul prato
  CALL Rett(c, px + 2, py, px + 12, py + 2, C_OMBRA)

  ' testa: due righe di capelli e tre di viso, allineate sull'asse del corpo
  CALL Rett(c, px + 5, py - 26, px + 9, py - 25, C_CAPELLI)
  CALL Rett(c, px + 5, py - 24, px + 9, py - 22, C_TESTA)

  ' busto e cintura: la cintura stacca la maglia dai pantaloni
  CALL Rett(c, px + 4, py - 21, px + 10, py - 13, C_MAGLIA)
  CALL Rett(c, px + 4, py - 12, px + 10, py - 12, C_GAMBE)

  ' braccia: una colonna per lato, all'altezza delle spalle
  CALL Rett(c, px + 3, py - 20, px + 3, py - 14, C_GAMBE)
  CALL Rett(c, px + 11, py - 20, px + 11, py - 14, C_GAMBE)

  ' gambe: la gamba sinistra avanti nei fotogrammi pari, la destra nei dispari.
  ' Il piede è un rettangolo più largo della gamba: è ciò che rende leggibile il passo.
  IF passo MOD 2 = 0 THEN
    CALL Rett(c, px + 4, py - 11, px + 5, py - 3, C_GAMBE)
    CALL Rett(c, px + 4, py - 2, px + 6, py - 1, C_GAMBE)
    CALL Rett(c, px + 9, py - 11, px + 10, py - 3, C_GAMBE)
    CALL Rett(c, px + 8, py - 2, px + 10, py - 1, C_GAMBE)
  ELSE
    CALL Rett(c, px + 3, py - 11, px + 4, py - 4, C_GAMBE)
    CALL Rett(c, px + 2, py - 3, px + 4, py - 1, C_GAMBE)
    CALL Rett(c, px + 9, py - 11, px + 10, py - 5, C_GAMBE)
    CALL Rett(c, px + 9, py - 4, px + 11, py - 1, C_GAMBE)
  END IF
END SUB

' -------------------------------------------------------------------------------------------------
' Un fotogramma completo, dal fondo verso l'osservatore: cielo, sole, nuvole, colline, terreno,
' personaggio. L'ordine conta: chi viene dopo copre chi viene prima.
' -------------------------------------------------------------------------------------------------
SUB DisegnaFrame(c AS LONG, passo AS INTEGER, px AS INTEGER, py AS INTEGER)
  DIM x AS INTEGER, y AS INTEGER, dy AS INTEGER, dx AS INTEGER
  DIM t AS DOUBLE, col AS LONG

  ' 1. cielo: una riga per volta, dal celeste carico in alto al celeste pallido sull'orizzonte
  FOR y = 0 TO ORIZZONTE
    t = y / ORIZZONTE
    col = RGBc(INT(79 + 89 * t), INT(126 + 79 * t), INT(178 + 54 * t))
    FOR x = 0 TO W - 1
      CANVAS SET c, x, y, col
    NEXT x
  NEXT y

  ' 2. sole: il disco si disegna per righe, e ogni riga è larga quanto la corda del cerchio
  FOR dy = -SOLE_R TO SOLE_R
    dx = INT(SQR(SOLE_R * SOLE_R - dy * dy))
    CALL Rett(c, SOLE_X - dx, SOLE_Y + dy, SOLE_X + dx, SOLE_Y + dy, C_SOLE)
  NEXT dy

  ' 3. due nuvole: tre rettangoli sovrapposti bastano a farle leggere come nuvole
  CALL Rett(c, 42, 22, 74, 26, C_NUVOLA)
  CALL Rett(c, 50, 17, 68, 21, C_NUVOLA)
  CALL Rett(c, 118, 40, 146, 43, C_NUVOLA)
  CALL Rett(c, 126, 36, 140, 39, C_NUVOLA)

  ' 4. colline: due strati, dal profilo giù fino al suolo. Riempire fino al suolo, e non fino
  '    a una quota fissa, evita la striscia di cielo che resterebbe sotto una collina bassa.
  FOR x = 0 TO W - 1
    FOR y = CollinaLontana(x) TO Suolo(x)
      CANVAS SET c, x, y, C_COLLINA_LONTANA
    NEXT y
  NEXT x
  FOR x = 0 TO W - 1
    FOR y = CollinaVicina(x) TO Suolo(x)
      CANVAS SET c, x, y, C_COLLINA_VICINA
    NEXT y
  NEXT x

  ' 5. terreno: tre righe di erba, poi terra. La riga di contorno del prato è più scura,
  '    così il profilo si legge anche dove il colore è uniforme.
  FOR x = 0 TO W - 1
    FOR y = Suolo(x) TO H - 1
      IF y <= Suolo(x) + 3 THEN
        CANVAS SET c, x, y, C_ERBA
      ELSE
        CANVAS SET c, x, y, C_TERRA
      END IF
    NEXT y
    IF x MOD 7 = 0 THEN CANVAS SET c, x, Suolo(x), C_ERBA_SCURA
  NEXT x

  ' 6. il personaggio, in piedi sulla quota del terreno
  CALL Personaggio(c, px, py, passo)
END SUB

' -------------------------------------------------------------------------------------------------
SUB Main()
  DIM WK$ AS STRING, P$ AS STRING, ok AS LONG
  DIM c AS LONG, striscia AS LONG
  DIM f AS INTEGER, px AS INTEGER, py AS INTEGER, x AS INTEGER, y AS INTEGER

  WK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
  P$ = WK$ + "/terreno_striscia.png"

  ' la striscia è larga NF fotogrammi; il fondo scuro fa da cornice fra un fotogramma e l'altro
  CANVAS NEW W * NF, H INTO striscia
  CANVAS CLEAR striscia, C_SEPARATORE
  CANVAS NEW W, H INTO c

  FOR f = 0 TO NF - 1
    px = X0 + f * PASSO_X
    py = Suolo(px)

    CALL DisegnaFrame(c, f, px, py)
    PRINT "frame "; f; "  piedi a x="; px; " y="; py

    ' copia il fotogramma nella striscia: BLIT scrive sul canvas attivo, quindi si passa
    ' alla striscia, si copia, e si torna al fotogramma per il giro successivo.
    CANVAS USE striscia
    CANVAS BLIT c, f * W, 0
    CANVAS USE 0

    ' separatore verticale: un pixel scuro fra i fotogrammi
    IF f < NF - 1 THEN
      FOR y = 0 TO H - 1
        CANVAS SET striscia, (f + 1) * W - 1, y, RGBc(20, 24, 32)
      NEXT y
    END IF
  NEXT f

  CANVAS SAVE striscia, P$ INTO ok
  PRINT "striscia="; W * NF; "x"; H; "  png="; ok
  PRINT "file: "; P$

  CANVAS END
  CANVAS FREE c
  CANVAS FREE striscia
END SUB
