' wsp.inc — namespace WSP: Weight-Space Programming (libgmbwsp, sorgenti: GMBasic/gmbllm/llmwsp_*).
'
' Leggere, sezionare, MODIFICARE e riscrivere i pesi di un modello (safetensors densi Qwen3/Llama), senza ricompilare il motore:
'   m = WSP.LOADMODEL("~/ai/Models/Qwen3-8B")        ' apre lo store e costruisce il documento gmb.wsp@1
'   n = WSP.DIM(m, "layers")                         ' dimensioni del modello
'   r = WSP.COPYSCORE(m, 5, 7, "OV", s())            ' analisi senza forward: s(0)=Σλ/Σ|λ|, s(1)=frazione di autovalori > 0
'   r = WSP.SCALESLICE(m, "layers.5.attn.o.heads.7", 0.5)     ' edit su un OVERLAY (la base resta immutabile)
'   r = WSP.MATERIALIZE(m, "~/out/model-edit")       ' scrive il checkpoint nuovo + wsp_summary.txt (nessuna scrittura silenziosa)
'   r = WSP.FREEMODEL(m)
'
' Convenzioni: handle > 0 (massimo 8 vivi, uso sequenziale); errore = -1 e messaggio in WSP.LASTERROR(h) (h = 0: errori globali).
' Indirizzi di slice: "layers.<L>.attn.{q,k,v,o}.{heads|kv_heads}.<i>", "layers.<L>.mlp.{gate,up,down}.neurons.<i>", "embed", ...
' (WSP.LEAFADDR li elenca) oppure "@<asse>:<da>:<a>:<nome tensore>" per una slice arbitraria. Le funzioni di analisi e di derivazione
' richiedono il documento (famiglie Qwen3/Llama); per le altre WSP.HASDOC = 0 e valgono solo gli indirizzi "@...".
' Politica: le primitive di edit sono generali; ogni edit resta nel journal. Nessuna ricetta per rimuovere protezioni di sicurezza.
NAMESPACE WSP

' ---- modello ------------------------------------------------------------------------------------------------------
DECLARE FUNCTION LOADMODEL LIB "gmbwsp" ALIAS "wsp_loadmodel" (dir AS STRING) AS LONG
DECLARE FUNCTION FREEMODEL LIB "gmbwsp" ALIAS "wsp_freemodel" (h AS LONG) AS LONG
DECLARE FUNCTION LASTERROR LIB "gmbwsp" ALIAS "wsp_lasterror" (h AS LONG) AS STRING
DECLARE FUNCTION HASDOC LIB "gmbwsp" ALIAS "wsp_hasdoc" (h AS LONG) AS LONG
' key: tensors | bytes | shards | layers | hidden | heads | kv_heads | head_dim | inter | vocab | leaves   (-1 se non disponibile)
DECLARE FUNCTION DIM LIB "gmbwsp" ALIAS "wsp_dim" (h AS LONG, key AS STRING) AS DOUBLE
DECLARE FUNCTION MODELTYPE LIB "gmbwsp" ALIAS "wsp_modeltype" (h AS LONG) AS STRING
DECLARE FUNCTION TENSORCOUNT LIB "gmbwsp" ALIAS "wsp_tensorcount" (h AS LONG) AS LONG
DECLARE FUNCTION TENSORNAME LIB "gmbwsp" ALIAS "wsp_tensorname" (h AS LONG, i AS LONG) AS STRING
DECLARE FUNCTION TENSORINFO LIB "gmbwsp" ALIAS "wsp_tensorinfo" (h AS LONG, name AS STRING) AS STRING
DECLARE FUNCTION LEAFCOUNT LIB "gmbwsp" ALIAS "wsp_leafcount" (h AS LONG) AS LONG
DECLARE FUNCTION LEAFADDR LIB "gmbwsp" ALIAS "wsp_leafaddr" (h AS LONG, i AS LONG) AS STRING
' Violazioni dell'invariante di copertura/coerenza del documento (0 = completo).
DECLARE FUNCTION CHECKDOC LIB "gmbwsp" ALIAS "wsp_checkdoc" (h AS LONG) AS LONG
' Scrive il documento canonico JSON gmb.wsp@1.
DECLARE FUNCTION DOCJSON LIB "gmbwsp" ALIAS "wsp_docjson" (h AS LONG, path AS STRING) AS LONG
' sha256 del contenuto CORRENTE di un tensore (overlay se modificato).
DECLARE FUNCTION SHA LIB "gmbwsp" ALIAS "wsp_sha" (h AS LONG, tensor AS STRING) AS STRING
' Legge la slice CORRENTE come double in out(); ritorna gli elementi (o -1).
DECLARE FUNCTION READSLICE LIB "gmbwsp" ALIAS "wsp_readslice__ffi" (h AS LONG, addr AS STRING, out() AS DOUBLE, cap AS LONG) AS LONG

' ---- analisi senza forward ------------------------------------------------------------------------------------------
' kind: "q" | "k" | "v" | "o"; scrive la norma di Frobenius di ogni testa (o gruppo kv) in out(); ritorna quante.
DECLARE FUNCTION NORMS LIB "gmbwsp" ALIAS "wsp_norms__ffi" (h AS LONG, layer AS LONG, kind AS STRING, out() AS DOUBLE, cap AS LONG) AS LONG
' circuit: "OV" | "QK". out(0) = Σλ/Σ|λ|, out(1) = frazione di autovalori a parte reale > 0 (le DUE definizioni di copying score).
' QK senza RoPE/qk-norm: approssimazione per i modelli che le usano.
DECLARE FUNCTION COPYSCORE LIB "gmbwsp" ALIAS "wsp_copyscore__ffi" (h AS LONG, layer AS LONG, head AS LONG, circuit AS STRING, out() AS DOUBLE) AS LONG
DECLARE FUNCTION SPECTRUM LIB "gmbwsp" ALIAS "wsp_spectrum__ffi" (h AS LONG, layer AS LONG, head AS LONG, circuit AS STRING, wr() AS DOUBLE, wi() AS DOUBLE, cap AS LONG) AS LONG
' Projection Kernel fra lo spazio scritto da (l1,h1) e quello letto da (l2,h2) come "q"|"k"|"v" (0..head_dim; -1 errore).
DECLARE FUNCTION COMPOSE LIB "gmbwsp" ALIAS "wsp_compose" (h AS LONG, l1 AS LONG, h1 AS LONG, l2 AS LONG, h2 AS LONG, which AS STRING) AS DOUBLE
' Importanza SENZA dati (solo pesi) di neuroni MLP e gruppi kv: baseline debole, da validare con PPL/KL.
DECLARE FUNCTION MLPIMPORTANCE LIB "gmbwsp" ALIAS "wsp_mlpimportance__ffi" (h AS LONG, layer AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE FUNCTION KVIMPORTANCE LIB "gmbwsp" ALIAS "wsp_kvimportance__ffi" (h AS LONG, layer AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
' Indici dei k punteggi più alti, in ordine crescente di indice.
DECLARE FUNCTION TOPK LIB "gmbwsp" ALIAS "wsp_topk__ffi" (score() AS DOUBLE, n AS LONG, k AS LONG, out() AS LONG) AS LONG

' ---- edit su overlay (0 = ok, -1 = rifiutata e nulla cambia; LASTREPORT: toccati/totale, variazione di norma) ------------
DECLARE FUNCTION SCALESLICE LIB "gmbwsp" ALIAS "wsp_scaleslice" (h AS LONG, addr AS STRING, alpha AS DOUBLE) AS LONG
DECLARE FUNCTION ZEROSLICE LIB "gmbwsp" ALIAS "wsp_zeroslice" (h AS LONG, addr AS STRING) AS LONG
DECLARE FUNCTION SETVALUES LIB "gmbwsp" ALIAS "wsp_setvalues__ffi" (h AS LONG, addr AS STRING, v() AS DOUBLE, n AS LONG) AS LONG
DECLARE FUNCTION ADDDELTA LIB "gmbwsp" ALIAS "wsp_adddelta__ffi" (h AS LONG, addr AS STRING, v() AS DOUBLE, n AS LONG) AS LONG
DECLARE FUNCTION COPYSLICE LIB "gmbwsp" ALIAS "wsp_copyslice" (h AS LONG, dst AS STRING, src AS STRING) AS LONG
' Toglie dalla slice le componenti lungo ndirs direzioni (len elementi ciascuna). vec_axis: 0 = le colonne sono i vettori
' (o_proj/down_proj), 1 = le righe (embed, q/k/v/gate/up). flags: 1 = norm-preserving, 2 = direzione innocua (dirs = d, h).
DECLARE FUNCTION PROJECTOUT LIB "gmbwsp" ALIAS "wsp_projectout__ffi" (h AS LONG, addr AS STRING, dirs() AS DOUBLE, ndirs AS LONG, len AS LONG, vec_axis AS LONG, flags AS LONG) AS LONG
DECLARE FUNCTION LASTREPORT LIB "gmbwsp" ALIAS "wsp_lastreport" (h AS LONG) AS STRING

' ---- journal, scrittura, ricette ------------------------------------------------------------------------------------
DECLARE FUNCTION UNDOEDIT LIB "gmbwsp" ALIAS "wsp_undoedit" (h AS LONG) AS LONG
DECLARE FUNCTION REDOEDIT LIB "gmbwsp" ALIAS "wsp_redoedit" (h AS LONG) AS LONG
DECLARE FUNCTION CHECKPOINT LIB "gmbwsp" ALIAS "wsp_checkpoint" (h AS LONG, name AS STRING) AS LONG
DECLARE FUNCTION ROLLBACK LIB "gmbwsp" ALIAS "wsp_rollback" (h AS LONG, name AS STRING) AS LONG
DECLARE FUNCTION JOURNALPOS LIB "gmbwsp" ALIAS "wsp_journalpos" (h AS LONG) AS LONG
DECLARE FUNCTION JOURNALLEN LIB "gmbwsp" ALIAS "wsp_journallen" (h AS LONG) AS LONG
' Riepilogo testuale delle differenze rispetto alla base; DIFFCOUNT = tensori che differiscono davvero.
DECLARE FUNCTION SUMMARY LIB "gmbwsp" ALIAS "wsp_summary" (h AS LONG) AS STRING
DECLARE FUNCTION DIFFCOUNT LIB "gmbwsp" ALIAS "wsp_diffcount" (h AS LONG) AS LONG
' Scrive il checkpoint (shard intatti clonati, solo lo shard modificato riscritto) + wsp_summary.txt; ritorna gli shard riscritti.
DECLARE FUNCTION MATERIALIZE LIB "gmbwsp" ALIAS "wsp_materialize" (h AS LONG, outdir AS STRING) AS LONG
' RECIPE/REPLAY: il journal come programma riproducibile (recipe.gmbderive + vettori binari) e la sua riesecuzione.
DECLARE FUNCTION SAVERECIPE LIB "gmbwsp" ALIAS "wsp_saverecipe" (h AS LONG, dir AS STRING) AS LONG
DECLARE FUNCTION REPLAYRECIPE LIB "gmbwsp" ALIAS "wsp_replayrecipe" (h AS LONG, dir AS STRING) AS LONG
' Patch sparsa a slice: ritorna i byte scritti / 0 se applicata.
DECLARE FUNCTION EXPORTPATCH LIB "gmbwsp" ALIAS "wsp_exportpatch" (h AS LONG, path AS STRING) AS LONG
DECLARE FUNCTION APPLYPATCH LIB "gmbwsp" ALIAS "wsp_applypatch" (h AS LONG, path AS STRING) AS LONG

' ---- chirurgia di architettura (modello derivato) --------------------------------------------------------------------
' layer_map(nl): layer di uscita <- layer sorgente (nl = 0: identità). kv_keep(nl_out*kvm): gruppi kv tenuti per layer di uscita
' (kvm = 0: nessuna potatura). neuron_keep(nl_out*neuk): neuroni tenuti (neuk = 0: nessuna). Valida prima di scrivere e rivalida dopo.
DECLARE FUNCTION DERIVE LIB "gmbwsp" ALIAS "wsp_derive__ffi" (h AS LONG, outdir AS STRING, layer_map() AS LONG, nl AS LONG, kv_keep() AS LONG, kvm AS LONG, neuron_keep() AS LONG, neuk AS LONG) AS LONG

' ---- verifica, gate, provenienza ---------------------------------------------------------------------------------------
' flags: 1 = documento, 2 = finitezza NaN/Inf. 0 = ok; >0 = problemi (dettaglio in LASTERROR(0)).
DECLARE FUNCTION VERIFYMODEL LIB "gmbwsp" ALIAS "wsp_verifymodel" (dir AS STRING, flags AS LONG) AS LONG
DECLARE FUNCTION MODELHASH LIB "gmbwsp" ALIAS "wsp_modelhash" (h AS LONG) AS STRING
' 1 se value <= threshold (le <> 0) oppure value >= threshold (le = 0); NaN = 0.
DECLARE FUNCTION GATE LIB "gmbwsp" ALIAS "wsp_gate" (value AS DOUBLE, threshold AS DOUBLE, le AS LONG) AS LONG
' Catena base -> ops -> output in JSON (path), con il testo della ricetta.
DECLARE FUNCTION PROVENANCE LIB "gmbwsp" ALIAS "wsp_provenance" (h AS LONG, outdir AS STRING, recipe_text AS STRING, path AS STRING) AS LONG

' ---- sessione agente: verbi testuali su dati, budget, trascrizione e replay deterministico ----------------------------
' Verbi: NORMS SPECTRUM COMPOSE JOURNAL SUMMARY SHA DEFINE SCALE ZERO SET ADDDELTA COPY PROJECTOUT CHECKPOINT ROLLBACK UNDO REDO.
' Ogni altro verbo (shell, rete, file) è RIFIUTATO e registrato. AGENTRC: 0 ok, 1 rifiutato (verbo/budget), -1 errore.
DECLARE FUNCTION AGENTNEW LIB "gmbwsp" ALIAS "wsp_agentnew" (h AS LONG, max_edits AS LONG, max_forwards AS LONG) AS LONG
DECLARE FUNCTION AGENTEXEC LIB "gmbwsp" ALIAS "wsp_agentexec" (h AS LONG, cmd AS STRING) AS STRING
DECLARE FUNCTION AGENTRC LIB "gmbwsp" ALIAS "wsp_agentrc" (h AS LONG) AS LONG
DECLARE FUNCTION AGENTTRANSCRIPT LIB "gmbwsp" ALIAS "wsp_agenttranscript" (h AS LONG, path AS STRING) AS LONG
DECLARE FUNCTION AGENTSTOP LIB "gmbwsp" ALIAS "wsp_agentstop" (h AS LONG) AS LONG
' Riesegue una trascrizione su una sessione NUOVA (AGENTNEW): 0 = tutti gli output coincidono, n > 0 = divergenza al comando n.
DECLARE FUNCTION AGENTREPLAY LIB "gmbwsp" ALIAS "wsp_agentreplay" (h AS LONG, path AS STRING) AS LONG
DECLARE FUNCTION AGENTRUN LIB "gmbwsp" ALIAS "wsp_agentrun" (h AS LONG, path AS STRING) AS LONG
DECLARE FUNCTION AGENTEDITS LIB "gmbwsp" ALIAS "wsp_agentedits" (h AS LONG) AS LONG

' ---- fixture a verità nota (per test e sample autosufficienti) ----------------------------------------------------------
' kind: "copy" (1 layer) | "induction" (2 layer). Scrive un piccolo checkpoint safetensors + config.json (H=16, 4 teste, MHA).
DECLARE FUNCTION FIXTURE LIB "gmbwsp" ALIAS "wsp_fixture" (kind AS STRING, outdir AS STRING, seed AS LONG) AS LONG

' ---- pulizia confinata ---------------------------------------------------------------------------------------------------
' Rimuove SOLO i file che WSP produce (.safetensors .json .txt .f64 .jsonl .wsppatch .gmbderive .gmbarch) e la directory se resta
' vuota; non scende in sottodirectory (salvo "arch"). Ritorna i file rimossi. Serve a rieseguire sample/test senza residui.
DECLARE FUNCTION REMOVEOUTPUT LIB "gmbwsp" ALIAS "wsp_removeoutput" (dir AS STRING) AS LONG
