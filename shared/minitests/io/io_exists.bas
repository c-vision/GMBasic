' MiniTest IO/FILES — esistenza file/cartella (deterministico, nessun side effect).
' Copre: IO.FILEEXISTS, IO.DIREXISTS.
' I path sono scelti perché NON esistono: l'output non dipende dalla CWD, che è diversa
' tra le esecuzioni in-process (root repo) e VM/C (cartella del .bas) → la parity regge.
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: 0|0
SUB Main()
    PRINT IO.FILEEXISTS("__gmb_minitest_no_such_file__.tmp")
    PRINT IO.DIREXISTS("__gmb_minitest_no_such_dir__")
END SUB
