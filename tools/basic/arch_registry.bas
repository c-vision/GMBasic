' arch_registry.bas — OpenArch A1 (registry architetture come dato) + A2 (validatore).
' Equivalente del vecchio tools/arch_registry.py, senza Python.
'
' Uso: gmb arch_registry.bas selftest
'      gmb arch_registry.bas build <model_dir> [--out DIR]
'      gmb arch_registry.bas build-all <models_root> [--out DIR]
'      gmb arch_registry.bas validate <model_dir> <registry.json>
'      gmb arch_registry.bas checks <registry.json>
IMPORT IO
IMPORT JSON

DIM g_err AS LONG
DIM g_spec AS JSON
DIM g_contract AS JSON
DIM g_reg AS JSON

FUNCTION Q(s AS STRING) AS STRING
  RETURN CHR$(34) + s + CHR$(34)
END FUNCTION

FUNCTION JNull(s AS STRING) AS INTEGER
  IF LEN(s) = 0 THEN RETURN -1
  IF s = "null" THEN RETURN -1
  IF s = Q("null") THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION BaseName(p AS STRING) AS STRING
  DIM i AS LONG, last AS LONG
  last = 0
  i = 1
  DO WHILE i <= LEN(p)
    IF MID$(p, i, 1) = "/" THEN last = i
    i = i + 1
  LOOP
  IF last > 0 THEN RETURN MID$(p, last + 1)
  RETURN p
END FUNCTION

SUB PutNum(o AS JSON, k AS STRING, v AS STRING)
  IF LEN(v) = 0 THEN
    o(k) = JSON.PARSE("null")
  ELSE
    o(k) = VAL(v)
  END IF
END SUB

SUB PutStr(o AS JSON, k AS STRING, v AS STRING)
  IF LEN(v) = 0 THEN
    o(k) = JSON.PARSE("null")
  ELSE
    o(k) = v
  END IF
END SUB

SUB PutBool(o AS JSON, k AS STRING, v AS STRING)
  DIM b AS JSON
  IF LEN(v) = 0 THEN
    o(k) = JSON.PARSE("null")
  ELSE
    IF LOWER$(v) = "true" THEN
      o(k) = TRUE
    ELSE
      o(k) = FALSE
    END IF
  END IF
END SUB

SUB PutRaw(o AS JSON, k AS STRING, v AS STRING)
  ' VM/C: membro JSON assente -> "" oppure "{}" (non null); normalizza.
  IF v = "" OR v = "{}" OR v = "null" OR LEN(v) = 0 OR JNull(v) <> 0 THEN
    o(k) = JSON.PARSE("null")
  ELSE
    o(k) = JSON.PARSE(v)
  END IF
END SUB

FUNCTION First(tc AS JSON, cfg AS JSON, name AS STRING) AS STRING
  DIM v AS STRING
  v = JSON.EXTRACT(tc, name)
  IF LEN(v) > 0 THEN RETURN v
  RETURN JSON.EXTRACT(cfg, name)
END FUNCTION

FUNCTION FirstOf(tc AS JSON, cfg AS JSON, names AS STRING) AS STRING
  DIM a$() AS STRING
  DIM i AS LONG, v AS STRING
  a$ = SPLIT(names, ",")
  i = 0
  DO WHILE i <= UBOUND(a$)
    v = First(tc, cfg, a$(i))
    IF LEN(v) > 0 THEN RETURN v
    i = i + 1
  LOOP
  RETURN ""
END FUNCTION

FUNCTION Conv(mt AS STRING) AS STRING
  IF mt = "llama" OR mt = "qwen2" OR mt = "qwen3" OR mt = "qwen3_moe" OR mt = "qwen3_next" OR mt = "qwen3_5_text" OR mt = "qwen3_5_moe_text" OR mt = "gpt_oss" THEN RETURN "neox|pre|perlayer|none"
  IF mt = "qwen4_exp_text" THEN RETURN "neox|pre|perlayer|gdn"
  IF mt = "gemma4_text" THEN RETURN "neox|pre_post|perlayer|none"
  IF mt = "glm4_moe" THEN RETURN "neox-partial|pre|perlayer|none"
  IF mt = "deepseek_v2" THEN RETURN "neox|pre|mla|none"
  IF mt = "kimi_linear" THEN RETURN "none|pre|perlayer|linear"
  RETURN ""
END FUNCTION

FUNCTION Attn(heads AS STRING, kv AS STRING) AS STRING
  IF LEN(heads) = 0 OR LEN(kv) = 0 THEN RETURN ""
  IF kv = "1" THEN RETURN "MQA"
  IF kv = heads THEN RETURN "MHA"
  RETURN "GQA"
END FUNCTION

SUB Extract(cfg AS JSON)
  DIM tc AS JSON, tct AS STRING, mt AS STRING, v AS STRING, sw AS STRING
  DIM rs AS STRING, q AS STRING, bits AS STRING, gs AS STRING, arch AS STRING
  DIM conv AS STRING, rope AS STRING, resid AS STRING, kvt AS STRING, rec AS STRING
  DIM heads AS STRING, kv AS STRING, hd AS STRING, eps AS STRING, tie AS STRING
  DIM maskv AS STRING, i AS LONG, qo AS JSON, qn AS JSON, kk AS STRING
  DIM a$() AS STRING
  tct = JSON.STRINGIFY(cfg.text_config)
  IF JNull(tct) <> 0 THEN tc = cfg ELSE tc = JSON.PARSE(tct)
  g_spec = JSON.PARSE("{}")
  g_contract = JSON.PARSE("{}")
  mt = First(tc, cfg, "model_type")
  PutStr(g_spec, "model_type", mt)
  PutNum(g_spec, "hidden_size", First(tc, cfg, "hidden_size"))
  v = FirstOf(tc, cfg, "num_hidden_layers,num_layers")
  PutNum(g_spec, "num_hidden_layers", v)
  PutNum(g_spec, "num_attention_heads", First(tc, cfg, "num_attention_heads"))
  PutNum(g_spec, "num_key_value_heads", First(tc, cfg, "num_key_value_heads"))
  PutNum(g_spec, "head_dim", First(tc, cfg, "head_dim"))
  PutNum(g_spec, "intermediate_size", First(tc, cfg, "intermediate_size"))
  v = First(tc, cfg, "num_local_experts")
  IF LEN(v) = 0 THEN v = First(tc, cfg, "num_experts")
  IF LEN(v) = 0 THEN v = First(tc, cfg, "n_routed_experts")
  PutNum(g_spec, "num_experts", v)
  v = First(tc, cfg, "num_experts_per_tok")
  IF LEN(v) = 0 THEN v = First(tc, cfg, "experts_per_token")
  IF LEN(v) = 0 THEN v = First(tc, cfg, "n_expert_used")
  PutNum(g_spec, "experts_per_token", v)
  PutNum(g_spec, "rope_theta", First(tc, cfg, "rope_theta"))
  v = FirstOf(tc, cfg, "rms_norm_eps,layer_norm_eps")
  PutNum(g_spec, "rms_norm_eps", v)
  PutBool(g_spec, "tie_word_embeddings", First(tc, cfg, "tie_word_embeddings"))
  sw = JSON.EXTRACT(tc, "sliding_window")
  IF LEN(sw) = 0 THEN sw = JSON.EXTRACT(cfg, "sliding_window")
  IF LEN(sw) > 0 AND LEFT$(sw, 1) = "[" THEN
    maskv = ""
    i = 0
    DO WHILE i <= 9
      v = JSON.EXTRACT(tc, "sliding_window[" + LTRIM$(MID$(STR$(i), 1)) + "]")
      IF LEN(v) = 0 THEN v = JSON.EXTRACT(cfg, "sliding_window[" + LTRIM$(MID$(STR$(i), 1)) + "]")
      IF LEN(v) = 0 THEN EXIT DO
      IF VAL(v) <> 0 AND LEN(maskv) = 0 THEN maskv = v
      i = i + 1
    LOOP
  ELSEIF LEN(sw) > 0 THEN
    maskv = sw
  ELSE
    maskv = ""
  END IF
  PutNum(g_spec, "sliding_window", maskv)
  PutNum(g_spec, "vocab_size", First(tc, cfg, "vocab_size"))
  rs = JSON.STRINGIFY(tc.rope_scaling)
  IF rs = "{}" OR rs = "" THEN rs = "null"
  IF JNull(rs) <> 0 THEN rs = JSON.STRINGIFY(cfg.rope_scaling)
  IF rs = "{}" OR rs = "" THEN rs = "null"
  IF rs = "null" OR rs = "{}" OR rs = CHR$(34)+CHR$(34) OR LEN(rs) = 0 THEN
    g_spec("rope_scaling") = JSON.PARSE("null")
  ELSE
    g_spec("rope_scaling") = JSON.PARSE(rs)
  END IF
  q = JSON.STRINGIFY(tc.quantization)
  IF q = "{}" OR q = "" THEN q = "null"
  IF JNull(q) <> 0 THEN q = JSON.STRINGIFY(cfg.quantization)
  IF q = "{}" OR q = "" THEN q = "null"
  IF q = "null" OR q = "{}" OR q = CHR$(34)+CHR$(34) OR LEN(q) = 0 THEN
    g_spec("quant") = JSON.PARSE("null")
  ELSE
    qo = JSON.PARSE(q)
    bits = JSON.EXTRACT(qo, "bits")
    gs = JSON.EXTRACT(qo, "group_size")
    qn = JSON.PARSE("{}")
    PutNum(qn, "bits", bits)
    PutNum(qn, "group_size", gs)
    g_spec("quant") = qn
  END IF
  arch = JSON.STRINGIFY(cfg.architectures)
  PutRaw(g_spec, "architectures", arch)
  heads = JSON.EXTRACT(g_spec, "num_attention_heads")
  kv = JSON.EXTRACT(g_spec, "num_key_value_heads")
  hd = JSON.EXTRACT(g_spec, "head_dim")
  eps = JSON.EXTRACT(g_spec, "rms_norm_eps")
  tie = JSON.EXTRACT(g_spec, "tie_word_embeddings")
  conv = Conv(mt)
  IF LEN(conv) = 0 THEN
    rope = "null"
    resid = "null"
    kvt = "null"
    rec = "null"
  ELSE
    a$ = SPLIT(conv, "|")
    rope = Q(a$(0)) : resid = Q(a$(1)) : kvt = Q(a$(2)) : rec = Q(a$(3))
  END IF
  PutStr(g_contract, "_source", "convenzione dell" + CHR$(39) + "architettura (best-effort; verificare per-modello se critico)")
  PutRaw(g_contract, "rope_convention", rope)
  PutStr(g_contract, "attention", Attn(heads, kv))
  PutNum(g_contract, "kv_heads", kv)
  PutNum(g_contract, "q_heads", heads)
  PutNum(g_contract, "head_dim", hd)
  PutNum(g_contract, "eps", eps)
  PutBool(g_contract, "tie_word_embeddings", tie)
  PutRaw(g_contract, "kv_layout", kvt)
  PutStr(g_contract, "position_semantics", "absolute-0based")
  PutRaw(g_contract, "residual_order", resid)
  PutStr(g_contract, "attention_scale", "1/sqrt(head_dim)")
  IF LEN(maskv) > 0 THEN
    PutStr(g_contract, "mask", "sliding_window")
  ELSE
    g_contract("mask") = JSON.PARSE("null")
  END IF
  PutRaw(g_contract, "recurrent_state", rec)
END SUB

FUNCTION Invariants(spec AS JSON) AS STRING
  DIM h AS STRING, kv AS STRING, ne AS STRING, ept AS STRING
  DIM rope AS STRING, eps AS STRING, nl AS STRING, out AS STRING
  out = ""
  h = JSON.EXTRACT(spec, "num_attention_heads")
  kv = JSON.EXTRACT(spec, "num_key_value_heads")
  IF LEN(h) > 0 AND LEN(kv) > 0 THEN
    IF VAL(kv) <> 0 THEN
      IF INT(VAL(h)) MOD INT(VAL(kv)) <> 0 THEN out = out + "num_attention_heads non multiplo di num_key_value_heads" + CHR$(10)
    END IF
  END IF
  ne = JSON.EXTRACT(spec, "num_experts")
  ept = JSON.EXTRACT(spec, "experts_per_token")
  IF LEN(ne) > 0 AND LEN(ept) > 0 THEN
    IF VAL(ept) > VAL(ne) THEN out = out + "experts_per_token > num_experts" + CHR$(10)
  END IF
  rope = JSON.EXTRACT(spec, "rope_theta")
  IF LEN(rope) > 0 AND VAL(rope) <= 0 THEN out = out + "rope_theta non positivo" + CHR$(10)
  eps = JSON.EXTRACT(spec, "rms_norm_eps")
  IF LEN(eps) > 0 AND VAL(eps) <= 0 THEN out = out + "rms_norm_eps non positivo" + CHR$(10)
  nl = JSON.EXTRACT(spec, "num_hidden_layers")
  IF LEN(nl) > 0 AND VAL(nl) <= 0 THEN out = out + "num_hidden_layers <= 0" + CHR$(10)
  RETURN out
END FUNCTION

FUNCTION Golden(reg AS JSON) AS STRING
  DIM spec AS JSON, con AS JSON, s AS STRING, rope AS STRING
  spec = reg.spec : con = reg.contract
  s = "linear" + CHR$(10) + "rms_norm_weight" + CHR$(10) + "swiglu" + CHR$(10)
  IF LEN(JSON.EXTRACT(con, "attention")) > 0 THEN s = s + "attention_rows" + CHR$(10)
  IF LEN(JSON.EXTRACT(spec, "head_dim")) > 0 THEN s = s + "head_rms (qk-norm)" + CHR$(10)
  rope = JSON.EXTRACT(con, "rope_convention")
  IF LEFT$(rope, 4) = "neox" THEN s = s + "rope_tail_ext (neox)" + CHR$(10)
  IF JSON.EXTRACT(con, "mask") = "sliding_window" THEN s = s + "swa_indices" + CHR$(10)
  IF JSON.EXTRACT(con, "kv_layout") = "mla" THEN s = s + "attention_mixed (mla)" + CHR$(10)
  IF LEN(JSON.EXTRACT(spec, "num_experts")) > 0 THEN s = s + "topk_desc (router, MoE)" + CHR$(10)
  RETURN s
END FUNCTION

SUB Chk(cond AS INTEGER, msg AS STRING)
  IF cond = 0 THEN
    PRINT "selftest FAIL: "; msg
    g_err = -1
  END IF
END SUB

FUNCTION Selftest() AS INTEGER
  DIM q AS STRING, c1 AS STRING, c2 AS STRING, e AS JSON, gs AS STRING
  q = CHR$(34)
  c1 = "{" + q + "model_type" + q + ":" + q + "llama" + q + "," + q + "vocab_size" + q + ":100," + q + "hidden_size" + q + ":64," + q + "num_attention_heads" + q + ":8," + q + "num_key_value_heads" + q + ":2," + q + "num_hidden_layers" + q + ":2," + q + "head_dim" + q + ":8," + q + "intermediate_size" + q + ":128," + q + "rms_norm_eps" + q + ":1e-5," + q + "rope_theta" + q + ":10000.0," + q + "tie_word_embeddings" + q + ":true}"
  Extract(JSON.PARSE(c1))
  Chk(VAL(JSON.EXTRACT(g_spec, "hidden_size")) = 64, "hidden_size 64")
  Chk(JSON.EXTRACT(g_contract, "attention") = "GQA", "attention GQA")
  c2 = "{" + q + "model_type" + q + ":" + q + "llama" + q + "," + q + "num_attention_heads" + q + ":8," + q + "num_key_value_heads" + q + ":1}"
  Extract(JSON.PARSE(c2))
  Chk(JSON.EXTRACT(g_contract, "attention") = "MQA", "attention MQA")
  c2 = "{" + q + "model_type" + q + ":" + q + "gemma4" + q + "," + q + "text_config" + q + ":{" + q + "model_type" + q + ":" + q + "llama" + q + "," + q + "hidden_size" + q + ":99}" + "}"
  Extract(JSON.PARSE(c2))
  Chk(VAL(JSON.EXTRACT(g_spec, "hidden_size")) = 99, "nested text_config")
  e = JSON.PARSE("{" + q + "num_attention_heads" + q + ":8," + q + "num_key_value_heads" + q + ":2}")
  Chk(LEN(Invariants(e)) = 0, "invariants ok")
  e = JSON.PARSE("{" + q + "num_attention_heads" + q + ":8," + q + "num_key_value_heads" + q + ":3}")
  Chk(LEN(Invariants(e)) > 0, "invariants modulo")
  e = JSON.PARSE("{" + q + "num_experts" + q + ":8," + q + "experts_per_token" + q + ":16}")
  Chk(LEN(Invariants(e)) > 0, "invariants ept")
  e = JSON.PARSE("{" + q + "rope_theta" + q + ":-1}")
  Chk(LEN(Invariants(e)) > 0, "invariants rope")
  IF g_err <> 0 THEN RETURN 1
  PRINT "arch_registry selftest: OK"
  RETURN 0
END FUNCTION

FUNCTION Build(model_dir AS STRING, out_dir AS STRING) AS INTEGER
  DIM cfg_path AS STRING, cfg AS JSON, name AS STRING
  cfg_path = model_dir + "/config.json"
  IF NOT FILEEXISTS(cfg_path) THEN
    PRINT "ERRORE: config.json non trovato in "; model_dir
    RETURN 2
  END IF
  cfg = JSON.LOAD(cfg_path)
  name = BaseName(model_dir)
  Extract(cfg)
  g_reg = JSON.PARSE("{}")
  g_reg("model") = name
  ' Path RELATIVO alla cartella del modello (il config.json sta lì): il descrittore è
  ' versionato e non deve contenere il path assoluto della macchina che l'ha generato.
  g_reg("config") = "config.json"
  g_reg("spec") = g_spec
  g_reg("contract") = g_contract
  IF LEN(out_dir) > 0 THEN
    IF NOT DIREXISTS(out_dir) THEN MKDIR out_dir
    JSON SAVE g_reg, out_dir + "/" + name + ".json"
  END IF
  RETURN 0
END FUNCTION

FUNCTION DoValidate(model_dir AS STRING, reg_path AS STRING) AS INTEGER
  DIM cur AS JSON, reg AS JSON
  DIM keys$() AS STRING
  DIM i AS LONG, a AS STRING, b AS STRING, drift AS LONG
  DIM il$() AS STRING
  IF NOT FILEEXISTS(model_dir + "/config.json") OR NOT FILEEXISTS(reg_path) THEN
    PRINT "ERRORE: model_dir o registry non trovato"
    RETURN 2
  END IF
  Extract(JSON.LOAD(model_dir + "/config.json"))
  reg = JSON.LOAD(reg_path)
  keys$ = SPLIT("model_type,hidden_size,num_hidden_layers,num_attention_heads,num_key_value_heads,head_dim,intermediate_size,num_experts,experts_per_token,rope_theta,rms_norm_eps,tie_word_embeddings,sliding_window,vocab_size", ",")
  drift = 0
  i = 0
  DO WHILE i <= UBOUND(keys$)
    a = JSON.EXTRACT(reg.spec, keys$(i))
    b = JSON.EXTRACT(g_spec, keys$(i))
    IF a <> b THEN
      drift = drift + 1
      PRINT "  "; keys$(i); ": registry="; a; " config="; b
    END IF
    i = i + 1
  LOOP
  a = Invariants(g_spec)
  IF LEN(a) > 0 THEN
    il$ = SPLIT(a, CHR$(10))
    i = 0
    DO WHILE i <= UBOUND(il$)
      IF LEN(il$(i)) > 0 THEN
        drift = drift + 1
        PRINT "  invariant: registry=- config="; il$(i)
      END IF
      i = i + 1
    LOOP
  END IF
  IF drift = 0 THEN
    PRINT "[arch_registry] OK — config coerente col registry"
    RETURN 0
  END IF
  PRINT "[arch_registry] DRIFT ("; drift; ")"
  RETURN 1
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM cmd AS STRING, model AS STRING, outd AS STRING, regp AS STRING
  DIM i AS LONG, reg AS JSON
  DIM kids$() AS STRING
  DIM gs$() AS STRING
  IF COMMANDCOUNT() = 0 THEN cmd = "" ELSE cmd = COMMAND$(1)
  model = "" : outd = "" : regp = ""
  i = 2
  DO WHILE i <= COMMANDCOUNT()
    IF COMMAND$(i) = "--out" AND i < COMMANDCOUNT() THEN
      i = i + 1 : outd = COMMAND$(i)
    ELSEIF LEN(model) = 0 THEN
      model = COMMAND$(i)
    ELSEIF LEN(regp) = 0 THEN
      regp = COMMAND$(i)
    END IF
    i = i + 1
  LOOP
  IF LEN(cmd) = 0 THEN cmd = "selftest"
  IF cmd = "selftest" THEN RETURN Selftest()
  IF cmd = "build" THEN
    IF LEN(model) = 0 THEN
      PRINT "ERRORE: build <model_dir>"
      RETURN 2
    END IF
    IF Build(model, outd) <> 0 THEN RETURN 2
    PRINT JSON.STRINGIFY(g_reg)
    RETURN 0
  END IF
  IF cmd = "build-all" THEN
    IF LEN(model) = 0 THEN
      PRINT "ERRORE: build-all <models_root>"
      RETURN 2
    END IF
    kids$ = IO.DIR(model + "/*")
    i = 0
    DO WHILE i <= UBOUND(kids$)
      IF FILEEXISTS(model + "/" + kids$(i) + "/config.json") THEN
        IF Build(model + "/" + kids$(i), outd) <> 0 THEN RETURN 2
      END IF
      i = i + 1
    LOOP
    PRINT "[arch_registry] scritti in "; outd
    RETURN 0
  END IF
  IF cmd = "validate" THEN
    IF LEN(model) = 0 OR LEN(regp) = 0 THEN
      PRINT "ERRORE: validate <model_dir> <registry>"
      RETURN 2
    END IF
    RETURN DoValidate(model, regp)
  END IF
  IF cmd = "checks" THEN
    IF LEN(model) = 0 THEN
      PRINT "ERRORE: checks <registry>"
      RETURN 2
    END IF
    reg = JSON.LOAD(model)
    PRINT "[arch_registry] golden checks per "; JSON.EXTRACT(reg, "model"); ":"
    gs$ = SPLIT(Golden(reg), CHR$(10))
    i = 0
    DO WHILE i <= UBOUND(gs$)
      IF LEN(gs$(i)) > 0 THEN PRINT "  - "; gs$(i)
      i = i + 1
    LOOP
    RETURN 0
  END IF
  PRINT "comando sconosciuto: "; cmd
  RETURN 2
END FUNCTION
