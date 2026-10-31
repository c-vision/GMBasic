' AI.ARTIFACTPUT / ARTIFACTLINK / ARTIFACTGET / ARTIFACTLIST / ARTIFACTREMOVE
' Orchestrazione agenti (A6): Artifact Store — gli artefatti sono RIFERIMENTI (file/uri),
' mai copie del contenuto. Runtime condiviso: Basic.Agents.ArtifactStore (C#) / gmb_artifact.c.
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.ARTIFACTPUT("relazione", "/tmp/relazione.md")
  ok = AI.ARTIFACTLINK("repository", "https://example.com/repo")
  PRINT "riferimento relazione: "; AI.ARTIFACTGET("relazione")
  PRINT AI.ARTIFACTLIST()
  ok = AI.ARTIFACTREMOVE("repository")
  PRINT "dopo remove: "; AI.ARTIFACTLIST()
END SUB
