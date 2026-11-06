' Weight-Space Programming — una sessione a verbi che un agente (o un programma) può pilotare con budget, e il suo REPLAY.
' La sessione è una sandbox di DATI: nessun verbo apre rete, shell o file; quelli non ammessi sono rifiutati e registrati.
IMPORT WSP

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM r AS LONG
  DIM dir AS STRING = GMBWORK$ + "/wsp-sample-agent"
  IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$
  r = WSP.REMOVEOUTPUT(dir)
  MKDIR dir
  r = WSP.FIXTURE("induction", dir, 3)
  DIM m AS LONG
  m = WSP.LOADMODEL(dir)
  DIM tr AS STRING = dir + "/transcript.jsonl"

  r = WSP.AGENTNEW(m, 2, 0)                ' budget: 2 edit, 0 forward
  r = WSP.AGENTTRANSCRIPT(m, tr)
  DIM cmds(5) AS STRING
  cmds(0) = "SPECTRUM 1 0 ov"
  cmds(1) = "SCALE layers.0.attn.o.heads.1 0.5"
  cmds(2) = "ZERO layers.0.attn.o.heads.3"
  cmds(3) = "ZERO layers.0.attn.o.heads.2"         ' oltre il budget
  cmds(4) = "SHELL rm -rf /"                       ' verbo non ammesso
  cmds(5) = "SUMMARY"
  DIM i AS LONG
  DIM o AS STRING
  FOR i = 0 TO 5
    o = WSP.AGENTEXEC(m, cmds(i))
    PRINT "["; WSP.AGENTRC(m); "] "; cmds(i); "  ->  "; LEFT$(o, 70)
  NEXT i
  r = WSP.AGENTSTOP(m)
  r = WSP.AGENTRUN(m, dir + "/run.json")

  ' Replay su una sessione nuova: ogni output deve coincidere con quello registrato.
  r = WSP.AGENTNEW(m, 2, 0)
  PRINT "replay della trascrizione: "; WSP.AGENTREPLAY(m, tr); " (0 = identico)"
  r = WSP.FREEMODEL(m)
  r = WSP.REMOVEOUTPUT(dir)
END SUB
