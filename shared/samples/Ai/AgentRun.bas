' AGENT.RUN — loop Brain→Azione (CUA) su VM/C
' Il brain è il completer embedding GMBasic (registrato via gmb_agent_brain_register dal
' runtime/embedding GMBasic) oppure GMB_BRAIN_URL esplicito. Senza brain → errore esplicito.
IMPORT AGENT WITH ALIAS

SUB Main()
  DIM log$ AS STRING
  log$ = AGENT.RUN("Qual è il titolo di example.com? Usa il browser.", 6)
  PRINT log$
  PRINT "AGENT.RUN terminato."
END SUB
