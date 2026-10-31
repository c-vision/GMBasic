' STYLE "primary": token di stile = accento del tema — parità 4 backend (IL, P-Code, VM/C, C99Native).
' EXPECT: style=ok
BUTTON 4, 4, 20, 6, "OK" STYLE "primary"
BUTTON 4, 14, 20, 6, "NEW" STYLE "primary rounded radius=10"
DIM s AS STRING
s = "style=ok"
PRINT s
