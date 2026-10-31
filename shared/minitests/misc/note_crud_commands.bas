' FILE: misc/note_crud_commands.bas
' MiniTest per i COMANDI statement del second brain L2 non coperti da note_domain.bas:
' AI NOTE UPDATE / APPEND / MOVE / DELETE (forma legacy a spazio, §2.0 AGENTS.md).
' Verifica sia l'effetto (NOTEGET/NOTEEXISTS) sia la PRESERVAZIONE del frontmatter
' (NOTETAGS) su UPDATE/APPEND, come fa il C# NoteManager.RebuildWithFrontmatter.
' Root del vault come LETTERALE (§2.0: un identificatore nudo diventa il suo NOME, non il
' valore) e directory creata prima (VaultCreate torna -1 se manca).
' EXPECT: get:hello world|upd:contenuto aggiornato|app:contenuto aggiornato riga in coda|tags2:alpha,beta|moved: 1|del: 0
IMPORT AI

SUB Main()
  IF IO.DirExists("bin/note-crud-vault") = 0 THEN MKDIR "bin/note-crud-vault"
  AI VAULT CREATE "nv", "bin/note-crud-vault"
  AI VAULT INDEX "nv"
  AI NOTE CREATE "n1.md", "hello world", "alpha,beta"

  PRINT "get:"; AI.NOTEGET("nv", "n1.md")

  AI NOTE UPDATE "n1.md", "contenuto aggiornato"
  PRINT "upd:"; AI.NOTEGET("nv", "n1.md")

  AI NOTE APPEND "n1.md", " riga in coda"
  PRINT "app:"; AI.NOTEGET("nv", "n1.md")
  PRINT "tags2:"; AI.NOTETAGS("nv", "n1.md")

  AI NOTE MOVE "n1.md", "n2.md"
  PRINT "moved:"; AI.NOTEEXISTS("nv", "n2.md")

  AI NOTE DELETE "n2.md"
  PRINT "del:"; AI.NOTEEXISTS("nv", "n2.md")
END SUB
