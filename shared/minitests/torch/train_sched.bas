' MiniTest: L2 scheduler/warmup/accum/clip/resume (parita' 4 backend).
' Senza seed: schedule e resume sono deterministici (interi nel JSON di MODELSTATE),
' le loss solo direzione (convergenza), MAI valori esatti.
' JSON costruito con CHR(34) per non dipendere dall'escape di virgolette.
' EXPECT: sched: 1|step: 1|conv: 1|res: 1|acc: 1|clip: 1|bad: 1
IMPORT TORCH

SUB Main()
  DIM q AS STRING = CHR(34)
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg AS STRING = "{" + q + "vocab" + q + ":8," + q + "d_model" + q + ":16," + q + "n_layers" + q + ":1," + q + "n_heads" + q + ":1," + q + "block_size" + q + ":8}"
  DIM i AS LONG

  ' cosine + warmup 20, decay 200, 200 step
  DIM mc AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM initC AS DOUBLE = TORCH.MODELLOSS(mc, tA, t2)
  DIM opts AS STRING = "{" + q + "scheduler" + q + ":" + q + "cosine" + q + "," + q + "warmup_steps" + q + ":20," + q + "decay_steps" + q + ":200}"
  FOR i = 1 TO 200
    TORCH.MODELTRAINSTEP(mc, tA, t2, "adamw", "crossentropy", 0.05, opts)
  NEXT
  DIM stC AS STRING = TORCH.MODELSTATE(mc)
  DIM k1 AS STRING = q + "sched" + q + ":" + q + "cosine" + q
  DIM k2 AS STRING = q + "sched_step" + q + ":200"
  IF INSTR(stC, k1) > 0 THEN PRINT "sched: 1" ELSE PRINT "sched: 0"
  IF INSTR(stC, k2) > 0 THEN PRINT "step: 1" ELSE PRINT "step: 0"
  DIM endC AS DOUBLE = TORCH.MODELLOSS(mc, tA, t2)
  IF endC < initC THEN PRINT "conv: 1" ELSE PRINT "conv: 0"

  ' resume: 30 step con ckpt, nuovo modello stesso ckpt, 1 step -> step 31
  DIM rp AS STRING = "/tmp/gmb_sched_resume.json"
  ON ERROR RESUME NEXT
  KILL rp
  ON ERROR GOTO 0
  DIM m1 AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM ropts AS STRING = "{" + q + "resume_ckpt" + q + ":" + q + rp + q + "}"
  FOR i = 1 TO 30
    TORCH.MODELTRAINSTEP(m1, tA, t2, "adamw", "crossentropy", 0.05, ropts)
  NEXT
  TORCH.MODELFREE(m1)
  DIM m2 AS LONG = TORCH.MODELCREATE("transformer", cfg)
  TORCH.MODELTRAINSTEP(m2, tA, t2, "adamw", "crossentropy", 0.05, ropts)
  DIM stR AS STRING = TORCH.MODELSTATE(m2)
  DIM k3 AS STRING = q + "sched_step" + q + ":31"
  IF INSTR(stR, k3) > 0 THEN PRINT "res: 1" ELSE PRINT "res: 0"
  TORCH.MODELFREE(m2)
  ON ERROR RESUME NEXT
  KILL rp
  ON ERROR GOTO 0

  ' accum + clip smoke: finiti
  DIM ma AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM aopts AS STRING = "{" + q + "accum_steps" + q + ":2}"
  FOR i = 1 TO 10
    TORCH.MODELTRAINSTEP(ma, tA, t2, "adamw", "crossentropy", 0.05, aopts)
  NEXT
  IF TORCH.MODELLOSS(ma, tA, t2) < 1000000 THEN PRINT "acc: 1" ELSE PRINT "acc: 0"
  DIM mk AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM copts AS STRING = "{" + q + "clip_norm" + q + ":1.0}"
  FOR i = 1 TO 10
    TORCH.MODELTRAINSTEP(mk, tA, t2, "adamw", "crossentropy", 0.05, copts)
  NEXT
  IF TORCH.MODELLOSS(mk, tA, t2) < 1000000 THEN PRINT "clip: 1" ELSE PRINT "clip: 0"

  ' scheduler non valido -> errore esplicito (-1)
  DIM rc AS LONG = TORCH.MODELTRAINSTEP(ma, tA, t2, "adamw", "crossentropy", 0.05, "{" + q + "scheduler" + q + ":" + q + "xyz" + q + "}")
  IF rc = -1 THEN PRINT "bad: 1" ELSE PRINT "bad: 0"

  TORCH.MODELFREE(mc) : TORCH.MODELFREE(ma) : TORCH.MODELFREE(mk)
  TORCH.MODELFREE(tA) : TORCH.MODELFREE(t2)
END SUB
