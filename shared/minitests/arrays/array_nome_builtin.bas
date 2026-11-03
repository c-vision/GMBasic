' MiniTest: un array locale che omonima un builtin non qualificato (fill → BROWSER.Fill)
' vince sul builtin: lettura/scrittura devono restare numeriche su tutte le gambe.
' EXPECT: fill= 7
SUB Main()
    DIM fill(3)
    fill(0) = 7
    PRINT "fill="; fill(0)
END SUB
