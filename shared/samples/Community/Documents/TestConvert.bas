IMPORT CONVERT
' FILE: Samples/Documents/TestConvert.bas
' =================================================================================================
'       Test comandi CONVERT con file fixture
' =================================================================================================

SUB Main()
    DIM fixtures AS STRING
    fixtures = "../Fixture/"
    DIM work AS STRING
    work = "Samples/Documents/work/"

    PRINT "=== Test 1: CONVERT.FORMATS() ==="
    DIM fmts AS STRING
    fmts = CONVERT.FORMATS()
    PRINT LEFT(fmts, 50), "..."

    PRINT ""
    PRINT "=== Test 2: CSV -> Markdown ==="
    DIM csvMd AS STRING
    csvMd = CONVERT.TOMD(fixtures + "test.csv")
    PRINT LEFT(csvMd, 100)

    PRINT ""
    PRINT "=== Test 3: TXT -> HTML ==="
    DIM txtHtml AS STRING
    txtHtml = CONVERT.TOHTML(fixtures + "test.txt")
    PRINT LEFT(txtHtml, 100)

    PRINT ""
    PRINT "=== Test 4: HTML -> Markdown ==="
    DIM htmlMd AS STRING
    htmlMd = CONVERT.TOMD(fixtures + "test.html")
    PRINT LEFT(htmlMd, 100)

    PRINT ""
    PRINT "=== Test 5: JSON -> Markdown ==="
    DIM jsonMd AS STRING
    jsonMd = CONVERT.TOMD(fixtures + "test.json")
    PRINT LEFT(jsonMd, 100)

    PRINT ""
    PRINT "=== Test 6: TXT -> Text ==="
    DIM txtTxt AS STRING
    txtTxt = CONVERT.TOTEXT(fixtures + "test.txt")
    PRINT LEFT(txtTxt, 100)

    PRINT ""
    PRINT "All tests completed."
END SUB
