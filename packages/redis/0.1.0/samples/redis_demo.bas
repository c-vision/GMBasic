' ============================================================================
'  redis_demo.bas — uso di Redis da GMBasic via package + FFI (namespace REDIS).
'  5 gambe: tools/parity5 samples/includes/redis_demo.bas
'
'  REQUIRE "redis"  +  IMPORT REDIS WITH ALIAS  (come per duckdb).
'  Copre TUTTI i membri. Se un server Redis e' in ascolto su 127.0.0.1:6379 esegue il
'  demo completo; altrimenti degrada con un messaggio deterministico.
'  Avvio:  docker run --rm -p 6379:6379 redis:7-alpine
' ============================================================================

REQUIRE "redis"
IMPORT REDIS WITH ALIAS

SUB Main()
    DIM h AS LONG
    PRINT "available="; REDIS.AVAILABLE()
    h = REDIS.CONNECT("127.0.0.1", 6379)
    IF h = 0 THEN
        PRINT "redis_demo: nessun server su 127.0.0.1:6379 (avviane uno per il demo completo)"
        EXIT SUB
    END IF

    PRINT "alive="; REDIS.ALIVE(h)
    REDIS.SELECT(h, 0)
    REDIS.FLUSHDB(h)
    PRINT "ping="; REDIS.PING(h)

    REDIS.SET(h, "name", "hello")
    PRINT "get="; REDIS.GET(h, "name")
    REDIS.SETEX(h, "tmp", 100, "v")
    PRINT "ttl="; REDIS.TTL(h, "tmp")
    PRINT "strlen="; REDIS.STRLEN(h, "name")
    PRINT "incr="; REDIS.INCR(h, "n", 5)
    PRINT "exists="; REDIS.EXISTS(h, "name")

    REDIS.HSET(h, "user", "city", "Rome")
    PRINT "hget="; REDIS.HGET(h, "user", "city")
    PRINT "hexists="; REDIS.HEXISTS(h, "user", "city"); " hlen="; REDIS.HLEN(h, "user")

    REDIS.LPUSH(h, "queue", "a")
    REDIS.LPUSH(h, "queue", "b")
    PRINT "llen="; REDIS.LLEN(h, "queue"); " lpop="; REDIS.LPOP(h, "queue")

    REDIS.SADD(h, "tags", "red")
    PRINT "sismember="; REDIS.SISMEMBER(h, "tags", "red"); " scard="; REDIS.SCARD(h, "tags")

    PRINT "expire="; REDIS.EXPIRE(h, "name", 100)
    PRINT "publish="; REDIS.PUBLISH(h, "chan", "msg")
    REDIS.HDEL(h, "user", "city")
    PRINT "del="; REDIS.DEL(h, "name")
    PRINT "dbsize="; REDIS.DBSIZE(h)

    ' comando generico + introspezione risposta (int / array / errore)
    REDIS.DO(h, "DBSIZE")
    PRINT "do_int type="; REDIS.REPLYTYPE(h); " int="; REDIS.REPLYINT(h)
    REDIS.DO(h, "KEYS *")
    PRINT "do_keys type="; REDIS.REPLYTYPE(h); " count="; REDIS.REPLYCOUNT(h)
    REDIS.DO(h, "NOTACOMMAND")
    PRINT "err="; REDIS.ERR(h)

    REDIS.CLOSE(h)
    PRINT "redis_demo: completato"
END SUB
