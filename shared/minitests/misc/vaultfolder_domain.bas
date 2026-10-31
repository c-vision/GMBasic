' FILE: misc/vaultfolder_domain.bas
' Second Brain L1 — folder di un vault: AI.VAULTADDFOLDER/REMOVEFOLDER/FOLDERS.
' EXPECT: add:-1|folders:[]|remove: 0|folders2:[]
IMPORT AI

SUB Main()
  DIM v AS STRING = "work/vaultfolder"
  IF IO.DirExists(v) = 0 THEN MKDIR v
  DIM d AS STRING = v + "/docs"
  IF IO.DirExists(d) = 0 THEN MKDIR d
  AI VAULT CREATE "vf", v
  PRINT "add:"; AI.VAULTADDFOLDER("vf", ".")
  PRINT "folders:"; AI.VAULTFOLDERS("vf")
  PRINT "remove:"; AI.VAULTREMOVEFOLDER("vf", ".")
  PRINT "folders2:"; AI.VAULTFOLDERS("vf")
END SUB
