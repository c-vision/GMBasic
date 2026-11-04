' MiniTest COLLECTIONS — CACHE.TryGetValue(key, out): predicato + OUT-PARAM assegnato.
' Regressione: l'out-param non veniva scritto su nessuna gamba (la variabile restava
' invariata, "Trovato key1: " senza valore). Semantica canonica: se la chiave esiste il
' valore è assegnato e il predicato è -1; se non esiste la variabile resta INVARIATA e il
' predicato è 0. Copre stringa e numerico, valore presente e assente.
'
' EXPECT: found=-1 |v=value1|miss= 0 |w=|nfound=-1 |n= 7|nmiss= 0 |n2= 99
IMPORT COLLECTIONS

SUB Main()
  DIM c AS CACHE<STRING, STRING>
  c.Set("key1", "value1")

  DIM v$ AS STRING
  PRINT "found="; c.TryGetValue("key1", v$); "|v="; v$

  DIM w$ AS STRING
  PRINT "miss="; c.TryGetValue("nope", w$); "|w="; w$

  DIM cn AS CACHE<STRING, INTEGER>
  cn.Set("k", 7)
  DIM n AS INTEGER = 99
  PRINT "nfound="; cn.TryGetValue("k", n); "|n="; n

  DIM m AS INTEGER = 99
  PRINT "nmiss="; cn.TryGetValue("x", m); "|n2="; m
END SUB
