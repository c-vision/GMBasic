' MiniTest: REPLACE$/REPLACE — sostituisce TUTTE le occorrenze (parità 4 backend).
' REGRESSIONE (2026-11-02): assente in VM/C dal chunk A1 ("REPLACE$ non definita") mentre
' C99Native la emetteva: le pagine BSP che usano DASHBOARD.EscapeHtml non compilavano.
' Old VUOTO = inserimento in ogni posizione, inizio e fine inclusi (semantica canonica
' gmb_str_replace, allineata su TUTTI i backend il 2026-11-02; .NET string.Replace lancia).
' EXPECT: a+b+c|a_b_c|abc|zbc|aaa|-a-b-c-
SUB Main()
  PRINT REPLACE$("a-b-c", "-", "+")
  PRINT REPLACE$("a-b-c", "-", "_")
  PRINT REPLACE$("abc", "-", "+")
  PRINT REPLACE$("abc", "a", "z")
  PRINT REPLACE$("aaa", "a", "a")
  PRINT REPLACE$("abc", "", "-")
END SUB
