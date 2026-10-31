' Sample pacchetto redis: usa il pacchetto via REQUIRE + IMPORT (gli .inc sono in includes/).
REQUIRE "redis"
IMPORT REDIS WITH ALIAS

SUB Main()
    PRINT "redis available="; REDIS.AVAILABLE()
END SUB
