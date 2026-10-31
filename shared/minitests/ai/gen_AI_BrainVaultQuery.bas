' TAG: heavy
' TAG: unsupported-c99native
' EXPECT: vault= 0
' MiniTest: AI.BRAINVAULTQUERY su vault inesistente -> risposta deterministica "0 risultati".
' HEAVY (2026-10-29): la query passa dall'agente RAG, quindi richiede un PROVIDER di embedding
' configurato + modelli locali. Senza, il test fallisce per l'ambiente, non per il linguaggio
' (vedi Bugs/(solved) - AI-Residui-SkillsPaths-BrainVault). Con un modello configurato:
'   GMB_RUN_HEAVY=1 zsh tools/sh/test_minitests_parity.sh "ai/gen_AI_BrainVaultQuery.bas"
IMPORT AI

SUB Main()
    PRINT "vault="; AI.BRAINVAULTQUERY("x", 1)
END SUB
