' GmbModuleCall.bas — GMB MODULE CALL: invocare un export di una libreria esterna.
' Registra una dylib su uno slot e ne chiama un simbolo con firma "simbolo|sig|args"
' (sig: prima lettera = ritorno, poi i parametri; 'd'=double, 'i'=int64).
GMB MODULE REGISTER "sys", "/usr/lib/libSystem.B.dylib"
GMB MODULE CALL "sys", "llabs|ii|-7"
GMB MODULE CALL "sys", "sqrt|dd|2"
