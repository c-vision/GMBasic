' EXPECT: tick= 3
DECLARE SUB vec_reset LIB "libvec_module" ()
DECLARE SUB vec_bump LIB "libvec_module" (v AS DOUBLE)
DECLARE FUNCTION vec_tick LIB "libvec_module" () AS LONG

SUB Main()
    vec_reset()
    vec_bump(1)
    vec_bump(2)
    vec_bump(3)
    PRINT "tick="; vec_tick()
END SUB
