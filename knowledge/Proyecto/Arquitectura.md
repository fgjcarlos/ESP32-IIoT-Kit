# Arquitectura del Sistema

#proyecto

## Topologia

```
┌─────────────────────────────────────────────────────────┐
│                   SERVIDOR (LAN / Cloud)                │
│  Mosquitto (MQTT) <-> API Bun/TS + SQLite <-> Dashboard │
└───────┬─────────────────────────────────────────────────┘
        │ MQTT (WiFi STA)
┌───────▼────────┐
│    GATEWAY      │ <-- WiFi AP (panel web embebido)
│    ESP32-S3     │
│  8MB Flash      │     Firmware: C (ESP-IDF v5.x)
│  2MB PSRAM      │     ESP-NOW + WiFi APSTA + MQTT + HTTP
└───────┬────────┘
        │ ESP-NOW (2.4GHz, sin cifrar en v1.0)
   ┌────┼──────────────┐
   │    │              │
Nodo A  Nodo B      Nodo C
ESP32-C3             ESP32-C3
Temp/DO  pH/ORP     Nivel
Deep Sleep + ESP-NOW
```

## Decisiones clave
- [[001-c-vs-tinygo]] — C + ESP-IDF
- [[002-esp-now-sin-cifrado]] — 20 peers sin cifrar
- [[003-protocolo-unificado]] — Un protocolo para todos los mensajes

## Protocolos (prioridad)
1. **ESP-NOW** — backbone nodo→gateway. Ver [[ESP-NOW]]
2. **WiFi AP** — config local, dashboard embebido
3. **WiFi STA** — internet para MQTT, OTA, SNTP

## Restricciones hardware
| Restriccion | Valor |
|-------------|-------|
| ESP-NOW max payload | 250 bytes |
| ESP-NOW max peers (sin cifrar) | 20 |
| Gateway flash | 8MB (2x3MB OTA + 1.9MB SPIFFS + NVS) |
| Node flash | 4MB (2x1.5MB OTA + NVS) |
| Node bateria target | 12+ meses (3400mAh, wake cada 5min) |
| FreeRTOS stack default | 4KB |

## Protocolo de mensajes
Ver [[Protocolo-Mensajes]] y `docs/replanificacion/02-protocolo-unificado.md`
