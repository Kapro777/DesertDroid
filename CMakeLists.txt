cmake_minimum_required(VERSION 3.20)
project(DesertDroid C)

# pcrecomp com os 14 patches (patches/pcrecomp/) aplicados no commit 35548b8
set(PCRECOMP "${CMAKE_SOURCE_DIR}/../external/pcrecomp" CACHE PATH
    "checkout do sp00nznet/pcrecomp com os patches aplicados")
# C gerado pelo lift — derivado do jogo do usuario, NUNCA commitado
set(GEN_DIR "${CMAKE_SOURCE_DIR}/../work/gen" CACHE PATH
    "saida do run_lift.py lift (gitignored)")

file(GLOB GEN_SOURCES CONFIGURE_DEPENDS "${GEN_DIR}/recomp_*.c")
if(NOT GEN_SOURCES)
  message(FATAL_ERROR "Sem C gerado em ${GEN_DIR} — rode o pipeline lift primeiro")
endif()

set(RT "${PCRECOMP}/runtime")
set(COMMON_DEFS RECOMP_FLAT_MEMORY RECOMP_STRICT_ICALL RECOMP_TRACE)
set(COMMON_INC "${RT}/recomp32" "${RT}/win32hle"
               "${CMAKE_SOURCE_DIR}/src/host" "${GEN_DIR}")

# ---- codigo gerado: o jogo inteiro recompilado em C ----
add_library(dd_gen OBJECT ${GEN_SOURCES})
target_compile_definitions(dd_gen PRIVATE ${COMMON_DEFS})
target_include_directories(dd_gen PRIVATE ${COMMON_INC})
# TUs gigantes: sem warnings, otimizacao leve; -fcommon: codigo traduzido de
# binario antigo pode ter globais tentativos duplicados entre TUs (gcc>=10
# usa -fno-common por padrao)
target_compile_options(dd_gen PRIVATE -w -O1 -fno-strict-aliasing -fcommon)

# ---- host DesertDroid + runtime portavel (win32hle) ----
add_executable(DesertDroid
  src/host/main.c
  src/host/dd_runtime.c
  src/host/dd_shims.c
  src/host/dd_kernel32.c
  src/host/dd_sync.c
  src/host/dd_locale.c
  src/host/dd_fs.c
  src/host/dd_d3d9.c
  ${RT}/win32hle/guest_mem.c
  ${RT}/win32hle/host_lite.c
  ${RT}/win32hle/kernel32.c
  ${RT}/win32hle/kernel32_crt.c
  ${RT}/win32hle/kernel32_ext.c
  ${RT}/win32hle/user32.c
  ${RT}/win32hle/gdi32.c
  ${RT}/win32hle/pe_loader.c
  ${RT}/win32hle/host.c
  $<TARGET_OBJECTS:dd_gen>)
target_compile_definitions(DesertDroid PRIVATE ${COMMON_DEFS})
target_include_directories(DesertDroid PRIVATE ${COMMON_INC})
target_compile_options(DesertDroid PRIVATE -O2 -Wall -Wno-unused-but-set-variable)
find_package(Threads REQUIRED)
target_link_libraries(DesertDroid PRIVATE Threads::Threads m)
