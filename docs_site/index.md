# ESP32-IIoT-Kit

Plataforma generica de monitorizacion industrial construida con ESP32.

## Que es este proyecto

Un kit educativo y de referencia para construir sistemas de monitorizacion IIoT con microcontroladores ESP32. El sistema recoge variables del entorno (temperatura, pH, humedad, presion, nivel u otras) y las transmite de forma inalambrica a un gateway central que las procesa y sirve en un dashboard embebido.

**Este es un proyecto educativo** — el objetivo principal es aprender desarrollo de sistemas embebidos con ESP-IDF, comunicaciones inalambricas, protocolos IoT y programacion de interfaces web ligeras.

> ¿Buscas un caso de uso especifico? Consulta los ejemplos en `examples/fish-farm/` para ver una implementacion de monitorizacion de piscifactoria sobre esta misma plataforma.

## Arquitectura

```
┌──────────────────────────────────────────────────────┐
│          SERVIDOR (LAN / Cloud) — opcional           │
│         Mosquitto (MQTT) <-> API REST + SQLite       │
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

## Empezar

1. **[Tutorial](tutorial/README.md)** — Conceptos teoricos de cada fase
2. **[Tareas](tareas/README.md)** — Paso a paso para implementar
3. **[Referencia](referencia/protocolo.md)** — Protocolo, APIs, errores comunes
4. **[Decisiones](decisiones/001-c-vs-tinygo.md)** — Por que se tomo cada decision tecnica

## Stack

| Capa | Tecnologia |
|------|------------|
| Firmware | C + ESP-IDF v5.x |
| Comunicacion | ESP-NOW + WiFi + MQTT (opcional) |
| Dashboard embebido | Preact + Vite (SPIFFS en gateway) |
| Backend opcional | Bun/Node + TypeScript + SQLite |
| Hardware | ESP32-S3 (gateway) + ESP32-C3 (nodos) |
