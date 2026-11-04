' CLIPPER 5.3 — catture nelle lambda ANNIDATE (una lambda dentro un'altra).
' Ogni livello intermedio cattura per conto proprio le variabili che servono ai
' livelli interni: parametri dell'esterno, catture del chiamante, parametri di
' funzione e globali, a profondità arbitraria (qui 2, 3 e 4 livelli).
' EXPECT: two= 16|three= 16|four= 20|mixed= 1016|mut= 16|kmut= 21|ret= 42|ro= 42|cnt1= 1|cnt2= 3|s=b
FUNCTION MkAdder(k AS INTEGER) AS FUNCTION(x) AS DOUBLE
  RETURN { |x| RETURN x + k }
END FUNCTION

' Closure ritornata con cattura di una LOCALE del creatore (cella/box condivisa:
' le chiamate successive vedono lo stato accumulato, non un puntatore morto).
FUNCTION MkCounter() AS FUNCTION(x) AS DOUBLE
  DIM c AS INTEGER
  c = 0
  RETURN { |x| c = c + x : RETURN c }
END FUNCTION

SUB Main()
  DIM k AS INTEGER
  k = 10
  ' due livelli: parametro dell'esterno + cattura del chiamante (EVAL annidato)
  DIM outer = { |x| RETURN { |y| RETURN x + y + k } }
  PRINT "two="; EVAL(EVAL(outer, 5), 1)          ' 16
  ' tre livelli (blocchi intermedi in variabili: catena EVAL esplicita)
  DIM g = { |a| RETURN { |b| RETURN { |c| RETURN a + b + c + k } } }
  DIM g1 = EVAL(g, 1)
  DIM g2 = EVAL(g1, 2)
  PRINT "three="; EVAL(g2, 3)                    ' 16
  ' quattro livelli
  DIM h = { |a| RETURN { |b| RETURN { |c| RETURN { |d| RETURN a + b + c + d + k } } } }
  DIM h1 = EVAL(h, 1)
  DIM h2 = EVAL(h1, 2)
  DIM h3 = EVAL(h2, 3)
  PRINT "four="; EVAL(h3, 4)                     ' 20
  ' cattura mista: parametro, locale del chiamante e globale
  DIM gl AS INTEGER
  gl = 1000
  DIM mix = { |p| RETURN { |q| RETURN { |r| RETURN p + q + r + k + gl } } }
  DIM m1 = EVAL(mix, 1)
  DIM m2 = EVAL(m1, 2)
  PRINT "mixed="; EVAL(m2, 3)                    ' 1016
  ' mutazione della cattura DUE livelli più in basso (write-through sul chiamante)
  DIM bump = { |d| RETURN { |x| k = k + d + x : RETURN k } }
  PRINT "mut="; EVAL(EVAL(bump, 5), 1)           ' 16
  ' tre livelli: k cresce ancora 2+3+0 = 21
  DIM bump3 = { |a| RETURN { |b| RETURN { |c| k = k + a + b + c : RETURN k } } }
  DIM b1 = EVAL(bump3, 2)
  DIM b2 = EVAL(b1, 3)
  PRINT "kmut="; EVAL(b2, 0)                     ' 21
  ' closure ritornata da una FUNCTION (parametro catturato)
  DIM ad = MkAdder(7)
  PRINT "ret="; EVAL(ad, 35)                     ' 42
  ' la closure cattura la VARIABILE, non il valore: la modifica del chiamante si vede
  DIM ro AS INTEGER
  ro = 5
  DIM rof = { || RETURN ro }
  ro = 42
  PRINT "ro="; EVAL(rof)                         ' 42
  ' stato persistente nella closure ritornata
  DIM cnt = MkCounter()
  PRINT "cnt1="; EVAL(cnt, 1)                    ' 1
  PRINT "cnt2="; EVAL(cnt, 2)                    ' 3
  ' cattura di una STRINGA per riferimento (live, non istantanea)
  DIM s AS STRING
  s = "a"
  DIM sf = { || RETURN s }
  s = "b"
  PRINT "s="; EVAL(sf)                           ' b
END SUB
