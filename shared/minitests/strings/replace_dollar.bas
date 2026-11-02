' MiniTest: REPLACE$/REPLACE — sostituisce TUTTE le occorrenze (parità 4 backend).
' REGRESSIONE (2026-11-02): assente in VM/C dal chunk A1 ("REPLACE$ non definita") mentre
' C99Native la emetteva: le pagine BSP che usano DASHBOARD.EscapeHtml non compilavano.
' Nota: il caso `REPLACE$("abc","","-")` (old vuoto) NON è coperto qui — divergenza
' preesistente da allineare: IL/PCode lanciano (string.Replace .NET), C99/C99Native
' inseriscono (gmb_str_replace). Vedi TODO §6/F8.
' EXPECT: a+b+c|a_b_c|abc|zbc|aaa
SUB Main()
  PRINT REPLACE$("a-b-c", "-", "+")
  PRINT REPLACE$("a-b-c", "-", "_")
  PRINT REPLACE$("abc", "-", "+")
  PRINT REPLACE$("abc", "a", "z")
  PRINT REPLACE$("aaa", "a", "a")
END SUB
