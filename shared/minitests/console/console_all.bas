' MiniTest NAMESPACE CONSOLE — tutte le 12 funzioni della superficie.
' ALERT, BGRGB, CELL, CELLSTR, CONFIRM, FGRGB, GETCURSORPOS, GETPALETTECOLOR,
' GETTEXTMETRICS, INKEY, MESSAGE, Point. Headless: i modali ritornano subito (nessun
' park) e i getter restituiscono liste/larghezze deterministiche.
' EXPECT: fg_len= 1|bg_len= 1|pal_n= 3|cur_n= 2|tm_n= 2|point=-1|cell= 7|cellstr=[]|inkey=[]|alert= 0|confirm= 0|message=[x]
IMPORT CONSOLE

SUB Main()
  PRINT "fg_len="; LEN(FGRGB("x", 1, 2, 3))
  PRINT "bg_len="; LEN(BGRGB("x", 1, 2, 3))
  DIM pl = GETPALETTECOLOR(1)
  PRINT "pal_n="; UBOUND(pl) + 1
  DIM cp = GETCURSORPOS()
  PRINT "cur_n="; UBOUND(cp) + 1
  DIM tm = GETTEXTMETRICS()
  PRINT "tm_n="; UBOUND(tm) + 1
  PRINT "point="; CONSOLE.POINT(1, 2)
  PRINT "cell="; CONSOLE.CELL(0, 0, 0)
  PRINT "cellstr=["; CONSOLE.CELLSTR(0, 0); "]"
  PRINT "inkey=["; INKEY(0); "]"
  PRINT "alert="; ALERT("x")
  PRINT "confirm="; CONFIRM("x")
  PRINT "message=["; CONSOLE.MESSAGE("x"); "]"
END SUB
