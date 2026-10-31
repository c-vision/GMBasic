' EXPECT: im=L0@1,Main@2|deep=Mid@1,L0@2,Main@3
IMPORT LLM
SUB Top()
END SUB
SUB Mid()
  Top()
END SUB
SUB L0()
  Mid()
END SUB
SUB Main()
  L0()
  PRINT "im="; LLM.IMPACT("Mid")
  PRINT "deep="; LLM.IMPACT("Top")
END SUB
