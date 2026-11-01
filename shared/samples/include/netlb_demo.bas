' ============================================================================
'  netlb_demo.bas — uso COMPLESSO di netlb (load balancing) da GM-BASIC via
'  package + FFI. Eseguibile sulle 5 gambe:
'    tools/parity5 samples/includes/netlb_demo.bas
' ============================================================================
'
'  REQUIRE "netlb"   +   IMPORT NETLB WITH ALIAS   (perche' servono)
'  ---------------------------------------------------------------------------
'  REQUIRE attiva un PACCHETTO del package manager di GM-BASIC. Per "netlb":
'    - risolve/installa il pacchetto (se manca lo scarica dal registry in ~/.gmb/packages);
'    - include automaticamente l'include FFI del pacchetto (netlb.inc), che dichiara
'      le funzioni native LB (DECLARE ... LIB "netlb");
'    - fornisce la libreria nativa (libnetlb, C99 self-contained) e i flag di link
'      (-I/-L/-l) risolti dal manifest (ffi.headers/ffi.libs).
'  Senza REQUIRE bisognerebbe scrivere a mano le DECLARE e linkare la libreria.
'
'  Modello: la lib e' PURA (nessun thread/socket). L'host (qui lo script) possiede
'  I/O e tempo e PASSA gli eventi: PICK -> BEGINREQ -> ... -> ENDREQ(ok, lat_ms).
' ---------------------------------------------------------------------------

REQUIRE "netlb"
IMPORT NETLB WITH ALIAS          ' namespace: consente NETLB.<Membro>(...)

' ----------------------------------------------------------------------------
'  1) Pool di backend con policy P2C (Power of Two Choices + EWMA).
'     POOLNEW(policy, seed) -> handle LONG; BACKENDADD(h, nome, peso_milli, cap).
' ----------------------------------------------------------------------------
DIM h AS LONG
DIM i AS LONG
DIM id AS LONG
DIM a AS LONG
DIM b AS LONG
DIM c AS LONG
DIM n AS LONG

h = NETLB.POOLNEW(NETLB_P2C, 12345)
NETLB.BACKENDADD(h, "srv-a", 2000, 0)      ' peso 2.0
NETLB.BACKENDADD(h, "srv-b", 1000, 0)      ' peso 1.0
NETLB.BACKENDADD(h, "srv-c", 1000, 0)      ' peso 1.0
PRINT "1) backend="; NETLB.BACKENDCOUNT(h); " ready="; NETLB.READY(h)

' ----------------------------------------------------------------------------
'  2) 300 richieste simulate: PICK -> BEGINREQ -> ENDREQ(latenza).
'     srv-a e' piu' veloce (5 ms) degli altri (20 ms): P2C+EWMA lo preferisce.
' ----------------------------------------------------------------------------
a = 0
b = 0
c = 0
FOR i = 1 TO 300
  id = NETLB.PICK(h, "")
  NETLB.BEGINREQ(h, id)
  IF id = 0 THEN
    NETLB.ENDREQ(h, id, 1, 5)
    a = a + 1
  ELSEIF id = 1 THEN
    NETLB.ENDREQ(h, id, 1, 20)
    b = b + 1
  ELSE
    NETLB.ENDREQ(h, id, 1, 20)
    c = c + 1
  END IF
NEXT i
PRINT "2) P2C distribuzione: a="; a; " b="; b; " c="; c
PRINT "   latenza EWMA (ms): a="; NETLB.EWMA(h, 0); " b="; NETLB.EWMA(h, 1)

' ----------------------------------------------------------------------------
'  3) Salute: guasto su srv-b (3 fallimenti consecutivi) -> eiezione passiva.
'     PROBE(h, id, ok) per il rientro attivo.
' ----------------------------------------------------------------------------
FOR i = 1 TO 3
  NETLB.BEGINREQ(h, 1)
  NETLB.ENDREQ(h, 1, 0, 10)
NEXT i
PRINT "3) srv-b sano? "; NETLB.HEALTHY(h, 1); " (0 = eiettato)"
NETLB.PROBE(h, 1, 1)                        ' rientro
PRINT "   dopo probe, srv-b sano? "; NETLB.HEALTHY(h, 1)

' ----------------------------------------------------------------------------
'  4) Affinita' (consistent hash): la stessa key -> sempre lo stesso backend.
' ----------------------------------------------------------------------------
NETLB.SETPOLICY(h, NETLB_CHASH)
DIM k1 AS LONG
DIM k2 AS LONG
k1 = NETLB.PICK(h, "user:42")
k2 = NETLB.PICK(h, "user:42")
PRINT "4) chash: "; k1; " == "; k2; " ? "; (k1 = k2)

' ----------------------------------------------------------------------------
'  5) JIQ (Join-Idle-Queue): pool fresco, il backend occupato viene saltato.
' ----------------------------------------------------------------------------
DIM hj AS LONG
DIM pick1 AS LONG
DIM pick2 AS LONG
hj = NETLB.POOLNEW(NETLB_JIQ, 1)
NETLB.BACKENDADD(hj, "a", 1000, 0)
NETLB.BACKENDADD(hj, "b", 1000, 0)
pick1 = NETLB.PICK(hj, "")                  ' a (idle)
NETLB.BEGINREQ(hj, pick1)                   ' a ora occupato
pick2 = NETLB.PICK(hj, "")                  ' b (l'unico idle)
PRINT "5) JIQ idle-first: j1="; pick1; " j2="; pick2; " idle="; NETLB.IDLECOUNT(hj)
NETLB.RELEASE(hj)

' ----------------------------------------------------------------------------
'  6) Sticky soft: lo stesso cookie resta sullo stesso backend (TTL).
' ----------------------------------------------------------------------------
DIM hs AS LONG
DIM s1 AS LONG
DIM s2 AS LONG
hs = NETLB.POOLNEW(NETLB_RR, 7)
NETLB.BACKENDADD(hs, "a", 1000, 0)
NETLB.BACKENDADD(hs, "b", 1000, 0)
NETLB.SETSTICKY(hs, 60000)                  ' TTL 60 s
s1 = NETLB.PICK(hs, "sess:1")
s2 = NETLB.PICK(hs, "sess:1")               ' stesso backend
PRINT "6) sticky soft: sess1="; s1; "=="; s2; " voci="; NETLB.STICKYCOUNT(hs)
NETLB.RELEASE(hs)

' ----------------------------------------------------------------------------
'  7) Draining: BACKENDREMOVE toglie dai pick (le richieste in corso restano).
' ----------------------------------------------------------------------------
NETLB.BACKENDREMOVE(h, 2)
DIM only_ok AS LONG
only_ok = 1
FOR i = 1 TO 50
  id = NETLB.PICK(h, "")
  IF id = 2 THEN only_ok = 0
NEXT i
PRINT "7) draining srv-c: nessun pick su c = "; only_ok

' ----------------------------------------------------------------------------
'  8) Retry con esclusione: pool fresco, PICKEXCL evita i backend gia' tentati.
'     SHOULDRETRY(h, id, attempt) applica il budget di retry.
' ----------------------------------------------------------------------------
DIM h2 AS LONG
DIM r2 AS LONG
DIM r3 AS LONG
h2 = NETLB.POOLNEW(NETLB_RR, 7)
NETLB.BACKENDADD(h2, "a", 1000, 0)
NETLB.BACKENDADD(h2, "b", 1000, 0)
NETLB.BACKENDADD(h2, "c", 1000, 0)
r2 = NETLB.PICKEXCL(h2, "", "0")            ' escludo il backend 0 -> deve uscire 1
r3 = NETLB.SHOULDRETRY(h2, 0, 1)            ' budget di retry: 1 = conviene ritentare
PRINT "8) retry: pick_excl(no 0)="; r2; " should_retry="; r3
NETLB.RELEASE(h2)

' ----------------------------------------------------------------------------
'  9) Config + metriche + affinity (membri rimanenti): AVAILABLE, SETWEIGHT,
'     SETCAP, SLOWSTART, RETRYBUDGET, SETNOW, INFLIGHT, LOAD, PICKCOUNT,
'     MAGLEVBUILD, SETHRWSKEL, HRWHASHES.
' ----------------------------------------------------------------------------
DIM hm AS LONG
hm = NETLB.POOLNEW(NETLB_WRR, 3)
PRINT "9) available="; NETLB.AVAILABLE()
NETLB.BACKENDADD(hm, "x", 1000, 0)
NETLB.BACKENDADD(hm, "y", 1000, 0)
NETLB.SETWEIGHT(hm, 0, 3000)                ' x: peso 3.0
NETLB.SETCAP(hm, 0, 4)                      ' x: max 4 richieste in corso
NETLB.SLOWSTART(hm, 1000)
NETLB.RETRYBUDGET(hm, 250)
NETLB.SETNOW(hm, 5000)
id = NETLB.PICK(hm, "")                     ' WRR: x (peso 3)
NETLB.BEGINREQ(hm, id)
PRINT "   inflight0="; NETLB.INFLIGHT(hm, 0); " load0="; NETLB.LOAD(hm, 0); " picks0="; NETLB.PICKCOUNT(hm, 0)
NETLB.ENDREQ(hm, id, 1, 7)
NETLB.SETPOLICY(hm, NETLB_MAGLEV)
PRINT "   maglev="; NETLB.MAGLEVBUILD(hm, 4099)
NETLB.SETPOLICY(hm, NETLB_HRW)
NETLB.SETHRWSKEL(hm, 4, 4)
id = NETLB.PICK(hm, "user:1")
PRINT "   hrwhash="; NETLB.HRWHASHES(hm)
NETLB.RELEASE(hm)

' ----------------------------------------------------------------------------
' 10) Chiusura.
' ----------------------------------------------------------------------------
NETLB.RELEASE(h)
PRINT "netlb_demo: completato"
