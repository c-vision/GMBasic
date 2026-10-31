' FILE sequenziali Clipper — catena completa OPEN/USING/PRINT#/WRITE#/INPUT#/LINE INPUT#/EOF/CLOSE
' EXPECT: r1= 101 |Mario, Rossi| 123.45 |-1|raw=raw line|r2= 202 |Luigi|-50 | 0|eof=-1|eof2= 0|done
' Sintassi canone: samples/Community/Files/SequentialFilesFull.bas
SUB Main()
    USING F = OPEN("work_seqfull.txt", OUTPUT)
        WRITE #F, 101, "Mario, Rossi", 123.45, TRUE
        PRINT #F, "raw line"
    END USING
    USING F = OPEN("work_seqfull.txt", APPEND)
        WRITE #F, 202, "Luigi", -50, FALSE
    END USING
    USING F = OPEN("work_seqfull.txt", INPUT)
        DIM id AS INTEGER
        DIM nome AS STRING
        DIM saldo AS DOUBLE
        DIM flag AS INTEGER
        INPUT #F, id, nome, saldo, flag
        PRINT "r1="; id; "|"; nome; "|"; saldo; "|"; flag
        DIM raw AS STRING
        LINE INPUT #F, raw
        PRINT "raw="; raw
        INPUT #F, id, nome, saldo, flag
        PRINT "r2="; id; "|"; nome; "|"; saldo; "|"; flag
        PRINT "eof="; EOF(F)
    END USING
    OPEN "work_seqfull.txt" FOR INPUT AS #1
    PRINT "eof2="; EOF(1)
    CLOSE #1
    KILL "work_seqfull.txt"
    PRINT "done"
END SUB
