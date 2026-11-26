' MiniTest: semantica di CONTAINS per struttura, identica sulle 4 gambe.
'  - MAPPE (TREE/DICT/…): Contains(x) = "la CHIAVE x esiste" (== ContainsKey), -1/0;
'  - SEQUENZE (LIST/…): Contains(x) = "l'ELEMENTO x esiste", -1/0.
' Il valore NON c'entra: `tree.Contains(10)` su una mappa a chiavi STRING è 0 anche se
' 10 è un valore presente (per i valori esisterebbe ContainsValue, NON esposto dal
' linguaggio: nessuna gamba lo implementa).
' Prima: IL inferiva VoidType per `mappa.Contains(...)` (Dictionary non ha Contains),
' PCode/VM-C rendevano nil, solo C99 rispondeva.
' EXPECT: tree.Contains(key a)=-1|tree.Contains(val 10)= 0|dict.Contains(key a)=-1|dict.Contains(val 10)= 0|list.Contains(10)=-1|list.Contains(99)= 0
IMPORT COLLECTIONS
SUB Main()
  DIM t AS TREE<STRING, INTEGER>
  t("a") = 10
  t("b") = 20
  PRINT "tree.Contains(key a)="; t.Contains("a")
  PRINT "tree.Contains(val 10)="; t.Contains(10)
  DIM d AS DICT
  d("a") = 10
  PRINT "dict.Contains(key a)="; d.Contains("a")
  PRINT "dict.Contains(val 10)="; d.Contains(10)
  DIM l AS LIST
  l.Add(10)
  l.Add(20)
  PRINT "list.Contains(10)="; l.Contains(10)
  PRINT "list.Contains(99)="; l.Contains(99)
END SUB
