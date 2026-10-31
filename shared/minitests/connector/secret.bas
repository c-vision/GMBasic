' EXPECT: empty=|set=sk_test
IMPORT AI
SUB Main()
  AI CONNECTOR REGISTER "mt_sec", "test"
  PRINT "empty="; AI.CONNECTORSECRETGET("mt_sec", "api_key")
  AI CONNECTOR SECRET SET "mt_sec", "api_key", "sk_test"
  PRINT "set="; AI.CONNECTORSECRETGET("mt_sec", "api_key")
  AI CONNECTOR UNREGISTER "mt_sec"
END SUB
