' EXPECT: dt= 17|fps= 60|scala= 1|alpha= 0
' TEMPO A PASSO FISSO — le letture del dominio CLOCK allo stato iniziale.
' L'orologio parte a 60 Hz (default del motore): un passo dura 1/60 s = 16,66 ms, la scala è 1
' (tempo reale) e ALPHA è 0 perché la coda è vuota. Nessuna lettura dipende dal tempo di parete:
' questi quattro numeri sono gli stessi su tutte e quattro le gambe.
IMPORT CLOCK

SUB Main()
  PRINT "dt="; INT(CLOCK.DT() * 1000 + 0.5)
  PRINT "fps="; INT(CLOCK.FPS() + 0.5)
  PRINT "scala="; INT(CLOCK.SCALE() + 0.5)
  PRINT "alpha="; INT(CLOCK.ALPHA() + 0.5)
END SUB
