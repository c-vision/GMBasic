' TAG: unsupported-c99native
' EXPECT: count= 3  ready= 1|rr= 0  1  2|pickcount_c= 1|inflight0= 1|ewma0= 10  inflight0b= 0|load0= 10|healthy1= 0|healthy1b= 1|idle= 3|stickysame= 0 = 0|stickycnt= 1|hrwh= 0|maglev= 0|excl= 1|retry= 1|done
' REQUIRE (package manager) + FFI netlb (namespace NETLB) — load balancing.
' Esercita TUTTI i 30 membri: parametri e risultati identici sui 4 backend.
REQUIRE "netlb"
IMPORT NETLB WITH ALIAS

SUB Main()
    DIM h AS LONG
    IF NETLB.AVAILABLE() = 0 THEN
        PRINT "norelib"
        EXIT SUB
    END IF

    h = NETLB.POOLNEW(NETLB_RR, 7)
    NETLB.BACKENDADD(h, "a", 1000, 0)
    NETLB.BACKENDADD(h, "b", 1000, 0)
    NETLB.BACKENDADD(h, "c", 1000, 0)
    PRINT "count="; NETLB.BACKENDCOUNT(h); " ready="; NETLB.READY(h)

    PRINT "rr="; NETLB.PICK(h, ""); NETLB.PICK(h, ""); NETLB.PICK(h, "")
    PRINT "pickcount_c="; NETLB.PICKCOUNT(h, 2)

    NETLB.SETPOLICY(h, NETLB_LC)
    NETLB.SETWEIGHT(h, 0, 2000)
    NETLB.SETCAP(h, 0, 5)
    NETLB.SLOWSTART(h, 1000)
    NETLB.RETRYBUDGET(h, 500)
    NETLB.SETNOW(h, 1000)

    NETLB.BEGINREQ(h, 0)
    PRINT "inflight0="; NETLB.INFLIGHT(h, 0)
    NETLB.ENDREQ(h, 0, 1, 10)
    PRINT "ewma0="; NETLB.EWMA(h, 0); " inflight0b="; NETLB.INFLIGHT(h, 0)
    PRINT "load0="; NETLB.LOAD(h, 0)

    NETLB.PROBE(h, 1, 0)
    PRINT "healthy1="; NETLB.HEALTHY(h, 1)
    NETLB.PROBE(h, 1, 1)
    PRINT "healthy1b="; NETLB.HEALTHY(h, 1)

    PRINT "idle="; NETLB.IDLECOUNT(h)
    NETLB.SETSTICKY(h, 1000)
    PRINT "stickysame="; NETLB.PICK(h, "k"); "="; NETLB.PICK(h, "k")
    PRINT "stickycnt="; NETLB.STICKYCOUNT(h)

    NETLB.SETPOLICY(h, NETLB_HRW)
    NETLB.SETHRWSKEL(h, 2, 2)
    PRINT "hrwh="; NETLB.HRWHASHES(h)
    NETLB.SETPOLICY(h, NETLB_MAGLEV)
    PRINT "maglev="; NETLB.MAGLEVBUILD(h, 4099)

    NETLB.SETPOLICY(h, NETLB_RR)
    PRINT "excl="; NETLB.PICKEXCL(h, "", "0")
    PRINT "retry="; NETLB.SHOULDRETRY(h, 0, 1)

    NETLB.BACKENDREMOVE(h, 2)
    NETLB.RELEASE(h)
    PRINT "done"
END SUB
