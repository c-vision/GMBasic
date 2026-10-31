' EXPECT: 1|-1|-1
IMPORT HTTP
DIM cookie$ = HTTP.SESSIONCREATE("demo", "admin")
PRINT INSTR(cookie$, "GMBSESSION=")
PRINT (INSTR(cookie$, "HttpOnly") > 0)
PRINT (INSTR(cookie$, "SameSite=Lax") > 0)
