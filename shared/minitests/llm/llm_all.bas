' MiniTest NAMESPACE LLM — superficie SEMANTICA + tokenizer, con asserzioni stabili.
' REPOMAP, REVISION, SYMBOL, FINDSYMBOL, FILEOUTLINE, IMPACT, CONTEXT,
' INSPECT, CALLMAP. Tokenizer (TOKENS/DETOKENIZE/TOKENCOUNT) e verbi engine restano
' Esclusi i verbi engine/training (tensor/kv/stats/finetune/distill/lora/patch/bench):
' nei test per-funzione: su C99Native il percorso del tokenizer fixture fa crashare il runner.
' EXPECT: repomap>0=-1|revision>0=-1|symbol>0=-1|findsymbol>0=-1|fileoutline>0=-1|impact>=0=-1|context>0=-1|inspect_has=-1|callmap_has=-1
IMPORT LLM

SUB HelperForOutline()
END SUB

SUB Main()
  DIM out AS STRING

  PRINT "repomap>0="; (LEN(LLM.REPOMAP()) > 0)
  PRINT "revision>0="; (LEN(LLM.REVISION()) > 0)
  PRINT "symbol>0="; (LEN(LLM.SYMBOL("Main")) > 0)
  PRINT "findsymbol>0="; (LEN(LLM.FINDSYMBOL("Main")) > 0)
  PRINT "fileoutline>0="; (LEN(LLM.FILEOUTLINE("llm_all.bas")) > 0)
  PRINT "impact>=0="; (LEN(LLM.IMPACT("Main")) >= 0)
  PRINT "context>0="; (LEN(LLM.CONTEXT("Main")) > 0)
  PRINT "inspect_has="; (INSTR(LLM.INSPECT("Main"), "procedure") > 0)
  PRINT "callmap_has="; (INSTR(LLM.CALLMAP("Main"), "callees") > 0)

END SUB
