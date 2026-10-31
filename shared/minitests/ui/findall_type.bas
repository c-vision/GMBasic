' MiniTest UI — FINDALL/TYPE (CUA accessibility), tipi dichiarati.
' FINDALL → Array (JSON-array), TYPE → Bit (1/0). Oracolo deterministico con needle
' vuota su tutte le gambe (VMC: tipi dichiarati dal 2026-09-14, prima JSON-stringhe).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: []|0
IMPORT UI

SUB Main()
    PRINT UI.FINDALL("")
    PRINT UI.TYPE("", "")
END SUB
