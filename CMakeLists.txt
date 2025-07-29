cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ot_template)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_sources(app PRIVATE
    src/main.c
    lib/thread_utils.c
    lib/mqtt_utils.c
    lib/sht75.c
    lib/shell_utils.c
	lib/ds18b20.c
)