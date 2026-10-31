' EXPECT: {|"version": 1,|"providers": {|"fixture-embedding": {|"role": "embedding",|"kind": "fixture",|"config": {},|"priority": 0,|"status": "Available",|"weight": 1,|"isFree": false,|"costInputPer1m": 0,|"costOutputPer1m": 0,|"creditsRemaining": -1|},|"fixture-completion": {|"role": "completion",|"kind": "fixture",|"config": {},|"priority": 0,|"status": "Available",|"weight": 1,|"isFree": false,|"costInputPer1m": 0,|"costOutputPer1m": 0,|"creditsRemaining": -1|},|"keyword-reranker": {|"role": "reranker",|"kind": "keyword",|"config": {},|"priority": 0,|"status": "Available",|"weight": 1,|"isFree": false,|"costInputPer1m": 0,|"costOutputPer1m": 0,|"creditsRemaining": -1|}|},|"roleOrder": {|"embedding": [|"fixture-embedding"|],|"completion": [|"fixture-completion"|],|"reranker": [|"keyword-reranker"|]|},|"activeIndex": {|"embedding": 0,|"completion": 0,|"reranker": 0|},|"schemes": {|"hf": "https://huggingface.co",|"huggingface": "https://huggingface.co",|"ollama": "http://localhost:11434",|"openai": "https://api.openai.com/v1",|"anthropic": "https://api.anthropic.com/v1"|}|}
IMPORT AI

SUB Main()
    PRINT AI.PROVIDERS()
END SUB
