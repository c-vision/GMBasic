' EXPECT: refresh=ok
IMPORT AI
SUB Main()
  AI CONNECTOR REGISTER "mt_ref", "test"
  AI CONNECTOR REFRESH "mt_ref"
  PRINT "refresh=ok"
  AI CONNECTOR UNREGISTER "mt_ref"
END SUB
