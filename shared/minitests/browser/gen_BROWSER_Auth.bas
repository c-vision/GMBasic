' EXPECT: (nessun auth profile)
IMPORT BROWSER

SUB Main()
    ' AUTH LIST funziona senza browser (legge directory)
    PRINT BROWSER.AUTH("list", "")
END SUB
