' FILE: Clipper/ClipperDbfFixtureRead.bas
' Compile-only smoke: golden fixture path exists (runtime needs license).

IMPORT IO WITH ALIAS

SUB Main()
    CLS
    PRINT "ClipperDbfFixtureRead — dbase_03 fixture path check"

    DIM cFixture AS STRING
    cFixture = "Samples/Fixtures/Dbf/working/dbase3/dbase_03.dbf"

    IF IO.FileExists(cFixture) THEN
        PRINT "OK: "; cFixture
    ELSE
        PRINT "SKIP: "; cFixture
    END IF
END SUB
