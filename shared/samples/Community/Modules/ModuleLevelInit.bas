' FILE: Modules/ModuleLevelInit.bas
' ======================================================================================================================
' GUIDA: codice di livello MODULO eseguito prima di SUB Main, e visibile nelle SUB
' ======================================================================================================================
' Le istruzioni a livello di modulo (DIM, assegnazioni, chiamate) sono eseguite PRIMA di SUB Main
' e le variabili sono condivise con le SUB. Vale su tutti e 5 i backend.
' ======================================================================================================================
DIM BASE_PATH AS STRING
DIM COUNTER AS INTEGER
BASE_PATH = "/tmp/gmbasic-module"
COUNTER = 41
PRINT "init modulo: BASE_PATH="; BASE_PATH; " COUNTER="; COUNTER

SUB Main()
    PRINT "in SUB Main: BASE_PATH="; BASE_PATH
    PRINT "in SUB Main: COUNTER+1="; COUNTER + 1
    IF BASE_PATH = "/tmp/gmbasic-module" AND COUNTER = 41 THEN
        PRINT "module_ok"
    ELSE
        PRINT "module_bad"
    END IF
END SUB
