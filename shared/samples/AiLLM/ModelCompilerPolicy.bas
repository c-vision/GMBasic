' ModelCompilerPolicy.bas — Policy DSL Livello A (Model Compiler, namespace LLM).
' Regole dichiarative WHEN/THEN: l'utente guida il QUANTIZE per-tensore senza env.
IMPORT LLM

SUB Main()
  ' Ricetta: esperti q4k di default; ultimi layer (>=12) e down_exps a q6k.
  LLM POLICY DEFINE "ricetta_esperimenti", "ROLE=ROUTED_EXPERT&LAYER>=12:q6k;LIKE=*_down_exps*:q6k;DEFAULT:q4k"
  ' Mostra il piano SENZA attivarlo (report-only).
  LLM POLICY APPLY "ricetta_esperimenti" DRYRUN
  ' Attivala: il quantize la consulta per-tensore con precedenza sui default.
  LLM POLICY APPLY "ricetta_esperimenti"
  ' Diff vs default su un modello reale (PROJECTFOLDER risolve la cartella del sorgente).
  LLM POLICY DIFF, (PROJECTFOLDER() + "/../../Fixtures/models/tiny_moe.gguf")
  LLM POLICY ITEM 0
  PRINT "policy attiva: il quantize decide per-tensore dalla regola."

  ' --- QuantIR I11: policy a 4 assi (pesi + precisione del CALCOLO: activation/accum/output). ---
  LLM POLICY DEFINE "i11_calc", "ROLE=ATTENTION:w=q4_k,a=fp8,acc=f16,o=f16;DEFAULT:q4k"
  LLM POLICY APPLY "i11_calc"
  LLM TENSOR QUANTIR "../../Fixtures/models/tiny_moe.gguf"
  PRINT "I11: gli assi activation/accum/output sono risolti (report QUANTIR)."
END SUB
