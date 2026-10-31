' EXPECT: 0|ok|ok
' Tre casi: input vuoto (falso), password CORRETTA (dove la VM/C ritornava -1 per un valore
' inventato: l'oracolo PasswordVerify -> bool = 1/0, commit dd22f8c46) e password ERRATA.
' Forma IMPORT ... AS CR come samples/Crypto/CryptoHashing.bas: con `WITH ALIAS` la
' gamba IL lancia NotSupportedException, e con `PRINT CRYPTOGRAPHY.PASSWORDVERIFY(...)` diretto le
' gambe IL/PCode interrompono il programma -> si usa IF/THEN.
IMPORT CRYPTOGRAPHY AS CR

SUB Main()
    PRINT CR.PASSWORDVERIFY("", "")
    hsh$ = CR.PASSWORDHASH("segreta")
    IF CR.PASSWORDVERIFY("segreta", hsh$) THEN PRINT "ok" ELSE PRINT "fail"
    IF CR.PASSWORDVERIFY("sbagliata", hsh$) THEN PRINT "fail" ELSE PRINT "ok"
END SUB
