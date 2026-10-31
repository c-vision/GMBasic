' MiniTest console — GET da tastiera (bloccante) con SENDKEY.
' SENDKEY inietta nel buffer PRIMA della lettura: deterministico su tutte le gambe,
' mai hang (IL: InkeyQueue; PCode: KeyboardService; C: typeahead; harness: stdin non-tty).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: k=X
SENDKEY "X"
GET k$
PRINT "k="; k$
