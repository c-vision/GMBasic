' MiniTest: costanti DIALOG (%OK/%CANCEL/%YES/%NO/%YESNO/%OKCANCEL/%ABORT/%RETRY/%IGNORE/%ABORTRETRYIGNORE/%RETRYCANCEL)
'   e ALIGNMENT (%LEFT/%CENTER/%RIGHT) e decorazione finestra (%SINGLE/%DOUBLE/%NONE).
'   In headless ALERT/CONFIRM ritornano 0. Le costanti sono validate dal linker.
' EXPECT: ok= 1|cancel= 2|abort= 3|retry= 4|ignore= 5|yes= 6|no= 7|yesno= 102|okcancel= 101|abortretry= 104|retrycancel= 105|none= 0|left= 0|center= 1|right= 2|single= 1|dbl= 2|none2= 0|ync= 103
SUB Main
  PRINT "ok="; %OK
  PRINT "cancel="; %CANCEL
  PRINT "abort="; %ABORT
  PRINT "retry="; %RETRY
  PRINT "ignore="; %IGNORE
  PRINT "yes="; %YES
  PRINT "no="; %NO
  PRINT "yesno="; %YESNO
  PRINT "okcancel="; %OKCANCEL
  PRINT "abortretry="; %ABORTRETRYIGNORE
  PRINT "retrycancel="; %RETRYCANCEL
  PRINT "none="; %NONE
  PRINT "left="; %LEFT
  PRINT "center="; %CENTER
  PRINT "right="; %RIGHT
  PRINT "single="; %SINGLE
  PRINT "dbl="; %DOUBLE
  PRINT "none2="; %NONE
  PRINT "ync="; %YESNOCANCEL
END SUB
