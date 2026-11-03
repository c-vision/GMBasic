' TAG: unsupported-c99native
' EXPECT: noredis
' REQUIRE (package manager) + FFI Redis (namespace REDIS).
'   REQUIRE "redis" risolve il pacchetto (~/.gmbasic/packages) e include redis.inc.
'   IMPORT REDIS WITH ALIAS -> REDIS.<Membro>(...).
' Deterministico SENZA un server Redis: AVAILABLE()=1 (lib presente) e CONNECT a una porta
' chiusa ritorna 0. Il protocollo RESP e' coperto dal mock test in C (gmbsuite); l'end-to-end
' reale richiede un server (es. docker run -p 6379:6379 redis:7-alpine).
REQUIRE "redis"
IMPORT REDIS WITH ALIAS

SUB Main()
    DIM h AS LONG
    IF REDIS.AVAILABLE() = 0 THEN
        PRINT "norelib"
        EXIT SUB
    END IF
    h = REDIS.CONNECT("127.0.0.1", 6390)     ' porta quasi certamente chiusa
    IF h = 0 THEN
        PRINT "noredis"
    ELSE
        PRINT "unexpected connect"
        REDIS.CLOSE(h)
    END IF
END SUB
