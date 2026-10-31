' MiniTest: orchestrazione (B6) — metriche fleet-level (aggregazione per-agente).
' Deterministico. Runtime condiviso Basic.Agents.MetricsRegistry (C#) / gmb_metrics.c (C).
' EXPECT: record= 1|get= 150|list={"metrics":{"a1":{"latency_ms":12,"tokens":150},"a2":{"tokens":30}}}|fleet={"agents":{"a1":{"latency_ms":12,"tokens":150},"a2":{"tokens":30}},"totals":{"latency_ms":12,"tokens":180},"count":2}|reset= 2
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.METRICRECORD("a1", "tokens", 100)
  ok = AI.METRICRECORD("a1", "tokens", 50)
  ok = AI.METRICRECORD("a2", "tokens", 30)
  ok = AI.METRICRECORD("a1", "latency_ms", 12)
  PRINT "record="; ok
  PRINT "get="; AI.METRICGET("a1", "tokens")
  PRINT "list="; AI.METRICLIST()
  PRINT "fleet="; AI.FLEETSTATS()
  PRINT "reset="; AI.METRICRESET()
END SUB
