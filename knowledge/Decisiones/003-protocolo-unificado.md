# ADR-003: Protocolo de mensajeria unificado

## Estado
Aceptada (2026-05-23)

## Contexto
El plan original definia el protocolo de mensajeria DOS veces con estructuras contradictorias:

- **Fase 0 (T0.8.2)**: `sensor_data_t` con campos fijos (temperatura, ph, oxigeno, turbidez_raw, rssi, bateria_pct) y tipos de mensaje diferentes (MSG_TYPE_SENSOR_DATA, MSG_TYPE_HEARTBEAT)
- **Fase 2 (T2.2.1)**: `msg_sensor_data_t` con header generico + `values[]` array y tipos diferentes (MSG_TYPE_DATA, MSG_TYPE_ACK, MSG_TYPE_DISCOVERY, MSG_TYPE_CONFIG)

Ambas definiciones no son compatibles entre si.

## Decision
Adoptar la **estructura de Fase 2** (header generico + values[]) como unico protocolo, implementado en Fase 0.

## Alternativas consideradas

1. **Campos fijos (Fase 0 original)**
   - Pro: Mas simple de leer, cada campo tiene nombre
   - Contra: No extensible — agregar un sensor nuevo requiere cambiar la struct
   - Contra: Desperdicia bytes si un nodo solo mide temperatura (todos los campos se envian vacios)

2. **Header + values[] generico (Fase 2)** (elegida)
   - Pro: Extensible — agregar un tipo de sensor no cambia la struct
   - Pro: Eficiente — solo envias los values que necesitas (`value_count`)
   - Pro: Header comun permite routing sin deserializar todo el mensaje
   - Contra: Menos legible (values[0] en vez de .temperatura)

## Consecuencias
- El protocolo se define en `firmware/common/protocol/protocol.h`
- Se implementa una sola vez en Fase 0 (T0.8.2)
- Fase 2 USA el protocolo, no lo redefine
- La interpretacion de `values[]` depende de `sensor_type`:
  - TEMPERATURE: values[0] = grados C
  - PH: values[0] = pH, values[1] = mV raw
  - etc.
- Documentacion completa en `docs/replanificacion/02-protocolo-unificado.md`

## Referencias
- [[ESP-NOW]]
- `docs/replanificacion/02-protocolo-unificado.md` (source of truth)
