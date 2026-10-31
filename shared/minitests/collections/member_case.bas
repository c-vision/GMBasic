' EXPECT: low= 3|canon= 3|get= 1|["a","b"]
IMPORT COLLECTIONS
IMPORT JSON
SUB Main()
  ' F1 Incongruenze — membri collection in GRAFIE DIVERSE (.count()/.Count()):
  ' il dispatch usa il TokenKind (regola d'oro), il lexeme e' solo grafia.
  ' Stesso identico risultato per .count() e .Count().
  DIM l AS LIST OF INTEGER = NEW LIST OF INTEGER
  l.Add(1) : l.Add(2) : l.Add(3)
  PRINT "low="; l.count()
  PRINT "canon="; l.Count()
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  d.set("a", 1) : d.add("b", 2)
  PRINT "get="; d.get("a")
  PRINT JSON.Stringify(d.keys())
END SUB
