# PsychicHttp 3.1.2 (MIT) compilato SENZA Arduino: copia nostra in src/ (vedi NOTICE-PSYCHIC.txt).
# La copia usa il ramo nativo ESP-IDF (std::string, POSIX) scegliendo con PSYCHIC_ARDUINO (mai definito qui).
file(GLOB PSY_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
idf_component_register(SRCS ${PSY_SRCS} INCLUDE_DIRS src
                       REQUIRES esp_https_server esp_http_server esp_netif esp_timer esp_rom mbedtls aj_json)
# ArduinoJson vede ARDUINO (lo definisce ancora il core) e includerebbe Arduino.h: si spegne il supporto a String/Stream/Print di Arduino
target_compile_definitions(${COMPONENT_LIB} PUBLIC -DESP32
                           -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0
                           -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-rtti -w)
