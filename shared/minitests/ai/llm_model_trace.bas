' MiniTest: LLM MODEL TRACE — trace esperti `.moetrace` (K1): avvio/stop del comando.
' Il replay offline (moe_sim --verify) e' verificato in LLMEnv (test_moe_gguf_adapter);
' qui si verifica il plumbing del comando su tutti i backend.
' EXPECT: load_ok|trace_on_ok|trace_off_ok|done
IMPORT LLM
SUB Main()
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  ON ERROR GOTO E1
  LLM MODEL TRACE "m", "work/llm_model_trace.moetrace"
  PRINT "trace_on_ok"
  GOTO N1
E1:
  PRINT "trace_on_err"
N1:
  ON ERROR GOTO E2
  LLM MODEL TRACE "m", OFF
  PRINT "trace_off_ok"
  GOTO N2
E2:
  PRINT "trace_off_err"
N2:
  LLM MODEL UNLOAD "m"
  PRINT "done"
END SUB
