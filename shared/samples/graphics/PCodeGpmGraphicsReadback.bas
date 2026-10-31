' PCodeVM: SPRITE GETPIXELS (readback) in un LOOP e in un FINALLY (RETURN attraverso
' il cleanup), dentro una goroutine. Il park grafico (ParkGraphics) viene completato
' dal MAIN thread (pump SDL in test_pcode_graphics_headless). Verifica tramite l'output
' NORMALE del programma: CPRINT forza il marker sul console host, catturato in
' result.Output. Solo primitives GPM — nessuna modifica runtime/compiler.
' Headless: SDL_VIDEODRIVER=dummy test_pcode_graphics_headless.

IMPORT GRAPHICS
IMPORT CONCURRENCY

FUNCTION CaptureFinally(BYREF src AS LONG, BYREF px() AS INTEGER) AS LONG
  TRY
    RETURN 7
  FINALLY
    SPRITE GETPIXELS src, px
  END TRY
END FUNCTION

SUB ReadbackWork(wg AS WAITGROUP, Result AS CHANNEL OF INTEGER)
  DIM ok AS INTEGER = 1
  DIM src AS LONG
  DIM i AS INTEGER

  SPRITE LOAD "sprite.png", src
  IF src <= 0 THEN ok = 0

  ' LOOP: readback 3x — il park deve completare via pump.
  DIM px(10) AS INTEGER
  FOR i = 1 TO 3
    SPRITE GETPIXELS src, px
    IF UBOUND(px) < 1 OR px(1) <= 0 THEN ok = 0
  NEXT i

  ' FINALLY: RETURN attraversa il cleanup che parcheggia sul readback.
  DIM fpx(10) AS INTEGER
  DIM rv AS LONG = CaptureFinally(src, fpx)
  IF UBOUND(fpx) < 1 OR fpx(1) <= 0 THEN ok = 0
  IF rv <> 7 THEN ok = 0

  SEND ok TO Result
  DONE wg
END SUB

SUB Main()
  SCREEN 12
  DIM wg AS WAITGROUP
  DIM Result AS CHANNEL OF INTEGER
  GO SUB ReadbackWork(wg, Result) IN wg
  DIM v AS INTEGER
  RECEIVE v FROM Result
  IF v = 1 THEN
    CPRINT "GFX-READBACK-OK"
  ELSE
    CPRINT "FAIL:GFX-READBACK"
  END IF
  WAIT wg
  CPRINT "GFX-READBACK-DONE"
END SUB
