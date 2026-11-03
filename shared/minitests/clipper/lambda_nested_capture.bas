' CLIPPER 5.3 — catture nelle lambda ANNIDATE (una lambda dentro un'altra).
' Ogni livello intermedio cattura per conto proprio le variabili che servono ai
' livelli interni: parametri dell'esterno, catture del chiamante, parametri di
' funzione e globali, a profondità arbitraria (qui 2, 3 e 4 livelli).
' EXPECT: two= 16|three= 16|four= 20|mixed= 1016|mut= 16|kmut= 21|ret= 42
FUNCTION MkAdder(k AS INTEGER) AS FUNCTION(x) AS DOUBLE
  RETURN { |x| RETURN x + k }
END FUNCTION

SUB Main()
  DIM k AS INTEGER
  k = 10
  ' due livelli: parametro dell'esterno + cattura del chiamante
  DIM outer = { |x| RETURN { |y| RETURN x + y + k } }
  PRINT "two="; EVAL(EVAL(outer, 5), 1)          ' 16
  ' tre livelli
  DIM g = { |a| RETURN { |b| RETURN { |c| RETURN a + b + c + k } } }
  PRINT "three="; EVAL(EVAL(EVAL(g, 1), 2), 3)   ' 16
  ' quattro livelli
  DIM h = { |a| RETURN { |b| RETURN { |c| RETURN { |d| RETURN a + b + c + d + k } } } }
  PRINT "four="; EVAL(EVAL(EVAL(EVAL(h, 1), 2), 3), 4)  ' 20
  ' cattura mista: parametro, locale del chiamante e globale
  DIM gl AS INTEGER
  gl = 1000
  DIM mix = { |p| RETURN { |q| RETURN { |r| RETURN p + q + r + k + gl } } }
  PRINT "mixed="; EVAL(EVAL(EVAL(mix, 1), 2), 3)  ' 1016
  ' mutazione della cattura DUE livelli più in basso (write-through sul chiamante)
  DIM bump = { |d| RETURN { |x| k = k + d + x : RETURN k } }
  PRINT "mut="; EVAL(EVAL(bump, 5), 1)            ' 16
  ' tre livelli: k cresce ancora 2+3+0 = 21
  DIM bump3 = { |a| RETURN { |b| RETURN { |c| k = k + a + b + c : RETURN k } } }
  PRINT "kmut="; EVAL(EVAL(EVAL(bump3, 2), 3), 0) ' 21
  ' closure ritornata da una FUNCTION (parametro catturato per valore)
  DIM ad = MkAdder(7)
  PRINT "ret="; EVAL(ad, 35)                      ' 42
END SUB
