' MiniTest NAMESPACE MOUSE — tutte le 11 funzioni della superficie.
' Headless (nessun dispositivo): lo stato è fermo (0) su tutte le gambe; il test verifica
' che le funzioni esistano, siano dispatchate e che CLICK accetti entrambe le arità.
' EXPECT: x= 0|y= 0|button= 0|button1= 0|left= 0|middle= 0|right= 0|wheel= 0|present= 0|click4= 0|click0= 0|dblclick= 0|move= 0
IMPORT MOUSE

SUB Main()
  PRINT "x="; MOUSE.X()
  PRINT "y="; MOUSE.Y()
  PRINT "button="; MOUSE.BUTTON()
  PRINT "button1="; MOUSE.BUTTON(1)
  PRINT "left="; MOUSE.LEFT()
  PRINT "middle="; MOUSE.MIDDLE()
  PRINT "right="; MOUSE.RIGHT()
  PRINT "wheel="; MOUSE.WHEEL()
  PRINT "present="; MOUSE.PRESENT()
  PRINT "click4="; MOUSE.CLICK(1, 1, 2, 2)
  PRINT "click0="; MOUSE.CLICK()
  PRINT "dblclick="; MOUSE.DBLCLICK()
  PRINT "move="; MOUSE.MOVE()
END SUB
