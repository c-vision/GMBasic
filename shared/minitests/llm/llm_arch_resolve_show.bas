' EXPECT: LLM ARCH 'tiny_arch' ()|ruoli (0):|quant (0):|recipe= boost_fmt= scale_suffix=|modality=text source.format=safetensors mmproj=
' MiniTest: LLM ARCH RESOLVE + SHOW su un modello SENZA descrittore dedicato (tiny_arch).
' Copre il RIPIEGO GENERICO: `arch` dal config.json, nessun ruolo/quant, path vuoto.
' NOTA (2026-10-29): la risoluzione legge SOLO la cartella ABI di LLMEnv (`paths.abiDir`), non
' piu' `<modello>/arch/*.gmbarch`: il descrittore per-modello della vecchia fixture non e' piu'
' consultato (una sola sede: due copie dello stesso descrittore divergevano). La copertura del
' descrittore REALE e' in llm_arch_check / llm_tensor_role, che usano la fixture tiny_hf
' (model_type=qwen3_moe, descrittore presente in LLMEnv/ABI).
IMPORT LLM
SUB Main()
  LLM ARCH RESOLVE "../../samples/Fixtures/models/tiny_arch"
  LLM ARCH SHOW
END SUB
