# Piscifactoria ESP32

Sistema de monitorizacion inteligente para piscifactorias construido con microcontroladores ESP32.

**Proyecto educativo** — aprendiendo ESP-IDF, comunicaciones inalambricas, IoT y desarrollo fullstack construyendo algo real.

## Arquitectura

```
Servidor (MQTT + API + Dashboard)
        │
   MQTT (WiFi)
        │
   ┌────▼────┐
   │ Gateway  │ ← WiFi AP (config web)
   │ ESP32-S3 │   ESP-NOW + WiFi APSTA + MQTT
   └────┬────┘
        │ ESP-NOW
   ┌────┼────────┐
 Nodo A  Nodo B  Nodo C    ← ESP32-C3, bateria, deep sleep
```

## Stack

| Capa | Tecnologia |
|------|------------|
| Firmware | C + ESP-IDF v5.x |
| Comunicacion | ESP-NOW + WiFi + MQTT |
| Backend | Bun + TypeScript + SQLite |
| Frontend | Vite + React + Tailwind |

## Documentacion

- **[Tutorial completo](https://fgjcarlos.github.io/ESP32-fg/)** — Teoria, tareas paso a paso, y referencia tecnica
- `Tutorial/` — Conceptos teoricos por fase
- `Fases/` — Tareas detalladas con criterios de aceptacion
- `knowledge/` — Vault Obsidian con notas de aprendizaje
- `docs/replanificacion/` — Decisiones tecnicas y protocolo

## Fases

| Fase | Descripcion | Semanas |
|------|-------------|---------|
| 0 | Preparacion y aprendizaje | 3 |
| 1 | Gateway - Nucleo | 5 |
| 2 | Nodos + ESP-NOW fiable | 5 |
| 3 | Sensores y actuadores | 3 |
| 4 | MQTT + Dashboard web | 4 |
| 5 | OTA + Seguridad + Pruebas | 5 |

## Licencia

MIT
