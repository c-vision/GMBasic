SUB Main()
    DIM m(1, 1) AS INTEGER
    m(0,0)=1 : m(0,1)=2
    DIM row(1) AS INTEGER
    FOR EACH row IN m
        PRINT row(0); ","; row(1)
    NEXT
END SUB
