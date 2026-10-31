' CLIPPER 5.3 — macro operator & (expr, variable, nesting); `&` dentro un letterale stringa e' SEMPRE testo
' EXPECT: m1= 21|m2= 10|m3= 5|m4=hi|m5=Hello &cName!|m6=&cLocal
' Nessuna "text substitution" nei letterali: `"Hello &cName!"` e `"&cLocal"` restano testo (m5, m6), globali o locali che siano.
GLOBAL X AS INTEGER
GLOBAL cName AS STRING
SUB Main()
  X = 10
  DIM cVar AS STRING = "X"
  DIM cExpr AS STRING = "X * 2 + 1"
  PRINT "m1="; &(cExpr)          ' 21
  PRINT "m2="; &cVar             ' 10  (macro variable reference)
  PRINT "m3="; &("2+3")          ' 5
  DIM cThree AS STRING = "hi"
  DIM cTwo AS STRING = "cThree"
  DIM cOne AS STRING = "&cTwo"
  PRINT "m4="; &cOne             ' hi  (nested macro)
  cName = "world"
  PRINT "m5=Hello &cName!"       ' Hello &cName!  (letterale: mai espanso, anche se cName e' globale)
  DIM cLocal AS STRING = "no"
  PRINT "m6=&cLocal"             ' &cLocal  (letterale)
END SUB
