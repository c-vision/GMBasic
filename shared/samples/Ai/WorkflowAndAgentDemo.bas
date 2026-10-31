' WorkflowAndAgentDemo.bas — dimostra le feature AI portate in C99Native:
'   DSL AI WORKFLOW (begin/STEP/END) + AI.WorkflowSteps/List
'   ciclo agent CREATE/SET/STATUS/EVENTS (runtime C gmb_agent_*)
' Verificato su C99Native (gira senza rete).
IMPORT AI
SUB Main()
  ' --- 1. DSL WORKFLOW ---
  AI WORKFLOW Pipeline
  AI WORKFLOW STEP s1, "prepara"
  AI WORKFLOW STEP s2, "elabora"
  AI WORKFLOW STEP s3, "finalizza"
  END WORKFLOW
  DIM steps AS STRING
  steps = AI.WorkflowSteps("Pipeline")
  PRINT "WORKFLOW Pipeline steps: "; steps
  DIM wl AS STRING
  wl = AI.WorkflowList()
  PRINT "Workflow registrati: "; wl
  PRINT

  ' --- 2. Ciclo agent (CREATE/SET/STATUS/EVENTS) ---
  AI AGENT CREATE Assist
  AI AGENT SET Assist systemPrompt "Assistente demo"
  DIM st AS STRING
  st = AI.AGENTSTATUS("Assist")
  ' L'id agente e' non deterministico per definizione (GUID): si verifica il campo stabile.
  IF INSTR(st, "name=Assist") > 0 THEN PRINT "AGENT status: name=Assist ok" ELSE PRINT "AGENT status: FAIL"; st
  AI AGENT DELETE Assist
  PRINT "Agent rimosso."
  PRINT "Demo completato."
END SUB
