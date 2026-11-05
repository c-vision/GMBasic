' CLMDecision — AI DECIDE con il provider `clm` (Contrastive Language Models).
'
' CLM e' un PROVIDER del primitivo AI DECIDE (stesso contratto NOUL/CHOICE/SCORE), non una sintassi
' nuova: si registra col ruolo %DECIDE e si usa con USING o come decisore attivo. Decide per
' similarita' fra l'embedding dello stato e quello dei candidati; PROBABILITY e' una distribuzione
' sulle opzioni (CALIBRATED resta null).
'
' Qui il server CLM (`baseUrl`) NON e' raggiungibile: il programma mostra l'errore esplicito del
' provider e poi la catena di FALLBACK scritta dal programma verso `rules` (deterministico).
' Con un server `clm-serve` in ascolto basta cambiare `baseUrl`.
IMPORT AI

SUB Main()
    DIM r AS STRING
    DIM ticket AS STRING = "{""ticket"":""Login fallito dopo il reset della password""}"

    AI REGISTER "team", %DECIDE, "clm", "{""baseUrl"":""http://127.0.0.1:9"",""timeoutMs"":""1500""}"

    ' 1) Il provider non risponde: errore ESPLICITO (mai un risultato inventato).
    AI DECIDE ticket, "CHOICE:auth|billing", "team" INTO r
    PRINT "clm: "; LEFT$(r, 27)

    ' 2) Fallback ESPLICITO verso `rules`: il programma sceglie cosa fare se CLM non c'e'.
    AI DECIDE "il cliente ha detto SI", "NOUL", "team" FALLBACK "rules" INTO r
    PRINT "fallback: "; r
END SUB
