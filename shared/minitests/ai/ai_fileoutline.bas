' EXPECT: procedure Foo;function Calc;procedure Main
' MiniTest: LLM.FILEOUTLINE(file$) — outline per-file (§3.6 PARSEC), fold a link-time.
IMPORT LLM
SUB Foo()
END SUB
FUNCTION Calc() AS DOUBLE
  Calc = 1
END FUNCTION
SUB Main()
  PRINT LLM.FILEOUTLINE("ai_fileoutline.bas")
END SUB
