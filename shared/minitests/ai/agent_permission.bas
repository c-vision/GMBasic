' MiniTest: AI PERMISSION (Sandbox/YOLO + tool policy) — parita' 4 backend.
' EXPECT: level:Sandbox|policies:browser_set_cookie=allow
IMPORT AI

SUB Main()
    AI AGENT CREATE "P"
    AI PERMISSION P, sandbox
    PRINT "level:"; AI.AGENTGET("P", "permission")
    AI PERMISSION P, TOOL browser_set_cookie, allow
    PRINT "policies:"; AI.AGENTGET("P", "toolpolicies")
END SUB
