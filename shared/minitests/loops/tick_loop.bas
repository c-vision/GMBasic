' MiniTest — ciclo TICK su tutte e 4 le gambe (IL, PCodeVM, VM/C, C99Native).
' Il TICK è un frame loop: il corpo gira, `EXIT TICK` esce, e il rate (0 = nessun limiter) NON
' cambia il risultato. Qui si verificano i VALORI: il ritmo del frame non è deterministico e non
' entra in un oracle (si misura a parte, con una sonda a tempo di parete).
' EXPECT: n= 5|m= 3|k= 2
DIM n AS INTEGER
DIM m AS INTEGER
DIM k AS INTEGER

' rate esplicito 0 = nessun limiter, corpo eseguito fino a EXIT TICK.
n = 0
TICK 0
  n = n + 1
  IF n >= 5 THEN EXIT TICK
END TICK

' TICK senza rate: stesso default 0 (nessun limiter) su tutte e 4 le gambe.
m = 0
TICK
  m = m + 1
  IF m >= 3 THEN EXIT TICK
END TICK

' END TICK [rate]: il rate di chiusura sovrascrive quello di apertura (qui 0, come sopra).
k = 0
TICK 60
  k = k + 1
  IF k >= 2 THEN EXIT TICK
END TICK 0

PRINT "n="; n
PRINT "m="; m
PRINT "k="; k
