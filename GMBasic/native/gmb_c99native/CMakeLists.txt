# Back-end C99NATIVE: .bas -> sorgente C (gmb_c99_emit*). L'FFI e' emessa come binding
# dinamico (gmb_ffi_dl_open/sym) dentro il C generato; per l'eseguibile serve un `cc`.
file(GLOB GMB_C99_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_c99_emit*.c")

add_library(gmb_c99native OBJECT ${GMB_C99_SRC})
target_include_directories(gmb_c99native PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(gmb_c99native PUBLIC gmb_frontend)
