' PROVENANCE (Second Brain §2) — AI.SIGNNOTE / AI.VERIFYNOTE / AI.SHAREENCRYPT / AI.SHAREDECRYPT.
' Firma HMAC-SHA256 del contenuto con chiave condivisa e verifica; condivisione cifrata
' AES-256-GCM (base64).
' Parità tra i backend (IL/PCodeVM/VM-C/C99): la stessa firma esce identica ovunque, perché
' l'implementazione vive nel runtime C condiviso (gmb_provenance_sign/verify e
' gmb_aes_encrypt_password) e nel wrapper C# (stesso HMACSHA256 / AES-256-GCM).

IMPORT AI WITH ALIAS

CONTENT$ = "nota di test per provenance"
KEY$ = "chiave-segreta-42"

' AI.SIGNNOTE(content$, key$) → firma hex (64 caratteri).
SIG$ = AI.SIGNNOTE(CONTENT$, KEY$)
PRINT "SIG:", SIG$

' AI.VERIFYNOTE(content$, key$, sig$) → 1 se la firma è autentica, 0 altrimenti.
PRINT "VERIFY OK:", AI.VERIFYNOTE(CONTENT$, KEY$, SIG$)
PRINT "VERIFY TAMPERED:", AI.VERIFYNOTE("altro contenuto", KEY$, SIG$)

' Verifica che la firma sia l'attesa (parità col runtime C).
IF SIG$ = "f649104f87bf898efce8eb903d6d27b207db27ac1d432393fda7d21af2f9c7e0" THEN
    PRINT "PARITY: ok"
ELSE
    PRINT "PARITY: MISMATCH"
END IF

' AI.SHAREENCRYPT(content$, key$) → base64 AES-256-GCM (salt|nonce|tag|ciphertext).
CIPHER$ = AI.SHAREENCRYPT(CONTENT$, KEY$)
' Il cifrato NON si stampa: AES-GCM usa un IV CASUALE, quindi cambia a ogni esecuzione e fra le
' gambe -> la stampa non e' deterministica e la parita' e' impossibile. Si verifica la
' proprieta' (lunghezza + round-trip piu' sotto), non il valore.
PRINT "CIPHER LEN OK:", IIF(LEN(CIPHER$) > 10, 1, 0)

' AI.SHAREDECRYPT(cipher$, key$) → testo originale (chiave errata = errore esplicito).
PRINT "DECRYPT:", AI.SHAREDECRYPT(CIPHER$, KEY$)
