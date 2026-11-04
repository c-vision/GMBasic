' MiniTest: LLM MODEL SAVE <gguf>, <dir>.mlx su fixture grande -> emitter MLX + round-trip. 4 backend.
IMPORT LLM
SUB Main()
  MKDIR "work"
  LLM MODEL SAVE "../../samples/Fixtures/models/tiny_big_moe.gguf", "work/emit.mlx"
  IF IO.FILEEXISTS("work/emit.mlx/config.json") AND IO.FILEEXISTS("work/emit.mlx/model.safetensors") THEN
    PRINT "save-mlx: config+safetensors ok"
  ELSE
    PRINT "save-mlx: FALLITO"
  END IF
  LLM MODEL CONVERT "work/emit.mlx", "work/back.gguf", "GGUF"
  IF IO.FILEEXISTS("work/back.gguf") THEN
    PRINT "save-mlx: round-trip ok"
  ELSE
    PRINT "save-mlx: round-trip FALLITO"
  END IF
END SUB
