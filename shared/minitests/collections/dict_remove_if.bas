' MiniTest COLLECTIONS — `DICT.Remove(k)` come PREDICATO (IF / WHILE), parità 4 gambe.
'
' `DICT.Remove(k)` restituisce -1 (rimosso) / 0 (assente) e si usa dentro una condizione.
' Su C99Native non compilava: l'emissione era un predicato numerico ma il tipo dell'espressione
' veniva classificato stringa/val, così `IF d.Remove(k) THEN` produceva
' `gmb_string_t _icond = <int>` (bug aperto C99Native-DICT-Remove-in-IF). Idem in WHILE.
'
' EXPECT: rm-ok|rm-absent|len=[ 2 ]|while-[ 1 ]|chain-ok|empty-ok
IMPORT COLLECTIONS

SUB Main()
  DIM d AS DICT OF STRING, STRING
  d("a") = "1"
  d("b") = "2"
  d("c") = "3"

  ' (1) IF con chiave presente
  IF d.Remove("a") THEN PRINT "rm-ok" ELSE PRINT "rm-fail"
  ' (2) IF con chiave assente → 0 = falso
  IF d.Remove("zz") THEN PRINT "rm-bad" ELSE PRINT "rm-absent"
  ' (3) conteggio dopo una rimozione
  PRINT "len=["; d.Count(); "]"

  ' (4) WHILE su Remove: scorre finché la chiave esiste (una sola volta)
  DIM n AS INTEGER
  n = 0
  WHILE d.Remove("b")
    n = n + 1
  WEND
  PRINT "while-["; n; "]"

  ' (5) catena AND: la prima vera (rimuove "c"), la seconda falsa (già via)
  IF d.Remove("c") AND NOT d.Remove("c") THEN PRINT "chain-ok" ELSE PRINT "chain-fail"

  ' (6) dict vuoto: Remove è falso, non un errore
  IF d.Remove("c") THEN PRINT "empty-bad" ELSE PRINT "empty-ok"
END SUB
