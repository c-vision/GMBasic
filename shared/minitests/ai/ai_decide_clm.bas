' MiniTest: provider `clm` (Contrastive Language Models) del primitivo AI DECIDE, wire /v1/systemone.
' Senza rete e senza modello: verifica la REGISTRAZIONE del kind e gli errori ESPLICITI
' (config senza baseUrl, percorso locale non cablato, server irraggiungibile) + il fallback scritto.
' Il wire, la prosa e la head sono coperti da gmb_decision_clm_selfcheck (C) e dai test C# di ClmDecisionProvider.
' EXPECT: clm-nourl={"error":"clm: baseUrl mancante nel config del provider %DECIDE"}|clm-local={"error":"clm: config locale incompleta: servono encoder (Qwen3-8B) e head (safetensors)"}|clm-fallback={"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}|clm-down={"error":"clm: HTTP fallito
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
    AI REGISTER "clmdown", %DECIDE, "clm", "{""baseUrl"":""http://127.0.0.1:9"",""timeoutMs"":""1500""}"
    AI DECIDE "sì", "NOUL", "clmdown" INTO r
    PRINT "clm-down="; LEFT$(r, 27)
END SUB
