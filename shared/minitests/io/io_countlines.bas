' MiniTest IO/FILES — IO.COUNTLINES su file inesistente (deterministico, nessun side effect).
' Copre: IO.COUNTLINES.
'
' BUG CORRETTO (VMC): il func_code 581 (COUNTLINESFUNC) finiva nel tronco STRINGA e chiamava
' gmb_path_resolve (= realpath): COUNT restituiva un PERCORSO invece del numero di righe.
' Ora chiama gmb_count_lines (§0bis).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: 0
SUB Main()
    PRINT IO.COUNTLINES("__gmb_minitest_no_such_file__.txt")
END SUB
