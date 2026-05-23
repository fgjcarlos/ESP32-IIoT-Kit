# Estructura del Vault Obsidian

## Filosofia

El vault de Obsidian acompaña el desarrollo del proyecto. Cada vez que aprendas un concepto nuevo, lo documentas en una nota. Las notas se enlazan entre si con `[[wiki-links]]` creando un grafo de conocimiento que crece con el proyecto.

**Regla**: si tardas mas de 5 minutos entendiendo algo, merece una nota en Obsidian.

## Estructura de carpetas

```
knowledge/
├── _MOC.md                      ← Map of Content (indice principal)
├── Conceptos/                   ← Notas sobre conceptos tecnicos
│   ├── ESP-IDF.md
│   ├── FreeRTOS.md
│   ├── ESP-NOW.md
│   ├── WiFi-Modos.md
│   ├── Deep-Sleep.md
│   ├── NVS.md
│   ├── MQTT.md
│   ├── I2C.md
│   ├── SPI.md
│   ├── ADC.md
│   ├── GPIO.md
│   ├── OTA.md
│   ├── Watchdog.md
│   ├── CMake-ESP-IDF.md
│   └── Kconfig.md
├── Decisiones/                  ← Architecture Decision Records (ADR)
│   ├── _template.md
│   ├── 001-c-vs-tinygo.md
│   ├── 002-esp-now-sin-cifrado.md
│   └── 003-protocolo-unificado.md
├── Diario/                      ← Entradas de aprendizaje por dia
│   └── (se llena durante desarrollo)
├── Referencia/                  ← Quick reference cards
│   ├── ESP32-S3-Pinout.md
│   ├── ESP32-C3-Pinout.md
│   ├── APIs-ESP-IDF-Cheatsheet.md
│   ├── Errores-Comunes.md
│   └── Protocolo-Mensajes.md
└── Proyecto/                    ← Estado y arquitectura del proyecto
    ├── Arquitectura.md
    ├── Progreso.md
    └── Problemas-Resueltos.md
```

## Como usar cada carpeta

### Conceptos/

Una nota por concepto. Estructura sugerida:

```markdown
# Nombre del Concepto

## Que es
(1-2 parrafos explicando el concepto en tus propias palabras)

## Por que importa en este proyecto
(como se usa en este proyecto IIoT)

## Como funciona
(detalle tecnico, diagramas si aplica)

## API clave
(funciones principales que vas a usar)

## Gotchas
(cosas que te sorprendieron o errores que cometiste)

## Enlaces
- [[Concepto-Relacionado-1]]
- [[Concepto-Relacionado-2]]
- [Doc oficial](url)
```

### Decisiones/

Architecture Decision Records. Se crean cuando tomas una decision tecnica no obvia.

```markdown
# ADR-XXX: Titulo de la decision

## Estado
Aceptada | Rechazada | Reemplazada por [[ADR-YYY]]

## Contexto
(que problema estabas resolviendo)

## Decision
(que decidiste hacer)

## Alternativas consideradas
1. Opcion A: (pros/contras)
2. Opcion B: (pros/contras)

## Consecuencias
(que implica esta decision para el futuro)
```

### Diario/

Entradas diarias de aprendizaje. No tiene que ser largo — 3-5 puntos de lo que aprendiste.

```markdown
# 2026-05-24

## Que hice
- Instale ESP-IDF v5.4
- Compile hello_world para ESP32-S3

## Que aprendi
- `source export.sh` es necesario en cada terminal nueva
- El ESP32-S3 aparece como /dev/ttyACM0, no ttyUSB0

## Dudas
- Por que FreeRTOS usa ticks en vez de milisegundos?
  → Respuesta: [[FreeRTOS#Ticks vs Milisegundos]]

## Siguiente
- Compilar para ESP32-C3
```

### Referencia/

Quick reference cards. Informacion que consultas frecuentemente.

### Proyecto/

Estado actual del proyecto, arquitectura actualizada, problemas resueltos con solucion.

## Obsidian plugins recomendados

| Plugin | Para que |
|--------|----------|
| Dataview | Consultas sobre notas (ej. "todas las decisiones aceptadas") |
| Graph View | (core) Visualizar conexiones entre conceptos |
| Templates | (core) Usar los templates de Decisiones/ y Diario/ |
| Excalidraw | Diagramas de arquitectura y circuitos |
| Kanban | Tracking de tareas por fase (alternativa visual) |

## Tags sugeridos

```
#concepto - Nota de concepto
#decision - ADR
#diario - Entrada de diario
#referencia - Quick reference
#bug - Problema encontrado y solucion
#gotcha - Comportamiento sorprendente
#fase/0 #fase/1 ... - Fase del proyecto
```

## Workflow sugerido

1. **Antes de empezar una tarea**: lee el tutorial correspondiente y crea/actualiza las notas de Conceptos/ relevantes
2. **Durante la tarea**: si encuentras un gotcha o bug, anotalo en la nota del concepto con tag #gotcha o #bug
3. **Al terminar el dia**: escribe una entrada en Diario/
4. **Al tomar una decision tecnica**: crea un ADR en Decisiones/
5. **Cada semana**: revisa el Graph View para ver que conceptos estan aislados (sin enlaces) y conectalos
