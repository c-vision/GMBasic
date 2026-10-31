' MiniTest: JSON.KEYS(v) — chiavi di un oggetto JSON (ordine di inserimento).
' Sul C99Native mancava del tutto ("simbolo non trovato: JSON.KEYS"); KEYS è un TokenKind
' condiviso (dict.Keys/collections) → il dispatch JSON si attiva solo per val/stringa.
' EXPECT: keys=["user","roles"]
IMPORT JSON

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM doc AS JSON = JSON.PARSE("{" + q + "user" + q + ": 1, " + q + "roles" + q + ": 2}")
  PRINT "keys="; JSON.STRINGIFY(JSON.KEYS(doc))
END SUB
