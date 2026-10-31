' FORMAT di una data il cui formato FINISCE con un letterale fra apici: il risultato deve essere terminato
' (VM/C e transpiler C leggevano memoria non inizializzata dopo il testo: gmb_fmt_datetime non terminava
' `out` nei rami letterale/escape). Il test usa solo token indipendenti dalla cultura.
' EXPECT: [2025 giorno]|[2025-12-25 fine]|[fine 2025]
SUB Main()
  DIM g AS DOUBLE = DATESERIAL(2025, 12, 25)
  PRINT "["; FORMAT(g, "yyyy 'giorno'"); "]"
  PRINT "["; FORMAT(g, "yyyy-MM-dd 'fine'"); "]"
  PRINT "["; FORMAT(g, "'fine' yyyy"); "]"
END SUB
