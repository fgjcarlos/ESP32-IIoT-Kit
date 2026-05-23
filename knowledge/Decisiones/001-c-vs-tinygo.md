# ADR-001: C + ESP-IDF en vez de TinyGo

## Estado
Aceptada (2026-05-23)

## Contexto
Antes de iniciar el proyecto, se evaluo si usar TinyGo (Go para microcontroladores) en vez de C con ESP-IDF. TinyGo ofrece un lenguaje mas ergonomico (goroutines, GC, type safety) y tiene soporte experimental para ESP32-S3 y ESP32-C3.

## Decision
Usar **C con ESP-IDF v5.x** como unico lenguaje de firmware.

## Alternativas consideradas

1. **TinyGo**
   - Pro: Go es mas seguro que C (GC, no pointer arithmetic), desarrollo mas rapido, binarios comparables en tamano para WiFi
   - Contra CRITICO: No soporta [[ESP-NOW]], [[Deep-Sleep]], ni [[NVS]] runtime. Estos tres son pilares del proyecto
   - Contra: Scheduler cooperativo single-core (no [[FreeRTOS]])
   - Contra: WiFi apenas aterrizo en v0.41 (abril 2026), ecosistema inmaduro

2. **C con ESP-IDF** (elegida)
   - Pro: Soporte completo de todas las APIs del ESP32 (ESP-NOW, deep sleep, NVS, OTA, FreeRTOS)
   - Pro: Documentacion exhaustiva, miles de ejemplos oficiales
   - Pro: Proyecto educativo — aprender C embebido da fundamentos transferibles
   - Contra: Mas propenso a bugs de memoria (sin GC)
   - Contra: Desarrollo mas lento que Go para logica de negocio

## Consecuencias
- Todo el firmware se escribe en C11
- Se usa el toolchain de ESP-IDF (`idf.py build/flash/monitor`)
- Se deben seguir las convenciones de ESP-IDF (componentes, Kconfig, esp_err_t)
- Reevaluar TinyGo en 12-18 meses cuando ESP-NOW y deep sleep puedan estar soportados

## Referencias
- [[ESP-IDF]]
- [[ESP-NOW]]
- [[Deep-Sleep]]
- Analisis completo en `docs/replanificacion/01-decisiones-corregidas.md`
