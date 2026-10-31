' MiniTest: LLM POLICY DEFINE + SAVE/LOAD .gmbpolicy — la policy DSL sopravvive al round-trip.
IMPORT LLM
SUB Main()
  MKDIR "work"
  LLM POLICY DEFINE "mypol", "ROLE=ROUTED_EXPERT&LAYER>=1:q6k;LIKE=*_down_exps:q6k;DEFAULT:q4k"
  LLM POLICY SAVE "work/p.gmbpolicy"
  LLM POLICY LOAD "work/p.gmbpolicy"
  LLM POLICY APPLY "mypol", "DRYRUN"
END SUB
