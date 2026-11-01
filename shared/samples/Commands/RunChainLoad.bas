' FILE: samples/Commands/RunChainLoad.bas
' RUN "comando" — chain-load via shell
' Esegue un comando/script via shell e termina
IMPORT SYSTEM
SUB Main()
  SET SHELL ON
  PRINT "Eseguo comando esterno..."
  RUN "echo Ciao dal chain-load"
  PRINT "Mai raggiunto (RUN non torna)"
END SUB