' Sample pacchetto webp: usa il pacchetto via REQUIRE + IMPORT (gli .inc sono in includes/).
REQUIRE "webp"
IMPORT WEBP WITH ALIAS

SUB Main()
    PRINT "webp available="; WEBP.AVAILABLE()
END SUB
