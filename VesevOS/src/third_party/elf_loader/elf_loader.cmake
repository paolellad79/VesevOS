# VesevOS - elf_loader.cmake
# Il caricatore ELF (copia nostra in questa cartella) entra nel firmware di serie sull'ESP32-S3; sugli altri chip no.
# Per toglierlo: idf.py -DVOS_WITH_ELF=0 build (nessun file del caricatore entra e il glob di idf/main/CMakeLists.txt lo salta).
# Il servizio runtime "elf" e comunque spento di fabbrica.
# Si usa in due tempi dal CMakeLists del componente main:
#   include(.../elf_loader.cmake)  PRIMA di idf_component_register: toglie dal glob i file del caricatore e, se acceso, aggiunge quelli giusti
#   vos_elf_options()              DOPO idf_component_register: opzioni di compilazione (le CONFIG_ELF_* che il caricatore trovava in Kconfig)
# Il componente esp_mm (esp_cache.h) sta sempre nei REQUIRES di main: i REQUIRES si leggono in un passo a parte dove -DVOS_WITH_ELF non arriva.
# Per ora solo ESP32-S3 (codice e dati in PSRAM). Altri chip: vedi docs/progetto/piano-integrazione-elf.md, sezione 6.
set(VOS_ELF_DIR ${CMAKE_CURRENT_LIST_DIR})

if(NOT DEFINED VOS_WITH_ELF)
  if(IDF_TARGET STREQUAL "esp32s3")
    set(VOS_WITH_ELF 1)
  else()
    set(VOS_WITH_ELF 0)
  endif()
endif()

# Il glob del chiamante ha preso tutti i .c della cartella (anche quelli che non servono): si tolgono sempre.
list(FILTER VOS_SRCS EXCLUDE REGEX "/third_party/elf_loader/")

if(VOS_WITH_ELF)
  if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "VOS_WITH_ELF: per ora solo ESP32-S3 (chip scelto: ${IDF_TARGET})")
  endif()
  list(APPEND VOS_SRCS
       ${VOS_ELF_DIR}/src/esp_elf.c
       ${VOS_ELF_DIR}/src/esp_elf_symbol.c
       ${VOS_ELF_DIR}/src/esp_elf_validate.c
       ${VOS_ELF_DIR}/src/esp_elf_adapter.c
       ${VOS_ELF_DIR}/src/arch/esp_elf_xtensa.c)
endif()

function(vos_elf_options)
  if(NOT VOS_WITH_ELF)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE VOS_WITH_ELF=0)   # il codice non deve ricadere sul valore di serie dell'S3
    return()
  endif()
  # Valori che il caricatore leggeva da Kconfig. Vanno dati a tutto il componente: alcuni cambiano anche la struttura esp_elf_t
  # che usa il nostro driver (drivers/vos_drv_elf.cpp). Tabelle dei simboli di libc e IDF spente: i simboli li da il nostro risolutore.
  target_compile_definitions(${COMPONENT_LIB} PRIVATE
      VOS_WITH_ELF=1
      CONFIG_ELF_LOADER=1
      CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1
      CONFIG_ELF_LOADER_CACHE_OFFSET=1
      CONFIG_ELF_LOADER_LOAD_PSRAM=1
      CONFIG_ELF_LOADER_NUMBER_SYMBOLS=4
      ELF_LOADER_VER_MAJOR=2 ELF_LOADER_VER_MINOR=0 ELF_LOADER_VER_PATCH=0)   # versione del caricatore copiato (in origine la dava il suo CMakeLists)
endfunction()
