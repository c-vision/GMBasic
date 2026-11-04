' MiniTest: VECTORDB.ListCollections su db dedicato (in work/, mai scritto da altri test).
' Il db sta sotto work/ (output, gitignored) per non sporcare la dir del test.
' NOTA: sul PCode il path STATEMENT emette il syscall legacy e da' "" invece di [] —
' divergenza tracciata in "(open) - VectorDB-ListCollections-PCode-Legacy-Syscall";
' il file e' escluso dal confronto cross-leg ma verificato su IL/VM-C/C99Native.
' EXPECT: lc=[]
IMPORT VECTORDB
IMPORT IO
SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  PRINT "lc="; VECTORDB.ListCollections("work/lc-oracolo")
END SUB
