# Tutti i sorgenti del firmware: .cpp e .c (funzioni VesevOS). Il punto di ingresso (app_main) sta in src/vos_main.cpp.
# Fase 5 (22/10/2026): niente piu Arduino come componente; il vecchio VesevOS.ino non si compila piu (e solo un errore chiaro).
set(VOS ${CMAKE_CURRENT_LIST_DIR}/../../repo/VesevOS)
file(GLOB_RECURSE VOS_SRCS ${VOS}/src/*.cpp ${VOS}/src/*.c)
file(GLOB_RECURSE VOS_DIRS LIST_DIRECTORIES true ${VOS}/src/*)
# Moduli ELF: dentro il firmware sull'ESP32-S3 (servizio runtime spento di fabbrica). Per toglierli: idf.py -DVOS_WITH_ELF=0 build (vedi elf_loader.cmake).
include(${VOS}/src/third_party/elf_loader/elf_loader.cmake)
set(VOS_INC ${VOS})
foreach(d ${VOS_DIRS})
  if(IS_DIRECTORY ${d})
    list(APPEND VOS_INC ${d})
  endif()
endforeach()
# Componenti per chip, secondo le capacita del chip lette da soc_caps.h (la stessa fonte di ESP-IDF; qui non c'e ancora sdkconfig,
# quindi non si possono usare le CONFIG_*). BLE: SOC_BLE_SUPPORTED. Wi-Fi (MQTT, ESP-NOW): SOC_WIFI_SUPPORTED (l'H2 non lo ha).
# Gli interruttori VOS_WITH_* li decide il file di scheda (VOS_HAS_BLE, VOS_HAS_WIFI) in boards/vos_packages.h: nessun -D qui.
function(vos_chip_cap nome risultato)
  set(${risultato} 0 PARENT_SCOPE)
  set(caps ${IDF_PATH}/components/soc/${IDF_TARGET}/include/soc/soc_caps.h)
  if(EXISTS ${caps})
    file(STRINGS ${caps} riga REGEX "^#define[ \t]+${nome}[ \t]+\\(?1\\)?[ \t]*(/.*)?$")
    if(riga)
      set(${risultato} 1 PARENT_SCOPE)
    endif()
  endif()
endfunction()
vos_chip_cap(SOC_BLE_SUPPORTED VOS_CAP_BLE)
vos_chip_cap(SOC_WIFI_SUPPORTED VOS_CAP_WIFI)
set(VOS_REQ_CHIP)
# Bluetooth in PAUSA su tutte le schede (decisione dell'utente, 19/11/2026): NimBLE e il componente bt non entrano nella compilazione e il
# codice usa VOS_WITH_BLE=0 (package spento, come sulle schede senza Bluetooth). Per riaccenderlo: idf.py -DVOS_BLE_PAUSA=0 build.
# Le righe CONFIG_BT_* dei sdkconfig.defaults restano com'erano: non servono finche la pausa e attiva.
if(NOT DEFINED VOS_BLE_PAUSA)
  set(VOS_BLE_PAUSA 1)
endif()
if(VOS_CAP_BLE AND NOT VOS_BLE_PAUSA)
  list(APPEND VOS_REQ_CHIP esp-nimble-cpp bt)
endif()
if(VOS_CAP_WIFI)
  list(APPEND VOS_REQ_CHIP mqtt)
endif()
# Solo l'S3 (Xtensa con PSRAM) ha questi due.
if(IDF_TARGET STREQUAL "esp32s3")
  list(APPEND VOS_REQ_CHIP esp_psram xtensa ulp)
endif()
# Componenti comuni a tutte le schede (stessa lista della 1.8.22: era sparita con la modifica del 13/11).
set(VOS_REQ_COMUNI PsychicHttp aj_json mbedtls esp_https_server esp_http_server esp_http_client app_update esp_partition nvs_flash
                   esp_wifi esp_netif esp_event esp_timer lwip wpa_supplicant efuse esp_adc driver esp_driver_gpio esp_driver_tsens
                   esp_driver_i2c esp_driver_rmt esp_driver_uart esp_driver_usb_serial_jtag esp_driver_spi esp_driver_sdspi
                   spi_flash bootloader_support mdns joltwallet__littlefs fatfs sdmmc vfs
                   esp_system esp_hw_support esp_pm freertos heap log esp_mm)
idf_component_register(SRCS ${VOS_SRCS}
                       INCLUDE_DIRS ${VOS_INC}
                       REQUIRES ${VOS_REQ_COMUNI} ${VOS_REQ_CHIP})
vos_elf_options()
# Coprocessore ULP-RISC-V (solo S3): il programma FISSO delle soglie sta in repo/VesevOS/ulp/, FUORI da src/ apposta, cosi il glob dei sorgenti
# sopra non lo prende e non si compila con il resto (gira sul coprocessore, con la toolchain RISC-V). ulp_embed_binary lo compila, lo incorpora
# nel firmware (simboli _binary_ulp_vos_soglie_bin_start/_end) e genera ulp_vos_soglie.h (variabile ulp_tabella). Il file della CPU che lo usa e drivers/vos_drv_ulp.cpp.
if(IDF_TARGET STREQUAL "esp32s3")
  set(ulp_app_name ulp_vos_soglie)
  ulp_embed_binary(${ulp_app_name} "${VOS}/ulp/vos_ulp_soglie.c" "${VOS}/src/drivers/vos_drv_ulp.cpp")
endif()
if(VOS_BLE_PAUSA)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE VOS_WITH_BLE=0)   # Bluetooth in pausa: package spento in tutto il codice
endif()
# Avvisi: sul NOSTRO codice restano quelli standard di ESP-IDF (-Wall -Wextra, mai -Werror).
# Le terze parti (funzioni VesevOS in third_party/) non sono nostre: li si spegne solo per quei file con -w.
# PsychicHttp e ArduinoJson sono componenti a parte e hanno gia il loro -w.
file(GLOB_RECURSE VOS_THIRD_PARTY ${VOS}/src/third_party/*.c ${VOS}/src/third_party/*.cpp)
set_source_files_properties(${VOS_THIRD_PARTY} PROPERTIES COMPILE_OPTIONS "-w")
# ESP-IDF di fabbrica mette -Werror=all (ogni avviso ferma la build). Lo trasformiamo in -Wall per tutti:
# gli avvisi si mostrano ma non fermano la compilazione.
# -Wsign-compare e spento di fabbrica da ESP-IDF: qui lo riaccendiamo perche utile per trovare bug.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wsign-compare)
idf_build_replace_option_from_property(COMPILE_OPTIONS "-Werror=all" "-Wall")
# Gancio del panico (drivers/vos_drv_panic.cpp): il linker manda ogni chiamata a esp_panic_handler a __wrap_esp_panic_handler,
# che copia motivo e traccia, chiama il nostro gancio e poi il gestore vero (__real_esp_panic_handler).
# Era l'opzione del core Arduino; qui va detta noi. Il -u tiene il file nel link anche se nessuno lo chiama per nome.
idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=esp_panic_handler" APPEND)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,__wrap_esp_panic_handler")
