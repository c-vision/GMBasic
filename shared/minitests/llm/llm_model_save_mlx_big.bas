' MiniTest: LLM MODEL SAVE <gguf>, <dir>.mlx su fixture grande -> emitter MLX + round-trip. 4 backend.
' EXPECT: save-mlx: config+safetensors ok|save-mlx: round-trip ok
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  LLM MODEL SAVE "../../samples/Fixtures/models/tiny_big_moe.gguf", GMBWORK$ + "/emit.mlx"
  IF IO.FILEEXISTS(GMBWORK$ + "/emit.mlx/config.json") AND IO.FILEEXISTS(GMBWORK$ + "/emit.mlx/model.safetensors") THEN
    PRINT "save-mlx: config+safetensors ok"
  ELSE
    PRINT "save-mlx: FALLITO"
  END IF
  LLM MODEL CONVERT GMBWORK$ + "/emit.mlx", GMBWORK$ + "/back.gguf", "GGUF"
  IF IO.FILEEXISTS(GMBWORK$ + "/back.gguf") THEN
    PRINT "save-mlx: round-trip ok"
  ELSE
    PRINT "save-mlx: round-trip FALLITO"
  END IF
END SUB
