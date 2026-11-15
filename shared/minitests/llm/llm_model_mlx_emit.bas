' MiniTest: EMITTER MLX — LLM MODEL SAVE <gguf>, <dir>.mlx produce un artefatto MLX
' (config.json + model.safetensors) e il round-trip MLX->GGUF (LLM MODEL CONVERT) regge.
' EXPECT: emitter-mlx: config+safetensors scritti|round-trip: mlx->gguf ok
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  LLM MODEL SAVE "../../samples/Fixtures/models/tiny_moe.gguf", GMBWORK$ + "/tiny_emit.mlx"
  IF IO.FILEEXISTS(GMBWORK$ + "/tiny_emit.mlx/config.json") AND IO.FILEEXISTS(GMBWORK$ + "/tiny_emit.mlx/model.safetensors") THEN
    PRINT "emitter-mlx: config+safetensors scritti"
  ELSE
    PRINT "emitter-mlx: FALLITO"
  END IF
  LLM MODEL CONVERT GMBWORK$ + "/tiny_emit.mlx", GMBWORK$ + "/tiny_emit_rt.gguf", "GGUF"
  IF IO.FILEEXISTS(GMBWORK$ + "/tiny_emit_rt.gguf") THEN
    PRINT "round-trip: mlx->gguf ok"
  ELSE
    PRINT "round-trip: FALLITO"
  END IF
END SUB
