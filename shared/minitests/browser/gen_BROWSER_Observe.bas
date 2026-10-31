' EXPECT: error: CDP connect fallita su
IMPORT BROWSER

SUB Main()
    DIM obs$ = BROWSER.OBSERVE()
    PRINT LEFT$(obs$, 30)
END SUB
