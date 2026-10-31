IMPORT CONVERT
' FILE: Samples/Documents/ConvertBasics.bas
' Usa i file fixture in Samples/Fixture/ (report.docx, data.xlsx).
' Il runner imposta la CWD sulla dir del sample (Samples/Documents), quindi
' per raggiungere Samples/Fixture/ serve risalire di un livello.

SUB Main()
    DIM mdResult AS STRING
    DIM fx AS STRING
    fx = "../Fixture/"

    mdResult = CONVERT.TOMD(fx + "report.docx")
    PRINT "Markdown length: ", LEN(mdResult)
    PRINT LEFT(mdResult, 80)

    mdResult = CONVERT.TOHTML(fx + "data.xlsx")
    PRINT "HTML length: ", LEN(mdResult)
    PRINT LEFT(mdResult, 80)
END SUB
