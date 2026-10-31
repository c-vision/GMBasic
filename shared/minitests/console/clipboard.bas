' MiniTest: CLIP nudo (gruppo Console, parità 4 backend).
' CLIP GET era rotto su PCode (letto mai pushato), stub su IL (GET→""),
' no-op su assente su C99Native (2026-09-21: push PCode, Clip reale IL,
' runtime condiviso gmb_clipboard_* + emettitori C).
' Su IL/PCode il clipboard è in-memory (headless); su VM/C/C99/C99Native è quello
' di sistema (pbcopy/pbpaste su macOS): il round-trip richiede la pasteboard.
' EXPECT: got=GMB-CLIP-TOK|cleared=
IMPORT CLIP

SUB Main()
  CLIP SET "GMB-CLIP-TOK"
  DIM g AS STRING
  CLIP GET g
  PRINT "got="; g
  CLIP CLEAR
  DIM c AS STRING
  CLIP GET c
  PRINT "cleared="; c
END SUB
