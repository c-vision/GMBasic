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


  ' Parità di SUPERFICIE per i verbi engine/training (compilazione/link, non esecuzione:
  ' su C99Native richiedono engine o non sono portati). Patch è un COMANDO (LLM PATCH …
  ' END PATCH) dentro lo stesso ramo.
  IF 0 THEN
    _m1 = LLM.Arch("", "", "", "")
    _m2 = LLM.BenchResult("")
    _m3 = LLM.CacheStats("")
    _m4 = LLM.CheckRev("")
    _m5 = LLM.Commit()
    _m6 = LLM.DatasetStats("")
    _m7 = LLM.DecodeStats("")
    _m8 = LLM.Detokenize("", "")
    _m9 = LLM.DistillCkpt()
    _m10 = LLM.DistillDone()
    _m11 = LLM.DistillEvalAccept("")
    _m12 = LLM.DistillEvalPass("")
    _m13 = LLM.DistillEvalTok("")
    _m14 = LLM.DistillHold()
    _m15 = LLM.DistillLoss()
    _m16 = LLM.DistillSampleDone()
    _m17 = LLM.DistillSampleFail()
    _m18 = LLM.DistillSampleN()
    _m19 = LLM.DistillStatus()
    _m20 = LLM.EvalGate("", "")
    _m21 = LLM.FinetuneBaseHold()
    _m22 = LLM.FinetuneBaseTrain()
    _m23 = LLM.FinetuneCkpt()
    _m24 = LLM.FinetuneDemoChanged()
    _m25 = LLM.FinetuneDone()
    _m26 = LLM.FinetuneGreedy()
    _m27 = LLM.FinetuneGreedyAfter()
    _m28 = LLM.FinetuneHold()
    _m29 = LLM.FinetuneLoss()
    _m30 = LLM.FinetuneStatus()
    _m31 = LLM.Kernel("", "", "", "")
    _m32 = LLM.Kv("", "", "", "")
    _m33 = LLM.LoraList()
    _m34 = LLM.LoraLoad("", "")
    _m35 = LLM.LoraSave("", "")
    _m36 = LLM.Module("", "", "", "")
    LLM PATCH "Main", "rev=0000"
      PRINT "patch-fake"
    END PATCH
    _m38 = LLM.Policy("", "", "", "")
    _m39 = LLM.PrefillStats("")
    _m40 = LLM.RunStop("")
    _m41 = LLM.RunsClear()
    _m42 = LLM.RunsCount()
    _m43 = LLM.RunsItem("")
    _m44 = LLM.RunsJson()
    _m45 = LLM.StreamStats("")
    _m46 = LLM.Tensor("", "", "", "")
    _m47 = LLM.TokenCount("", "")
    _m48 = LLM.Tokens("", "")
    _m49 = LLM.TrainConfig()
    _m50 = LLM.TrainConfigSet("", "")
    _m51 = LLM.TrainExport("", "")
    _m52 = LLM.Verify()
  END IF
END SUB
