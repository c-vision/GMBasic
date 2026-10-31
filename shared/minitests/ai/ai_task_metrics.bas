' EXPECT: {"tokens":100,"ttft":42,"tool_calls":3,"llm_calls":5,"latency":250}
' MiniTest: §3.6 PARSEC — AI.TASKMETRICS$(task) (metriche task standard, JSON).
IMPORT AI
SUB Main()
  AI.METRICRECORD("t", "tokens", 100)
  AI.METRICRECORD("t", "ttft", 42)
  AI.METRICRECORD("t", "tool_calls", 3)
  AI.METRICRECORD("t", "llm_calls", 5)
  AI.METRICRECORD("t", "latency", 250)
  PRINT AI.TASKMETRICS$("t")
END SUB
