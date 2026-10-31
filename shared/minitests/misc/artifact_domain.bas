' MiniTest: orchestrazione agenti (A6) — Artifact Store (riferimenti, senza duplicare contenuto).
' Deterministico: PUT/LINK/GET/LIST/REMOVE. Runtime condiviso Basic.Agents.ArtifactStore (C#) /
' gmb_artifact.c (C): stessa semantica e stesso formato JSON.
' EXPECT: put= 1|get=/tmp/report.md|list={"artifacts":[{"name":"repo","kind":"link","ref":"https://example.com/repo"},{"name":"report","kind":"file","ref":"/tmp/report.md"}]}|remove= 1|list2={"artifacts":[{"name":"report","kind":"file","ref":"/tmp/report.md"}]}
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.ARTIFACTPUT("report", "/tmp/report.md")
  ok = AI.ARTIFACTLINK("repo", "https://example.com/repo")
  PRINT "put="; ok
  PRINT "get="; AI.ARTIFACTGET("report")
  PRINT "list="; AI.ARTIFACTLIST()
  PRINT "remove="; AI.ARTIFACTREMOVE("repo")
  PRINT "list2="; AI.ARTIFACTLIST()
END SUB
