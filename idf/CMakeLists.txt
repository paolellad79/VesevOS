# VesevOS - progetto ESP-IDF 5.5 (fase 5: senza Arduino come componente; la configurazione e nostra).
# I sorgenti sono quelli del repo (../repo/VesevOS), non una copia.
cmake_minimum_required(VERSION 3.16)
# configurazione: VOS_CFG=vesevos (la nostra, di fabbrica) | arduino (copia di quella del core Arduino, solo per confronti) | qemu (prova in emulazione)
if(NOT DEFINED ENV{VOS_CFG})
  set(ENV{VOS_CFG} vesevos)
endif()
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults.$ENV{VOS_CFG}")
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# File lock dei componenti: uno per scheda RISC-V, cosi cambiare chip non riscrive quello dell'S3 (che resta dependencies.lock).
if("$ENV{VOS_CFG}" MATCHES "^(c3|c6|h2)$")
  idf_build_set_property(DEPENDENCIES_LOCK "dependencies.lock.$ENV{VOS_CFG}")
endif()
# Versione scritta nel descrittore dell'app: la stessa di VOS_VERSION (la legge anche il recovery, riga "Firmware attuale").
# Senza questa riga il descrittore dice sempre "1", che non e la versione vera (20/11/2026).
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../repo/VesevOS/src/core/vos_common.h" VOS_RIGA_VERSIONE REGEX "^#define VOS_VERSION ")
string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" VOS_VERSIONE_APP "${VOS_RIGA_VERSIONE}")
set(PROJECT_VER "${VOS_VERSIONE_APP}")
project(VesevOS)
# Correzione per ESP-IDF 5.5.5: con l'opzione 15 del DHCP (drivers/vos_drv_dhcp.cpp) il componente lwip non compila senza "struct dhcp;" dichiarata prima (vedi lwip_dhcp_fix.h).
idf_component_get_property(vos_lwip_lib lwip COMPONENT_LIB)
target_compile_options(${vos_lwip_lib} PRIVATE -include ${CMAKE_CURRENT_LIST_DIR}/lwip_dhcp_fix.h)
