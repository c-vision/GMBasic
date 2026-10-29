' gen_gmbarch_abi.bas — genera <arch>.gmbarch da config.json + nomi tensori
' (da gen_gmbarch_abi.py). Uso: gen_gmbarch_abi.bas [--models DIR]... [--out DIR] [--dry]
' Scrive in ../LLMEnv/ABI (default) SOLO i descrittori mancanti: quelli esistenti sono curati a mano.
IMPORT IO
IMPORT JSON
IMPORT GREP

DIM g_rule(64) AS STRING
DIM g_role(64) AS STRING
DIM g_nr AS LONG
DIM g_pat(2000) AS STRING
DIM g_np AS LONG

SUB InitRules()
  g_rule(0)="vision_tower" : g_role(0)="VISION"
  g_rule(1)="vision_model" : g_role(1)="VISION"
  g_rule(2)="embed_vision" : g_role(2)="VISION"
  g_rule(3)="aligner" : g_role(3)="VISION_ALIGNER"
  g_rule(4)="embed_tokens" : g_role(4)="EMBEDDING"
  g_rule(5)="tok_embeddings" : g_role(5)="EMBEDDING"
  g_rule(6)="wte." : g_role(6)="EMBEDDING"
  g_rule(7)=".embed." : g_role(7)="EMBEDDING"
  g_rule(8)="word_embeddings" : g_role(8)="EMBEDDING"
  g_rule(9)="lm_head" : g_role(9)="OUTPUT"
  g_rule(10)="output.weight" : g_role(10)="OUTPUT"
  g_rule(11)=".head." : g_role(11)="OUTPUT"
  g_rule(12)="head.weight" : g_role(12)="OUTPUT"
  g_rule(13)="attn_sink" : g_role(13)="ATTN_SINK"
  g_rule(14)="self_attn.sinks" : g_role(14)="ATTN_SINK"
  g_rule(15)="switch_mlp" : g_role(15)="ROUTED_EXPERT"
  g_rule(16)="mlp.experts" : g_role(16)="ROUTED_EXPERT"
  g_rule(17)="experts." : g_role(17)="ROUTED_EXPERT"
  g_rule(18)="shared_experts" : g_role(18)="SHARED_EXPERT"
  g_rule(19)="shared_expert." : g_role(19)="SHARED_EXPERT"
  g_rule(20)="shared_expert_gate" : g_role(20)="ROUTER"
  g_rule(21)="mlp.gate.weight" : g_role(21)="ROUTER"
  g_rule(22)="mlp.gate.e_score" : g_role(22)="ROUTER"
  g_rule(23)="e_score_correction_bias" : g_role(23)="ROUTER"
  g_rule(24)="mlp.router" : g_role(24)="ROUTER"
  g_rule(25)=".router." : g_role(25)="ROUTER"
  g_rule(26)="q_a_proj" : g_role(26)="ATTN_Q_A"
  g_rule(27)="q_b_proj" : g_role(27)="ATTN_Q_B"
  g_rule(28)="q_lora" : g_role(28)="ATTN_Q_A"
  g_rule(29)="kv_a_proj" : g_role(29)="ATTN_KV"
  g_rule(30)="kv_b_proj" : g_role(30)="ATTN_KV"
  g_rule(31)="kv_a_layernorm" : g_role(31)="NORM"
  g_rule(32)="q_norm" : g_role(32)="NORM"
  g_rule(33)="k_norm" : g_role(33)="NORM"
  g_rule(34)="q_proj" : g_role(34)="ATTN_Q"
  g_rule(35)="k_proj" : g_role(35)="ATTN_KV"
  g_rule(36)="v_proj" : g_role(36)="ATTN_V"
  g_rule(37)="o_proj" : g_role(37)="ATTN_WO"
  g_rule(38)="linear_attn" : g_role(38)="ATTN_LINEAR"
  g_rule(39)="gate_proj" : g_role(39)="FFN"
  g_rule(40)="up_proj" : g_role(40)="FFN"
  g_rule(41)="down_proj" : g_role(41)="FFN"
  g_rule(42)="mlp." : g_role(42)="FFN"
  g_rule(43)="feed_forward" : g_role(43)="FFN"
  g_rule(44)="ffn." : g_role(44)="FFN"
  g_rule(45)="norm" : g_role(45)="NORM"
  g_rule(46)="layernorm" : g_role(46)="NORM"
  g_rule(47)=".ln_" : g_role(47)="NORM"
  g_nr = 48
END SUB

FUNCTION Q(s AS STRING) AS STRING
  RETURN JSON.STRINGIFY(s)
END FUNCTION

FUNCTION Ends(s AS STRING, x AS STRING) AS INTEGER
  IF LEN(s) < LEN(x) THEN RETURN 0
  RETURN RIGHT$(s, LEN(x)) = x
END FUNCTION

FUNCTION NormName(full AS STRING) AS STRING
  DIM i AS LONG, out AS STRING, c AS STRING
  i = 1
  DO WHILE i <= LEN(full)
    c = MID$(full, i, 1)
    IF c = "." AND i < LEN(full) AND MID$(full, i + 1, 1) >= "0" AND MID$(full, i + 1, 1) <= "9" THEN
      out = out + ".N."
      i = i + 1
      DO WHILE i <= LEN(full) AND MID$(full, i, 1) >= "0" AND MID$(full, i, 1) <= "9" : i = i + 1 : LOOP
    ELSE
      out = out + c
      i = i + 1
    END IF
  LOOP
  IF RIGHT$(out, 3) = ".N." THEN out = LEFT$(out, LEN(out) - 1)
  RETURN out
END FUNCTION

SUB AddPattern(p AS STRING)
  DIM i AS LONG, found AS INTEGER
  IF Ends(p, ".scales") OR Ends(p, ".biases") OR Ends(p, ".scale") OR Ends(p, ".bias") THEN EXIT SUB
  found = 0
  FOR i = 0 TO g_np - 1
    IF g_pat(i) = p THEN found = 1 : EXIT FOR
  NEXT i
  IF found = 0 THEN g_pat(g_np) = p : g_np = g_np + 1
END SUB

FUNCTION Tensors(dir AS STRING) AS STRING
  DIM p AS STRING, idx AS JSON, wm AS JSON, a$() AS STRING, k AS STRING
  DIM b() AS BYTE, i AS LONG, n AS LONG, hdr AS STRING, h AS JSON
  g_np = 0
  p = dir + "/model.safetensors.index.json"
  IF FILEEXISTS(p) THEN
    idx = JSON.PARSE(IO.READFILE(p))
    wm = idx.weight_map
    FOR EACH k IN JSON.KEYS(wm) : AddPattern(NormName(k)) : NEXT
    RETURN ""
  END IF
  p = dir + "/model.safetensors"
  IF NOT FILEEXISTS(p) THEN RETURN ""
  b() = IO.READBYTES(p)
  n = 0
  FOR i = 0 TO 7 : n = n + b(i) * 2 ^ (8 * i) : NEXT i
  hdr = ""
  FOR i = 0 TO n - 1 : hdr = hdr + CHR$(b(8 + i)) : NEXT i
  h = JSON.PARSE(hdr)
  FOR EACH k IN JSON.KEYS(h)
    IF k <> "__metadata__" THEN AddPattern(NormName(k))
  NEXT
  RETURN ""
END FUNCTION

FUNCTION HasField(o AS JSON, k AS STRING) AS INTEGER
  DIM s AS STRING
  s = JSON.EXTRACT(o, k)
  IF s = "null" THEN RETURN 0
  RETURN 1
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM out AS STRING, dry AS INTEGER, rdir AS STRING
  DIM roots$() AS STRING, names$() AS STRING
  DIM i AS LONG, j AS LONG, nr AS LONG, n AS LONG, h AS LONG
  DIM cfg AS JSON, scfg AS JSON, mt AS STRING, cfgp AS STRING
  DIM pset AS STRING, roles AS STRING, cmap AS STRING, st AS STRING
  DIM doc AS STRING, p AS STRING, nm AS STRING, role AS STRING
  DIM mla AS INTEGER, moe AS INTEGER, vis AS INTEGER, qn AS INTEGER, kn AS INTEGER
  InitRules()
  out = "../LLMEnv/ABI" : dry = 0   ' LLMEnv e' l'unico proprietario dei .gmbarch; i file gia' presenti NON si sovrascrivono
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    IF COMMAND$(i) = "--out" AND i < COMMANDCOUNT() THEN i = i + 1 : out = COMMAND$(i)
    IF COMMAND$(i) = "--dry" THEN dry = 1
    IF COMMAND$(i) = "--models" AND i < COMMANDCOUNT() THEN
      i = i + 1
      REDIM roots$(UBOUND(roots$) + 1)
      roots$(UBOUND(roots$)) = COMMAND$(i)
    END IF
    i = i + 1
  LOOP
  IF UBOUND(roots$) < 0 THEN
    REDIM roots$(3)
    roots$(0) = "/Users/gaetano/ai/Models"
    roots$(1) = "/Users/gaetano/ai/models"
    roots$(2) = "/Volumes/Armor5TB/LocalLLM/ai/models"
    roots$(3) = "/Users/gaetano/.cache/huggingface/hub"
  END IF
  IF NOT DIREXISTS(out) THEN MKDIR out
  FOR i = 0 TO UBOUND(roots$)
    rdir = roots$(i)
    IF DIREXISTS(rdir) THEN
      names$() = IO.DIR(rdir + "/*")
      FOR j = 0 TO UBOUND(names$)
        p = rdir + "/" + names$(j)
        IF DIREXISTS(p) THEN
          cfgp = p + "/config.json"
          IF FILEEXISTS(cfgp) THEN
            cfg = JSON.PARSE(IO.READFILE(cfgp))
            scfg = cfg
            IF HasField(cfg, "text_config") <> 0 THEN scfg = JSON.PARSE(JSON.STRINGIFY(cfg.text_config))
            IF HasField(cfg, "language_model") <> 0 THEN scfg = JSON.PARSE(JSON.STRINGIFY(cfg.language_model))
            mt = JSON.EXTRACT(scfg, "model_type")
            IF LEN(mt) = 0 THEN mt = JSON.EXTRACT(cfg, "model_type")
            IF LEN(mt) = 0 THEN mt = JSON.EXTRACT(cfg, "architectures[0]")
            IF LEN(mt) > 0 THEN
              nm = LOWER$(mt)
              IF nm <> "silero_vad" AND nm <> "flux" AND nm <> "vae" AND nm <> "clip" AND nm <> "t5" AND nm <> "whisper" THEN
                Tensors(p)
                pset = "": qn = 0 : kn = 0 : moe = 0 : vis = 0
                FOR n = 0 TO g_np - 1 : pset = pset + g_pat(n) + CHR$(10) : NEXT n
                qn = (INSTR(pset, "q_norm") > 0) : kn = (INSTR(pset, "k_norm") > 0)
                moe = (INSTR(pset, "mlp.gate.weight") > 0) OR (INSTR(pset, "switch_mlp") > 0) OR (INSTR(pset, "mlp.experts") > 0) OR (INSTR(pset, "mlp.router") > 0)
                vis = (INSTR(pset, "vision_tower") > 0) OR (INSTR(pset, "embed_vision") > 0)
                mla = HasField(scfg, "q_lora_rank") OR HasField(scfg, "kv_lora_rank") OR INSTR(pset, "q_a_proj") > 0 OR INSTR(pset, "kv_a_proj") > 0
                roles = ""
                FOR n = 0 TO g_np - 1
                  FOR nr = 0 TO g_nr - 1
                    IF INSTR(g_pat(n), g_rule(nr)) > 0 THEN
                      IF INSTR(roles, Q(g_rule(nr))) = 0 THEN
                        IF LEN(roles) > 0 THEN roles = roles + ","
                        roles = roles + "{" + Q("match") + ":" + Q(g_rule(nr)) + "," + Q("role") + ":" + Q(g_role(nr)) + "}"
                      END IF
                      EXIT FOR
                    END IF
                  NEXT nr
                NEXT n
                cmap = ""
                IF HasField(scfg, "num_hidden_layers") THEN cmap = cmap + Q("n_layers") + ":" + Q("num_hidden_layers") + ","
                IF HasField(scfg, "hidden_size") THEN cmap = cmap + Q("hidden") + ":" + Q("hidden_size") + ","
                IF HasField(scfg, "num_attention_heads") THEN cmap = cmap + Q("n_heads") + ":" + Q("num_attention_heads") + ","
                IF HasField(scfg, "num_key_value_heads") THEN cmap = cmap + Q("n_kv") + ":" + Q("num_key_value_heads") + ","
                IF HasField(scfg, "head_dim") THEN cmap = cmap + Q("head_dim") + ":" + Q("head_dim") + ","
                IF HasField(scfg, "vocab_size") THEN cmap = cmap + Q("vocab") + ":" + Q("vocab_size") + ","
                IF HasField(scfg, "rope_theta") THEN cmap = cmap + Q("rope_theta") + ":" + Q("rope_theta") + ","
                IF HasField(scfg, "intermediate_size") THEN cmap = cmap + Q("dense_inter") + ":" + Q("intermediate_size") + ","
                IF HasField(scfg, "num_experts") THEN cmap = cmap + Q("experts") + ":" + Q("num_experts") + ","
                IF HasField(scfg, "num_experts_per_tok") THEN cmap = cmap + Q("top_k") + ":" + Q("num_experts_per_tok") + ","
                IF HasField(scfg, "moe_intermediate_size") THEN cmap = cmap + Q("moe_inter") + ":" + Q("moe_intermediate_size") + ","
                IF RIGHT$(cmap, 1) = "," THEN cmap = LEFT$(cmap, LEN(cmap) - 1)
                st = Q("norm") + ":{" + Q("type") + ":" + Q("rmsnorm") + "," + Q("prim") + ":" + Q("rmsnorm") + "},"
                IF mla THEN
                  st = st + Q("attention") + ":{" + Q("type") + ":" + Q("mla") + "," + Q("steps") + ":[{op:q_lora,prim:},{op:kv_lora,prim:},{op:rope,prim:},{op:sdpa,prim:},{op:sink,prim:}]},"
                ELSE
                  st = st + Q("attention") + ":{" + Q("type") + ":" + Q("gqa") + "," + Q("steps") + ":["
                  IF qn THEN st = st + "{op:q_norm,prim:rmsnorm},"
                  IF kn THEN st = st + "{op:k_norm,prim:rmsnorm},"
                  st = st + "{op:rope,prim:},{op:sdpa,prim:}]},"
                END IF
                IF moe THEN st = st + Q("moe") + ":{" + Q("experts") + ":" + Q("cfg:experts") + "," + Q("top_k") + ":" + Q("cfg:top_k") + "," + Q("steps") + ":[{op:router_topk,prim:},{op:experts,prim:},{op:shared,prim:}]},"
                IF vis THEN st = st + Q("vision") + ":{" + Q("type") + ":" + Q("vit") + "," + Q("present") + ":true},"
                IF RIGHT$(st, 1) = "," THEN st = LEFT$(st, LEN(st) - 1)
                doc = "{" + Q("arch") + ":" + Q(mt) + "," + Q("source") + ":{" + Q("format") + ":" + Q("safetensors") + "," + Q("config") + ":" + Q("config.json") + "," + Q("index") + ":" + Q("model.safetensors.index.json") + "},"
                IF LEN(cmap) > 0 THEN doc = doc + Q("config_map") + ":{" + cmap + "},"
                IF LEN(roles) > 0 THEN doc = doc + Q("roles") + ":[" + roles + "],"
                doc = doc + Q("quant") + ":{" + Q("NORM") + ":" + Q("f32") + "," + Q("EMBEDDING") + ":" + Q("f16") + "," + Q("OUTPUT") + ":" + Q("q6_k") + "," + Q("recipe") + ":" + Q("_M") + "," + Q("recipe_boost") + ":[" + Q("attn_v") + "," + Q("ffn_down") + "]," + Q("recipe_boost_fmt") + ":" + Q("q6_k") + "},"
                doc = doc + Q("structures") + ":{" + st + "}}"
                IF dry = 0 AND NOT FILEEXISTS(out + "/" + mt + ".gmbarch") THEN
                  h = IO.FCREATE(out + "/" + mt + ".gmbarch")
                  IO.FWRITE(h, doc)
                  IO.FCLOSE(h)
                END IF
                PRINT mt; " <- "; p
              END IF
            END IF
          END IF
        END IF
      NEXT j
    END IF
  NEXT i
  RETURN 0
END FUNCTION
