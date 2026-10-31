' EXPECT: tick= 2
' ALIAS su DECLARE SUB (void) + SUB zero-arg + FUNCTION zero-arg con ALIAS.
DECLARE SUB s_reset LIB "libvec_module" ALIAS "vec_reset" ()
DECLARE SUB s_bump LIB "libvec_module" ALIAS "vec_bump" (v AS DOUBLE)
DECLARE FUNCTION s_tick LIB "libvec_module" ALIAS "vec_tick" () AS LONG

SUB Main()
    s_reset()
    s_bump(1)
    s_bump(2)
    PRINT "tick="; s_tick()
END SUB
