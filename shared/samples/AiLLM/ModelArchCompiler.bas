' ============================================================================
' ModelArchCompiler.bas — il MODEL COMPILER LLM e la DESCRIZIONE d'architettura.
'
' Mostra TUTTI i comandi del namespace LLM implementati:
'   ARCH   : LIST | RESOLVE <dir> | SHOW | CHECK <dir> | PLAN <dir>, <out>
'   POLICY : SHOW | SET <ruolo>, <formato> | RESET
'   MODEL COMPILER (verbi): PLAN | QUANT | MERGE | PRUNE | EXTRACT | PROFILE
'   SUPERFICIE .gmbarch : TENSOR <ROLE|LIST> | KERNEL SHOW | MODULE SHOW | KV SHOW
'   CONVERSIONE DI FORMATO : MODEL CONVERT <src>, <dst>[, GGUF|MLX]
'
' La descrizione (.gmbarch) e' data-driven: un modello = un file di descrizione, non codice
' per-architettura. Gli ARCH comandi lavorano sull'indice+config (leggeri); i verbi del model
' compiler (PLAN/QUANT/...) girano su file modello reali.
' ============================================================================
IMPORT LLM

SUB Main()
  MKDIR "work"

  PRINT "=== 1) DESCRIZIONE D'ARCHITETTURA (.gmbarch) ==="
  LLM ARCH LIST
  LLM ARCH RESOLVE "../../Fixtures/models/tiny_arch"
  LLM ARCH SHOW
  LLM ARCH CHECK "../../Fixtures/models/tiny_arch"
  LLM ARCH PLAN "../../Fixtures/models/tiny_arch", "work/tiny_plan.txt"
  PRINT "piano scritto in work/tiny_plan.txt"
  ' ARCHRECON: valida la descrizione `.gmbarch` risolta dal modello (UNICA fonte; GMAD eliminato).
  LLM TENSOR ARCHRECON "../../Fixtures/models/tiny_moe.gguf"

  PRINT "=== 2) POLICY dei formati (ruolo -> formato) ==="
  LLM POLICY SHOW
  LLM POLICY SET "ROUTED_EXPERT", "iq2_xxs"
  LLM POLICY SET "ATTN_KV", "q4_k"
  PRINT "-- dopo SET:"
  LLM POLICY SHOW
  LLM POLICY RESET

  PRINT "=== 3) I VERBI DEL MODEL COMPILER (dispatch verificato su tutti i 4 backend) ==="
  ' Su file ASSENTI mostrano l'errore ESPLICITO (mai silenzioso): qui li esercitiamo cosi',
  ' senza I/O pesante su modelli reali.
  ON ERROR GOTO E1
  LLM PLAN "work/no.gguf", "work/plan.txt"
  E1:
  ON ERROR GOTO E2
  LLM QUANT "work/no.gguf", "work/out.gguf"
  E2:
  ON ERROR GOTO E3
  LLM MERGE "work/a.gguf", "work/b.gguf", "work/out.gguf"
  E3:
  ON ERROR GOTO E4
  LLM PRUNE "work/no.gguf", "work/out.gguf", 4
  E4:
  ON ERROR GOTO E5
  LLM EXTRACT "work/no.gguf", "work/out.gguf", 0, 0
  E5:
  ON ERROR GOTO Fine
  LLM PROFILE "work/no.gguf", "work/out.txt"
  Fine:
  PRINT "tutti i verbi esercitati (errori catturati esplicitamente)"

  PRINT "=== 4) SUPERFICIE .gmbarch: TENSOR / KERNEL / MODULE / KV ==="
  ' TENSOR: ruolo + formato di un tensore (o lista di tutti i ruoli) dalla descrizione risolta.
  LLM TENSOR ROLE "../../Fixtures/models/tiny_arch", "layers.0.attn.wkv.weight"
  LLM TENSOR LIST "../../Fixtures/models/tiny_arch"
  ' KERNEL: primitive (kernel) dichiarate dalle strutture della descrizione.
  LLM KERNEL SHOW "../../Fixtures/models/tiny_arch"
  ' MODULE: strutture (moduli) dichiarate + se il converter le sa mappare.
  LLM MODULE SHOW "../../Fixtures/models/tiny_arch"
  ' KV: metadati RUNTIME (engine) della descrizione: state / kv.dtype / activation.dtype.
  LLM KV SHOW "../../Fixtures/models/tiny_arch"

  PRINT "=== 5) CONVERSIONE DI FORMATO: LLM MODEL CONVERT <src>, <dst>[, GGUF|MLX] ==="
  ' GGUF/MLX usano il converter del progetto codec (guidato dalla .gmbarch).
  ' container esperti. Qui su sorgente ASSENTE: errore ESPLICITO catturato (mai silenzioso).
  ON ERROR GOTO C1
  LLM MODEL CONVERT "work/no.safetensors", "work/out.gguf", "GGUF"
  C1:
  ' "F16": conversione a tensori F16 (sorgente per LLM QUANT/auto; solo F16/F32 sono quantizzabili).
  ON ERROR GOTO C2
  LLM MODEL CONVERT "work/no.safetensors", "work/out_f16.gguf", "F16"
  C2:
  PRINT "conversione di formato esercitata (GGUF + F16; errori espliciti catturati)"

  PRINT "=== 6) INSPECT + EMITTER: LLM MODEL INSPECT | SAVE <src>, <dst> ==="
  ' INSPECT: dump read-only del descriptor (dir HF oppure file .gguf).
  ON ERROR GOTO I1
  LLM MODEL INSPECT "work/no.gguf"
  I1:
  ' SAVE: EMITTER — round-trip/emissione dell'artefatto (metadata + byte tensori).
  ' Implementato su tutti e 5 i backend (IL/PCodeVM/VM/C/C99Native).
  ON ERROR GOTO S1
  LLM MODEL SAVE "work/no.gguf", "work/emitted.gguf"
  S1:
  PRINT "INSPECT + EMITTER esercitati (errori espliciti catturati)"
END SUB
