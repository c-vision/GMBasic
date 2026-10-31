' ============================================================================
' ModelCompilerCompress.bas — da un GGUF a un GGUF PIU' PICCOLO.
'
' La pipeline del model compiler (namespace LLM), pilotata da BASIC:
'   1. PLAN    ispeziona i tensori: ruolo + formato proposto
'   2. POLICY  dichiara le REGOLE (punto-di-decisione -> valore) del compilatore
'   3. PRUNE   riduce gli esperti (router coerente)
'   4. QUANT   comprime i tensori secondo la policy (con imatrix)
'   5. SAVE    round-trip di verifica del risultato
'
' Input: un GGUF (es. olmoe-1b-7b, MoE 64 esperti / top-8).
' Metti il modello in models/ e l'imatrix (llama-imatrix) in work/.
' Le uscite finiscono in work/ accanto a questo sample.
' ============================================================================

' --- 1) ISPEZIONA: il piano elenca ruolo + formato proposto per ogni tensore ---
LLM PLAN "models/olmoe_f16.gguf", "work/piano.txt"

' --- 2) REGOLE: la policy e' la tabella "punto-di-decisione -> valore".
'     (una sola dichiarazione, poi ogni comando la consulta) ---
LLM POLICY SET "ROUTED_EXPERT", "iq2xxs"   ' esperti MoE: ~2.06 bit, guidati dall'importanza
LLM POLICY SET "FFN", "q4_0"               ' FFN densi: ~4.5 bit
LLM POLICY SET "ATTENTION", "q8_0"         ' proiezioni attention: ~8.5 bit
LLM POLICY SET "EMBEDDING", "f16"          ' embedding: 16 bit (nessuna perdita)
LLM POLICY SET "IMATRIX", "work/olmoe.imatrix.gguf"
LLM POLICY SET "PRUNE_EXPERT", "usage"     ' potatura: USO REALE degli esperti (routing calibrato, dai counts dell'imatrix)
LLM POLICY SHOW                             ' ispeziona le regole attive

' --- 3) POTA: 64 -> 32 esperti (stessa fetta sul router, *.expert_count aggiornato) ---
LLM PRUNE "models/olmoe_f16.gguf", "work/pruned.gguf", 32

' --- 4) COMPRIMI: quantizza per RUOLO secondo la policy (imatrix inclusa) ---
LLM QUANT "work/pruned.gguf", "work/olmoe_compressed.gguf"

' --- 5) VERIFICA: round-trip del risultato (metadata + byte, riscritto identico) ---
LLM MODEL SAVE "work/olmoe_compressed.gguf", "work/verify.gguf"

PRINT "compresso: work/olmoe_compressed.gguf"
