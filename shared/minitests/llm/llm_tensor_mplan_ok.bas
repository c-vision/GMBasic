' MiniTest: LLM TENSOR MPLAN positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR MPLAN '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe layers=2 (M10 compile-time memory planner)|# MEMORY OBJECTS (region	bytes	device	lifetime	residency	prefetch	eviction	transfer_cost)|embed/output	80	gpu	model	RESIDENT	NONE	NEVER	one-time(mmap)|per-layer weights	384	gpu	model	RESIDENT	NEXT_LAYER	WHEN_PRESSURE	one-time(mmap)|experts	192	gpu|ssd	model/block	RESIDENT|COLD	ROUTER->N_BLOCKS_AHEAD	WHEN_PRESSURE	stream_on_use|kv	kv/token*session	gpu	session	RESIDENT	NONE	AFTER_BLOCK	0|activation	scratch	gpu	step	TRANSIENT	NONE	AFTER_BLOCK	0|# totale pesi=0.0 MiB (embed/output=0.0, per-layer=0.0, experts=0.0)|# DATA-MOVEMENT PLAN: PREFETCH expert-group BEFORE MOE_LAYER[N] (router) | RESIDENT KV FOR DECODE | WIRE weights-at-load | EVICT on_pressure | UNWIRE after_block
IMPORT LLM
SUB Main()
  LLM TENSOR MPLAN "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
