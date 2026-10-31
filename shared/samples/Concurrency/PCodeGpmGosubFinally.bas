' PCodeVM: GOSUB condivide locals col chiamante e RETURN attraversa FINALLY sospendibile.
' Una CALL annidata nel GOSUB deve usare RETURN di procedura, non consumare la continuation.

SUB NestedCall(BYREF x AS LONG)
  TRY
    x = x + 100
    RETURN
  FINALLY
    SLEEP 1
    x = x + 1000
  END TRY
END SUB

SUB Main()
  DIM value AS LONG = 1

  GOSUB UpdateValue

  IF value = 1112 THEN
    PRINT "GOSUB-SAME-FRAME-FINALLY-OK"
  ELSE
    PRINT "FAIL:GOSUB-VALUE:"; value
  END IF
  PRINT "PCODE-GPM-GOSUB-FINALLY-DONE"

UpdateValue:
  TRY
    NestedCall(value)
    value = value + 1
    RETURN
  FINALLY
    SLEEP 1
    value = value + 10
  END TRY
END SUB
