' MiniTest console — SAVE SCREEN / RESTORE SCREEN (buffer schermo nominati).
' Comandi legacy a spazio (§2.0: niente parentesi; il lexer fonde "SAVE SCREEN" in un
' unico token, Lexer.cs). Forma a una parola SAVESCREEN NON esiste nel lexer.
' Headless: wiring su tutte le gambe (parse→codegen→runtime senza errori). La semantica
' pixel richiede display: limite documentato, coperta da sample+guida del blocco GRAPHICS.
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: ok
SUB Main()
    SAVE SCREEN "gmb_mt_scr"
    RESTORE SCREEN "gmb_mt_scr"
    PRINT "ok"
END SUB
