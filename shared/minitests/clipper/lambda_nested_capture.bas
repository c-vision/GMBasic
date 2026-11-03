' CLIPPER 5.3 — catture nelle lambda ANNIDATE (una lambda dentro un'altra).
' Ogni livello intermedio cattura per conto proprio le variabili che servono ai
' livelli interni: parametri dell'esterno, catture del chiamante, globali e
' parametri di funzione, a profondità arbitraria (qui 2, 3 e 4 livelli).
'
' Nota (limiti noti del backend IL, non delle gambe C): una cattura SOLA-LETTURA
' poi riassegnata dal chiamante e la cattura di una STRINGA restano per VALORE su
' IL (snapshot), e la MUTAZIONE dal livello interno (2+ livelli) va in overflow
' aritmetico: quei casi sono coperti dalle gambe C (VM/C e C99Native, verdi) e
' documentati in Guide/Clipper-Macro-Eval-Blocks-Compat.md. Qui si verificano le
' forme comuni a tutti e quattro i backend.
' EXPECT: two= 16|three= 16|four= 20|mixed= 1016|ret= 42|cnt1= 1|cnt2= 3
FUNCTION MkAdder(k AS INTEGER) AS FUNCTION(x) AS DOUBLE
  RETURN { |x| RETURN x + k }
END FUNCTION

' Closure ritornata con cattura di una LOCALE del creatore (stato accumulato tra
' le chiamate: la cella sopravvive al ritorno del creatore).
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
  ' closure ritornata da una FUNCTION (parametro catturato)
  DIM ad = MkAdder(7)
  PRINT "ret="; EVAL(ad, 35)                     ' 42
  ' mutazione della cattura dentro la closure ritornata (stato persistente)
  DIM cnt = MkCounter()
  PRINT "cnt1="; EVAL(cnt, 1)                    ' 1
  PRINT "cnt2="; EVAL(cnt, 2)                    ' 3
END SUB
