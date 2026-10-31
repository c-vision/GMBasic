' FILE: Preprocessor/MultiTarget.bas
' ====================================================================================
' MULTI-TARGET PRECOMPILER — #IFDEF / #IF con costanti piattaforma
' ====================================================================================
' Costanti predefinite disponibili:
'   OS:      WINDOWS | LINUX | MACOS  (es. #IFDEF WINDOWS)
'   Arch:    X64 | ARM64 | X86 | ARM   (es. #IFDEF ARM64)
'   Build:   DEBUG | RELEASE
'   Espressioni: #IF LINUX OR MACOS THEN, #IF X64 AND RELEASE THEN
' ====================================================================================

' Test 1: #IFDEF per piattaforma
#IFDEF WINDOWS
  PRINT "Piattaforma: WINDOWS"
#ENDIF
#IFDEF LINUX
  PRINT "Piattaforma: LINUX"
#ENDIF
#IFDEF MACOS
  PRINT "Piattaforma: MACOS"
#ENDIF

' Test 2: #IF con espressioni booleane
#IF WINDOWS OR LINUX THEN
  PRINT "Sistema desktop (Windows o Linux)"
#ENDIF
#IF MACOS OR LINUX THEN
  PRINT "Sistema Unix-like (macOS o Linux)"
#ENDIF

' Test 3: #IFDEF per architettura
#IFDEF X64
  PRINT "Architettura: x64"
#ENDIF
#IFDEF ARM64
  PRINT "Architettura: ARM64"
#ENDIF

' Test 4: #IF combinato OS + architettura
#IF MACOS AND ARM64 THEN
  PRINT "Apple Silicon (macOS su ARM64)"
#ENDIF

' Test 5: #IFDEF per build type
#IFDEF DEBUG
  PRINT "Build: DEBUG"
#ENDIF
#IFDEF RELEASE
  PRINT "Build: RELEASE"
#ENDIF

' Test 6: #IF / #ELSEIF / #ELSE
#IFDEF WINDOWS
  PRINT "OS = Windows"
#ELSEIF MACOS
  PRINT "OS = macOS"
#ELSE
  PRINT "OS = Linux/Altro"
#ENDIF

PRINT "--- FINE TEST MULTI-TARGET ---"
END
