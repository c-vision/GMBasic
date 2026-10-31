' MiniTest: namespace CONSOLE — funzioni DETERMINISTICHE headless — parita' 4 backend.
' Coperti: FGRGB/BGRGB (ANSI true-color), INKEY (""), MESSAGE (getter/setter),
' CELL/CELLSTR, POINT (-1 headless), GETCURSORPOS [0,0], GETTEXTMETRICS [80,24],
' GETPALETTECOLOR (VGA16).
' NON coperti (UI-modali, rendono a schermo come SDL/grafica, non deterministici tra
' backend): ALERT, CONFIRM — esclusi per natura, come i test grafici.
' @no-tui: test deterministico headless — niente canary ESC (INKEY deve dare "").
' EXPECT: cursor= 0 , 0|fg=X|bg=Y|inkey=[]|msg=[x]|cell= 7|cellstr=[]|point=-1|metrics= 80 , 24|pal= 0 , 0 , 170|alert= 0|confirm= 0
IMPORT CONSOLE

SUB Main
  ' GETCURSORPOS per primo (all'origine): il cursore headless avanza coi PRINT su IL,
  ' mentre PCode/VM-C/C99 lo riportano statico [0,0] — all'origine i 5 coincidono.
  DIM c() = CONSOLE.GETCURSORPOS()
  PRINT "cursor="; c(0); ","; c(1)
  PRINT "fg="; CONSOLE.FGRGB("X", 255, 128, 0)
  PRINT "bg="; CONSOLE.BGRGB("Y", 10, 20, 30)
  PRINT "inkey=["; CONSOLE.INKEY(); "]"
  PRINT "msg=["; CONSOLE.MESSAGE("x"); "]"
  PRINT "cell="; CONSOLE.CELL(0,0,0)
  PRINT "cellstr=["; CONSOLE.CELLSTR(0,0); "]"
  PRINT "point="; CONSOLE.POINT(0,0)
  DIM m() = CONSOLE.GETTEXTMETRICS()
  PRINT "metrics="; m(0); ","; m(1)
  DIM p() = CONSOLE.GETPALETTECOLOR(1)
  PRINT "pal="; p(0); ","; p(1); ","; p(2)
  PRINT "alert="; CONSOLE.ALERT("a")
  PRINT "confirm="; CONSOLE.CONFIRM("c")
END SUB
