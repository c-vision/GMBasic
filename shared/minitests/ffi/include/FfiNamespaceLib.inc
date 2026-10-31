' FILE: samples/FFI/FfiNamespaceLib.inc — libreria FFI namespace
' Contiene le dichiarazioni DECLARE ... LIB dentro NAMESPACE.
' Include via IMPORT FfiNamespaceLib  oppure  IMPORT FfiNamespaceLib AS FFL.

NAMESPACE FfiNamespaceLib

' DOUBLE — contratto all-double (firma omogenea)
PUBLIC DECLARE FUNCTION vec_scale LIB "./NativeVec/libvec_module" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE

' ALIAS — il nome BASIC diverso dal simbolo C
PUBLIC DECLARE FUNCTION ns_add LIB "./NativeVec/libvec_module" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ns_prime LIB "./NativeVec/libvec_module" ALIAS "vec_isprime" (n AS LONG) AS LONG

' SUB void + FUNCTION long (stato condiviso: hit counter)
PUBLIC DECLARE SUB vec_bump LIB "./NativeVec/libvec_module" (v AS DOUBLE)
PUBLIC DECLARE FUNCTION vec_tick LIB "./NativeVec/libvec_module" () AS LONG