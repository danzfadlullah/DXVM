cmake_minimum_required(VERSION 3.22)
project(panvklauncher C)

add_library(panvklauncher SHARED
    probe.c
)

target_compile_options(panvklauncher PRIVATE -w -O2 -fvisibility=default)
target_link_libraries(panvklauncher PRIVATE log dl)

# Vulkan capability suite (re-implemented from PanProbe's MIT tests, see NOTICE.md).
# VKTESTS_NO_MAIN drops the host script's main(): the library is driven from Kotlin.
add_library(vktests SHARED
    vktests/vktests.c
    vktests/vktests_jni.c
)
target_compile_definitions(vktests PRIVATE VKTESTS_NO_MAIN)
target_compile_options(vktests PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(vktests PRIVATE vulkan log)

# Built-in X server natives. Sources are Winlator (LGPL-2.1), see third_party/winlator/README.md.
add_library(winlator SHARED
    winlator/drawable.c
    winlator/sysvshared_memory.c
    winlator/xconnector_epoll.c
)
target_compile_options(winlator PRIVATE -O2 -Wno-unused-function -Wimplicit-function-declaration)
target_link_options(winlator PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--as-needed")
target_link_libraries(winlator PRIVATE log android jnigraphics)

add_library(VkLayer_fbread SHARED
    fbread_layer.c
)
target_compile_options(VkLayer_fbread PRIVATE -O2 -fvisibility=hidden)
target_link_options(VkLayer_fbread PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--as-needed")
target_link_libraries(VkLayer_fbread PRIVATE log)

# LD_PRELOAD shim: Android gamepad shm -> SDL virtual Xbox 360 pad for Wine's winebus.
add_library(padshim SHARED
    padshim.c
)
target_compile_options(padshim PRIVATE -O2 -fvisibility=hidden)
target_link_options(padshim PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--as-needed")
target_link_libraries(padshim PRIVATE log dl)
