' FILE: wsp/agent_replay.bas
' WSP (FFI) — sessione agente: verbi su dati, budget, rifiuto dei verbi non ammessi, trascrizione JSONL e replay deterministico.
' EXPECT: spec=-1  0 |pk=-1 |scale= 0 |budget= 1 -1 |shell= 1 -1 |fwd= 1 |edits= 2 |replay= 0 |other= 3 |missing=-1
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM d AS STRING = GMBWORK$ + "/wsp-agent"
  DIM d2 AS STRING = GMBWORK$ + "/wsp-agent2"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(d): r = WSP.REMOVEOUTPUT(d2)
  MKDIR d
  MKDIR d2
  r = WSP.FIXTURE("induction", d, 3)
  r = WSP.FIXTURE("induction", d2, 4)
  DIM m AS LONG
  m = WSP.LOADMODEL(d)
  DIM tr AS STRING = d + "/transcript.jsonl"
  r = WSP.AGENTNEW(m, 2, 0)
  r = WSP.AGENTTRANSCRIPT(m, tr)
  DIM o AS STRING
  o = WSP.AGENTEXEC(m, "SPECTRUM 1 0 ov")
  PRINT "spec="; (INSTR(o, "copy_sum_ratio=1") > 0); WSP.AGENTRC(m);
  o = WSP.AGENTEXEC(m, "COMPOSE 0 0 1 0 k")
  PRINT "|pk="; (INSTR(o, "PK=4") > 0);
  o = WSP.AGENTEXEC(m, "SCALE layers.0.attn.o.heads.1 2.0")
  PRINT "|scale="; WSP.AGENTRC(m);
  o = WSP.AGENTEXEC(m, "ZERO layers.0.attn.o.heads.3")
  o = WSP.AGENTEXEC(m, "ZERO layers.0.attn.o.heads.2")
  PRINT "|budget="; WSP.AGENTRC(m); (INSTR(o, "budget di edit esaurito") > 0);
  o = WSP.AGENTEXEC(m, "SHELL ls /")
  PRINT "|shell="; WSP.AGENTRC(m); (INSTR(o, "non ammesso") > 0);
  o = WSP.AGENTEXEC(m, "FORWARD 1 2")
  PRINT "|fwd="; WSP.AGENTRC(m);
  PRINT "|edits="; WSP.AGENTEDITS(m);
  r = WSP.AGENTSTOP(m)
  r = WSP.AGENTRUN(m, d + "/run.json")
  REM replay su una sessione NUOVA dello stesso modello: tutti gli output coincidono
  r = WSP.AGENTNEW(m, 2, 0)
  PRINT "|replay="; WSP.AGENTREPLAY(m, tr);
  r = WSP.FREEMODEL(m)
  REM replay su un modello DIVERSO (altro seed): divergenza al comando 1
  m = WSP.LOADMODEL(d2)
  r = WSP.AGENTNEW(m, 2, 0)
  PRINT "|other="; WSP.AGENTREPLAY(m, tr);
  PRINT "|missing="; WSP.AGENTREPLAY(m, d + "/inesistente.jsonl")
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(d): r = WSP.REMOVEOUTPUT(d2)
END SUB
