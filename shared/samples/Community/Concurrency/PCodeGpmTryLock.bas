' MUTEX.TRYLOCK: acquisizione non bloccante (parita' Go mu.TryLock(), Go 1.18).
' Valore booleano canonico GMBASIC = -1 (vero) / 0 (falso), come BOF/EOF/FOUND.
IMPORT CONCURRENCY
IMPORT MUTEX

SUB Main()
  DIM mu AS MUTEX
  DIM ok AS LONG
  ok = MUTEX.TRYLOCK(mu)
  IF ok = -1 THEN PRINT "TRYLOCK-1-OK"
  ok = MUTEX.TRYLOCK(mu)
  IF ok = 0 THEN PRINT "TRYLOCK-BLOCCATO-OK"
  MUTEX UNLOCK mu
END SUB
