' AI.METRICRECORD / METRICGET / METRICLIST / FLEETSTATS / METRICRESET
' Orchestrazione agenti (B6): metriche fleet-level — token/costo/latenza per agente, con
' aggregazione di flotta (totali per chiave). Runtime condiviso: MetricsRegistry / gmb_metrics.c.
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.METRICRECORD("researcher", "tokens", 1200)
  ok = AI.METRICRECORD("researcher", "latency_ms", 340)
  ok = AI.METRICRECORD("coder", "tokens", 800)
  ok = AI.METRICRECORD("coder", "tokens", 450)
  PRINT "token researcher: "; AI.METRICGET("researcher", "tokens")
  PRINT "per-agente: "; AI.METRICLIST()
  PRINT "fleet: "; AI.FLEETSTATS()
  ok = AI.METRICRESET()
END SUB
