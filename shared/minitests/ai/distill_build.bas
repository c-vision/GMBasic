' MiniTest: Stream D / D1 — TRAIN DISTILL BUILD (jsonl -> .bin) — parita' 4 backend.
' Usa un tokenizer BPE fixture (tiny) e un jsonl di 2 coppie; verifica che i .bin siano
' scritti e che i conteggi siano consistenti (pairs>0, train+hold>0).
' EXPECT: ok: 1|train: 1|both: 1
IMPORT AI
IMPORT LLM
IMPORT IO

SUB Main()
  DIM fixdir AS STRING = PROJECTFOLDER() + "/../../samples/Fixtures/ai"
  DIM tok AS STRING = fixdir + "/tiny_tokenizer.json"
  DIM jp AS STRING = "/tmp/gmb_d1_pairs.jsonl"
  DIM out AS STRING = "/tmp/gmb_d1_ds"

  DIM h AS LONG = IO.FCREATE(jp)
  IO.FWRITE(h, "{""prompt"": ""hello world"", ""response"": ""two plus two""}" + CHR(10))
  IO.FWRITE(h, "{""question"": ""the cat"", ""answer"": ""sat on the mat""}" + CHR(10))
  IO.FCLOSE(h)

  TRAIN DISTILL BUILD jp, { "tokenizer": tok, "out": out, "window": 4, "holdout": 0.5 }
  DIM st AS STRING = LLM.DISTILLSTATUS()
  DIM q AS STRING = CHR(34)
  IF INSTR(st, q + "pairs" + q + ":2") > 0 THEN PRINT "ok: 1" ELSE PRINT "ok: 0"
  IF IO.FILEEXISTS(out + ".train.bin") THEN PRINT "train: 1" ELSE PRINT "train: 0"
  IF IO.FILEEXISTS(out + ".train.bin") AND IO.FILEEXISTS(out + ".hold.bin") THEN PRINT "both: 1" ELSE PRINT "both: 0"

  ON ERROR RESUME NEXT
  KILL jp
  KILL out + ".train.bin"
  KILL out + ".hold.bin"
  ON ERROR GOTO 0
END SUB
