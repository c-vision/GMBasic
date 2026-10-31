' Sample pacchetto video: usa il pacchetto via REQUIRE + IMPORT (gli .inc sono in includes/).
REQUIRE "video"
IMPORT VIDEO WITH ALIAS

SUB Main()
    PRINT "video available="; VIDEO.AVAILABLE()
END SUB
