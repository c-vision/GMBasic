' mongo.inc — pacchetto `mongo` (namespace MONGO), shim FFI `libgmbmongo`.
NAMESPACE MONGO

PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbmongo" ALIAS "gmbmongo_available" () AS LONG
PUBLIC DECLARE FUNCTION CONNECT LIB "gmbmongo" ALIAS "gmbmongo_connect" (url AS STRING) AS LONG
PUBLIC DECLARE FUNCTION PING LIB "gmbmongo" ALIAS "gmbmongo_ping" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION CLOSE LIB "gmbmongo" ALIAS "gmbmongo_close" (h AS LONG) AS LONG
