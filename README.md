# ESP32-IIoT-Kit

Plataforma generica de monitorizacion industrial construida con microcontroladores ESP32.

**Proyecto educativo** — aprendiendo ESP-IDF, comunicaciones inalambricas, IoT y desarrollo de interfaces embebidas construyendo algo real.

> ¿Buscas una implementacion concreta? Consulta `examples/fish-farm/` para el caso de uso de piscifactoria sobre esta misma plataforma.

## Arquitectura

```
Navegador ──WiFi AP──► Gateway ESP32-S3-WROOM-1-N16R8
                         · Preact SPA servida desde SPIFFS
                         · API REST + WebSocket (esp_http_server)
                         · ESP-NOW hacia los nodos
                         · MQTT opcional (cloud / LAN)
        ▲ ESP-NOW
   ┌────┼────────┐
 Nodo A  Nodo B  Nodo C    ← ESP32-C3, bateria, deep sleep
```

El gateway es **autonomo**: no necesita servidor externo. Un broker MQTT externo es una integracion opcional, no un componente obligatorio.

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
| Modulo | ESP32-S3-WROOM-1-N16R8 — 16 MB flash Quad-SPI, 8 MB PSRAM Octal-SPI | Confirmado |
| GPIO35 / GPIO36 / GPIO37 | Ocupados por la PSRAM Octal | No disponibles para perifericos |
| GPIO48 | LED RGB onboard | Reservado |
| GPIO19 / GPIO20 | USB nativo (D- / D+) | Reservados mientras se use USB |
| Pinout del carrier | Solo hay una imagen candidata del usuario | **Provisional, pendiente de verificacion fisica** |
| Pantalla externa | ST7789V2 de 1,9", 8 pines, 170(H) x 320(V) RGB, SPI de 4 hilos | **Pendiente de verificacion fisica** |

La fuente de verdad unica del hardware es **`docs/hardware/esp32-s3-wroom-1-n16r8.md`**. Alli se distingue lo confirmado por sonda de lo reportado por el vendedor: el pinout del carrier, las dimensiones mecanicas, los niveles logicos y el consumo de retroiluminacion de la pantalla **no estan validados contra la unidad fisica entregada** y no deben usarse como base de diseno mecanico ni electrico.

## Estado del firmware

La base del gateway es **modular** y vive en `firmware/gateway/components/`, con un componente porsubsistema y sus tests Unity en `components/*/test/`:

| Componente | Responsabilidad |
|------------|------------------|
| `board_profile` / `board_rgb` | Perfil de placa (16 MB flash, 8 MB PSRAM) y LED RGB de GPIO48 |
| `wifi_manager` / `espnow_manager` | WiFi APSTA y backbone ESP-NOW |
| `http_server` | API REST y WebSocket |
| `mqtt_bridge` | MQTT plano, integracion opcional |
| `sparkplug_b` | Modo opcional MQTT Sparkplug B v3.0.0 (EoN-Node) |
| `ota_manager` / `ota_manifest` | OTA con manifiesto firmado y rollback |
| `brand_profile` / `assets_brand` | Capa white-label y sus activos |
| `lcd_driver` / `lvgl_port` / `ui_screens` / `display_manager` | Cadena de display (sLCD ST7789, pendientes de hardware) |

El protocolo compartido vive en `firmware/common/protocol/`. `firmware/node/` (nodos ESP32-C3) y `firmware/gateway/web/` (SPA Preact) estan **planificados pero todavia no existen en disco**: no los trates como proyectos ejecutables.

## Documentacion

- **[Tutorial completo](https://fgjcarlos.github.io/ESP32-IIoT-Kit/)** — Teoria, tareas paso a paso, y referencia tecnica
- `Tutorial/` — Conceptos teoricos por fase
- `Fases/` — Tareas detalladas con criterios de aceptacion
- `knowledge/` — Vault Obsidian con notas de aprendizaje
- `docs/hardware/` — Perfil del gateway S3: **fuente de verdad del hardware**
- `docs/replanificacion/` — Decisiones tecnicas y protocolo
- `docs/ota/` — Seguridad del OTA y manifiesto firmado
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
| 6 | Mejoras futuras (incluye Sparkplug B opcional) | Continuo |

El cronograma corregido y su origen estan en `docs/replanificacion/03-fases-corregidas.md`.

## Verificacion

```bash
source $HOME/esp/esp-idf/export.sh      # idf.py no esta en PATH por defecto
cd firmware/gateway && idf.py build
cd firmware/gateway/test && idf.py test  # Unity, en el dispositivo
mkdocs build --strict                    # sitio de documentacion
```

## Licencia

MIT
