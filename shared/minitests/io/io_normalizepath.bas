' MiniTest IO/FILES — IO.NORMALIZEPATH (assoluto + risoluzione di "." e "..").
' Copre: IO.NORMALIZEPATH.
' Solo path ASSOLUTI: su quelli la normalizzazione non consulta la CWD.
'
' BUG CORRETTO (VMC): usava realpath(), che richiede che il path ESISTA e altrimenti
' restituisce l'input invariato → VMC non normalizzava ("/a/b/../c" restava tale).
' Ora chiama gmb_fs_normalize_path, lo stesso runtime condiviso di IL/PCode/C99 (§0bis).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: /a/c|/a/b/c
SUB Main()
    PRINT IO.NORMALIZEPATH("/a/b/../c")
    PRINT IO.NORMALIZEPATH("/a/./b/c")
END SUB
