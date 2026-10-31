' EXPECT: v1=uno|v2=lista|v3=range|v4=gt|velse=altro
SUB Main()
    DIM r AS STRING
    ' SELECT CASE: tutte e 4 le forme di CASE (valore/lista/range/comparazione) + ELSE
    SELECT CASE 1
        CASE 1
            r = "uno"
        CASE 2, 3, 4
            r = "lista"
        CASE 5 TO 10
            r = "range"
        CASE IS > 20
            r = "gt"
        CASE ELSE
            r = "altro"
    END SELECT
    PRINT "v1="; r

    SELECT CASE 3
        CASE 1
            r = "uno"
        CASE 2, 3, 4
            r = "lista"
        CASE 5 TO 10
            r = "range"
        CASE ELSE
            r = "altro"
    END SELECT
    PRINT "v2="; r

    SELECT CASE 7
        CASE 1
            r = "uno"
        CASE 2, 3, 4
            r = "lista"
        CASE 5 TO 10
            r = "range"
        CASE ELSE
            r = "altro"
    END SELECT
    PRINT "v3="; r

    SELECT CASE 25
        CASE 1
            r = "uno"
        CASE 2, 3, 4
            r = "lista"
        CASE 5 TO 10
            r = "range"
        CASE IS > 20
            r = "gt"
        CASE ELSE
            r = "altro"
    END SELECT
    PRINT "v4="; r

    SELECT CASE 15
        CASE 1
            r = "uno"
        CASE 5 TO 10
            r = "range"
        CASE IS > 20
            r = "gt"
        CASE ELSE
            r = "altro"
    END SELECT
    PRINT "velse="; r
END SUB
