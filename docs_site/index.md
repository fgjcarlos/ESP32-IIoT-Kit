# Piscifactoria ESP32

Sistema de monitorizacion inteligente para piscifactorias construido con ESP32.

## Que es este proyecto

Un sistema completo de monitorizacion y control para granjas de peces, usando microcontroladores ESP32. Mide parametros criticos del agua (temperatura, pH, oxigeno disuelto, nivel) y los transmite de forma inalambrica a un dashboard web.

**Este es un proyecto educativo** — el objetivo principal es aprender desarrollo de sistemas embebidos con ESP-IDF, comunicaciones inalambricas, protocolos IoT y desarrollo web fullstack.

## Arquitectura

```
┌──────────────────────────────────────────────────────┐
│              SERVIDOR (LAN / Cloud)                  │
│  Mosquitto (MQTT) <-> API Bun/TS + SQLite <-> React  │
└───────┬──────────────────────────────────────────────┘
        │ MQTT (WiFi STA)
┌───────▼────────┐
│    GATEWAY      │ <-- WiFi AP (panel web embebido)
│    ESP32-S3     │     C (ESP-IDF v5.x)
└───────┬────────┘
        │ ESP-NOW (2.4GHz)
   ┌────┼──────────┐
Nodo A  Nodo B   Nodo C
ESP32-C3 (Deep Sleep + ESP-NOW)
```

## Empezar

1. **[Tutorial](tutorial/README.md)** — Conceptos teoricos de cada fase
2. **[Tareas](tareas/README.md)** — Paso a paso para implementar
3. **[Referencia](referencia/protocolo.md)** — Protocolo, APIs, errores comunes
4. **[Decisiones](decisiones/001-c-vs-tinygo.md)** — Por que se tomo cada decision tecnica

## Stack

| Capa | Tecnologia |
|------|------------|
| Firmware | C + ESP-IDF v5.x |
| Comunicacion | ESP-NOW + WiFi + MQTT |
| Backend | Bun/Node + TypeScript + SQLite |
| Frontend | Vite + React + Tailwind |
| Hardware | ESP32-S3 (gateway) + ESP32-C3 (nodos) |
