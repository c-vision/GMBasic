' EXPECT: lens= 5 | 3 | 5|up=ALPHA|BET|GAMMA|long= 5 | 5|has_bet= 0
SUB Main()
    DIM names(2) AS STRING
    names(0) = "alpha"
    names(1) = "bet"
    names(2) = "gamma"
    DIM d AS DICT OF STRING, INTEGER = { n : LEN(n) FOR n IN names }
    PRINT "lens="; d("alpha"); "|"; d("bet"); "|"; d("gamma")
    DIM u AS DICT OF STRING, STRING = { n : UPPER(n) FOR n IN names }
    PRINT "up="; u("alpha"); "|"; u("bet"); "|"; u("gamma")
    DIM lg AS DICT OF STRING, INTEGER = { n : LEN(n) FOR n IN names IF LEN(n) > 3 }
    PRINT "long="; lg("alpha"); "|"; lg("gamma")
    PRINT "has_bet="; lg.COUNT() - 2
END SUB
