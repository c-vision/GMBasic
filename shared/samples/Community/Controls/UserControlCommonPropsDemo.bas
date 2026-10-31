' UserControlCommonPropsDemo.bas — superficie BASIC della base Control.
' Esercita tutte le proprietà comuni ai controlli utente su IL / PCode / VM-C / C99Native.
IMPORT CONTROLS

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12

  p.X = 5
  p.Y = 6
  p.Width = 40
  p.Height = 12
  p.Visible = 1
  p.Enabled = 1
  p.TabIndex = 0
  p.TabStop = 1
  p.Tag = "demo"
  p.ToolTipText = "common-props"
  p.FontName = "Arial"
  p.FontSize = 10
  p.ForeColor = "#E0E0E0"
  p.BackColor = "#101820"
  p.Appearance = 1
  p.MousePointer = 2
  p.RightToLeft = 0
  p.Locked = 0
  p.StartUpPosition = 2
  p.ShowInTaskbar = 1
  p.WhatsThisHelpID = 7
  p.WordWrap = 1
  p.AutoSize = 0
  p.BackStyle = 1
  p.BorderStyle = 1
  p.UseMnemonic = 1
  p.CausesValidation = 1

  PRINT "common-props-demo-ok"
  PRINT p.X; ","; p.Y; " "; p.FontName; " "; p.FontSize; " "; p.Appearance; " "; p.StartUpPosition

  DIM sel AS INTEGER
  DIM h AS INTEGER
  DIM n AS INTEGER
  DIM items AS STRING
  LISTBOX 2, 16, 20, 5, "One|Two|Three" INTO sel ID h NAME "DemoList"
  CONTROL SET "DemoList", "items", 0, "Red|Green|Blue"
  CONTROL GET "DemoList", "listcount" INTO n
  CONTROL GETS "DemoList", "items" INTO items
  CONTROL SET "DemoList", "listindex", 1
  CONTROL GET "DemoList", "listindex" INTO sel
  PRINT "listbox-demo-ok "; n; " "; items; " "; sel
  CONTROL REMOVE p
END SUB
