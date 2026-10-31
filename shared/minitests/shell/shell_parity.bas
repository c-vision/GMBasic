' Minitest SET SHELL ON + SYSTEM.SHELLOUT — parità fra i 4 backend.
' EXPECT: shellout=gmb-shell-ok|DONE
SUB Main()
  SET SHELL ON
  DIM o AS STRING
  o = SYSTEM.SHELLOUT("echo gmb-shell-ok")
  PRINT "shellout="; TRIM(o)
  PRINT "DONE"
END SUB
