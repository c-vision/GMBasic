' MiniTest: VECTORDB.ListCollections su db dedicato (in bin/, mai scritto da altri test).
' Il db sta sotto bin/ (output, gitignored) per non sporcare la dir del test.
' NOTA: sul PCode il path STATEMENT emette il syscall legacy e da' "" invece di [] —
' divergenza tracciata in "(open) - VectorDB-ListCollections-PCode-Legacy-Syscall";
' il file e' escluso dal confronto cross-leg ma verificato su IL/VM-C/C99Native.
' EXPECT: lc=[]
IMPORT VECTORDB
IMPORT IO
SUB Main()
  MKDIR "bin"
  PRINT "lc="; VECTORDB.ListCollections("bin/lc-oracolo")
END SUB
