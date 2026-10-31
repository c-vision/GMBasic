' FILE: Concurrency/PCodeGpmClassFrameErrors.bas
' Scope ON ERROR attraverso costruttore, metodo e property setter PCodeVM.

CLASS FrameProbe
  PRIVATE DIM _value AS INTEGER

  PUBLIC SUB NEW(FailNow AS INTEGER)
    IF FailNow <> 0 THEN
      ERROR 971, "constructor"
    END IF
  END SUB

  PUBLIC SUB FailMethod()
    ERROR 972, "method"
  END SUB

  PUBLIC PROPERTY Value AS INTEGER
    GET
      RETURN SELF._value
    END GET
    SET(BYVAL NewValue AS INTEGER)
      IF NewValue < 0 THEN
        ERROR 973, "setter"
      END IF
      SELF._value = NewValue
    END SET
  END PROPERTY
END CLASS

SUB ClassFrameWorker()
  DIM Probe AS FrameProbe

  ON ERROR GOTO ClassFrameHandler
  Probe = NEW FrameProbe(1)
  PRINT "CLASS-CONSTRUCTOR-RESUMED"

  Probe = NEW FrameProbe(0)
  Probe.FailMethod()
  PRINT "CLASS-METHOD-RESUMED"

  Probe.Value = -1
  PRINT "CLASS-SETTER-RESUMED"
  ON ERROR GOTO 0
  END SUB

ClassFrameHandler:
  PRINT "CLASS-FRAME-HANDLER"
  RESUME NEXT

SUB Main()
  ClassFrameWorker()
  PRINT "PCODE-GPM-CLASS-FRAME-DONE"
END SUB
