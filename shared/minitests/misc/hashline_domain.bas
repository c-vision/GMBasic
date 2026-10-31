' MiniTest: orchestrazione (A10) — Hashline (edit ancorato al contenuto).
' Deterministico. Runtime condiviso Basic.Agents.HashlineTool (C#) / gmb_hashline.c (C).
' EXPECT: tag=1#E8D6594C| riga uno|2#A662F3D2| riga due|verify1= 1|edit=riga uno|riga due modificata|badedit=Error: hash mismatch riga 1 (atteso 0000, trovato E8D6594C)
IMPORT AI

SUB Main
  DIM s AS STRING
  s = "riga uno" + CHR(10) + "riga due"
  PRINT "tag="; AI.HASHLINE(s)
  PRINT "verify1="; AI.HASHVERIFY(s, "1")
  PRINT "edit="; AI.HASHEDIT(s, "2", "riga due modificata")
  PRINT "badedit="; AI.HASHEDIT(s, "1#0000", "x")
END SUB
