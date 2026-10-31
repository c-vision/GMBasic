# Back-end BYTECODE: .bas -> chunk A1/GMBC eseguito dalla VM/C (native/gmb_vm_c).
# NB: questo back-end C non emette (ancora) le chiamate FFI — vedi Todo (open) FFI bytecode.
add_library(gmb_bytecode OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/src/gmb_compiler.c")
target_include_directories(gmb_bytecode PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(gmb_bytecode PUBLIC gmb_frontend)
