# Arquitectura del Sistema

#proyecto

## Topologia

```
                 Browser (WiFi AP o STA)
                          |
          ┌───────────────┼───────────────┐
          │               │               │
     SPIFFS          REST API         WebSocket
   (Preact SPA)   (config, data,    (tiempo real,
                    OTA, status)    push de nodos)
          │               │               │
          └───────────────┼───────────────┘
                          │
                 ┌────────────────┐
                 │    GATEWAY      │ <-- WiFi APSTA
                 │    ESP32-S3     │     (AP: panel local)
                 │  8MB Flash      │     (STA: red/cloud opcional)
                 │  2MB PSRAM      │
                 │                 │     Firmware: C (ESP-IDF v5.x)
                 │  [MQTT bridge]  │     ESP-NOW + WiFi APSTA + HTTP + WS
                 │   (opcional)    │
                 └───────┬─────────┘
                         │ ESP-NOW (2.4GHz, peer-to-peer)
                    ┌────┼──────────────┐
                    │    │              │
                 Nodo A  Nodo B      Nodo C
                 ESP32-C3            ESP32-C3
                 Temp    Temp        Temp
                 Deep Sleep + ESP-NOW
```

**El gateway es autonomo**: no requiere ningun servidor externo para funcionar. La Preact SPA (servida desde SPIFFS) se comunica directamente con el gateway via REST y WebSocket. MQTT es una integracion opcional para conectar a cloud o LAN.

## Decisiones clave
- [[001-c-vs-tinygo]] — C + ESP-IDF
- [[002-esp-now-sin-cifrado]] — 20 peers sin cifrar
- [[003-protocolo-unificado]] — Un protocolo para todos los mensajes
- [[004-iiot-kit-transformation]] — Transformacion a plataforma IIoT generica <!-- ADR creado en 2026-05-23 -->

## Protocolos (prioridad)
1. **ESP-NOW** — backbone nodo→gateway. Ver [[ESP-NOW]]
2. **WiFi AP** — config local, Preact SPA embebida, REST API, WebSocket
3. **WiFi STA** — red/internet para MQTT opcional, OTA, SNTP

## API del gateway

| Endpoint | Metodo | Descripcion |
|----------|--------|-------------|
| `/api/status` | GET | Estado del gateway (uptime, heap, canal WiFi) |
| `/api/nodes` | GET | Lista de nodos registrados con ultima lectura |
| `/api/nodes/{mac}/readings` | GET | Historial de lecturas de un nodo |
| `/api/config` | GET/POST | Configuracion del gateway (SSID, umbrales, namespace MQTT) |
| `/api/actuators` | GET | Estado de todos los actuadores |
| `/api/actuators/{id}` | POST | Encender/apagar un actuador |
| `/api/alerts` | GET/POST | Reglas de alerta con histeresis |
| `/api/ota` | POST | Iniciar actualizacion OTA de un nodo |
| `ws://gateway/ws` | WebSocket | Push en tiempo real de lecturas de nodos |

## Restricciones hardware
| Restriccion | Valor |
|-------------|-------|
| ESP-NOW max payload | 250 bytes |
| ESP-NOW max peers (sin cifrar) | 20 |
| Gateway flash | 8MB (2x3MB OTA + 1.9MB SPIFFS + NVS) |
| SPIFFS Preact SPA budget | ≤200 KB gzipped (objetivo), 1.9 MB limite absoluto |
| Node flash | 4MB (2x1.5MB OTA + NVS) |
| Node bateria target | 12+ meses (3400mAh, wake cada 5min) |
| FreeRTOS stack default | 4KB |

## Protocolo de mensajes
Ver [[Protocolo-Mensajes]] y `docs/replanificacion/02-protocolo-unificado.md`
