' FILE: AI/ProviderConfig.bas
' ProviderConfig — register, configure, rotate embedding/completion providers.

IMPORT AI WITH ALIAS
IMPORT SQL WITH ALIAS

SUB Main()
  DIM ok AS BIT
  DIM activeName AS STRING
  DIM rotated AS STRING
  DIM answer$ AS STRING

  ok = AI.REGISTER("embedding", "emb-backup", "fixture", "")
  ok = AI.REGISTER("embedding", "ollama-demo", "ollama", "{""baseUrl"":""http://127.0.0.1:1"",""model"":""nomic-embed-text"",""dimensions"":""384""}")
  ok = AI.REGISTER("completion", "openai-demo", "openai", "{""apiKey"":""sk-demo"",""model"":""gpt-4o-mini""}")
  ok = AI.SETACTIVE("embedding", "emb-backup")
  ok = AI.SAVE("providers-demo.json")
  PRINT "Saved providers-demo.json"
  ' Re-register directly (AI.LOAD requires $GMBASIC_OPENAI_DEMO_APIKEY env var)
  ' Set it via: export GMBASIC_OPENAI_DEMO_APIKEY=sk-demo
  ok = AI.REGISTER("completion", "openai-demo", "openai", "{""apiKey"":""sk-demo"",""model"":""gpt-4o-mini""}")
  ok = AI.SETACTIVE("completion", "openai-demo")
  activeName = AI.ACTIVE("embedding")
  PRINT "Embedding active:"; activeName
  rotated = AI.ROTATE("embedding")
  PRINT "Rotate ->"; rotated

  SQL OPEN ":memory:" ALIAS appdb
  AI RAG INDEX TEXT "appdb", "docs", "p1", "Provider server routes embedding and completion."
  answer$ = AI.ASK("appdb", "docs", "What routes embedding?", 2)
  PRINT answer$
END SUB

ON ERROR RESUME NEXT
KILL "providers-demo.json"
KILL "Saved providers-demo.json"
