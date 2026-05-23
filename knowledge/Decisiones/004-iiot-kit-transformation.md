# ADR-004: Transformación a Plataforma IIoT Genérica (ESP32-IIoT-Kit)

## Estado
Aceptada (2026-05-23)

## Contexto
El proyecto se inició con el nombre "Piscifactoría ESP32" y todo el tutorial orientado al caso de uso de monitorización acuícola. Tras completar las Decisiones DC-01 a DC-13 y la replanificación de 2026-05-23, se constató que el **85–95 % del contenido técnico es completamente independiente del dominio**:

- La arquitectura gateway/nodo con ESP32-S3 + ESP32-C3 es genérica
- El protocolo ESP-NOW con header + values[] es genérico
- El driver de sensores analógicos (ADC + calibración) es genérico
- El dashboard embebido con Preact SPA + REST + WebSocket es genérico
- MQTT con espacio de nombres configurable es genérico

Solo los sensores electroquímicos (pH, OD, ORP, turbidez, nivel) y la jerarquía de tópicos `piscifactoria/...` son específicos del dominio acuícola.

El framing de piscifactoría limitaba artificialmente el alcance del tutorial: estudiantes de invernadero, industria alimentaria, calidad del aire o monitorización de infraestructuras no se identificaban con el proyecto aunque el 90 % del código les fuera directamente aplicable.

## Decision
Transformar el proyecto en **ESP32-IIoT-Kit**: plataforma IIoT genérica con arquitectura de referencia, donde el ejemplo de piscifactoría ocupa el directorio `examples/fish-farm/` como caso de uso ilustrativo, sin contaminar el tutorial principal.

## Alternativas consideradas

1. **Mantener el framing de piscifactoría**
   - Pro: Coherente con lo construido hasta ahora; sin coste de refactorización
   - Pro: El caso de uso concreto puede ser más motivador para cierto perfil de estudiante
   - Contra: Excluye implícitamente a la mayoría de los estudiantes de IIoT
   - Contra: El 90 % del tutorial es aplicable a cualquier dominio pero el nombre lo descarta
   - Contra: Limita el alcance y la comunidad potencial del proyecto educativo

2. **Abstracción total: eliminar todo ejemplo de dominio**
   - Pro: Tutorial completamente genérico, máximo alcance
   - Contra: Sin un caso de uso concreto, el tutorial pierde motivación y anclaje en la realidad
   - Contra: Los sensores genéricos (solo DS18B20) pueden parecer insuficientes para IIoT real
   - Contra: Perder el contenido ya redactado sobre pH, OD, ORP sería un desperdicio

3. **Núcleo genérico + directorio examples/** (elegida)
   - Pro: Tutorial principal aplicable a cualquier dominio IIoT
   - Pro: El contenido de dominio específico se preserva y organiza en `examples/`
   - Pro: Estudiantes pueden elegir su dominio y adaptar el ejemplo
   - Pro: DS18B20 como sensor universal en Fase 3; sensores acuícolas en `examples/fish-farm/sensors.md`
   - Contra: Requiere refactorización de ~37 archivos (cambio de nombre, vocabulario, arquitectura Fase 4)
   - Contra: La Fase 4 cambia completamente: de Bun/React/MQTT externo a Preact SPA embebida + REST + WebSocket

## Consecuencias

- **Nombre del repositorio**: renombrar de `ESP32-fg` a `ESP32-IIoT-Kit` en GitHub (pendiente — acción manual)
- **Fase 4**: reescritura completa. Se elimina la arquitectura de servidor externo (Bun/Node.js, SQLite, Mosquitto, React). La nueva Fase 4 documenta el dashboard embebido (Preact SPA en SPIFFS) + REST API + WebSocket + MQTT opcional
- **Fase 3**: restructuración. DS18B20 queda como ejemplo principal; sensores acuícolas se mueven a `examples/fish-farm/sensors.md`
- **Protocolo**: añadido `SENSOR_TYPE_CUSTOM = 0xFF` y espacio de nombres MQTT configurable (`mqtt_namespace` en NVS, valor por defecto `iiot-kit`)
- **Vocabulario**: tabla de sustitución aplicada en ~37 archivos (piscifactoria → sistema IIoT, estanque → nodo, etc.)
- **Knowledge vault**: se crean 4 nuevas notas de concepto (Preact, REST-API, WebSocket) y este ADR
- **Páginas GitHub**: URL actualizada a `https://fgjcarlos.github.io/ESP32-IIoT-Kit/`

## Referencias
- [[001-c-vs-tinygo]] — Decisión de lenguaje (sin cambio)
- [[002-esp-now-sin-cifrado]] — Protocolo ESP-NOW (sin cambio)
- [[003-protocolo-unificado]] — Protocolo de mensajería (extendido con SENSOR_TYPE_CUSTOM y namespace MQTT)
- `docs/replanificacion/02-protocolo-unificado.md` (source of truth del protocolo actualizado)
- `examples/fish-farm/README.md` — Caso de uso acuícola preservado
