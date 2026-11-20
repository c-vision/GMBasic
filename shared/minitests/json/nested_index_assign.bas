' Assegnazione a indici ANNIDATI `a[i][j] = v` (dict dentro dict).
' Il parser nativo la desugara nella forma portabile su tutte le gambe:
'     DIM __t AS JSON ; __t = a[i] ; __t[j] = v
' e il valore estratto è un RIFERIMENTO (la scrittura resta visibile dal padre).
' Prima il C99 la rifiutava (e prima ancora perdeva la scrittura in silenzio assegnando a
' un nome artificiale "a[]").
' EXPECT: k=1|p=yes|chain=ok
IMPORT JSON
SUB Main()
  DIM g AS JSON
  g = NEW JSON
  DIM u AS STRING
  u = "bob"
  g[u] = NEW JSON
  g[u]["k"] = "1"
  PRINT "k="; g[u]["k"]

  DIM o AS JSON
  o = NEW JSON
  o["p"] = "yes"
  g[u]["owned"] = o
  PRINT "p="; g[u]["owned"]["p"]

  ' catena di tre livelli
  DIM h AS JSON
  h = NEW JSON
  h["a"] = NEW JSON
  h["a"]["b"] = NEW JSON
  h["a"]["b"]["c"] = "ok"
  PRINT "chain="; h["a"]["b"]["c"]
END SUB
