# gmbadmin — CLI amministrativo in C (backend dei sottocomandi admin di `gmb`).
#
# Tutto su runtime C (pkg/fixtures/brain/grep/doc): il mondo C# resta solo per i comandi
# non ancora portati (F9c), che qui rispondono con un errore esplicito.
cmake_minimum_required(VERSION 3.10)
project(gmbadmin C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(../executor executor)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_agent_runtime gmb_agent_runtime_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../gmb_doc_c gmb_doc_build)

add_executable(gmbadmin
    gmbadmin.c
    gmbadminpkg.c
    gmbadminpkg2.c
    gmbadminrag.c
    gmbadmindaemon.c
    gmbadminfetch.c
    gmbadminbrain.c
    gmbadminsys.c
)

target_link_libraries(gmbadmin PRIVATE gmb_runtime gmb_agent_runtime gmb_pkg gmb_doc gmb_boot)
target_include_directories(gmbadmin PRIVATE ../gmb_runtime_c/include ../gmb_doc_c/include)

# Un tool appena compilato deve usare le librerie APPENA compilate (gmb_pkg/gmb_runtime),
# non quelle scaricate in ~/.gmbasic/libs/<rid>/ (cache on-demand, possibilmente vecchia:
# un simbolo nuovo farebbe fallire dyld). BUILD_RPATH entra PRIMA dell'rpath di cache.
set_target_properties(gmbadmin PROPERTIES BUILD_WITH_INSTALL_RPATH OFF)
# -Wl,-rpath esplicito: l'ordine dei flag decide l'ordine di LC_RPATH, quindi la lib del
# BUILD vince sulla cache on-demand.
target_link_options(gmbadmin PRIVATE
    "-Wl,-rpath,$<TARGET_FILE_DIR:gmb_pkg>")
