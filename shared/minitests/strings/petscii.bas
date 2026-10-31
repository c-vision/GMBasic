' EXPECT: clr= 16|colors= 28|num= 20|plain= 11|unknown= 20
IMPORT STRINGS
SUB Main()
  PRINT "clr="; LEN(STRINGS.PETSCII("{clr}Hello{home}"))
  PRINT "colors="; LEN(STRINGS.PETSCII("{wht}White{red}Red{grn}Green"))
  PRINT "num="; LEN(STRINGS.PETSCII("A{3 down}B{2 right}C"))
  PRINT "plain="; LEN(STRINGS.PETSCII("Hello World"))
  PRINT "unknown="; LEN(STRINGS.PETSCII("Ciao {unknown} mondo"))
END SUB
