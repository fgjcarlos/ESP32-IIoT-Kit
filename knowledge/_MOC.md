# Map of Content — ESP32-IIoT-Kit

## Conceptos

### Hardware
- [[GPIO]] - Pines de entrada/salida digital
- [[ADC]] - Conversion analogico-digital
- [[I2C]] - Bus de comunicacion serie (2 cables)
- [[SPI]] - Bus de comunicacion rapido (4 cables)
- [[Deep-Sleep]] - Modo de bajo consumo extremo

### Framework
- [[ESP-IDF]] - Framework de desarrollo de Espressif
- [[FreeRTOS]] - Sistema operativo en tiempo real
- [[CMake-ESP-IDF]] - Sistema de build
- [[Kconfig]] - Sistema de configuracion (menuconfig)
- [[NVS]] - Almacenamiento no volatil en flash

### Comunicacion
- [[ESP-NOW]] - Protocolo P2P de Espressif
- [[WiFi-Modos]] - STA, AP, APSTA
- [[MQTT]] - Protocolo de mensajeria IoT

### Sistema
- [[OTA]] - Actualizaciones over-the-air
- [[Watchdog]] - Temporizador de vigilancia
- [[White-label]] - Perfiles de marca sin bifurcar el firmware

### Plataforma embebida del gateway
- [[Preact]] - SPA ligera servida desde SPIFFS
- [[REST-API]] - API de datos y configuracion
- [[WebSocket]] - Push de datos en tiempo real

## Decisiones de arquitectura
- [[001-c-vs-tinygo]] - Por que C + ESP-IDF en vez de TinyGo
- [[002-esp-now-sin-cifrado]] - 20 peers sin cifrar vs 6 cifrados
- [[003-protocolo-unificado]] - Un solo protocolo para todo el sistema
- [[004-iiot-kit-transformation]] - Transformacion a plataforma IIoT generica
- [[005-ota-manifiesto-firmado]] - Manifiesto de publicacion firmado para el OTA

## Referencia rapida
- [[Errores-Comunes]]
- [[Protocolo-Mensajes]]
- `ESP32-S3-Pinout` (pendiente de crear; el perfil vigente vive en `docs/hardware/esp32-s3-wroom-1-n16r8.md`)
- `ESP32-C3-Pinout` (pendiente de crear)
- `APIs-ESP-IDF-Cheatsheet` (pendiente de crear)

## Proyecto
- [[Arquitectura]]
- [[Progreso]]
- `Problemas-Resueltos` (pendiente de crear)

## Diario
(las entradas se agregan automaticamente con el template)
