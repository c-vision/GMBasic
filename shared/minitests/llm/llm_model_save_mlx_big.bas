' MiniTest: LLM MODEL SAVE <gguf>, <dir>.mlx su fixture grande -> emitter MLX + round-trip. 4 backend.
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM MODEL SAVE "../../samples/Fixtures/models/tiny_big_moe.gguf", "bin/emit.mlx"
  IF IO.FILEEXISTS("bin/emit.mlx/config.json") AND IO.FILEEXISTS("bin/emit.mlx/model.safetensors") THEN
    PRINT "save-mlx: config+safetensors ok"
  ELSE
    PRINT "save-mlx: FALLITO"
  END IF
  LLM MODEL CONVERT "bin/emit.mlx", "bin/back.gguf", "GGUF"
  IF IO.FILEEXISTS("bin/back.gguf") THEN
    PRINT "save-mlx: round-trip ok"
  ELSE
    PRINT "save-mlx: round-trip FALLITO"
  END IF
END SUB
