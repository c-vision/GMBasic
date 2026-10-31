' BRAIN VAULT (Second Brain §2) — AI.BRAINVAULTBUILD / AI.BRAINVAULTQUERY / AI.BRAINVAULTWRITE.
' Genera un brain di DOCUMENTAZIONE dalla StdLib GMBasic (una pagina .md per namespace, con le
' firme esatte) e la indicizza in RAG: così puoi chiedere "come si scrive X in GMBasic" e
' ottenere i namespace rilevanti con le firme — base per code generation.
'
' Richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias) per indicizzare,
' e un provider LLM (%CHAT) per AI.BRAINVAULTWRITE. Le pagine .md finiscono in work/brain/,
' il db RAG in <rag>/brainvault.db (pattern §9.3/§11: output accanto al programma).

IMPORT AI WITH ALIAS

' Output accanto a questo programma (mai la root del repo).
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

' AI.BRAINVAULTBUILD(outputDir$) — genera brain/<Ns>.md dalla StdLib e li indicizza in RAG.
' Ritorna il numero di pagine indicizzate.
PRINT "BUILD:", AI.BRAINVAULTBUILD(WORK$)

' AI.BRAINVAULTQUERY(query$ [, topK]) — retrieval semantico: i namespace più rilevanti
' con le loro firme. Per code generation: passa i chunk all'LLM come contesto.
PRINT "QUERY:", AI.BRAINVAULTQUERY("ricerca vettoriale su collezioni sqlite", 3)

' AI.BRAINVAULTWRITE(richiesta$ [, topK]) — genera codice BASIC usando il brain vault
' come contesto (WRITE tool). Richiede un provider LLM attivo.
PRINT "WRITE:", AI.BRAINVAULTWRITE("scrivi una funzione che somma due numeri", 3)

PRINT "BrainVault ok"
