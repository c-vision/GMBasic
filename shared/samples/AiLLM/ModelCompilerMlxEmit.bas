' MiniTest: EMITTER MLX — LLM MODEL SAVE <gguf>, <dir>.mlx produce un artefatto MLX
' (config.json + model.safetensors) e il round-trip MLX->GGUF (LLM MODEL CONVERT) regge.
IMPORT LLM
SUB Main()
  MKDIR "work"
  LLM MODEL SAVE "../../Fixtures/models/tiny_moe.gguf", "work/tiny_emit.mlx"
  IF IO.FILEEXISTS("work/tiny_emit.mlx/config.json") AND IO.FILEEXISTS("work/tiny_emit.mlx/model.safetensors") THEN
    PRINT "emitter-mlx: config+safetensors scritti"
  ELSE
    PRINT "emitter-mlx: FALLITO"
  END IF
  LLM MODEL CONVERT "work/tiny_emit.mlx", "work/tiny_emit_rt.gguf", "GGUF"
  IF IO.FILEEXISTS("work/tiny_emit_rt.gguf") THEN
    PRINT "round-trip: mlx->gguf ok"
  ELSE
    PRINT "round-trip: FALLITO"
  END IF
END SUB
