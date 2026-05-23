# Replanificacion del Proyecto ESP32-IIoT-Kit

> **Nota historica**: Este documento fue redactado cuando el proyecto se llamaba "Piscifactoria ESP32". En mayo de 2026 el proyecto fue renombrado a ESP32-IIoT-Kit para reflejar su caracter de plataforma generica de monitorizacion industrial. El caso de uso de piscifactoria permanece como ejemplo en `examples/fish-farm/`. El contenido tecnico de este documento no ha cambiado.

Fecha: 2026-05-23
Estado: **Aprobada** (2026-05-23)

## Motivacion

Auditoria completa de la planificacion existente. Se encontraron 3 problemas criticos, 4 altos, 9 medios y 8 menores. Este documento corrige los problemas y reorganiza las dependencias entre fases.

## Indice de documentos

| Documento | Contenido |
|-----------|-----------|
| [01-decisiones-corregidas.md](01-decisiones-corregidas.md) | Decisiones tecnicas que cambian respecto al plan original |
| [02-protocolo-unificado.md](02-protocolo-unificado.md) | Definicion unica del protocolo ESP-NOW (source of truth) |
| [03-fases-corregidas.md](03-fases-corregidas.md) | Vista general de fases con dependencias corregidas |
| [04-estructura-obsidian.md](04-estructura-obsidian.md) | Estructura del vault Obsidian para aprendizaje |

## Resumen de cambios criticos

### 1. Peers ESP-NOW: de 20 cifrados a 20 SIN cifrar

El hardware ESP-NOW soporta **6 peers cifrados** o **20 sin cifrar**. El plan original dice 20 cifrados, lo cual es incorrecto. Se adopta la estrategia de 20 peers sin cifrado para v1.0, dejando cifrado como mejora en Fase 6.

### 2. Protocolo unificado

El plan original define el protocolo DOS veces con nombres y estructuras diferentes (Fase 0 vs Fase 2). Se unifica en un solo documento que se implementa en Fase 0 y se usa sin cambios en todas las fases posteriores.

### 3. Nodos con capacidad WiFi para OTA

El firmware del nodo necesita poder conectarse al AP del gateway para recibir OTA en Fase 5. Se agrega esta capacidad en Fase 2 (dormida, solo se activa cuando el gateway lo solicita via ESP-NOW).

### 4. MQTT plano a Sparkplug B (progresion formativa)

Fase 4 implementa MQTT plano con topics custom para aprender los fundamentos. Fase 6 migra a MQTT Sparkplug B (estandar IIoT) con payloads Protobuf, birth/death certificates y auto-discovery. Los nodos sensores no se ven afectados (solo hablan ESP-NOW). Ver DC-13.

### 5. Observaciones adicionales

- Watchdog timeout ajustado de 30s a 90s
- ACK del gateway movido a Fase 1
- sensor_manager.c se crea explicitamente en Fase 2
- Canal WiFi: mecanismo de discovery en vez de hardcode
- NVS encryption agregado a Fase 5
- Enlace roto al final de Fase 0 corregido
