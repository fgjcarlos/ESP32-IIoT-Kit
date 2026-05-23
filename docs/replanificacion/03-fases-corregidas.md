# Fases Corregidas

## Vista general

```
Fase 0: Preparacion y aprendizaje .......... 3 semanas (sin cambios)
Fase 1: Gateway - Nucleo ................... 5 semanas (+1 semana por ACK y validacion)
Fase 2: Nodos sensores + ESP-NOW fiable .... 5 semanas (+1 semana por WiFi dormido y discovery)
Fase 3: Sensores y actuadores .............. 3 semanas (sin cambios)
Fase 4: Dashboard Embebido + API REST ...... 4 semanas (sin cambios)
Fase 5: OTA + Seguridad + Pruebas .......... 5 semanas (+1 semana por NVS encryption y tests)
Fase 6: Mejoras futuras .................... Continuo

Total hasta v1.0: 25 semanas (antes: 22)
```

Las 3 semanas extra se deben a tareas nuevas que resuelven problemas que habrian causado retrabajo mucho mayor si se descubren durante la implementacion.

---

## Fase 0: Preparacion y aprendizaje (3 semanas)

### Cambios respecto al plan original

| Cambio | Motivo |
|--------|--------|
| T0.8.2 usa el protocolo unificado (02-protocolo-unificado.md) | Elimina la definicion duplicada de Fase 2 |
| Agregar T0.8.3: Validar tamanos con _Static_assert | Garantiza que las structs caben en ESP-NOW |
| Fase 0 incluye `sensor_type_t` en protocol.h | Evita que Fase 3 lo defina desde cero |

### Tareas modificadas

**T0.8.2 (modificada): Crear componente protocol con protocolo unificado**
- Implementar TODAS las estructuras de `02-protocolo-unificado.md`
- Implementar funciones de serializacion/deserializacion
- Implementar `protocol_validate()`
- Verificar con `_Static_assert` que todos los tamanos son correctos
- Verificar que `sizeof()` es identico compilando para esp32s3 y esp32c3

**T0.8.3 (nueva): Test manual del protocolo**
- En el gateway: crear una `msg_sensor_data_t` de prueba, serializar, deserializar, comparar
- En el nodo: lo mismo
- Enviar por ESP-NOW del nodo al gateway y verificar que se deserializa correctamente
- Tiempo estimado: 2-3 horas

### Correccion de enlace roto

El final de fase-00-preparacion.md dice:
```
Siguiente fase: [Fase 1 - Firmware del nodo sensor](fase-01-nodo-sensor.md)
```
Debe decir:
```
Siguiente fase: [Fase 1 - Gateway Nucleo](fase-01-gateway-nucleo.md)
```

---

## Fase 1: Gateway - Nucleo (5 semanas)

### Cambios respecto al plan original

| Cambio | Motivo |
|--------|--------|
| Agregar T1.1.5: Gateway envia ACK | Rompe la dependencia circular de Fase 2 |
| T1.1.4 (SNTP): agregar `sntp_is_synced()` | Evita timestamps epoch-0 en datos |
| T1.5.2: MAX_NODES = 20, encrypt = false | Correccion del limite de peers |
| T1.6.2: WDT timeout = 90s | Evita resets durante operaciones largas |
| Agregar T1.5.4: Whitelist basica de nodos | Seguridad minima sin cifrado |

### Tareas nuevas

**T1.1.5 (nueva): Gateway envia ACK por ESP-NOW**
- Al recibir un paquete con header valido (MSG_TYPE_DATA o MSG_TYPE_DISCOVERY):
  1. Deserializar header
  2. Validar con `protocol_validate()`
  3. Agregar nodo como peer si no existe (`esp_now_is_peer_exist()`)
  4. Construir `msg_ack_t` con sequence recibido, timestamp del gateway, canal actual
  5. Serializar y enviar al nodo
- Dependencias: T1.1.3 (espnow_manager), T0.8.2 (protocolo)
- Criterio: el ACK llega al nodo en < 50ms
- Tiempo estimado: 3-4 horas

**T1.5.4 (nueva): Whitelist basica de nodos**
- Modo OPEN (default): acepta todo, marca como "pendiente"
- Nodos pendientes se muestran en GET /api/nodes con `status: "pending"`
- POST /api/nodes/{mac}/approve aprueba un nodo (se guarda en NVS)
- Nodos aprobados se procesan normalmente
- Opcional: modo STRICT configurable via NVS (solo procesar aprobados)
- Dependencias: T1.5.2, T1.4.3
- Tiempo estimado: 4-5 horas

### Tareas modificadas

**T1.1.4 (modificada): SNTP con validacion**
- Agregar funcion publica: `bool sntp_is_synced(void)`
- Si SNTP no sincroniza en 30s, no bloquear — continuar con flag `synced = false`
- Exponer estado de sync en GET /api/status: `"time_synced": true/false`
- Si no sincronizado, timestamps usan uptime con flag `"time_source": "uptime"`

**T1.6.2 (modificada): Watchdog a 90 segundos**
- Calcular operacion mas larga: WiFi reconnect (60s backoff max) + SNTP (30s)
- WDT timeout = 90s para cubrir ese caso
- Cada subsistema (WiFi, MQTT, HTTP) alimenta el WDT en su propio loop
- Documentar calculo en el codigo

### Grafo de dependencias Fase 1

```
T1.1.1 (NVS) ─────────────────────────────────────┐
    │                                               │
T1.1.2 (WiFi APSTA) ──────────────────────┐        │
    │                                      │        │
T1.1.3 (ESP-NOW init) ──────────┐         │        │
    │                            │         │        │
T1.1.4 (SNTP)                   │         │        │
    │                            │         │        │
T1.1.5 (ACK) ←── T0.8.2         │         │        │
                                 │         │        │
T1.2.1 (Event handler) ←────────┼─────────┘        │
T1.2.2 (Backoff) ←── T1.2.1     │                  │
T1.2.3 (NVS WiFi) ←─────────────┼──────────────────┘
                                 │
T1.3.1 (HTTP server) ←──────────┘
T1.3.2 (Config WiFi) ←── T1.3.1, T1.2.3
T1.3.3 (EMBED_FILES) ←── T1.3.2
T1.3.4 (Static files) ←── T1.3.3

T1.4.1 (GET /api/status) ←── T1.3.1
T1.4.2 (GET /api/nodes) ←── T1.3.1, T1.1.3
T1.4.3 (POST /api/config) ←── T1.3.1, T1.2.3

T1.5.1 (Registro nodos) ←── T1.1.3
T1.5.2 (Array de nodos) ←── T1.5.1
T1.5.3 (Nodos en API) ←── T1.5.2, T1.4.2
T1.5.4 (Whitelist) ←── T1.5.2, T1.4.3   [NUEVA]

T1.6.1 (Tarea principal) ←── todo lo anterior
T1.6.2 (Watchdog 90s) ←── T1.6.1         [MODIFICADA]
```

---

## Fase 2: Nodos sensores + ESP-NOW fiable (5 semanas)

### Cambios respecto al plan original

| Cambio | Motivo |
|--------|--------|
| T2.2.1 ELIMINADA (redefinicion protocolo) | Ya esta en Fase 0 (T0.8.2) |
| T2.2.4 ELIMINADA (ACK en gateway) | Ya esta en Fase 1 (T1.1.5) |
| Agregar T2.1.4: Crear sensor_manager.c (simulado) | Fase 3 lo asume existente |
| Agregar T2.4.4: WiFi AP connect bajo demanda | Necesario para OTA en Fase 5 |
| T2.1.2: Canal por discovery, no hardcoded | Correccion DC-06 |
| Filtrado de sensor: dentro de un wake cycle | Correccion del diseno de T3.5.2 |

### Tareas eliminadas (ya implementadas en fases anteriores)

- **T2.2.1** (Definir estructuras protocolo) → Ya en T0.8.2
- **T2.2.4** (ACK desde gateway) → Ya en T1.1.5

### Tareas nuevas

**T2.1.4 (nueva): Crear sensor_manager.c con interfaz generica**
- Crear `sensor_manager.h` con interfaz:
  ```c
  esp_err_t sensor_manager_init(sensor_type_t type);
  esp_err_t sensor_manager_read(float *values, uint8_t *count);
  sensor_type_t sensor_manager_get_type(void);
  ```
- Implementacion simulada: genera valores aleatorios dentro de rangos realistas
  - TEMPERATURE: 18.0 - 28.0 C
  - PH: 6.0 - 8.5
  - DO: 4.0 - 12.0 mg/L
- El tipo de sensor se configura via Kconfig (menuconfig)
- En Fase 3, la implementacion simulada se reemplaza por drivers reales
- Dependencias: T2.1.1
- Tiempo estimado: 2-3 horas

**T2.4.4 (nueva): WiFi AP connect bajo demanda**
- Implementar funcion `espnow_node_wifi_connect(const char *ssid, const char *pass, uint8_t channel)`
- Al recibir MSG_TYPE_OTA_PREPARE por ESP-NOW:
  1. Extraer SSID, password y canal del mensaje
  2. Guardar en NVS para futuros intentos
  3. Desactivar ESP-NOW temporalmente
  4. Conectarse al AP del gateway en modo STA
  5. Timeout de 5 minutos: si no se completa OTA, desconectar y volver al ciclo normal
- NO se usa en Fase 2 (solo se implementa y testea). Se activa en Fase 5
- Dependencias: T2.1.2, T2.2.3
- Tiempo estimado: 4-5 horas

### Tareas modificadas

**T2.1.2 (modificada): Discovery de canal WiFi**
- En cold boot, antes de inicializar ESP-NOW:
  1. Iniciar WiFi en modo STA
  2. Ejecutar `esp_wifi_scan_start()` buscando el SSID del gateway (configurable via Kconfig)
  3. Si encuentra el AP: extraer canal, guardar en RTC memory y NVS
  4. Si no encuentra: usar ultimo canal conocido (NVS) o default (canal 1)
  5. Configurar ESP-NOW en el canal descubierto
- En wake por timer: usar canal de RTC memory (sin scan)
- Agregar `ap_channel` a `node_state_t` (RTC memory)
- Tiempo estimado extra: +1 hora sobre el original

**T2.3.1 (modificada): node_state_t ampliado**
```c
typedef struct {
    uint16_t sequence_number;
    uint16_t fail_count;
    uint16_t sleep_interval_sec;
    uint32_t total_boots;
    uint32_t total_send_ok;
    uint32_t total_send_fail;
    uint8_t  discovery_done;
    uint8_t  ap_channel;         // canal WiFi descubierto [NUEVO]
} node_state_t;
```

### Numeracion de tareas ajustada (Semana 2)

Con T2.2.1 y T2.2.4 eliminadas, la semana 2 queda:
- T2.2.2: Implementar serializacion (usa protocolo de T0.8.2)
- T2.2.3: Envio con numero de secuencia
- T2.2.5: Reintento con ACK (depende de T1.1.5 en vez de T2.2.4)

---

## Fase 3: Sensores y actuadores (3 semanas)

### Cambios respecto al plan original

| Cambio | Motivo |
|--------|--------|
| T3.1.2 dice "modificar" sensor_manager.c, no "crear" | Ya existe desde T2.1.4 |
| Filtrado de lecturas: multiples en un wake, no entre wakes | Correccion de diseno |
| Nombres de enum consistentes con protocolo | SENSOR_TYPE_TEMPERATURE, no SENSOR_TYPE_TEMP |

### Correccion de filtrado (T3.5.2)

**Original**: Acumular lecturas en RTC memory entre ciclos de deep sleep
**Corregido**: Tomar multiples lecturas en UN solo wake cycle

```
Ciclo de lectura (en un solo wake):
1. Leer sensor 5 veces con 100ms entre lecturas (500ms total)
2. Descartar min y max (outliers)
3. Promediar los 3 valores restantes
4. Enviar el promedio por ESP-NOW
5. Deep sleep
```

Motivo: acumular una lectura de hace 5 minutos con una de ahora no tiene sentido fisico. La temperatura del agua puede cambiar 2 grados en 5 minutos. Filtrar dentro de un wake cycle (500ms) captura ruido del sensor, no variacion real del medio.

---

## Fase 4: Dashboard Embebido + API REST (4 semanas)

### Cambios respecto al plan original

| Cambio | Motivo |
|--------|--------|
| QoS corregido: QoS 1 para datos, QoS 2 solo para comandos | QoS 2 para suscripciones es innecesario |
| Tabla sensor_readings incluye gateway_id | Preparacion multi-gateway |
| Validacion JSON con schema basico | Evitar crashes por payloads malformados |

### Schema de DB corregido

```sql
CREATE TABLE sensor_readings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    gateway_id TEXT NOT NULL,        -- NUEVO: identificador del gateway
    node_mac TEXT NOT NULL,
    sensor_type TEXT NOT NULL,
    value REAL NOT NULL,
    unit TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    time_synced BOOLEAN DEFAULT 1,   -- NUEVO: false si timestamp es uptime
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE nodes (
    mac TEXT PRIMARY KEY,
    gateway_id TEXT NOT NULL,        -- NUEVO
    sensor_type TEXT,
    firmware_version TEXT,
    last_seen INTEGER,
    battery_pct INTEGER,
    status TEXT DEFAULT 'active',
    sleep_interval INTEGER
);
```

### QoS corregido

| Topic | Direccion | QoS | Motivo |
|-------|-----------|-----|--------|
| .../nodo/{id}/temperatura | gateway -> broker | 1 | Datos sensor, tolera duplicados |
| .../nodo/{id}/status | gateway -> broker | 1 | Estado, se sobreescribe |
| .../alertas | gateway -> broker | 1 | Alertas, toleran duplicados |
| .../control/actuador/{id} | broker -> gateway | 2 | Comandos, exactly-once importa |
| .../config/nodo/{id} | broker -> gateway | 2 | Configuracion, exactly-once |

---

## Fase 5: OTA + Seguridad + Pruebas (5 semanas)

### Cambios respecto al plan original

| Cambio | Motivo |
|--------|--------|
| Agregar T5.4.5: NVS encryption | Credenciales en texto plano es un riesgo |
| WDT vs OTA confirmation: validar | Evitar que WDT mate el proceso de confirmacion |
| Agregar T5.5.1: Tests unitarios con Unity | No hay tests en ninguna fase |
| Condiciones de rollback definidas | Original no especifica que cuenta como "falla" |
| Nodo OTA usa T2.4.4 | Capacidad WiFi ya implementada |

### Tareas nuevas

**T5.4.5 (nueva): NVS encryption**
- Habilitar NVS encryption usando flash encryption key
- Generar clave en eFuse (documentar que es irreversible)
- Migrar datos existentes de NVS a formato cifrado
- Documentar proceso de recovery
- Tiempo estimado: 4-6 horas

**T5.5.1 (nueva): Tests unitarios con Unity**
- Crear directorio `test/` en gateway y node
- Tests para protocolo: serializar/deserializar todos los tipos de mensaje
- Tests para validacion: mensajes invalidos, buffer overflow, version incorrecta
- Tests para NVS config: set/get/default
- Ejecutar con `idf.py -T test` en el host (no necesita hardware)
- Tiempo estimado: 6-8 horas

### Condiciones de rollback OTA (definidas)

El nuevo firmware debe cumplir TODAS estas condiciones en 120 segundos (no 60) para confirmarse:
1. NVS inicializado correctamente
2. WiFi APSTA arrancado (AP visible)
3. ESP-NOW inicializado
4. HTTP server respondiendo en puerto 80
5. Si STA configurado: al menos un intento de conexion (no necesita exito)

Si alguna condicion falla → no se llama a `esp_ota_mark_app_valid()` → reboot → bootloader revierte a la particion anterior.

Timeout cambiado de 60s a 120s para dar margen al init completo (WiFi + MQTT + SNTP).

---

## Fase 6: Mejoras futuras (continuo)

### Cambios respecto al plan original

| Cambio | Motivo |
|--------|--------|
| Relay estatico ELIMINADO como feature existente | Nunca se implemento en v1.0 |
| Relay estatico agregado como T6.1 (nuevo) | Se implementa aqui, no se asume |
| Cifrado ESP-NOW por software agregado como T6.2 | Alternativa al limite de 6 peers cifrados |

### Tareas corregidas

**T6.1 (corregida): Relay estatico**
- Texto original: "En v1.0 ya existe relay estatico"
- Correccion: "Implementar relay estatico como feature nueva"
- Un nodo intermedio puede reenviar mensajes de otro nodo al gateway
- Configuracion via MSG_TYPE_CONFIG desde el gateway
- El nodo relay no modifica el mensaje, solo lo reenvia

**T6.2 (nueva): Cifrado por software**
- Implementar AES-256-GCM a nivel de payload (independiente de ESP-NOW encryption)
- Clave compartida derivada de un PSK configurado en NVS
- Esto elimina el limite de 6 peers cifrados del hardware
- Overhead: ~16 bytes de tag + ~16 bytes de IV = 32 bytes extra por mensaje
- Payload efectivo: 250 - 13 (header) - 32 (crypto) = 205 bytes (suficiente)

**T6.3 (nueva): Migracion a MQTT Sparkplug B**
- Objetivo formativo: tras dominar MQTT plano en Fase 4, adoptar el estandar industrial IIoT
- Subtareas:
  1. Integrar nanopb como componente ESP-IDF en el gateway
  2. Generar los .c/.h desde el .proto de Sparkplug B (`sparkplug_b.proto`)
  3. Implementar state machine Sparkplug en `mqtt_bridge.c`:
     - NBIRTH al conectar el gateway al broker
     - NDEATH como LWT (Will Message) con Protobuf
     - DBIRTH por cada nodo sensor registrado
     - DDEATH cuando un nodo deja de reportar (timeout)
     - DDATA para datos de sensores (reemplaza los publish custom de Fase 4)
     - NCMD/DCMD para comandos desde el servidor
  4. Cambiar topic namespace: `spBv1.0/{mqtt_ns}/{msg_type}/{gateway_id}/{node_id}`
  5. Adaptar el servidor (Bun/Node) para decodificar payloads Protobuf Sparkplug
  6. Adaptar el dashboard para consumir la nueva estructura de datos
  7. Mantener retrocompatibilidad temporal: flag en NVS para elegir MQTT plano o Sparkplug B
- Los nodos sensores NO requieren cambios (hablan ESP-NOW, no MQTT)
- Dependencias: T4.1.2 (MQTT bridge funcionando), T6.1 (relay, si se implemento)
- Tiempo estimado: 12-16 horas (gateway 6-8h, servidor 3-4h, dashboard 3-4h)

---

## Tabla de dependencias inter-fase

```
Fase 0                    Fase 1                    Fase 2
───────                   ───────                   ───────
T0.8.2 (protocolo) ──────> T1.1.5 (ACK) ──────────> T2.2.5 (retry)
T0.8.2 ──────────────────> T1.5.1 (registro) ─────> T2.4.1 (discovery)
T0.2.1 (blink gw) ───────> T1.1.2 (WiFi)
T0.5.2 (ESP-NOW rx) ─────> T1.1.3 (espnow init)
T0.6.1 (APSTA) ──────────> T1.1.2 (WiFi APSTA)

Fase 2                    Fase 3                    Fase 4
───────                   ───────                   ───────
T2.1.4 (sensor_mgr) ────> T3.1.2 (drivers reales)
T2.2.5 (retry) ─────────> (se usa sin cambios)
T2.4.4 (WiFi dormido) ──────────────────────────> (se usa en Fase 5)

Fase 4                    Fase 5
───────                   ───────
T4.1.2 (MQTT bridge) ───> T5.1.2 (OTA via MQTT trigger)
T4.2.1 (Mosquitto) ─────> T5.4.2 (MQTT TLS)
T4.3.1 (DB schema) ─────> T5.3.1 (stress test)
```

## Estimaciones ajustadas

Las tareas que la auditoria identifico como subestimadas:

| Tarea | Original | Ajustada | Motivo |
|-------|----------|----------|--------|
| T1.5.2 (registro nodos) | 3-4h | 6-8h | Thread safety + testing multi-nodo |
| T3.2.3 (calibracion UI) | 3-4h | 6-8h | State machine UART + NVS + testing |
| T4.3.4 (WebSocket server) | 3-4h | 6-8h | Subscription filtering + disconnect handling |
| T5.3.1 (stress test) | 4-6h | 8-10h | Setup + ejecucion real + analisis |
| T1.1.5 (ACK gateway) [nueva] | - | 3-4h | - |
| T1.5.4 (whitelist) [nueva] | - | 4-5h | - |
| T2.1.4 (sensor_manager) [nueva] | - | 2-3h | - |
| T2.4.4 (WiFi dormido) [nueva] | - | 4-5h | - |
| T5.4.5 (NVS encryption) [nueva] | - | 4-6h | - |
| T5.5.1 (tests Unity) [nueva] | - | 6-8h | - |
