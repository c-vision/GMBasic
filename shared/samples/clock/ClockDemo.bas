' ClockDemo.bas — il dominio CLOCK: un ciclo a passo fisso che non dipende dalla macchina.
'
' Il programma non dorme e non misura il tempo da sé: chiede al motore quanto dura UN passo
' (CLOCK.DT()), quanti passi al secondo sono nominali (CLOCK.FPS()) e quanta parte di passo
' resta in coda (CLOCK.ALPHA()). Poi governa il tempo: rallenta (SCALE), ferma (PAUSE) e
' riprende (RESUME) — leggendo sempre lo stato, mai assumendolo.
'
' `IMPORT CLOCK` è OBBLIGATORIA: il dominio non è implicito. Senza l'IMPORT il programma non
' compila — `namespace non importato: CLOCK (aggiungi IMPORT CLOCK).` — sia per il comando sia
' per le letture qualificate.
'
' L'output è interamente deterministico (interi): le stesse righe su C99Native, IL, PCodeVM e
' VM/C, senza finestre e senza attese.
IMPORT CLOCK

SUB Main()
  ' ── 1. Il passo nominale ──────────────────────────────────────────────────────
  ' 60 Hz = sessanta passi di simulazione al secondo; DT è la durata di un passo.
  CLOCK FPS 60
  PRINT "fps nominali: "; INT(CLOCK.FPS() + 0.5)
  PRINT "dt in ms:     "; INT(CLOCK.DT() * 1000 + 0.5)

  ' ── 2. Una simulazione a passo fisso ─────────────────────────────────────────
  ' Sessanta passi valgono un secondo di tempo simulato: la distanza percorsa non
  ' dipende da quanto è veloce la macchina, perché il passo è fisso.
  DIM i AS LONG
  DIM x AS DOUBLE
  FOR i = 1 TO 60
    x = x + 2.5 * CLOCK.DT()          ' velocità: 2,5 unità al secondo
  NEXT
  PRINT "x dopo 60 passi (cent.): "; INT(x * 100 + 0.5)

  ' ── 3. Slow motion, pausa e ripresa ──────────────────────────────────────────
  ' La scala moltiplica il tempo che entra: 0,5 = metà velocità, 0 = fermo.
  CLOCK SCALE 0.5
  PRINT "scala (centesimi): "; INT(CLOCK.SCALE() * 100 + 0.5)

  ' La pausa azzera la coda del tempo in sospeso: ALPHA riparte da zero.
  CLOCK PAUSE
  PRINT "alpha in pausa: "; INT(CLOCK.ALPHA() + 0.5)
  CLOCK RESUME
  CLOCK SCALE 1
  PRINT "scala dopo resume: "; INT(CLOCK.SCALE() * 100 + 0.5)

  ' ── 4. Cambiare frequenza cambia il passo, non il programma ──────────────────
  ' A 120 Hz lo stesso secondo di simulazione vale 120 passi più corti: è la stessa
  ' simulazione, campionata più fine.
  CLOCK FPS 120
  PRINT "dt a 120 Hz: "; INT(CLOCK.DT() * 1000 + 0.5)
END SUB
