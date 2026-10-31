cmake_minimum_required(VERSION 3.10)
project(gmb_chart C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Motore chart AUTONOMO e riusabile: pura libc/math, NESSUNA dipendenza da
# gmb_runtime/SDL/GMBasic. Il disegno passa per gmb_chart_renderer_t (function table)
# fornita dall'host. Linkabile da GMBasic (via adapter) e da app C esterne.
set(GMB_CHART_SOURCES
    src/gmb_chart_core.c
    src/gmb_chart_draw.c
)

add_library(gmb_chart STATIC ${GMB_CHART_SOURCES})
target_include_directories(gmb_chart PUBLIC include)
target_link_libraries(gmb_chart PUBLIC m)

add_library(gmb_chart_shared SHARED ${GMB_CHART_SOURCES})
target_include_directories(gmb_chart_shared PUBLIC include)
target_link_libraries(gmb_chart_shared PUBLIC m)
