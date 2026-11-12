' EXPECT: 474d2d4241534943|900150983cd24fb0d6963f7d28e17f72|ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
' MiniTest ENCODING: TOHEX e digest del pacchetto `crypto` (namespace CRYPTOGRAPHY FFI).
' La forma a 1 argomento che ritornava un ARRAY (FROMHEX/MD5 su array) è stata ritirata con
' la migrazione del namespace: qui si usano le funzioni canoniche su STRING.
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
SUB Main()
  PRINT TOHEX("GM-BASIC")
  PRINT MD5("abc")
  PRINT SHA256("abc")
END SUB
