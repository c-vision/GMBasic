' EXPECT: agent_ctl_ok
' Comandi AI AGENT di controllo/motore:
'   AI AGENT TRACE VERBOSE / ON / OFF   (imposta il livello di tracing; ritorne un valore)
'   AI AGENT SERVE <porta>              (avvia il server HTTP degli agent: /api/agents, /api/workflows, ...)
'   AI AGENT STOP SERVER                (ferma il server)
' Richiede IMPORT AI. Parità 5/5 (IL, P-Code, VM/C, C99Native).
IMPORT AI
SUB Main()
  AI AGENT TRACE VERBOSE
  AI AGENT TRACE ON
  AI AGENT TRACE OFF
  AI AGENT SERVE 8793
  AI AGENT STOP SERVER
  PRINT "agent_ctl_ok"
END SUB
