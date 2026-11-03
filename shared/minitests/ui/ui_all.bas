' MiniTest NAMESPACE UI — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: []|0|find=-1|click= 0
IMPORT UI

SUB T0()
    PRINT UI.FINDALL("")
    PRINT UI.TYPE("", "")
END SUB

SUB Extra()
  PRINT "find="; (INSTR(UI.FIND(""), "found") > 0)
  PRINT "click="; UI.CLICK("")
END SUB

SUB Main()
  T0()
  Extra()
END SUB
