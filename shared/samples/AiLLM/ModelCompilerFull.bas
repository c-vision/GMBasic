' ModelCompilerFull.bas — superficie COMPLETA del Model Compiler (namespace LLM) + registry moduli.
' Copre OGNI comando/funzione implementato:
'   POLICY  DEFINE, APPLY [DRYRUN], DIFF, ITEMS, ITEM <i>, SETITEM, CALLBACK <Proc>, CLEARITEM,
'           SET, SHOW, RESET, SAVE, LOAD, GATE
'   MODEL   SAVE <gguf>,<dir>.mlx   (emitter MLX) + CONVERT (round-trip)
'   GMB MODULE  REGISTER, PRIORITY, LIST, CALL
' La fixture viene copiata in work/ cosi' GATE scrive l'output accanto alla copia (mai in Fixtures).

IMPORT LLM

SUB Main()
  MKDIR "work"

  ' --- prepara una copia di lavoro (GATE scrive <base>.out.gguf accanto alla base) ---
  COPY "../Fixtures/models/tiny_moe.gguf" TO "work/tiny_moe.gguf"

  ' --- POLICY Livello A: regole WHEN/THEN (DEFINE, APPLY dry-run e attivo) ---
  LLM POLICY DEFINE "full", "ROLE=ROUTED_EXPERT&LAYER>=12:q6k;LIKE=*_down_exps*:q6k;DEFAULT:q4k"
  LLM POLICY APPLY "full" DRYRUN
  LLM POLICY APPLY "full"

  ' --- DIFF: piano per-tensore vs default (report-only) ---
  LLM POLICY DIFF, "work/tiny_moe.gguf"

  ' --- Piano memorizzato: lista (ITEMS) e singolo item (ITEM <i>) ---
  LLM POLICY ITEMS
  LLM POLICY ITEM 0

  ' --- POLICY Livello B: override per-tensore e callback algoritmico (GPM) ---
  LLM POLICY SETITEM "blk.0.ffn_gate_exps.weight", "q6k"
  LLM POLICY CALLBACK OnDecide
  LLM POLICY CLEARITEM

  ' --- POLICY per-ruolo (legacy): SET, SHOW, RESET ---
  LLM POLICY SET "FFN", "q4_0"
  LLM POLICY SHOW
  LLM POLICY RESET

  ' --- Persistenza della DSL: DEFINE -> SAVE -> LOAD ---
  LLM POLICY DEFINE "persist", "DEFAULT:q4k"
  LLM POLICY SAVE "work/full.gmbpolicy"
  LLM POLICY LOAD "work/full.gmbpolicy"

  ' --- GATE: quantizza la base con la policy attiva, misura e (se rifiutato) fa ROLLBACK ---
  LLM POLICY GATE, "work/tiny_moe.gguf"

  ' --- Emitter MLX + round-trip ---
  LLM MODEL SAVE "work/tiny_moe.gguf", "work/tiny_emit.mlx"
  IF IO.FILEEXISTS("work/tiny_emit.mlx/config.json") AND IO.FILEEXISTS("work/tiny_emit.mlx/model.safetensors") THEN
    PRINT "emitter-mlx: ok"
  ELSE
    PRINT "emitter-mlx: FALLITO"
  END IF
  LLM MODEL CONVERT "work/tiny_emit.mlx", "work/tiny_rt.gguf", "GGUF"

  ' --- Registry moduli esterni: register/priority/list/call ---
  GMB MODULE REGISTER "full.slot", "/usr/lib/libSystem.B.dylib"
  GMB MODULE PRIORITY "full.slot", 7
  GMB MODULE LIST
  GMB MODULE CALL "full.slot", "llabs|ii|-9"

  PRINT "full:done"
END SUB

FUNCTION OnDecide(name AS STRING, role AS STRING, dflt AS STRING) AS STRING
  IF role = "ROUTED_EXPERT" THEN RETURN "q6k"
  RETURN dflt
END FUNCTION
