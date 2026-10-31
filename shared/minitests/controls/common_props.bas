' EXPECT: common-props-ok
' Superficie BASIC della base Control: proprietà comuni a tutti i controlli.
' Parità 4 backend (IL / PCode / VM-C / C99Native).
IMPORT CONTROLS

CLASS Box INHERITS Control
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM b AS Box = NEW Box()
  CONTROL ADD b AT 3, 4 SIZE 20, 8

  b.X = 10
  b.Y = 11
  b.Width = 21
  b.Height = 9
  b.Visible = 1
  b.Enabled = 1
  b.TabIndex = 2
  b.TabStop = 1
  b.Tag = "tag-value"
  b.ToolTipText = "tooltip-value"
  b.FontName = "Arial"
  b.FontSize = 12
  b.ForeColor = "#FF8000"
  b.BackColor = "#003060"
  b.Appearance = 1
  b.MousePointer = 2
  b.RightToLeft = 0
  b.Locked = 0
  b.StartUpPosition = 2
  b.ShowInTaskbar = 1
  b.WhatsThisHelpID = 7
  b.WordWrap = 1
  b.AutoSize = 0
  b.BackStyle = 1
  b.BorderStyle = 1
  b.UseMnemonic = 1
  b.CausesValidation = 1

  IF b.X = 10 AND b.Y = 11 AND b.Width = 21 AND b.Height = 9 AND b.Visible = 1 AND b.Enabled = 1 AND b.TabIndex = 2 AND b.TabStop = 1 AND b.Tag = "tag-value" AND b.ToolTipText = "tooltip-value" AND b.FontName = "Arial" AND b.FontSize = 12 AND b.ForeColor = "#FF8000" AND b.BackColor = "#003060" AND b.Appearance = 1 AND b.MousePointer = 2 AND b.RightToLeft = 0 AND b.Locked = 0 AND b.StartUpPosition = 2 AND b.ShowInTaskbar = 1 AND b.WhatsThisHelpID = 7 AND b.WordWrap = 1 AND b.AutoSize = 0 AND b.BackStyle = 1 AND b.BorderStyle = 1 AND b.UseMnemonic = 1 AND b.CausesValidation = 1 THEN
    PRINT "common-props-ok"
  ELSE
    PRINT "common-props-no"
  END IF
  CONTROL REMOVE b
END SUB
