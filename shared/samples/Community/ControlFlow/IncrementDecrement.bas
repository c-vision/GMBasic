' FILE: ControlFlow/IncrementDecrement.bas
' =================================================================================================
'   GUIDA: operatori di incremento/decremento `++` e `--` (prefix e postfix)
' =================================================================================================
'
' `++t` / `--t` (PREFIX): incrementano/decrementano `t` di 1 e restituiscono il NUOVO valore.
' `t++` / `t--` (POSTFIX): incrementano/decrementano `t` di 1 e restituiscono il VECCHIO valore.
' Validi sia come STATEMENT (riga autonoma, valore scartato) sia come ESPRESSIONE.
' Operano su: variabili locali, campi di CLASSE (nudi o `SELF.`), elementi di array (1-D/2-D),
' su tipi numerici (INTEGER/DOUBLE/...). Parita' 4 backend.
'
' Uso tipico: contatori (dentro cicli FOR), aggiornamento di stato, lettura-del-vecchio.

TYPE Punto
  x AS INTEGER
  y AS INTEGER
END TYPE

CLASS Contatore
  PUBLIC DIM valore AS INTEGER
  PUBLIC DIM media AS DOUBLE

  PUBLIC SUB NEW(v AS INTEGER)
    valore = v
    media = 0.0
  END SUB

  SUB Aggiorna()
    valore++                 ' campo nudo (statement)
  END SUB

  FUNCTION IncrementaPre() AS INTEGER
    IncrementaPre = ++valore ' prefix: valore+1, ritorna il NUOVO
  END FUNCTION

  FUNCTION IncrementaPost() AS INTEGER
    IncrementaPost = valore++ ' postfix: ritorna il VECCHIO, poi valore+1
  END FUNCTION

  SUB MediaDouble(d AS DOUBLE)
    d--                      ' parametro DOUBLE
    media = d
  END SUB
END CLASS

SUB Main()
  ' --- statement su variabile locale ---
  DIM i AS INTEGER
  i = 5
  i++                        ' 5 -> 6
  ++i                        ' 6 -> 7
  PRINT "i="; i              ' 7
  i--                        ' 7 -> 6
  PRINT "i="; i              ' 6

  ' --- espressione: prefix vs postfix ---
  DIM old AS INTEGER
  DIM nuovo AS INTEGER
  old = i++                  ' old = 6 (vecchio), i = 7
  nuovo = ++i                ' i = 8, nuovo = 8
  PRINT "old="; old; " nuovo="; nuovo; " i="; i

  ' --- campi di CLASSE ---
  DIM c AS Contatore = NEW Contatore(10)
  c.Aggiorna()                       ' valore 10 -> 11
  PRINT "aggiorna="; c.valore
  PRINT "pre="; c.IncrementaPre(); " valore="; c.valore    ' 12, 12
  PRINT "post="; c.IncrementaPost(); " valore="; c.valore  ' 12, 13
  c.MediaDouble(9.5)                                       ' d-- => 8.5
  PRINT "media="; c.media

  ' --- contatore dentro un ciclo ---
  DIM n AS INTEGER
  DIM tot AS INTEGER
  DIM k AS INTEGER
  FOR k = 0 TO 5
    n++                      ' conta le iterazioni
    tot = tot + n             ' 1+2+3+4+5+6 = 21
  NEXT k
  PRINT "n="; n; " tot="; tot

  ' --- elementi di array (1-D e 2-D) ---
  DIM cont(4) AS INTEGER
  DIM griglia(2, 2) AS INTEGER
  DIM r AS INTEGER
  DIM q AS INTEGER
  FOR r = 0 TO 4
    cont(r)++                ' ogni elemento da 0 diventa 1
  NEXT r
  griglia(1, 1) = 10
  griglia(1, 1)--            ' 10 -> 9
  PRINT "cont0="; cont(0); " cont4="; cont(4); " griglia="; griglia(1, 1)

  ' --- postfix su elemento in expressione ---
  DIM catturato AS INTEGER
  catturato = cont(2)++      ' catturato = 1 (vecchio), cont(2) = 2
  PRINT "catturato="; catturato; " cont2="; cont(2)
END SUB
