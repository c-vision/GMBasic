' FILE: Concurrency/GpmV2GoFunctionString.bas
' =================================================================================================
'       G4-str (piano C99Native 2.6(m)): GO FUNCTION ... INTO con risultato STRINGA
' =================================================================================================
'
' Come GpmV2GoFunction.bas ma la funzione ritorna una stringa: il future risolve con
' gmb_make_val_string e la lettura del target fa await estraendo v_string.data.
'
' Output atteso:
'   R-HELLO-WORLD
'   MAIN-DONE

FUNCTION Greet(Who AS STRING) AS STRING
  SLEEP 5
  RETURN "HELLO-" + Who
END FUNCTION

SUB Main()
  DIM R AS STRING
  GO FUNCTION Greet("WORLD") INTO R
  SLEEP 30
  PRINT "R-"; R
  PRINT "MAIN-DONE"
END SUB
