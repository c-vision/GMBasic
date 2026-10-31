' PCodeVM: LOAD TEXTURE in un LOOP e in un FINALLY (RETURN attraverso il cleanup),
' dentro una goroutine. Il park grafico (syscall LOAD_TEXTURE via ParkGraphics) viene
' completato dal MAIN thread (pump SDL in test_pcode_graphics_headless). Verifica tramite
' l'output NORMALE del programma: SCREEN 0 chiude la grafica e PRINT torna al console host,
' che l'harness cattura in result.Output — nessun file, nessuna modifica runtime.
' Headless: SDL_VIDEODRIVER=dummy test_pcode_graphics_headless.

IMPORT GRAPHICS
IMPORT CONCURRENCY

FUNCTION LoadViaFinally(BYREF handle AS LONG) AS LONG
  TRY
    RETURN 7
  FINALLY
    LOAD TEXTURE "sprite.png" INTO handle
  END TRY
END FUNCTION

SUB TextureLoader(wg AS WAITGROUP, Result AS CHANNEL OF INTEGER)
  DIM ok AS INTEGER = 1
  DIM i AS INTEGER
  DIM h AS LONG

  ' LOOP: tre caricamenti consecutivi.
  FOR i = 1 TO 3
    LOAD TEXTURE "sprite.png" INTO h
    IF h <= 0 THEN ok = 0
  NEXT i

  ' FINALLY: RETURN attraversa il cleanup che parcheggia su LOAD TEXTURE.
  DIM fh AS LONG
  DIM rv AS LONG = LoadViaFinally(fh)
  IF fh <= 0 THEN ok = 0
  IF rv <> 7 THEN ok = 0

  SEND ok TO Result
  DONE wg
END SUB

SUB Main()
  SCREEN 12
  DIM wg AS WAITGROUP
  DIM Result AS CHANNEL OF INTEGER
  GO SUB TextureLoader(wg, Result) IN wg
  DIM v AS INTEGER
  RECEIVE v FROM Result
  IF v = 1 THEN
    CPRINT "GFX-LOADED-OK"
  ELSE
    CPRINT "FAIL:GFX-LOADED"
  END IF
  WAIT wg
  CPRINT "GFX-LOOP-FINALLY-DONE"
END SUB
