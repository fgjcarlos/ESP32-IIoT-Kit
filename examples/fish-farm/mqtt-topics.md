# Tópicos MQTT — Ejemplo Fish Farm

Este documento define la jerarquía de tópicos MQTT para el ejemplo de piscifactoría del ESP32-IIoT-Kit. Usa el sistema de espacio de nombres configurable descrito en `docs/replanificacion/02-protocolo-unificado.md`.

---

## Configuración del espacio de nombres

El gateway almacena el prefijo MQTT en NVS:

```
Clave NVS  : mqtt_namespace
Espacio NVS: "mqtt"
Valor      : "fish-farm"
```

Para configurar el gateway con este espacio de nombres:

```c
nvs_handle_t handle;
nvs_open("mqtt", NVS_READWRITE, &handle);
nvs_set_str(handle, "mqtt_namespace", "fish-farm");
nvs_commit(handle);
nvs_close(handle);
```

Una vez configurado, todos los tópicos publicados por el gateway usarán `fish-farm` como prefijo en lugar del valor por defecto `iiot-kit`.

---

## Árbol de tópicos

### Datos de sensores (publicados por el gateway)

```
fish-farm/{gateway_id}/nodo/{nodo_id}/temperature     → temperatura (°C × 10)
fish-farm/{gateway_id}/nodo/{nodo_id}/ph              → pH (× 100)
fish-farm/{gateway_id}/nodo/{nodo_id}/do              → oxígeno disuelto (mg/L × 100)
fish-farm/{gateway_id}/nodo/{nodo_id}/orp             → ORP (mV, puede ser negativo)
fish-farm/{gateway_id}/nodo/{nodo_id}/turbidity       → turbidez (NTU)
fish-farm/{gateway_id}/nodo/{nodo_id}/level           → nivel de agua (cm × 10)
fish-farm/{gateway_id}/nodo/{nodo_id}/status          → estado del nodo (JSON)
```

### Alertas y control (bidireccional)

```
fish-farm/{gateway_id}/alertas                        → alertas publicadas por el gateway (JSON)
fish-farm/{gateway_id}/control/actuador/{id}          → comandos de actuador (JSON, QoS 2)
```

### Configuración de nodos

```
fish-farm/{gateway_id}/config/nodo/{nodo_id}          → configuración enviada al nodo (JSON)
```

---

## Tabla de QoS

| Tópico | QoS | Retain | Descripción |
|--------|-----|--------|-------------|
| `.../temperature` | 1 | No | Dato de sensor; pérdida aceptable |
| `.../ph` | 1 | No | Dato de sensor; pérdida aceptable |
| `.../do` | 1 | No | Crítico biológicamente, pero el nodo reenvía si no hay ACK |
| `.../orp` | 1 | No | Dato de sensor |
| `.../turbidity` | 1 | No | Dato de sensor |
| `.../level` | 1 | No | Dato de nivel |
| `.../status` | 1 | Yes | Estado del nodo; retain para recuperar al reconectar |
| `.../alertas` | 1 | No | Alertas de umbral |
| `.../control/actuador/{id}` | 2 | No | Comandos de actuador — entrega exactamente una vez |
| `.../config/nodo/{nodo_id}` | 1 | Yes | Configuración retenida para recuperar al reiniciar el nodo |

---

## Ejemplos de payload

### Tópico de temperatura

```json
{
  "nodo_id": "AA:BB:CC:DD:EE:FF",
  "timestamp": 1748000000,
  "sequence": 42,
  "value": 245,
  "unit": "0.1°C"
}
```

### Tópico de alerta (pH fuera de rango)

```json
{
  "gateway_id": "12:34:56:78:9A:BC",
  "nodo_id": "AA:BB:CC:DD:EE:FF",
  "timestamp": 1748000100,
  "alert_type": "threshold",
  "sensor": "ph",
  "value": 540,
  "threshold_min": 650,
  "threshold_max": 850
}
```

### Comando de actuador (activar aireador)

```json
{
  "actuador_id": "aireador_1",
  "action": "on",
  "duration_s": 300
}
```

---

## Sustitución de variables

| Variable | Descripción | Ejemplo |
|----------|-------------|---------|
| `{gateway_id}` | MAC del gateway en formato hexadecimal | `1234_5678_9ABC` |
| `{nodo_id}` | MAC del nodo sensor | `AABB_CCDD_EEFF` |
| `{id}` | Identificador del actuador (configurable en NVS) | `aireador_1` |

---

## Referencias

- Protocolo de espacio de nombres configurable: `docs/replanificacion/02-protocolo-unificado.md`
- Fase 4 (integración MQTT opcional): `Fases/fase-04-mqtt-dashboard.md` — Subtarea 4.3
- Sensores acuícolas: `examples/fish-farm/sensors.md`
