' EXPECT: {"events":["before_task","after_task","before_agent","after_agent","before_tool","after_tool","before_model","after_model","on_error","on_retry","on_complete","on_verify","on_handoff"]}
IMPORT AI

SUB Main()
    PRINT AI.HOOKEVENTS()
END SUB
