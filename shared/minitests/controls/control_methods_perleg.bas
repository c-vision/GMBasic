' MiniTest: proprietà base dei controlli (NAME/TAG/TOOLTIP/VISIBLE/ENABLED/TABINDEX/
' TABSTOP/FOCUS), TEXTBOX (cursore/editing) e metodi a runtime CONTROL
' SET/GET/GETS/MOVE/FOCUS/ZORDER.
' GUI pixel: verificato per-gamba (emissione + compilazione + link) su C99Native/
' emitter C#; escluso dal cross-leg in-process (apre lo screen). Le gambe gestite
' raggiungono lo stesso runtime C via P/Invoke.
' EXPECT: control-ok
IMPORT CONTROLS
SCREEN 0
DIM ok = 1
DIM nome AS STRING
DIM val = 0
DIM cap AS STRING

SUB Main()
  CHECKBOX 20, 20, "A" INTO ok NAME "chk1" TOOLTIP "t" TAG "g" VISIBLE 1 ENABLED 1 TABINDEX 1 TABSTOP 1 FOCUS ON CHANGE OnCh
  TEXTBOX 20, 60, 200, 28 INTO nome NAME "txt1" TABINDEX 2
  BUTTON 20, 100, 120, 36, "Ok" ON CLICK OnOk NAME "btn1"
  CONTROL SET "chk1", "visible", 1
  CONTROL GET "chk1", "visible" INTO val
  CONTROL GETS "btn1", "caption" INTO cap
  CONTROL MOVE "btn1", 200, 100, 140, 40
  CONTROL FOCUS "chk1"
  CONTROL ZORDER "btn1", 1
  IF val = 1 AND cap = "Ok" THEN PRINT "control-ok" ELSE PRINT "control-no"
END SUB

SUB OnCh()
END SUB
SUB OnOk()
END SUB
