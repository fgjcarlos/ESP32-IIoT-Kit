# ESP32-IIoT-Kit

Plataforma generica de monitorizacion industrial construida con ESP32.

## Que es este proyecto

Un kit educativo y de referencia para construir sistemas de monitorizacion IIoT con microcontroladores ESP32. El sistema recoge variables del entorno (temperatura, pH, humedad, presion, nivel u otras) y las transmite de forma inalambrica a un gateway central que las procesa y sirve en un dashboard embebido.

**Este es un proyecto educativo** — el objetivo principal es aprender desarrollo de sistemas embebidos con ESP-IDF, comunicaciones inalambricas, protocolos IoT y programacion de interfaces web ligeras.

> ¿Buscas un caso de uso especifico? Consulta los ejemplos en `examples/fish-farm/` para ver una implementacion de monitorizacion de piscifactoria sobre esta misma plataforma.

## Arquitectura

```
┌──────────────────────────────────────────────────────┐
│        INTEGRACION EXTERNA (LAN / Cloud)              │
│                    — opcional —                      │
│   Broker MQTT (Mosquitto, HiveMQ, AWS IoT, ...)     │
│   Broker MQTT <-> API REST + historico               │
└───────┬──────────────────────────────────────────────┘
        │ MQTT opcional (WiFi STA)
┌───────▼────────┐
│    GATEWAY      │ <-- WiFi AP + Dashboard Preact embebido
│    ESP32-S3     │     C (ESP-IDF v5.x) + SPIFFS
└───────┬────────┘
        │ ESP-NOW (2.4GHz)
   ┌────┼──────────┐
Nodo A  Nodo B   Nodo C
ESP32-C3 (Deep Sleep + ESP-NOW)
```

El gateway es **autonomo**: el navegador se conecta a su WiFi AP y carga la SPA Preact desde SPIFFS; los datos llegan por WebSocket y por la API REST. MQTT es una integracion opcional para cloud o LAN: el kit no necesita ningun servidor externo para funcionar.

## Empezar

1. **[Tutorial](tutorial/README.md)** — Conceptos teoricos de cada fase
2. **[Tareas](tareas/README.md)** — Paso a paso para implementar
3. **[Conceptos](conceptos/ESP-NOW.md)** — Notas tecnicas del vault de aprendizaje
4. **[Referencia](referencia/Protocolo-Mensajes.md)** — Protocolo, APIs, errores comunes
5. **[Decisiones](decisiones/001-c-vs-tinygo.md)** — Por que se tomo cada decision tecnica

## Stack

| Capa | Tecnologia |
|------|------------|
| Firmware | C (C11) + ESP-IDF v5.x |
| Comunicacion | ESP-NOW + WiFi (AP/STA) + MQTT (opcional) |
| Dashboard embebido | Preact + Vite (SPA en SPIFFS del gateway) |
| API | REST + WebSocket sobre `esp_http_server` |
| Hardware | ESP32-S3-WROOM-1-N16R8 (gateway) + ESP32-C3 (nodos) |

## Hardware del gateway

| Dato | Valor | Estado |
|------|-------|--------|
| Modulo | ESP32-S3-WROOM-1-N16R8 | Confirmado |
| Flash | 16 MB Quad-SPI | Confirmado |
| PSRAM | 8 MB Octal-SPI | Confirmado |
| GPIO35/36/37 | Ocupados por la PSRAM Octal | No disponibles para periféricos |
| GPIO48 | LED RGB onboard | Reservado |
| GPIO19/GPIO20 | USB nativo (D-/D+) | Reservados mientras se use USB |
| Pinout del carrier | Provisional | Pendiente de verificacion fisica |
| Pantalla ST7789V2 de 1,9" | 8 pines, 170x320 RGB, SPI de 4 hilos | Pendiente de verificacion fisica |

La fuente de verdad del hardware es `docs/hardware/esp32-s3-wroom-1-n16r8.md` del repositorio. El pinout del carrier y la pantalla externa **no estan validados contra la unidad fisica entregada**: sus dimensiones, niveles logicos y consumo de retroiluminacion estan reportados por el vendedor y pendientes de medir, y no deben usarse como base de diseno.
