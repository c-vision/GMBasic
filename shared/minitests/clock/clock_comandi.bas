' EXPECT: fps= 30|dt= 33|scala= 50|alpha= 0|fps2= 120|dt2= 8|scala2= 100
' TEMPO A PASSO FISSO — i comandi del dominio CLOCK: FPS, SCALE, PAUSE, RESUME.
' Ogni comando si verifica con la LETTURA che gli corrisponde (§3.1: il comando scrive, la
' funzione legge lo stesso stato). I numeri sono interi esatti: DT è 1/FPS e la scala si legge
' in centesimi (50 = 0,5; 100 = 1). La pausa azzera la coda, quindi ALPHA torna 0; RESUME non
' riporta indietro il tempo fermo. Nessun valore dipende dal tempo di parete: identici sulle
' quattro gambe.
IMPORT CLOCK

SUB Main()
  ' ── frequenza del passo ──────────────────────────────────────────────────────
  CLOCK FPS 30
  PRINT "fps="; INT(CLOCK.FPS() + 0.5)
  PRINT "dt="; INT(CLOCK.DT() * 1000 + 0.5)

  ' ── scala del tempo (slow motion) ────────────────────────────────────────────
  CLOCK SCALE 0.5
  PRINT "scala="; INT(CLOCK.SCALE() * 100 + 0.5)

  ' ── pausa: la coda si azzera, il tempo fermo non si consuma al resume ────────
  CLOCK PAUSE
  PRINT "alpha="; INT(CLOCK.ALPHA() + 0.5)
  CLOCK RESUME

  ' ── cambiare frequenza cambia il passo, non il programma ────────────────────
  CLOCK FPS 120
  PRINT "fps2="; INT(CLOCK.FPS() + 0.5)
  PRINT "dt2="; INT(CLOCK.DT() * 1000 + 0.5)

  ' ── ritorno alla scala reale ────────────────────────────────────────────────
  CLOCK SCALE 1
  PRINT "scala2="; INT(CLOCK.SCALE() * 100 + 0.5)
END SUB
