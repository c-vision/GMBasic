IMPORT CONVERT
' FILE: Samples/Documents/ConvertCommand.bas
' Sintassi FUNZIONE del namespace CONVERT: `CONVERT.TOMD/TOHTML(file)`.
' Complementare a ConvertBasics.bas (stessa conversione, forma compatta).
' Usa i file fixture in Samples/Fixture/ (report.docx, data.xlsx).

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
