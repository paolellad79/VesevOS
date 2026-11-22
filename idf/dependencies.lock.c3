dependencies:
  espressif/mdns:
    component_hash: b5b30022c46302eab89c1d2e3b6fb5918a627563a7462f0ee284b22b9a934967
    dependencies:
    - name: idf
      require: private
      version: '>=5.0'
    source:
      registry_url: https://components.espressif.com/
      type: service
    version: 1.14.0
  h2zero/esp-nimble-cpp:
    component_hash: 82c026539b4704ac691265542a71681af46f1d9d94439a8ad5edbc82d48c7692
    dependencies:
    - name: espressif/esp_hosted
      registry_url: https://components.espressif.com
      require: private
      rules:
      - if: target in [esp32p4]
      version: '*'
    - name: espressif/esp_wifi_remote
      registry_url: https://components.espressif.com
      require: private
      rules:
      - if: target in [esp32p4]
      version: '>=0.5.3'
    - name: idf
      require: private
      rules:
      - if: target in [esp32p4]
      version: '>=5.3.0'
    source:
      registry_url: https://components.espressif.com/
      type: service
    version: 2.6.0
  idf:
    source:
      type: idf
    version: 5.5.5
  joltwallet/littlefs:
    component_hash: 3af20a3d75c49db9dc2286fd50862c34055c9f578419ad1f2bf7ed3ed01391fe
    dependencies:
    - name: idf
      require: private
      version: '>=5.0'
    source:
      registry_url: https://components.espressif.com/
      type: service
    version: 1.22.3
direct_dependencies:
- espressif/mdns
- h2zero/esp-nimble-cpp
- idf
- joltwallet/littlefs
manifest_hash: 7a031841559c615df6fa1a228e4f263e8fefbaa0de3e4b8c07b53b5f1f094d1a
target: esp32c3
version: 2.0.0
