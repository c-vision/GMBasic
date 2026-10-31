' MiniTest: orchestrazione (A8) — Session Replay (registrazione eventi di sessione).
' Deterministico. Runtime condiviso Basic.Agents.ReplayRegistry (C#) / gmb_replay.c (C).
' EXPECT: record= 1|get={"session":"s1","recording":true,"events":["ask: ciao","answer: hey"]}|stop= 1|list={"sessions":[{"session":"s1","recording":false,"count":2}]}|clear= 1
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.REPLAYSTART("s1")
  ok = AI.REPLAYRECORD("ask: ciao")
  ok = AI.REPLAYRECORD("answer: hey")
  PRINT "record="; ok
  PRINT "get="; AI.REPLAYGET("s1")
  PRINT "stop="; AI.REPLAYSTOP()
  PRINT "list="; AI.REPLAYLIST()
  PRINT "clear="; AI.REPLAYCLEAR()
END SUB
