' TAG: unsupported-c99native
' EXPECT: nomongo
' REQUIRE (package manager) + FFI MongoDB (namespace MONGO).
'   REQUIRE "mongo" risolve il pacchetto (~/.gmbasic/packages) e include mongo.inc.
'   IMPORT MONGO WITH ALIAS -> MONGO.<Membro>(...).
' Deterministico SENZA un server MongoDB: AVAILABLE()=1 (lib presente) e una connessione a una
' porta chiusa fallisce al PING. L'end-to-end reale richiede un server (es. docker run -p
' 27017:27017 mongo:7); vedi samples/packages/mongo@<ver>/mongo_demo.bas.
REQUIRE "mongo"
IMPORT MONGO WITH ALIAS

SUB Main()
    IF MONGO.AVAILABLE() = 0 THEN
        PRINT "nomongo"
        EXIT SUB
    END IF
    DIM h AS LONG
    h = MONGO.CONNECT("mongodb://127.0.0.1:27999/?serverSelectionTimeoutMS=1500")
    IF h = 0 THEN
        PRINT "nomongo"
        EXIT SUB
    END IF
    IF MONGO.PING(h) = 0 THEN
        PRINT "nomongo"
    ELSE
        PRINT "unexpected connect"
    END IF
    MONGO.CLOSE(h)
END SUB
