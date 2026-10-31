' Sample pacchetto avif: usa il pacchetto via REQUIRE + IMPORT (gli .inc sono in includes/).
REQUIRE "avif"
IMPORT AVIF WITH ALIAS

SUB Main()
    PRINT "avif available="; AVIF.AVAILABLE()
END SUB
