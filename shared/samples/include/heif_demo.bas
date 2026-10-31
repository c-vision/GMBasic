' Sample pacchetto heif: usa il pacchetto via REQUIRE + IMPORT (gli .inc sono in includes/).
REQUIRE "heif"
IMPORT HEIF WITH ALIAS

SUB Main()
    PRINT "heif available="; HEIF.AVAILABLE()
END SUB
