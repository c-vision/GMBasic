' MiniTest: FOR EACH su DICT — coppie k,v e KeyValuePair (ordine di inserimento).
' Sul VM/C era rotto: UBOUND(dict) dava -1 e l'accesso posizionale cercava la chiave
' "0"/"1"/... invece della N-esima entry → 0 iterazioni (chiuso 2026-09-18).
IMPORT COLLECTIONS
SUB Main()
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  d.Set("a", 1)
  d.Set("b", 2)
  d.Set("c", 3)
  DIM ks = 0 : DIM vs = 0
  FOR EACH k, v IN d
    ks = ks + 1 : vs = vs + v
  NEXT
  PRINT "ks="; ks; " vs="; vs
  DIM kv = 0
  FOR EACH e IN d
    kv = kv + e.Value
  NEXT
  PRINT "kv="; kv
END SUB
' EXPECT: ks= 3  vs= 6|kv= 6
