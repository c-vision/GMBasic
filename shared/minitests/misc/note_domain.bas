' FILE: misc/note_domain.bas
' Second Brain L2 — comandi VAULT/NOTE (C99Native + VM/C) e funzioni AI.NOTE*.
' EXPECT: get:hello world|title:n1|tags:alpha,beta|wc: 2|folder:/
IMPORT AI

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM v AS STRING = GMBWORK$ + "/note-vault"
  IF IO.DirExists(v) = 0 THEN MKDIR v
  AI VAULT CREATE "nv", v
  AI VAULT INDEX "nv"
  AI NOTE CREATE "n1.md", "hello world", "alpha,beta"

  PRINT "get:"; AI.NOTEGET("nv", "n1.md")
  PRINT "title:"; AI.NOTETITLE("nv", "n1.md")
  PRINT "tags:"; AI.NOTETAGS("nv", "n1.md")
  PRINT "wc:"; AI.NOTEWORDCOUNT("nv", "n1.md")
  PRINT "folder:"; AI.NOTEFOLDER("nv", "n1.md")
END SUB
