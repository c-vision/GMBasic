' MiniTest: AI.SyncFolder con vault inesistente -> conteggi 0/0/0 (nessuna scrittura,
' nessun errore: contratto "vault assente" deterministico).
' EXPECT: sync=0/0/0
IMPORT AI WITH ALIAS
SUB Main()
  PRINT "sync="; AI.SyncFolder("novault-zzz", "/tmp", "in")
END SUB
