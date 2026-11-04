' MiniTest: AI.VideoFrame — estrae un frame da un video Y4M nativo e lo scrive come PNG.
' Fixture deterministica (samples/Fixtures/video_test.y4m: 2x2 YUV420, 2 frame, 25fps).
' frame 0 → 1 (scritto in work/); indice fuori range → 0 (errore esplicito, mai crash).
' EXPECT: frame0= 1|oob= 0
IMPORT AI WITH ALIAS
IMPORT IO
SUB Main()
  ON ERROR RESUME NEXT
  KILL "work/vframe.png"
  MKDIR "work"
  ON ERROR GOTO 0
  PRINT "frame0="; AI.VideoFrame("../../samples/Fixtures/video_test.y4m", 0, "work/vframe.png")
  PRINT "oob="; AI.VideoFrame("../../samples/Fixtures/video_test.y4m", 99, "work/vframe_oob.png")
END SUB
