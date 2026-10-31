' EXPECT: clen= 207|call={|"connector": "mt_call",|"tool": "ping",|"args": {},|"status": "ok",|"note": "native connector \u0027mt_call\u0027 stub \u2014 F1 pending (test), use CONNECTOR MARKETPLACE GET to load full impl"|}
IMPORT AI
SUB Main()
  AI CONNECTOR REGISTER "mt_call", "test"
  PRINT "clen="; LEN(AI.CONNECTORCALL("mt_call", "ping", "{}"))
  PRINT "call="; AI.CONNECTORCALL("mt_call", "ping", "{}")
  AI CONNECTOR UNREGISTER "mt_call"
END SUB
