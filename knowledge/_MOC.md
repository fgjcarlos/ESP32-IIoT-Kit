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

## Decisiones de arquitectura
- [[001-c-vs-tinygo]] - Por que C + ESP-IDF en vez de TinyGo
- [[002-esp-now-sin-cifrado]] - 20 peers sin cifrar vs 6 cifrados
- [[003-protocolo-unificado]] - Un solo protocolo para todo el sistema

## Referencia rapida
- [[ESP32-S3-Pinout]]
- [[ESP32-C3-Pinout]]
- [[APIs-ESP-IDF-Cheatsheet]]
- [[Errores-Comunes]]
- [[Protocolo-Mensajes]]

## Proyecto
- [[Arquitectura]]
- [[Progreso]]
- [[Problemas-Resueltos]]

## Diario
(las entradas se agregan automaticamente con el template)
