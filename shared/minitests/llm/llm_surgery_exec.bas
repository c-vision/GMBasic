' MiniTest: i tool SURGERY producono modelli ESEGUIBILI (load+forward), non solo IR. Fixture piccolo.
' Percorsi RELATIVI (la CWD del test = questa cartella). Ogni surgery -> AI REGISTER + generazione;
' se il forward fallisce (bug "modello surgery non eseguibile") -> 'caught'. Altrimenti 'ran'.
' EXPECT: ran
IMPORT AI
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  ON ERROR GOTO H
  LLM TENSOR MOEDENSE "../../samples/Fixtures/models/tiny_big_moe.gguf", GMBWORK$ + "/td_dense.gguf"
  LLM POLICY SET "PRUNE_HEADS", "2"
  LLM TENSOR PRUNEHEADS "../../samples/Fixtures/models/tiny_big_moe.gguf", GMBWORK$ + "/td_h2.gguf"
  ' PRUNEFFN taglia il FFN DENSO -> si applica all'uscita di MOEDENSE (il fixture è MoE).
  LLM POLICY SET "FFN_KEEP", "128"
  LLM TENSOR PRUNEFFN GMBWORK$ + "/td_dense.gguf", GMBWORK$ + "/td_f128.gguf"
  DIM r AS STRING
  AI REGISTER "d1", "completion", GMBWORK$ + "/td_dense.gguf", "{""engine"":""mlx"",""maxTokens"":""4"",""contextSize"":""128""}"
  AI SET ACTIVE "completion", "d1"
  AI AGENT CREATE "a1"
  AI AGENT SET "a1", "skills", "OFF"
  AI AGENT SET "a1", "memory", "OFF"
  AI AGENT SET "a1", "plugins", "OFF"
  AI AGENT SET "a1", "maxtokens", "4"
  r = AI.AGENTASKSYNC("a1", "hi")
  AI AGENT DELETE "a1"
  AI REGISTER "d2", "completion", GMBWORK$ + "/td_h2.gguf", "{""engine"":""mlx"",""maxTokens"":""4"",""contextSize"":""128""}"
  AI SET ACTIVE "completion", "d2"
  AI AGENT CREATE "a2"
  AI AGENT SET "a2", "skills", "OFF"
  AI AGENT SET "a2", "memory", "OFF"
  AI AGENT SET "a2", "plugins", "OFF"
  AI AGENT SET "a2", "maxtokens", "4"
  r = AI.AGENTASKSYNC("a2", "hi")
  AI AGENT DELETE "a2"
  AI REGISTER "d3", "completion", GMBWORK$ + "/td_f128.gguf", "{""engine"":""mlx"",""maxTokens"":""4"",""contextSize"":""128""}"
  AI SET ACTIVE "completion", "d3"
  AI AGENT CREATE "a3"
  AI AGENT SET "a3", "skills", "OFF"
  AI AGENT SET "a3", "memory", "OFF"
  AI AGENT SET "a3", "plugins", "OFF"
  AI AGENT SET "a3", "maxtokens", "4"
  r = AI.AGENTASKSYNC("a3", "hi")
  AI AGENT DELETE "a3"
  LLM POLICY CLEARITEM
  PRINT "ran"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
