' EXPECT: at=tok123|rt=ref456
IMPORT AI
SUB Main()
  AI CONNECTOR REGISTER "mt_auth", "test"
  DIM q AS STRING = CHR$(34)
  DIM j AS STRING = "{" + q + "access_token" + q + ":" + q + "tok123" + q + ", " + q + "refresh_token" + q + ":" + q + "ref456" + q + "}"
  AI CONNECTOR AUTH "mt_auth", j
  PRINT "at="; AI.CONNECTORSECRETGET("mt_auth", "access_token")
  PRINT "rt="; AI.CONNECTORSECRETGET("mt_auth", "refresh_token")
  AI CONNECTOR UNREGISTER "mt_auth"
END SUB
