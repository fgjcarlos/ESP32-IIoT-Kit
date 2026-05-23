# Protocolo ESP-NOW Unificado (Source of Truth)

Este documento es la UNICA referencia del protocolo de mensajeria. Se implementa en `firmware/common/protocol/` y se usa sin modificacion en todas las fases.

## Principios de diseno

1. **Packed structs**: todas las estructuras usan `__attribute__((packed))` para eliminar padding
2. **Little-endian**: ambos ESP32 (S3 y C3) son little-endian. No se hace conversion de byte order
3. **Max 250 bytes**: limite de ESP-NOW por frame. Todas las estructuras deben caber
4. **Header comun**: todos los mensajes comparten la misma cabecera de 13 bytes
5. **Versionado**: campo `version` en la cabecera para compatibilidad futura

## Tipos de mensaje

```c
typedef enum {
    MSG_TYPE_DATA       = 0x01,  // Datos de sensor (nodo -> gateway)
    MSG_TYPE_ACK        = 0x02,  // Confirmacion (gateway -> nodo)
    MSG_TYPE_DISCOVERY  = 0x03,  // Auto-registro (nodo -> gateway)
    MSG_TYPE_CONFIG     = 0x04,  // Configuracion (gateway -> nodo)
    MSG_TYPE_OTA_PREPARE = 0x05, // Preparar OTA (gateway -> nodo)
} msg_type_t;
```

## Tipos de sensor

```c
typedef enum {
    SENSOR_TYPE_TEMPERATURE = 0x01,
    SENSOR_TYPE_PH          = 0x02,
    SENSOR_TYPE_DO          = 0x03,  // Oxigeno disuelto
    SENSOR_TYPE_ORP         = 0x04,
    SENSOR_TYPE_LEVEL       = 0x05,
    SENSOR_TYPE_TURBIDITY   = 0x06,
    SENSOR_TYPE_CUSTOM      = 0xFF,  // Tipo de sensor personalizado para sensores definidos por el usuario.
                                     // La interpretacion de values[] es especifica de la aplicacion.
} sensor_type_t;
```

La funcion `protocol_sensor_type_str()` retorna `"CUSTOM"` para el valor `0xFF`. Los valores existentes (0x01-0x06) son inmutables y no deben modificarse para mantener compatibilidad del protocolo.

## Cabecera comun (13 bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;        // 1 byte  - PROTOCOL_VERSION (actualmente 1)
    uint8_t  msg_type;       // 1 byte  - msg_type_t
    uint8_t  node_mac[6];    // 6 bytes - MAC del nodo origen
    uint16_t sequence;       // 2 bytes - numero de secuencia (little-endian)
    uint8_t  reserved;       // 1 byte  - reservado para flags futuros
    uint16_t payload_len;    // 2 bytes - longitud del payload en bytes
} msg_header_t;              // Total: 13 bytes
```

Validacion con _Static_assert:
```c
_Static_assert(sizeof(msg_header_t) == 13, "Header must be 13 bytes");
```

**Nota**: Se elimino `timestamp` de la cabecera. El timestamp se incluye en los payloads que lo necesitan. Motivo: el nodo no tiene hora real (no tiene SNTP), asi que su timestamp es solo uptime. El gateway agrega el timestamp real al procesar.

## Mensajes

### MSG_TYPE_DATA (nodo -> gateway)

```c
#define PROTOCOL_MAX_VALUES 4

typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint8_t      sensor_type;        // 1 byte  - sensor_type_t
    uint8_t      value_count;        // 1 byte  - cantidad de valores validos (1-4)
    float        values[PROTOCOL_MAX_VALUES]; // 16 bytes - lecturas del sensor
    uint32_t     uptime_ms;          // 4 bytes - milisegundos desde ultimo boot
    uint8_t      battery_pct;        // 1 byte  - porcentaje bateria (0-100, 0xFF = sin bateria)
    int8_t       rssi_last;          // 1 byte  - RSSI del ultimo ACK recibido
} msg_sensor_data_t;                 // Total: 13 + 24 = 37 bytes
```

Uso de `values[]`:
- Para TEMPERATURE: values[0] = grados Celsius. value_count = 1
- Para PH: values[0] = pH, values[1] = mV raw. value_count = 2
- Para DO: values[0] = mg/L, values[1] = temperatura compensacion. value_count = 2
- Para LEVEL: values[0] = centimetros. value_count = 1

### MSG_TYPE_ACK (gateway -> nodo)

```c
typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint16_t     ack_sequence;       // 2 bytes - sequence del mensaje confirmado
    uint32_t     gateway_timestamp;  // 4 bytes - epoch seconds del gateway (0 si no sincronizado)
    uint8_t      gateway_channel;    // 1 byte  - canal WiFi actual del gateway
} msg_ack_t;                         // Total: 13 + 7 = 20 bytes
```

**Nota**: `gateway_channel` se incluye en el ACK para que el nodo pueda ajustar su canal si cambio. Esto resuelve el problema de desincronizacion de canal.

### MSG_TYPE_DISCOVERY (nodo -> gateway)

```c
typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint32_t     firmware_version;   // 4 bytes - ej. 0x00010000 = v1.0.0
    uint8_t      sensor_type;        // 1 byte  - tipo de sensor principal
    uint16_t     sleep_interval_sec; // 2 bytes - intervalo de deep sleep actual
    uint8_t      battery_pct;        // 1 byte  - porcentaje bateria
} msg_discovery_t;                   // Total: 13 + 8 = 21 bytes
```

### MSG_TYPE_CONFIG (gateway -> nodo)

```c
typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint16_t     new_sleep_interval; // 2 bytes - nuevo intervalo en segundos (0 = no cambiar)
    uint8_t      flags;              // 1 byte  - bit 0: force_discovery, bit 1: reboot
} msg_config_t;                      // Total: 13 + 3 = 16 bytes
```

### MSG_TYPE_OTA_PREPARE (gateway -> nodo)

```c
typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint8_t      ap_ssid[32];        // 32 bytes - SSID del AP del gateway
    uint8_t      ap_ssid_len;        // 1 byte   - longitud real del SSID
    uint8_t      ap_password[64];    // 64 bytes - password del AP
    uint8_t      ap_password_len;    // 1 byte   - longitud real del password
    uint8_t      ap_channel;         // 1 byte   - canal WiFi del AP
} msg_ota_prepare_t;                 // Total: 13 + 99 = 112 bytes
```

## Funciones del protocolo

```c
// Constantes
#define PROTOCOL_VERSION      1
#define PROTOCOL_MAX_PAYLOAD  237  // 250 - 13 (header)
#define PROTOCOL_MAX_VALUES   4

// Serializacion (struct -> bytes)
int protocol_serialize(const void *msg, msg_type_t type, uint8_t *buf, size_t buf_len);

// Deserializacion (bytes -> struct)
esp_err_t protocol_deserialize_header(const uint8_t *buf, size_t len, msg_header_t *header);
esp_err_t protocol_deserialize(const uint8_t *buf, size_t len, msg_type_t type, void *msg);

// Utilidades
void protocol_fill_header(msg_header_t *header, msg_type_t type,
                          const uint8_t *mac, uint16_t seq, uint16_t payload_len);
const char *protocol_msg_type_str(msg_type_t type);
const char *protocol_sensor_type_str(sensor_type_t type);
bool protocol_validate(const uint8_t *buf, size_t len);
```

## Validacion

Un mensaje es valido si:
1. `len >= sizeof(msg_header_t)` (al menos 13 bytes)
2. `header.version == PROTOCOL_VERSION`
3. `header.msg_type` es un valor conocido del enum
4. `header.payload_len == len - sizeof(msg_header_t)`
5. Para MSG_TYPE_DATA: `value_count >= 1 && value_count <= PROTOCOL_MAX_VALUES`

## Flujo tipico

```
COLD BOOT:
  Nodo                              Gateway
   |--- MSG_DISCOVERY (broadcast) --->|
   |<-- MSG_ACK (con channel) --------|  (gateway registra nodo)

CICLO NORMAL:
  Nodo                              Gateway
   |--- MSG_DATA (unicast) ---------->|
   |<-- MSG_ACK (con timestamp) ------|
   |    (deep sleep)                  |

CONFIGURACION REMOTA:
  Nodo                              Gateway
   |--- MSG_DATA -------------------->|
   |<-- MSG_CONFIG (nuevo intervalo) -|
   |    (aplica config, deep sleep)   |

OTA:
  Gateway                            Nodo
   |--- MSG_OTA_PREPARE (via ESP-NOW)->|
   |    (nodo se conecta a WiFi AP)    |
   |<-- HTTP GET /ota/node.bin --------|
   |    (nodo aplica OTA, reinicia)    |
```

## Tabla de tamanos

| Mensaje | Tamano | Cabe en ESP-NOW |
|---------|--------|-----------------|
| msg_header_t | 13 bytes | - |
| msg_sensor_data_t | 37 bytes | Si (250 max) |
| msg_ack_t | 20 bytes | Si |
| msg_discovery_t | 21 bytes | Si |
| msg_config_t | 16 bytes | Si |
| msg_ota_prepare_t | 112 bytes | Si |

## Espacio de Nombres MQTT

El prefijo del topico MQTT es configurable por NVS para que la plataforma sirva cualquier dominio sin recompilar.

### Patron de topico

```
{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}
```

| Campo | Descripcion |
|-------|-------------|
| `mqtt_ns` | Namespace configurable. Se almacena en NVS con clave `"mqtt_namespace"`. Valor por defecto: `"iiot-kit"`. |
| `gateway_id` | Identificador unico del gateway (derivado del MAC o configurado manualmente). |
| `nodo_id` | Direccion MAC del nodo sensor (formato `AA:BB:CC:DD:EE:FF`). |
| `tipo_sensor` | Nombre del tipo de sensor en minusculas (ej. `temperature`, `ph`, `do`, `orp`, `level`, `turbidity`, `custom`). |

### Ejemplos de topicos

Con `mqtt_ns = "iiot-kit"` (valor por defecto):
```
iiot-kit/GW-001/nodo/AA:BB:CC:DD:EE:FF/temperature
iiot-kit/GW-001/alertas
iiot-kit/GW-001/control/actuador/0
```

Con `mqtt_ns = "greenhouse"` (instalacion de invernadero):
```
greenhouse/GW-001/nodo/AA:BB:CC:DD:EE:FF/temperature
greenhouse/GW-001/alertas
```

### Configuracion en NVS

```c
// Leer el namespace MQTT desde NVS (con fallback al valor por defecto)
nvs_handle_t h;
nvs_open("mqtt", NVS_READONLY, &h);
char mqtt_ns[32] = "iiot-kit";  // valor por defecto
size_t ns_len = sizeof(mqtt_ns);
nvs_get_str(h, "mqtt_namespace", mqtt_ns, &ns_len);
nvs_close(h);
```

**Nota**: El prefijo historico `piscifactoria/` era el namespace por defecto en versiones anteriores del proyecto. Con la transformacion a ESP32-IIoT-Kit, ese prefijo se convierte en un ejemplo de namespace especifico de dominio. Ver `examples/fish-farm/mqtt-topics.md` para la configuracion completa del dominio de acuicultura (`mqtt_ns = "fish-farm"`).
