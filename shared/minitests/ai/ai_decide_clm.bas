' MiniTest: provider `clm` (Contrastive Language Models) del primitivo AI DECIDE, wire /v1/systemone.
' Senza rete e senza modello: verifica la REGISTRAZIONE del kind e gli errori ESPLICITI
' (config senza baseUrl, percorso locale incompleto, server irraggiungibile) + il fallback scritto.
' Il wire, la prosa e la head sono coperti da gmb_decision_clm_selfcheck (C) e dai test C# di NativeClm (server HTTP locale).
' EXPECT: clm-nourl={"error":"clm: baseUrl mancante nel config del provider %DECIDE"}|clm-local={"error":"clm: config locale incompleta: servono encoder (Qwen3-8B) e head (safetensors)"}|clm-fallback={"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}|clm-rank-rules={"error":"RANK richiede il provider clm"}|clm-rank-nourl={"error":"clm: baseUrl mancante nel config del provider %DECIDE"}|clm-rank-one={"error":"clm: RANK richiede almeno 2 candidati"}|clm-down={"error":"clm: HTTP fallito
IMPORT AI

SUB Main()
    DIM r AS STRING
    AI REGISTER "clmnone", %DECIDE, "clm", "{}"
    AI DECIDE "sì", "NOUL", "clmnone" INTO r
    PRINT "clm-nourl="; r
    AI REGISTER "clmhead", %DECIDE, "clm", "{""head"":""/x/clm_head.safetensors""}"
    AI DECIDE "sì", "NOUL", "clmhead" INTO r
    PRINT "clm-local="; r
    AI DECIDE "sì", "NOUL", "clmnone" FALLBACK "rules" INTO r
    PRINT "clm-fallback="; r
    AI DECIDE "x", "RANK:a|b", "rules" INTO r
    PRINT "clm-rank-rules="; r
    AI DECIDE "x", "RANK:a|b", "clmnone" INTO r
    PRINT "clm-rank-nourl="; r
    AI REGISTER "clmone", %DECIDE, "clm", "{""baseUrl"":""http://127.0.0.1:9""}"
    AI DECIDE "x", "RANK:solo", "clmone" INTO r
    PRINT "clm-rank-one="; r
    AI REGISTER "clmdown", %DECIDE, "clm", "{""baseUrl"":""http://127.0.0.1:9"",""timeoutMs"":""1500""}"
    AI DECIDE "sì", "NOUL", "clmdown" INTO r
    PRINT "clm-down="; LEFT$(r, 27)
END SUB
