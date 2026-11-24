' MiniTest: assegnazione di stringa = COPIA, non alias (semantica BASIC).
' REGRESSIONE C99Native (2026-11-24): `ln = arr(k)` in un ciclo liberava il buffer
' dell'elemento (il free del valore precedente colpiva un buffer posseduto dall'array):
' gli elementi diventavano stringhe VUOTE. Stessa classe per `b = a` seguito da `a = ...`
' (b restava appeso a memoria liberata). Le stringhe C99 non sono refcountate: l'assegnazione
' da una sorgente "in prestito" (variabile, elemento di array, campo) deve copiare.
' EXPECT: b=[primo]|arr=[e0 e1 e2]|after=[e0 e1 e2]|x=[e2]|d=[primo]
SUB Main()
  DIM a AS STRING, b AS STRING, d AS STRING
  DIM arr(2) AS STRING
  DIM i AS LONG, ln AS STRING

  ' variabile -> variabile, poi la sorgente cambia
  a = "primo"
  b = a
  a = "secondo"
  PRINT "b=["; b; "]"

  ' elemento di array -> variabile, in un ciclo (senza e con concatenazione)
  arr(0) = "e0" : arr(1) = "e1" : arr(2) = "e2"
  FOR i = 0 TO 2
    ln = arr(i)
  NEXT i
  PRINT "arr=["; arr(0); " "; arr(1); " "; arr(2); "]"
  FOR i = 0 TO 2
    ln = arr(i) + ""
  NEXT i
  PRINT "after=["; arr(0); " "; arr(1); " "; arr(2); "]"
  PRINT "x=["; ln; "]"

  ' DIM con inizializzazione da variabile
  DIM e AS STRING = b
  b = "cambiato"
  d = e
  PRINT "d=["; d; "]"
END SUB
