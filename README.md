# ESP32-IIoT-Kit

Plataforma generica de monitorizacion industrial construida con microcontroladores ESP32.

**Proyecto educativo** — aprendiendo ESP-IDF, comunicaciones inalambricas, IoT y desarrollo de interfaces embebidas construyendo algo real.

> ¿Buscas una implementacion concreta? Consulta `examples/fish-farm/` para el caso de uso de piscifactoria sobre esta misma plataforma.

## Arquitectura

```
Servidor (MQTT + API REST) — opcional
        │
   MQTT opcional (WiFi STA)
        │
   ┌────▼────┐
   │ Gateway  │ ← WiFi AP + Dashboard Preact embebido (SPIFFS)
   │ ESP32-S3 │   ESP-NOW + WiFi APSTA + API REST local
   └────┬────┘
        │ ESP-NOW
   ┌────┼────────┐
 Nodo A  Nodo B  Nodo C    ← ESP32-C3, bateria, deep sleep
```

## Stack

| Capa | Tecnologia |
|------|------------|
| Firmware | C + ESP-IDF v5.x |
| Comunicacion | ESP-NOW + WiFi + MQTT (opcional) |
| Dashboard embebido | Preact + Vite (SPIFFS en gateway) |
| Backend opcional | Bun + TypeScript + SQLite |

## Documentacion

- **[Tutorial completo](https://fgjcarlos.github.io/ESP32-fg/)** — Teoria, tareas paso a paso, y referencia tecnica
- `Tutorial/` — Conceptos teoricos por fase
- `Fases/` — Tareas detalladas con criterios de aceptacion
- `knowledge/` — Vault Obsidian con notas de aprendizaje
- `docs/replanificacion/` — Decisiones tecnicas y protocolo
- `examples/fish-farm/` — Caso de uso: monitorizacion de piscifactoria

## Fases

| Fase | Descripcion | Semanas |
|------|-------------|---------|
| 0 | Preparacion y aprendizaje | 3 |
| 1 | Gateway - Nucleo | 5 |
| 2 | Nodos + ESP-NOW fiable | 5 |
| 3 | Sensores y actuadores | 3 |
| 4 | Dashboard Embebido + API REST | 4 |
| 5 | OTA + Seguridad + Pruebas | 5 |

## Licencia

MIT
