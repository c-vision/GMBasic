' MiniTest: AI DECIDE VERIFY — stessa richiesta su più provider, report accordo/drift.
' EXPECT: verify-split={"ok":true,"agreement":false,"argmaxDrift":1,"probabilitySpread":0,"results":[{"provider":"rules","result":{"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}},{"provider":"mock","result":{"kind":"NOUL","option":"false","value":"false","probability":1.000000,"confidence":null,"calibrated":null}}]}|verify-same={"ok":true,"agreement":true,"argmaxDrift":0,"probabilitySpread":0,"results":[{"provider":"mock","result":{"kind":"NOUL","option":"false","value":"false","probability":1.000000,"confidence":null,"calibrated":null}},{"provider":"mock","result":{"kind":"NOUL","option":"false","value":"false","probability":1.000000,"confidence":null,"calibrated":null}}]}
IMPORT AI

SUB Main()
    DIM r AS STRING
    AI DECIDE VERIFY "sì", "NOUL", "rules|mock" INTO r
    PRINT "verify-split="; r
    AI DECIDE VERIFY "sì", "NOUL", "mock|mock" INTO r
    PRINT "verify-same="; r
END SUB
