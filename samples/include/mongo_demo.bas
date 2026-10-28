' Sample pacchetto mongo.
REQUIRE "mongo"
IMPORT MONGO WITH ALIAS

SUB Main()
    PRINT "mongo available="; MONGO.AVAILABLE()
    DIM h AS LONG
    h = MONGO.CONNECT("mongodb://127.0.0.1:27999/?serverSelectionTimeoutMS=1500")
    PRINT "connect="; h
END SUB
