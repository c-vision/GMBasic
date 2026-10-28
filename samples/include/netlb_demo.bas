' Sample pacchetto netlb: usa il pacchetto via REQUIRE + IMPORT (gli .inc sono in includes/).
REQUIRE "netlb"
IMPORT NETLB WITH ALIAS

SUB Main()
    PRINT "netlb available="; NETLB.AVAILABLE()
END SUB
