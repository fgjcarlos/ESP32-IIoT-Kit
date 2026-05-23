# Fase 2: Nodos Sensores + ESP-NOW Fiable

> **CORRECCIONES APLICADAS** (2026-05-23): T2.2.1 ELIMINADA (protocolo ya definido en Fase 0). T2.2.4 ELIMINADA (ACK movido a Fase 1 como T1.1.5). Se agrega T2.1.4 (sensor_manager.c simulado), T2.4.4 (WiFi AP connect para OTA). T2.1.2 usa discovery de canal en vez de hardcode. Duracion ajustada a 5 semanas. Ver detalles en `docs/replanificacion/03-fases-corregidas.md`.

## Resumen

| Campo | Valor |
|-------|-------|
| **Objetivo** | Crear firmware generico para nodos ESP32-C3 con ciclo deep sleep, protocolo de mensajeria fiable sobre ESP-NOW, y auto-discovery |
| **Duracion estimada** | 4 semanas |
| **Prerequisitos** | Fase 0 y Fase 1 completadas |
| **Hardware necesario** | ESP32-C3-DevKitM (2-3 unidades), ESP32-S3 con firmware gateway de Fase 1 |
| **Total de tareas** | 14 |

## Dependencias externas

- **ESP-IDF v5.x** instalado y configurado para target `esp32c3`
- **Gateway de Fase 1** funcionando y flasheado en el ESP32-S3 (WiFi APSTA + ESP-NOW activos)
- **Monitor serie** disponible para ambos dispositivos (gateway y al menos un nodo)
- Recordar ejecutar `idf.py set-target esp32c3` en el proyecto del nodo

## Indice de tareas

| Codigo | Tarea | Dificultad | Semana |
|--------|-------|------------|--------|
| T2.1.1 | Crear estructura proyecto nodo | Basico | 1 |
| T2.1.2 | Inicializacion WiFi y ESP-NOW en nodo | Intermedio | 1 |
| T2.1.3 | Ciclo de vida completo del nodo | Intermedio | 1 |
| T2.2.1 | Definir estructuras del protocolo | Intermedio | 2 |
| T2.2.2 | Implementar serializacion del protocolo | Intermedio | 2 |
| T2.2.3 | Envio con numero de secuencia | Intermedio | 2 |
| T2.2.4 | Mecanismo ACK desde el gateway | Intermedio | 2 |
| T2.2.5 | Reintento en el nodo | Avanzado | 2 |
| T2.3.1 | Usar RTC memory para estado | Intermedio | 3 |
| T2.3.2 | Detectar causa de wake-up | Basico | 3 |
| T2.3.3 | Modo ahorro por fallos consecutivos | Intermedio | 3 |
| T2.4.1 | Mensaje de discovery | Intermedio | 4 |
| T2.4.2 | Procesamiento de discovery en gateway | Intermedio | 4 |
| T2.4.3 | Comando de configuracion remota | Avanzado | 4 |

---

## Sub-tarea 2.1: Firmware base del nodo (Semana 1)

**Objetivo del sub-grupo**: Tener un ESP32-C3 que arranca, se conecta por ESP-NOW, y entra en deep sleep de forma ciclica.

---

### Tarea T2.1.1: Crear estructura proyecto nodo

- **Dificultad**: Basico
- **Descripcion**:
  Crear el proyecto ESP-IDF para el nodo sensor ESP32-C3 desde cero. El objetivo es tener un ciclo minimo funcional: el chip arranca, imprime un log por UART, y entra en deep sleep durante 10 segundos. Al despertar, repite el ciclo.

  Pasos:
  1. Crear la carpeta `firmware/node/` con la estructura estandar de proyecto ESP-IDF (`main/`, `CMakeLists.txt`, etc.)
  2. Crear `firmware/node/main/main.c` con la funcion `app_main()`
  3. Dentro de `app_main()`:
     - Imprimir con `ESP_LOGI` el mensaje "Nodo arrancado" y el numero de boot (usar una variable estatica o RTC por ahora)
     - Configurar el temporizador de deep sleep con `esp_sleep_enable_timer_wakeup(10 * 1000000)` (10 segundos en microsegundos)
     - Llamar a `esp_deep_sleep_start()`
  4. Crear el `CMakeLists.txt` del proyecto raiz y el de `main/`
  5. Ejecutar `idf.py set-target esp32c3`
  6. Compilar con `idf.py build` y flashear con `idf.py flash monitor`

- **Archivos a crear/modificar**:
  - `firmware/node/CMakeLists.txt` (crear)
  - `firmware/node/main/CMakeLists.txt` (crear)
  - `firmware/node/main/main.c` (crear)

- **Criterio de aceptacion**:
  - [ ] El proyecto compila sin errores para target `esp32c3`
  - [ ] En el monitor serie se ve "Nodo arrancado" cada ~10 segundos
  - [ ] El consumo durante deep sleep es inferior a 10 uA (si tienes multimetro) o el log muestra claramente el ciclo wake/sleep
  - [ ] La estructura de carpetas sigue la convencion del proyecto (`firmware/node/main/`)

- **Dependencias**: Ninguna (primera tarea de esta fase)

- **Pistas**:
  - `esp_sleep_enable_timer_wakeup(uint64_t time_in_us)` - el argumento esta en **microsegundos**, no milisegundos
  - `esp_deep_sleep_start()` - no retorna nunca; al despertar, el chip ejecuta `app_main()` desde el principio
  - `ESP_LOGI(TAG, "mensaje")` - usa un `#define TAG "NODE"` o similar al inicio del archivo
  - Ejemplo oficial: `examples/system/deep_sleep/` en el repositorio de ESP-IDF

- **Errores comunes**:
  1. **Confundir microsegundos con milisegundos**: `esp_sleep_enable_timer_wakeup(10000)` son solo 10 milisegundos, no 10 segundos. Necesitas `10 * 1000000` (10 millones de microsegundos)
  2. **Olvidar `idf.py set-target esp32c3`**: si compilas para el target equivocado (esp32 o esp32s3), el binario no arrancara en el C3

- **Tiempo estimado**: 1-2 horas

---

### Tarea T2.1.2: Inicializacion WiFi y ESP-NOW en nodo

- **Dificultad**: Intermedio
- **Descripcion**:
  Inicializar la capa WiFi y ESP-NOW en el nodo ESP32-C3. El WiFi se inicia en modo STA (estacion) pero **no se conecta a ningun router**: solo se activa la radio para que ESP-NOW pueda funcionar. Despues se configura ESP-NOW y se añade el gateway como peer.

  Pasos:
  1. Crear los archivos `firmware/node/main/espnow_node.c` y `firmware/node/main/espnow_node.h`
  2. En `espnow_node.h`, declarar:
     - `esp_err_t espnow_node_init(void)` - inicializa WiFi + ESP-NOW
     - `esp_err_t espnow_node_send(const uint8_t *data, size_t len)` - envia datos al gateway
     - `void espnow_node_deinit(void)` - limpieza antes de dormir
  3. En `espnow_node.c`, implementar `espnow_node_init()`:
     - Inicializar NVS con `nvs_flash_init()` (WiFi lo necesita internamente)
     - Inicializar el event loop con `esp_event_loop_create_default()`
     - Inicializar WiFi: `esp_netif_init()`, `esp_wifi_init()` con config por defecto, `esp_wifi_set_mode(WIFI_MODE_STA)`, `esp_wifi_start()`
     - **No llamar** a `esp_wifi_connect()` - no queremos conectarnos a un router
     - Fijar el canal WiFi al mismo que usa el gateway (por ahora, hardcoded, por ejemplo canal 1): `esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)`
     - Inicializar ESP-NOW: `esp_now_init()`
     - Registrar callback de recepcion: `esp_now_register_recv_cb(espnow_node_recv_cb)`
     - Registrar callback de envio: `esp_now_register_send_cb(espnow_node_send_cb)`
     - Añadir el gateway como peer: rellenar `esp_now_peer_info_t` con la MAC del gateway (hardcoded por ahora) y llamar a `esp_now_add_peer()`
  4. Implementar los callbacks (por ahora solo logear):
     - `espnow_node_recv_cb`: logear que se recibio un paquete
     - `espnow_node_send_cb`: logear si el envio fue exitoso o no
  5. Actualizar `main.c` para llamar a `espnow_node_init()` despues del log inicial
  6. Actualizar `main/CMakeLists.txt` para incluir `espnow_node.c`

- **Archivos a crear/modificar**:
  - `firmware/node/main/espnow_node.c` (crear)
  - `firmware/node/main/espnow_node.h` (crear)
  - `firmware/node/main/main.c` (modificar - añadir llamada a init)
  - `firmware/node/main/CMakeLists.txt` (modificar - añadir source)

- **Criterio de aceptacion**:
  - [ ] WiFi arranca en modo STA sin conectarse a ningun router
  - [ ] ESP-NOW se inicializa correctamente (sin errores en el log)
  - [ ] El gateway aparece como peer registrado
  - [ ] Si envias un paquete de prueba (bytes cualquiera) con `espnow_node_send()`, el callback de envio reporta exito o fallo
  - [ ] El monitor serie del **gateway** muestra que recibe el paquete (si el gateway de Fase 1 tiene el callback de recepcion activo)

- **Dependencias**: T2.1.1

- **Pistas**:
  - `esp_wifi_set_mode(WIFI_MODE_STA)` - modo STA es suficiente para ESP-NOW, no necesitas AP en el nodo
  - `esp_now_add_peer()` - necesitas rellenar `peer_info.channel`, `peer_info.ifidx = WIFI_IF_STA`, y `peer_info.encrypt = false` (cifrado lo añadiremos mas adelante)
  - Para obtener la MAC del gateway: en el gateway ejecuta `esp_wifi_get_mac(WIFI_IF_AP, mac)` y apunta los 6 bytes
  - El canal WiFi del nodo **debe coincidir** con el del gateway. Si el gateway usa canal 6, el nodo debe usar canal 6
  - Ejemplo oficial: `examples/wifi/espnow/` en el repositorio de ESP-IDF

- **Errores comunes**:
  1. **No inicializar NVS antes de WiFi**: WiFi almacena datos de calibracion en NVS internamente. Si no llamas a `nvs_flash_init()` primero, `esp_wifi_init()` fallara con `ESP_ERR_NVS_NOT_INITIALIZED`
  2. **Canal WiFi diferente al del gateway**: ESP-NOW solo funciona si ambos dispositivos estan en el mismo canal. Si no ves los paquetes llegar al gateway, verifica el canal con `esp_wifi_get_channel()`

- **Tiempo estimado**: 3-4 horas

---

### Tarea T2.1.3: Ciclo de vida completo del nodo

- **Dificultad**: Intermedio
- **Descripcion**:
  Integrar todo en un ciclo de vida completo que se ejecuta **en una sola pasada** (sin bucle FreeRTOS). El nodo despierta, inicializa todo, lee un sensor simulado, envia los datos por ESP-NOW, espera brevemente un ACK (sin implementar ACK real todavia), y se duerme. Todo ocurre dentro de `app_main()`.

  Pasos:
  1. Refactorizar `main.c` para que `app_main()` siga este flujo exacto:
     ```
     wake -> init NVS -> init WiFi/ESP-NOW -> leer sensor (simulado) -> enviar ESP-NOW -> esperar 500ms -> deep sleep
     ```
  2. Para el sensor simulado: generar un valor flotante aleatorio entre 18.0 y 28.0 (simulando temperatura). Puedes usar `esp_random() % 100` para generar variacion
  3. Enviar los bytes del float por ESP-NOW usando `espnow_node_send()`
  4. Despues de enviar, esperar 500ms con `vTaskDelay(pdMS_TO_TICKS(500))` para dar tiempo al callback de envio
  5. Llamar a `espnow_node_deinit()` para limpiar ESP-NOW y WiFi antes de dormir
  6. Configurar deep sleep con el temporizador y llamar a `esp_deep_sleep_start()`
  7. Verificar que el ciclo completo (wake a sleep) tarda menos de 2 segundos

  **Importante**: No hay bucle `while(1)`. La funcion `app_main()` se ejecuta una vez y termina con `esp_deep_sleep_start()`. Al despertar, el chip reinicia y vuelve a ejecutar `app_main()` desde el principio.

- **Archivos a crear/modificar**:
  - `firmware/node/main/main.c` (modificar - refactorizar flujo completo)
  - `firmware/node/main/espnow_node.c` (modificar - implementar `deinit` y mejorar `send`)

- **Criterio de aceptacion**:
  - [ ] El nodo ejecuta el ciclo completo: wake -> init -> enviar -> sleep en menos de 2 segundos
  - [ ] En el monitor serie se ve: log de arranque, log de envio, log de sleep
  - [ ] El gateway recibe los datos enviados por el nodo (visible en el monitor del gateway)
  - [ ] No hay bucle `while(1)` en `app_main()`: la funcion termina con `esp_deep_sleep_start()`
  - [ ] Despues de cada deep sleep, el nodo repite el ciclo automaticamente

- **Dependencias**: T2.1.2

- **Pistas**:
  - `esp_deep_sleep_start()` no retorna nunca. Todo lo que haya despues de esa linea no se ejecuta
  - `esp_now_deinit()` y `esp_wifi_stop()` deben llamarse antes de dormir para limpiar recursos
  - El deep sleep del ESP32-C3 reinicia completamente el chip: toda la RAM normal se pierde. Solo sobreviven las variables en RTC memory y NVS
  - Para el sensor simulado: `float temp = 18.0f + (float)(esp_random() % 1000) / 100.0f;` da un valor entre 18.0 y 28.0

- **Errores comunes**:
  1. **Poner un bucle `while(1)` en `app_main()`**: en un nodo con deep sleep, el ciclo de vida es una sola ejecucion. Si pones un bucle, nunca llegaras al deep sleep y consumiras bateria innecesariamente
  2. **No esperar al callback de envio**: si llamas a `esp_deep_sleep_start()` inmediatamente despues de `esp_now_send()`, el paquete puede no haberse enviado aun. Necesitas un breve delay o un mecanismo de sincronizacion

- **Tiempo estimado**: 2-3 horas

---

### Checkpoint 2.1

Antes de continuar con el sub-grupo 2.2, verifica lo siguiente:

- [ ] El nodo ESP32-C3 ejecuta ciclos de deep sleep automaticamente (visible en monitor serie)
- [ ] Cada vez que despierta, envia datos por ESP-NOW al gateway
- [ ] El gateway recibe los datos (visible en el monitor serie del gateway)
- [ ] El ciclo completo (wake -> enviar -> sleep) dura menos de 2 segundos
- [ ] El proyecto del nodo compila sin warnings para target `esp32c3`

**Test manual**: Deja el nodo corriendo 5 minutos con sleep de 10 segundos. Deberias ver ~30 ciclos en el monitor serie sin ningun error ni cuelgue.

---

## Sub-tarea 2.2: Protocolo de mensajeria (Semana 2)

**Objetivo del sub-grupo**: Definir un protocolo binario sobre ESP-NOW con cabecera, serializacion, numeros de secuencia, ACKs desde el gateway, y reintentos en el nodo.

---

### Tarea T2.2.1: Definir estructuras del protocolo

- **Dificultad**: Intermedio
- **Descripcion**:
  Crear el archivo de cabecera compartido que define todas las estructuras del protocolo de mensajeria. Este archivo sera usado tanto por el nodo como por el gateway, y vive en la carpeta `firmware/common/protocol/`.

  Pasos:
  1. Crear (o modificar si ya existe) `firmware/common/protocol/protocol.h`
  2. Definir el enum `msg_type_t` con los tipos de mensaje:
     ```c
     typedef enum {
         MSG_TYPE_DATA      = 0x01,  // Datos de sensor (nodo -> gateway)
         MSG_TYPE_ACK       = 0x02,  // Confirmacion (gateway -> nodo)
         MSG_TYPE_DISCOVERY = 0x03,  // Auto-registro (nodo -> gateway)
         MSG_TYPE_CONFIG    = 0x04,  // Configuracion (gateway -> nodo)
     } msg_type_t;
     ```
  3. Definir el enum `sensor_type_t` con tipos de sensor:
     ```c
     typedef enum {
         SENSOR_TYPE_TEMPERATURE = 0x01,
         SENSOR_TYPE_PH          = 0x02,
         SENSOR_TYPE_DO          = 0x03,  // Oxigeno disuelto
         SENSOR_TYPE_ORP         = 0x04,
         SENSOR_TYPE_LEVEL       = 0x05,
     } sensor_type_t;
     ```
  4. Definir la cabecera comun (13 bytes):
     ```c
     typedef struct __attribute__((packed)) {
         uint8_t  msg_type;       // 1 byte  - msg_type_t
         uint8_t  node_mac[6];    // 6 bytes - MAC del nodo origen
         uint16_t sequence;       // 2 bytes - numero de secuencia
         uint32_t timestamp;      // 4 bytes - segundos desde epoch
     } msg_header_t;              // Total: 13 bytes
     ```
  5. Definir las estructuras de cada tipo de mensaje:
     - `msg_sensor_data_t`: header + `sensor_type` (1B) + `value_count` (1B) + `values[4]` (16B max, array de floats)
     - `msg_ack_t`: header + `sequence_ack` (2B) + `timestamp_gateway` (4B)
     - `msg_discovery_t`: header + `firmware_version` (4B, ej. 0x00020001 para v2.0.1) + `sensor_type` (1B) + `sleep_interval_sec` (2B)
     - `msg_config_t`: header + `new_sleep_interval_sec` (2B) + `flags` (1B, para futuras opciones)
  6. Añadir `#define PROTOCOL_VERSION 1` y `#define PROTOCOL_MAX_VALUES 4`
  7. Verificar que el tamaño total de la estructura mas grande no supere 250 bytes (limite ESP-NOW)

  **Nota sobre `__attribute__((packed))`**: Esto le dice al compilador que no añada bytes de padding entre los campos de la estructura. Sin esto, el compilador podria insertar bytes vacios para alinear los campos, y los datos serializados no coincidirian entre nodo y gateway.

- **Archivos a crear/modificar**:
  - `firmware/common/protocol/protocol.h` (crear o modificar)

- **Criterio de aceptacion**:
  - [ ] Todas las estructuras tienen `__attribute__((packed))`
  - [ ] `sizeof(msg_header_t)` == 13 bytes (verificar con un `ESP_LOGI` o `_Static_assert`)
  - [ ] `sizeof(msg_sensor_data_t)` < 250 bytes
  - [ ] Los enums cubren todos los tipos de mensaje y sensor definidos
  - [ ] El archivo compila sin warnings tanto para `esp32c3` (nodo) como para `esp32s3` (gateway)

- **Dependencias**: Ninguna (puede hacerse en paralelo con T2.1.x)

- **Pistas**:
  - Usa `_Static_assert(sizeof(msg_header_t) == 13, "Header must be 13 bytes")` para verificar tamaños en tiempo de compilacion
  - `__attribute__((packed))` es especifico de GCC (que es el compilador de ESP-IDF), asi que funciona sin problemas
  - Usa `#pragma once` o include guards (`#ifndef PROTOCOL_H`) al inicio del archivo
  - Los valores `uint16_t` y `uint32_t` se almacenan en **little-endian** en ESP32 (tanto S3 como C3), asi que no necesitas conversion de byte order si ambos extremos son ESP32

- **Errores comunes**:
  1. **Olvidar `__attribute__((packed))`**: sin esto, `sizeof(msg_header_t)` podria ser 16 en vez de 13 debido al padding del compilador. El gateway deserializaria basura
  2. **Definir arrays de tamaño variable en las estructuras**: ESP-NOW necesita saber el tamaño exacto del paquete. Usa arrays de tamaño fijo (ej. `float values[4]`) y un campo `value_count` para indicar cuantos son validos

- **Tiempo estimado**: 2-3 horas

---

### Tarea T2.2.2: Implementar serializacion del protocolo

- **Dificultad**: Intermedio
- **Descripcion**:
  Implementar funciones para serializar (struct -> bytes) y deserializar (bytes -> struct) los mensajes del protocolo. Dado que tanto el nodo (ESP32-C3) como el gateway (ESP32-S3) son little-endian, la serializacion se puede hacer con `memcpy` directo. Sin embargo, encapsulamos en funciones para tener validacion y un punto unico de cambio.

  Pasos:
  1. Crear `firmware/common/protocol/protocol.c`
  2. Implementar las funciones de serializacion:
     - `int protocol_serialize_sensor_data(const msg_sensor_data_t *msg, uint8_t *buf, size_t buf_len)`: copia la struct al buffer. Retorna el numero de bytes escritos, o -1 si el buffer es demasiado pequeño
     - `int protocol_serialize_ack(const msg_ack_t *msg, uint8_t *buf, size_t buf_len)`: idem para ACK
     - `int protocol_serialize_discovery(const msg_discovery_t *msg, uint8_t *buf, size_t buf_len)`: idem para discovery
     - `int protocol_serialize_config(const msg_config_t *msg, uint8_t *buf, size_t buf_len)`: idem para config
  3. Implementar las funciones de deserializacion:
     - `esp_err_t protocol_deserialize_header(const uint8_t *buf, size_t len, msg_header_t *header)`: extrae solo la cabecera (util para routing)
     - `esp_err_t protocol_deserialize_sensor_data(const uint8_t *buf, size_t len, msg_sensor_data_t *msg)`: extrae mensaje completo de datos
     - `esp_err_t protocol_deserialize_ack(const uint8_t *buf, size_t len, msg_ack_t *msg)`: extrae ACK
  4. Implementar funciones auxiliares:
     - `const char *protocol_msg_type_to_str(msg_type_t type)`: devuelve el nombre del tipo de mensaje como string (para logs)
     - `void protocol_fill_header(msg_header_t *header, msg_type_t type, const uint8_t *mac, uint16_t seq)`: rellena una cabecera con los datos proporcionados
  5. Añadir las declaraciones de todas las funciones a `protocol.h`

- **Archivos a crear/modificar**:
  - `firmware/common/protocol/protocol.c` (crear)
  - `firmware/common/protocol/protocol.h` (modificar - añadir declaraciones de funciones)

- **Criterio de aceptacion**:
  - [ ] Serializar una `msg_sensor_data_t` y deserializarla produce los mismos valores
  - [ ] Las funciones validan que el buffer tenga tamaño suficiente antes de escribir/leer
  - [ ] `protocol_deserialize_header()` puede extraer el tipo de mensaje sin deserializar el cuerpo completo
  - [ ] Todas las funciones retornan codigos de error claros (`ESP_OK`, `ESP_ERR_INVALID_SIZE`, etc.)
  - [ ] Test manual: crear una struct, serializar, deserializar, comparar con `memcmp` -> todo igual

- **Dependencias**: T2.2.1

- **Pistas**:
  - Para serializar con `memcpy`: `memcpy(buf, msg, sizeof(*msg))` funciona porque las estructuras son packed y ambos ESP32 son little-endian
  - Siempre valida `buf_len >= sizeof(msg_sensor_data_t)` antes de hacer `memcpy`
  - `protocol_deserialize_header()` es muy util: permite al gateway mirar el `msg_type` sin saber que tipo de mensaje es, y luego llamar al deserializador especifico
  - Incluye `<string.h>` para `memcpy` y `memcmp`

- **Errores comunes**:
  1. **No validar el tamaño del buffer**: si el buffer de recepcion de ESP-NOW tiene menos bytes de los esperados (por ejemplo, un paquete corrupto), un `memcpy` sin verificacion leera memoria fuera del buffer -> comportamiento indefinido
  2. **Asumir que ESP-NOW siempre entrega exactamente los bytes enviados**: en condiciones normales si, pero siempre verifica `len >= sizeof(struct_esperada)` antes de deserializar

- **Tiempo estimado**: 3-4 horas

---

### Tarea T2.2.3: Envio con numero de secuencia

- **Dificultad**: Intermedio
- **Descripcion**:
  Añadir un contador de secuencia al nodo que se incrementa con cada envio y sobrevive al deep sleep. Este numero permite al gateway detectar paquetes duplicados (mismo sequence recibido dos veces) o perdidos (saltos en el sequence number).

  Pasos:
  1. En `firmware/node/main/main.c` (o en un nuevo archivo `power_manager.c`), declarar una variable en RTC memory:
     ```c
     RTC_DATA_ATTR static uint16_t s_sequence_number = 0;
     ```
  2. Cada vez que el nodo envia un mensaje, usar `s_sequence_number` como el campo `sequence` de la cabecera
  3. Despues de enviar, incrementar `s_sequence_number++`
  4. Al rellenar la cabecera del mensaje, usar `protocol_fill_header()` con el sequence number actual
  5. Refactorizar `espnow_node_send()` para que acepte una `msg_sensor_data_t` completa (en vez de bytes crudos) y la serialice internamente
  6. Verificar que el sequence number se incrementa entre ciclos de deep sleep (visible en logs)

- **Archivos a crear/modificar**:
  - `firmware/node/main/main.c` (modificar - declarar `RTC_DATA_ATTR` variable, usar protocolo)
  - `firmware/node/main/espnow_node.c` (modificar - refactorizar `send` para usar protocolo)

- **Criterio de aceptacion**:
  - [ ] El sequence number se incrementa en cada ciclo de deep sleep (visible en logs: seq=0, seq=1, seq=2...)
  - [ ] Despues de un reset por alimentacion (desconectar y reconectar USB), el sequence vuelve a 0
  - [ ] El gateway puede leer el sequence number del paquete recibido (extraer la cabecera y logear `header.sequence`)
  - [ ] No hay desbordamiento problematico: `uint16_t` permite hasta 65535 secuencias antes de volver a 0 (suficiente)

- **Dependencias**: T2.1.3, T2.2.2

- **Pistas**:
  - `RTC_DATA_ATTR` es una macro de ESP-IDF que coloca la variable en la memoria RTC, que **sobrevive al deep sleep** pero **no a un reset de alimentacion**
  - En ESP32-C3, la macro es `RTC_DATA_ATTR` igual que en otros ESP32
  - Para verificar que la variable sobrevive: mira el log. Si ves seq=0 en cada arranque, la variable NO esta en RTC memory (probablemente te falta `RTC_DATA_ATTR`)
  - Para el timestamp: por ahora usa 0 o `esp_timer_get_time() / 1000000` (tiempo desde el ultimo boot, no hora real)

- **Errores comunes**:
  1. **Declarar la variable RTC dentro de una funcion local**: `RTC_DATA_ATTR` solo funciona con variables **estaticas globales** o **estaticas de funcion** (con `static`). Si la declaras como variable local sin `static`, el compilador dara error o la variable no persistira
  2. **Confundir RTC memory con NVS**: RTC memory es RAM que se mantiene activa durante deep sleep (consume algo de energia). NVS es flash persistente. Para un simple contador que cambia cada pocos minutos, RTC memory es mas eficiente (no desgasta la flash)

- **Tiempo estimado**: 2-3 horas

---

### Tarea T2.2.4: Mecanismo ACK desde el gateway

- **Dificultad**: Intermedio
- **Descripcion**:
  Modificar el gateway (ESP32-S3, firmware de Fase 1) para que, al recibir un paquete de tipo DATA por ESP-NOW, responda automaticamente con un mensaje ACK al nodo emisor. El ACK incluye el numero de secuencia recibido (para que el nodo sepa que ese paquete en concreto fue recibido) y el timestamp actual del gateway.

  Pasos:
  1. Incluir `protocol.h` en el proyecto del gateway. Opciones:
     - Copiar los archivos de `firmware/common/protocol/` a `firmware/gateway/components/protocol/`
     - O crear un symlink / referencia CMake a la carpeta comun
  2. En `firmware/gateway/main/espnow_manager.c`, modificar el callback de recepcion ESP-NOW:
     - Deserializar la cabecera del paquete recibido con `protocol_deserialize_header()`
     - Si `msg_type == MSG_TYPE_DATA`:
       a. Deserializar el mensaje completo con `protocol_deserialize_sensor_data()`
       b. Logear los datos del sensor (tipo, valor, secuencia)
       c. Construir un `msg_ack_t` con:
          - `msg_type = MSG_TYPE_ACK`
          - `sequence_ack = header.sequence` (el sequence del paquete recibido)
          - `timestamp_gateway = (uint32_t)time(NULL)` o el timestamp SNTP si esta disponible
       d. Serializar el ACK y enviarlo por ESP-NOW a la MAC del nodo emisor
  3. Para enviar el ACK, el gateway necesita tener al nodo como peer. Opciones:
     - Añadir el nodo como peer dinamicamente al recibir el primer mensaje
     - O usar `esp_now_send()` con la MAC del nodo (si no es peer, añadirlo primero con `esp_now_add_peer()`)
  4. Verificar que el ACK llega al nodo (visible en el callback de recepcion del nodo)

- **Archivos a crear/modificar**:
  - `firmware/gateway/main/espnow_manager.c` (modificar - añadir logica de ACK)
  - `firmware/gateway/main/espnow_manager.h` (modificar si es necesario)
  - `firmware/gateway/components/protocol/` (crear o enlazar desde common)
  - `firmware/gateway/CMakeLists.txt` (modificar si se añade el componente protocol)

- **Criterio de aceptacion**:
  - [ ] Al recibir un mensaje DATA, el gateway envia un ACK en menos de 50ms
  - [ ] El ACK contiene el sequence number correcto del mensaje recibido
  - [ ] El nodo recibe el ACK (visible en el callback `espnow_node_recv_cb` del nodo)
  - [ ] El gateway logea los datos del sensor recibido (tipo, valor, secuencia, MAC del nodo)
  - [ ] Si el nodo envia multiples paquetes, cada uno recibe su ACK con el sequence correcto

- **Dependencias**: T2.2.2, Gateway de Fase 1

- **Pistas**:
  - El callback de recepcion ESP-NOW en ESP-IDF v5.x tiene la firma: `void (*esp_now_recv_cb_t)(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)`
  - `recv_info->src_addr` contiene la MAC del emisor (6 bytes) - usala para enviar el ACK de vuelta
  - Para añadir un peer dinamicamente: verifica primero con `esp_now_is_peer_exist()` antes de llamar a `esp_now_add_peer()`
  - El envio del ACK ocurre **dentro del callback de recepcion**. Esto es rapido (no bloquea) pero ten cuidado con el tamaño del stack del callback

- **Errores comunes**:
  1. **No añadir el nodo como peer antes de enviar el ACK**: `esp_now_send()` falla si la MAC de destino no esta registrada como peer. Debes llamar a `esp_now_add_peer()` primero
  2. **Usar `time(NULL)` sin haber sincronizado SNTP**: si SNTP no esta configurado, `time(NULL)` devuelve 0 o un valor incorrecto. Para esta tarea, es aceptable enviar 0 como timestamp si SNTP no esta disponible

- **Tiempo estimado**: 3-4 horas

---

### Tarea T2.2.5: Reintento en el nodo

- **Dificultad**: Avanzado
- **Descripcion**:
  Implementar un mecanismo de reintento fiable en el nodo: envia el paquete, espera un ACK durante 500ms. Si no llega el ACK, reintenta hasta 3 veces. Si los 3 intentos fallan, logea el error y se duerme igualmente. Este es el mecanismo central de fiabilidad del protocolo.

  El reto principal es que el callback de recepcion ESP-NOW se ejecuta en un contexto diferente (tarea del sistema WiFi), asi que necesitas un mecanismo de sincronizacion para "despertar" al hilo principal cuando llega el ACK.

  Pasos:
  1. Crear un `EventGroupHandle_t` global en `espnow_node.c`:
     ```c
     static EventGroupHandle_t s_espnow_event_group;
     #define ACK_RECEIVED_BIT BIT0
     ```
  2. Crear el event group en `espnow_node_init()`:
     ```c
     s_espnow_event_group = xEventGroupCreate();
     ```
  3. En el callback de recepcion `espnow_node_recv_cb()`:
     - Deserializar la cabecera del paquete recibido
     - Si `msg_type == MSG_TYPE_ACK`:
       a. Deserializar el ACK completo
       b. Verificar que `sequence_ack` coincide con el sequence enviado
       c. Si coincide, activar el bit: `xEventGroupSetBits(s_espnow_event_group, ACK_RECEIVED_BIT)`
  4. Refactorizar `espnow_node_send()` a una nueva funcion `espnow_node_send_reliable()`:
     ```c
     esp_err_t espnow_node_send_reliable(const msg_sensor_data_t *msg, int max_retries) {
         for (int attempt = 0; attempt < max_retries; attempt++) {
             // Limpiar bits previos
             xEventGroupClearBits(s_espnow_event_group, ACK_RECEIVED_BIT);
             // Serializar y enviar
             // ...
             // Esperar ACK con timeout de 500ms
             EventBits_t bits = xEventGroupWaitBits(
                 s_espnow_event_group,
                 ACK_RECEIVED_BIT,
                 pdTRUE,        // limpiar bit al recibirlo
                 pdFALSE,       // no esperar todos los bits
                 pdMS_TO_TICKS(500)
             );
             if (bits & ACK_RECEIVED_BIT) {
                 ESP_LOGI(TAG, "ACK recibido en intento %d", attempt + 1);
                 return ESP_OK;
             }
             ESP_LOGW(TAG, "Timeout ACK, intento %d/%d", attempt + 1, max_retries);
         }
         ESP_LOGE(TAG, "Fallo tras %d intentos", max_retries);
         return ESP_ERR_TIMEOUT;
     }
     ```
  5. En `main.c`, reemplazar la llamada a `espnow_node_send()` por `espnow_node_send_reliable(&msg, 3)`
  6. Guardar el resultado (exito o fallo) para usarlo en tareas posteriores (modo ahorro)

- **Archivos a crear/modificar**:
  - `firmware/node/main/espnow_node.c` (modificar - añadir event group, logica de reintento)
  - `firmware/node/main/espnow_node.h` (modificar - actualizar declaracion de `send_reliable`)
  - `firmware/node/main/main.c` (modificar - usar `send_reliable`)

- **Criterio de aceptacion**:
  - [ ] Con el gateway encendido: el nodo envia, recibe ACK en el primer intento, y se duerme. Log: "ACK recibido en intento 1"
  - [ ] Con el gateway apagado: el nodo reintenta 3 veces (visible en logs), logea error, y se duerme. Tiempo total de espera ~1.5 segundos (3 x 500ms)
  - [ ] Si el gateway se enciende durante los reintentos: el nodo recibe el ACK en el intento correspondiente
  - [ ] El sequence number del ACK se verifica correctamente (un ACK con sequence incorrecto no cuenta)
  - [ ] El event group se crea y destruye sin memory leaks

- **Dependencias**: T2.2.3, T2.2.4

- **Pistas**:
  - `xEventGroupWaitBits()` bloquea la tarea actual (app_main) durante el timeout. El callback de recepcion se ejecuta en otra tarea y puede activar el bit
  - `pdMS_TO_TICKS(500)` convierte 500 milisegundos a ticks de FreeRTOS
  - No olvides incluir `freertos/event_groups.h`
  - El callback de recepcion ESP-NOW se ejecuta en la tarea WiFi. No hagas operaciones largas dentro del callback; solo verifica y activa el bit
  - Limpia el event group antes de cada envio con `xEventGroupClearBits()` para no confundir un ACK antiguo con uno nuevo

- **Errores comunes**:
  1. **No limpiar los bits del event group antes de cada intento**: si el bit quedo activado de un intento anterior (por ejemplo, un ACK que llego tarde), `xEventGroupWaitBits()` retornara inmediatamente sin esperar el nuevo ACK
  2. **Hacer operaciones pesadas en el callback de recepcion**: el callback se ejecuta en el contexto de la tarea WiFi con stack limitado. Solo deserializa la cabecera, compara el sequence, y activa el bit. No logees mensajes largos ni hagas calculos complejos dentro del callback

- **Tiempo estimado**: 4-5 horas

---

### Checkpoint 2.2

Antes de continuar con el sub-grupo 2.3, verifica lo siguiente:

- [ ] El protocolo esta definido en `firmware/common/protocol/` y se usa en ambos proyectos (nodo y gateway)
- [ ] Cada mensaje del nodo lleva un sequence number que se incrementa con cada envio
- [ ] El gateway responde con ACK incluyendo el sequence recibido
- [ ] El nodo espera ACK con timeout de 500ms y reintenta hasta 3 veces
- [ ] Con gateway encendido: envio exitoso en el primer intento (latencia < 50ms tipica)
- [ ] Con gateway apagado: 3 reintentos, log de error, y deep sleep igualmente

**Test de fiabilidad**: Deja el nodo corriendo con sleep de 10 segundos y el gateway encendido durante 10 minutos (~60 ciclos). Verifica en los logs del gateway que:
- Los sequence numbers son consecutivos (0, 1, 2, 3...)
- No hay duplicados
- Todos los ACKs se enviaron correctamente

---

## Sub-tarea 2.3: Deep sleep y persistencia (Semana 3)

**Objetivo del sub-grupo**: Gestionar el estado del nodo a traves de ciclos de deep sleep usando RTC memory, detectar el tipo de arranque, y adaptar el comportamiento segun fallos acumulados.

---

### Tarea T2.3.1: Usar RTC memory para estado

- **Dificultad**: Intermedio
- **Descripcion**:
  Centralizar todas las variables que necesitan sobrevivir al deep sleep en una estructura almacenada en RTC memory. Hasta ahora solo tenemos `s_sequence_number`; ahora añadiremos mas campos para gestionar el estado completo del nodo.

  Pasos:
  1. Crear `firmware/node/main/power_manager.c` y `power_manager.h`
  2. Definir una estructura que agrupe todo el estado persistente:
     ```c
     typedef struct {
         uint16_t sequence_number;      // Contador de secuencia para mensajes
         uint16_t fail_count;           // Envios fallidos consecutivos
         uint16_t sleep_interval_sec;   // Intervalo de deep sleep actual (segundos)
         uint32_t total_boots;          // Numero total de arranques
         uint32_t total_send_ok;        // Envios exitosos totales
         uint32_t total_send_fail;      // Envios fallidos totales
     } node_state_t;
     ```
  3. Declarar la instancia con `RTC_DATA_ATTR`:
     ```c
     RTC_DATA_ATTR static node_state_t s_node_state;
     ```
  4. Implementar funciones de acceso:
     - `node_state_t *power_manager_get_state(void)` - retorna puntero al estado
     - `void power_manager_init_state(void)` - inicializa todos los campos a valores por defecto (solo llamar en cold boot)
     - `void power_manager_enter_deep_sleep(void)` - configura el timer con `sleep_interval_sec` y llama a `esp_deep_sleep_start()`
     - `void power_manager_log_state(void)` - logea el estado actual (util para debug)
  5. Migrar `s_sequence_number` de `main.c` a esta nueva estructura
  6. Actualizar `main.c` para usar `power_manager_get_state()` en vez de la variable directa
  7. Definir valores por defecto: `sleep_interval_sec = 300` (5 minutos), `fail_count = 0`

- **Archivos a crear/modificar**:
  - `firmware/node/main/power_manager.c` (crear)
  - `firmware/node/main/power_manager.h` (crear)
  - `firmware/node/main/main.c` (modificar - usar power_manager)
  - `firmware/node/main/CMakeLists.txt` (modificar - añadir source)

- **Criterio de aceptacion**:
  - [ ] Todas las variables RTC estan agrupadas en `node_state_t`
  - [ ] `total_boots` se incrementa con cada ciclo de deep sleep (visible en logs)
  - [ ] `sequence_number` sigue incrementandose correctamente tras la migracion
  - [ ] Despues de un reset por alimentacion, todos los campos vuelven a sus valores por defecto
  - [ ] `power_manager_log_state()` muestra todos los campos en el monitor serie

- **Dependencias**: T2.2.3

- **Pistas**:
  - El tamaño de RTC memory en ESP32-C3 es de 8KB (RTC FAST memory). Tu estructura ocupa unos ~18 bytes, asi que no hay problema de espacio
  - `RTC_DATA_ATTR` coloca la variable en la seccion `.rtc.data` del linker. Puedes verificarlo en el mapa de memoria (`idf.py size-components`)
  - Las variables `RTC_DATA_ATTR` se inicializan a 0 en un cold boot (reset por alimentacion), **pero no en cada deep sleep wake**
  - Puedes usar `power_manager_log_state()` al principio de `app_main()` para ver el estado justo despues de despertar

- **Errores comunes**:
  1. **Asumir que `RTC_DATA_ATTR` sobrevive a un reset por alimentacion**: NO. Solo sobrevive al deep sleep. Un reset por alimentacion, un reset por boton, o un `esp_restart()` pone los valores a 0. Por eso necesitas detectar el tipo de arranque (tarea siguiente)
  2. **Poner la estructura `RTC_DATA_ATTR` dentro de una funcion sin `static`**: las variables automaticas (locales) no pueden ir en RTC memory. Debe ser `static` o global

- **Tiempo estimado**: 2-3 horas

---

### Tarea T2.3.2: Detectar causa de wake-up

- **Dificultad**: Basico
- **Descripcion**:
  Usar `esp_sleep_get_wakeup_cause()` para distinguir entre un primer arranque (cold boot, sin causa de wake-up) y un despertar por temporizador de deep sleep. Esto es crucial porque en un cold boot necesitamos inicializar las variables RTC a valores por defecto, mientras que en un wake por timer queremos mantener los valores existentes.

  Pasos:
  1. Al inicio de `app_main()`, antes de cualquier otra operacion, llamar a `esp_sleep_get_wakeup_cause()`
  2. Evaluar el resultado:
     ```c
     esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
     switch (cause) {
         case ESP_SLEEP_WAKEUP_TIMER:
             ESP_LOGI(TAG, "Wake-up por timer (deep sleep)");
             // Las variables RTC mantienen sus valores
             break;
         case ESP_SLEEP_WAKEUP_UNDEFINED:
         default:
             ESP_LOGI(TAG, "Cold boot - primer arranque");
             // Inicializar variables RTC a valores por defecto
             power_manager_init_state();
             break;
     }
     ```
  3. En `power_manager_init_state()`, establecer:
     - `sequence_number = 0`
     - `fail_count = 0`
     - `sleep_interval_sec = 300` (5 minutos, o 10 segundos para pruebas)
     - `total_boots = 0`
     - `total_send_ok = 0`
     - `total_send_fail = 0`
  4. Siempre incrementar `total_boots` despues de evaluar la causa (tanto en cold boot como en wake por timer)
  5. Logear la causa del wake-up y el estado actual al principio de cada ciclo

- **Archivos a crear/modificar**:
  - `firmware/node/main/main.c` (modificar - añadir deteccion de wake-up al inicio)
  - `firmware/node/main/power_manager.c` (modificar - implementar `init_state` correctamente)

- **Criterio de aceptacion**:
  - [ ] Al conectar el USB por primera vez: log muestra "Cold boot - primer arranque" y `total_boots = 0`
  - [ ] En los siguientes despertares: log muestra "Wake-up por timer" y `total_boots` se incrementa
  - [ ] Despues de desconectar y reconectar USB: vuelve a mostrar "Cold boot" y `total_boots` vuelve a 0
  - [ ] Las variables RTC se inicializan solo en cold boot, no en cada wake-up

- **Dependencias**: T2.3.1

- **Pistas**:
  - `esp_sleep_get_wakeup_cause()` no necesita ninguna inicializacion previa. Se puede llamar como primera instruccion de `app_main()`
  - En ESP32-C3, las causas de wake-up posibles son: `ESP_SLEEP_WAKEUP_TIMER`, `ESP_SLEEP_WAKEUP_GPIO`, `ESP_SLEEP_WAKEUP_UNDEFINED` (cold boot)
  - El valor `ESP_SLEEP_WAKEUP_UNDEFINED` (== 0) indica que no hubo un wake-up de deep sleep: es un arranque normal (power-on o reset)
  - Incluir `esp_sleep.h` para las constantes y funciones de sleep

- **Errores comunes**:
  1. **Inicializar variables RTC en cada wake-up**: si llamas a `power_manager_init_state()` sin comprobar la causa del wake-up, perderias el sequence_number y el fail_count en cada ciclo
  2. **Olvidar el caso `default` en el switch**: aunque solo esperes TIMER y UNDEFINED, siempre pon un `default` para capturar causas inesperadas (como GPIO si se configura mas adelante)

- **Tiempo estimado**: 1-2 horas

---

### Tarea T2.3.3: Modo ahorro por fallos consecutivos

- **Dificultad**: Intermedio
- **Descripcion**:
  Implementar un mecanismo de ahorro de energia que se activa cuando el nodo no puede comunicarse con el gateway. Si el nodo falla al enviar datos 5 veces consecutivas (no recibe ACK), cambia el intervalo de sleep de su valor normal (5 minutos) a un modo ahorro (30 minutos). Cuando un envio tiene exito, el fail_count se resetea y el intervalo vuelve a la normalidad.

  Pasos:
  1. Definir constantes en `power_manager.h`:
     ```c
     #define POWER_NORMAL_SLEEP_SEC    300   // 5 minutos (modo normal)
     #define POWER_SAVING_SLEEP_SEC    1800  // 30 minutos (modo ahorro)
     #define POWER_MAX_CONSECUTIVE_FAIL 5    // Fallos para activar modo ahorro
     ```
     Para pruebas, puedes usar valores mas cortos (10s normal, 30s ahorro, 3 fallos).
  2. En `main.c`, despues del envio (exitoso o fallido), actualizar el estado:
     - Si `espnow_node_send_reliable()` retorno `ESP_OK`:
       a. Incrementar `total_send_ok`
       b. Resetear `fail_count = 0`
       c. Restaurar `sleep_interval_sec = POWER_NORMAL_SLEEP_SEC`
       d. Logear "Envio exitoso, modo normal"
     - Si retorno error:
       a. Incrementar `total_send_fail`
       b. Incrementar `fail_count`
       c. Si `fail_count >= POWER_MAX_CONSECUTIVE_FAIL`:
          - Cambiar `sleep_interval_sec = POWER_SAVING_SLEEP_SEC`
          - Logear "Modo ahorro activado: demasiados fallos consecutivos"
       d. Logear el fail_count actual
  3. En `power_manager_enter_deep_sleep()`, usar `state->sleep_interval_sec` como duracion del timer
  4. Logear siempre el intervalo de sleep antes de dormir para poder verificar en el monitor

- **Archivos a crear/modificar**:
  - `firmware/node/main/power_manager.h` (modificar - añadir constantes)
  - `firmware/node/main/power_manager.c` (modificar - logica en `enter_deep_sleep`)
  - `firmware/node/main/main.c` (modificar - actualizar estado segun resultado del envio)

- **Criterio de aceptacion**:
  - [ ] Con gateway encendido: `fail_count` siempre es 0, `sleep_interval` es el normal
  - [ ] Con gateway apagado: `fail_count` se incrementa en cada ciclo. Al llegar a 5, el log muestra "Modo ahorro activado" y el sleep pasa a 30 minutos (o el valor de prueba)
  - [ ] Al encender el gateway de nuevo: el primer envio exitoso resetea `fail_count` a 0 y restaura el intervalo normal
  - [ ] Los contadores `total_send_ok` y `total_send_fail` reflejan el historico correcto
  - [ ] El intervalo de sleep se logea antes de cada deep sleep (verificable en monitor)

- **Dependencias**: T2.2.5, T2.3.2

- **Pistas**:
  - La logica es simple: un if/else despues de `send_reliable()`. No sobreingenieres esto
  - `esp_sleep_enable_timer_wakeup()` acepta microsegundos: `state->sleep_interval_sec * 1000000ULL`. Usa `ULL` para evitar overflow del `uint32_t` (300 * 1000000 = 300.000.000, que cabe en uint32, pero 1800 * 1000000 = 1.800.000.000, que tambien cabe, pero es buena practica usar ULL)
  - Para probar rapidamente, usa valores cortos: 10s normal, 30s ahorro, 3 fallos maximos

- **Errores comunes**:
  1. **No resetear `fail_count` cuando un envio tiene exito**: si solo incrementas pero nunca reseteas, el nodo entrara en modo ahorro y nunca volvera al modo normal, incluso cuando el gateway este disponible
  2. **Overflow al calcular microsegundos**: `1800 * 1000000` sin cast a `uint64_t` puede causar overflow en plataformas de 32 bits. Usa `(uint64_t)sleep_interval_sec * 1000000ULL`

- **Tiempo estimado**: 2-3 horas

---

### Checkpoint 2.3

Antes de continuar con el sub-grupo 2.4, verifica lo siguiente:

- [ ] El estado del nodo (sequence, fail_count, sleep_interval, contadores) sobrevive al deep sleep
- [ ] En cold boot, el estado se inicializa a valores por defecto
- [ ] En wake por timer, el estado se preserva
- [ ] Tras 5 fallos consecutivos, el intervalo de sleep cambia al modo ahorro (30 min)
- [ ] Tras un envio exitoso, el modo normal se restaura
- [ ] `power_manager_log_state()` muestra toda la informacion relevante

**Test de modo ahorro**: Apaga el gateway y deja el nodo corriendo con sleep de 10 segundos y umbral de 3 fallos. Verifica que:
1. Los primeros 3 ciclos muestran fail_count = 1, 2, 3
2. A partir del ciclo 4, el sleep cambia a modo ahorro (30s o el valor configurado)
3. Enciende el gateway: en el siguiente ciclo, el envio tiene exito, fail_count vuelve a 0, y el sleep vuelve a 10 segundos

---

## Sub-tarea 2.4: Auto-discovery y configuracion (Semana 4)

**Objetivo del sub-grupo**: Permitir que los nodos se auto-registren en el gateway al arrancar por primera vez, y que el gateway pueda enviar configuracion remota al nodo.

---

### Tarea T2.4.1: Mensaje de discovery

- **Dificultad**: Intermedio
- **Descripcion**:
  En el primer arranque del nodo (cold boot), antes de enviar datos de sensores, el nodo debe enviar un mensaje DISCOVERY al gateway. Este mensaje le dice al gateway: "existo, mi MAC es X, soy un sensor de tipo Y, mi firmware es version Z". El gateway puede usar esta informacion para registrar automaticamente nodos nuevos sin configuracion manual.

  Pasos:
  1. Añadir un flag en RTC memory para saber si el discovery ya se hizo:
     ```c
     // En node_state_t
     uint8_t discovery_done;  // 0 = no enviado, 1 = ya enviado
     ```
  2. En `main.c`, despues de inicializar ESP-NOW y antes de enviar datos, verificar si es necesario un discovery:
     ```c
     node_state_t *state = power_manager_get_state();
     if (!state->discovery_done) {
         // Es un cold boot, enviar discovery
         espnow_node_send_discovery();
     }
     ```
  3. Implementar `espnow_node_send_discovery()` en `espnow_node.c`:
     - Construir un `msg_discovery_t` con:
       - `msg_type = MSG_TYPE_DISCOVERY`
       - `node_mac` = MAC propia del nodo (obtener con `esp_wifi_get_mac()`)
       - `firmware_version = 0x00020000` (v2.0.0, codificado como major.minor.patch en 4 bytes)
       - `sensor_type = SENSOR_TYPE_TEMPERATURE` (hardcoded por ahora)
       - `sleep_interval_sec` = intervalo actual
     - Serializar y enviar con `espnow_node_send_reliable()` (reusar el mecanismo de reintento)
     - Si el envio es exitoso (ACK recibido), marcar `state->discovery_done = 1`
  4. El flag `discovery_done` se resetea a 0 en cold boot (ya que `power_manager_init_state()` lo inicializa) y permanece en 1 durante deep sleep cycles

- **Archivos a crear/modificar**:
  - `firmware/node/main/power_manager.h` (modificar - añadir `discovery_done` al struct)
  - `firmware/node/main/power_manager.c` (modificar - inicializar `discovery_done = 0` en `init_state`)
  - `firmware/node/main/espnow_node.c` (modificar - implementar `send_discovery`)
  - `firmware/node/main/espnow_node.h` (modificar - declarar `send_discovery`)
  - `firmware/node/main/main.c` (modificar - llamar a discovery en cold boot)

- **Criterio de aceptacion**:
  - [ ] En cold boot, el nodo envia un mensaje DISCOVERY antes de enviar datos
  - [ ] El gateway recibe el DISCOVERY y logea la informacion del nodo (MAC, tipo sensor, version firmware)
  - [ ] En los siguientes ciclos (wake por timer), el nodo NO envia DISCOVERY (solo datos)
  - [ ] Si el DISCOVERY falla (gateway apagado), se reintenta en el siguiente ciclo (discovery_done sigue en 0)
  - [ ] Despues de un reset por alimentacion, el DISCOVERY se reenvia

- **Dependencias**: T2.2.5, T2.3.2

- **Pistas**:
  - `esp_wifi_get_mac(WIFI_IF_STA, mac_buf)` obtiene la MAC de la interfaz STA del nodo (6 bytes)
  - El `discovery_done` es un flag simple en RTC memory. Sobrevive al deep sleep pero se resetea en cold boot, que es exactamente el comportamiento deseado
  - El gateway ya tiene logica para recibir y responder ACK (de T2.2.4). Solo necesita distinguir si el mensaje recibido es DATA o DISCOVERY mirando el `msg_type` de la cabecera
  - Para la version de firmware, puedes definir macros: `#define FW_VERSION_MAJOR 2`, `#define FW_VERSION_MINOR 0`, `#define FW_VERSION_PATCH 0`

- **Errores comunes**:
  1. **Enviar DISCOVERY en cada wake-up**: sin el flag `discovery_done`, el nodo enviaria un DISCOVERY en cada ciclo, desperdiciando bateria y llenando los logs del gateway de registros duplicados
  2. **No manejar el fallo del DISCOVERY**: si el gateway esta apagado durante el cold boot, el discovery debe fallar silenciosamente y reintentarse en el siguiente ciclo. No marcar `discovery_done = 1` si el envio fallo

- **Tiempo estimado**: 2-3 horas

---

### Tarea T2.4.2: Procesamiento de discovery en gateway

- **Dificultad**: Intermedio
- **Descripcion**:
  Modificar el gateway para que procese los mensajes DISCOVERY de los nodos y los registre automaticamente en su lista interna. Si el gateway de Fase 1 ya tiene un sistema de registro de nodos (T1.5.2), integrarlo ahi. Si no, crear uno basico.

  Pasos:
  1. En `espnow_manager.c` del gateway, dentro del callback de recepcion, añadir un caso para `MSG_TYPE_DISCOVERY`:
     ```c
     case MSG_TYPE_DISCOVERY: {
         msg_discovery_t discovery;
         protocol_deserialize_discovery(data, data_len, &discovery);
         ESP_LOGI(TAG, "Discovery de nodo: MAC=%02x:%02x:%02x:%02x:%02x:%02x, sensor=%d, fw=0x%08lx",
                  discovery.header.node_mac[0], discovery.header.node_mac[1],
                  discovery.header.node_mac[2], discovery.header.node_mac[3],
                  discovery.header.node_mac[4], discovery.header.node_mac[5],
                  discovery.sensor_type, (unsigned long)discovery.firmware_version);
         // Registrar nodo
         espnow_manager_register_node(&discovery);
         // Responder con ACK
         // ... (reutilizar logica de ACK de T2.2.4)
         break;
     }
     ```
  2. Implementar `espnow_manager_register_node()`:
     - Buscar si la MAC ya esta registrada (evitar duplicados)
     - Si es nueva, añadir a un array interno o lista:
       ```c
       typedef struct {
           uint8_t mac[6];
           sensor_type_t sensor_type;
           uint32_t firmware_version;
           uint32_t last_seen;         // Timestamp del ultimo mensaje
           uint16_t last_sequence;     // Ultima secuencia recibida
           bool is_registered;
       } registered_node_t;

       #define MAX_REGISTERED_NODES 20
       static registered_node_t s_nodes[MAX_REGISTERED_NODES];
       ```
     - Logear "Nuevo nodo registrado" o "Nodo ya conocido"
  3. Al responder con ACK al DISCOVERY, incluir el timestamp actual del gateway. El nodo puede usar este timestamp para tener una referencia de tiempo (sincronizacion basica)
  4. Cada vez que el gateway reciba un DATA de un nodo registrado, actualizar `last_seen` y `last_sequence`
  5. Opcional: crear una funcion `espnow_manager_get_nodes()` para que otros modulos puedan consultar la lista de nodos registrados (util para el servidor web del gateway)

- **Archivos a crear/modificar**:
  - `firmware/gateway/main/espnow_manager.c` (modificar - añadir procesamiento de DISCOVERY y registro de nodos)
  - `firmware/gateway/main/espnow_manager.h` (modificar - declarar `register_node`, `get_nodes`, y la estructura `registered_node_t`)

- **Criterio de aceptacion**:
  - [ ] Al recibir un DISCOVERY, el gateway logea toda la informacion del nodo
  - [ ] El nodo queda registrado en la lista interna del gateway
  - [ ] Si el mismo nodo envia otro DISCOVERY (por reset), no se crea un duplicado (se actualiza el existente)
  - [ ] El gateway responde con ACK al DISCOVERY (el nodo recibe el ACK y marca `discovery_done = 1`)
  - [ ] `espnow_manager_get_nodes()` devuelve la lista de nodos con su informacion actualizada
  - [ ] El campo `last_seen` se actualiza con cada mensaje DATA recibido del nodo

- **Dependencias**: T2.4.1, T2.2.4

- **Pistas**:
  - Para comparar MACs: `memcmp(node->mac, incoming_mac, 6) == 0`
  - `MAX_REGISTERED_NODES = 20` coincide con el limite de peers cifrados de ESP-NOW
  - Si usas un array fijo, busca el primer slot con `is_registered == false` para añadir un nuevo nodo
  - Para el timestamp: si SNTP esta configurado, usa `time(NULL)`. Si no, usa `esp_timer_get_time() / 1000000` (segundos desde boot del gateway)
  - Este registro es en RAM. Si el gateway se reinicia, se pierde. Para persistir, habria que guardarlo en NVS, pero eso es una mejora futura

- **Errores comunes**:
  1. **No verificar duplicados antes de registrar**: si no compruebas la MAC, un nodo que se resetea multiples veces llenaria la lista de entradas duplicadas hasta alcanzar el limite
  2. **Olvidar enviar ACK al DISCOVERY**: el nodo espera un ACK. Si el gateway procesa el DISCOVERY pero no responde, el nodo no marca `discovery_done = 1` y seguira enviando DISCOVERY en cada ciclo

- **Tiempo estimado**: 3-4 horas

---

### Tarea T2.4.3: Comando de configuracion remota

- **Dificultad**: Avanzado
- **Descripcion**:
  Implementar la capacidad del gateway de enviar un mensaje CONFIG al nodo, por ejemplo para cambiar su intervalo de sleep remotamente. El mecanismo mas sencillo es "piggybacking": incluir la configuracion como parte del ACK, o enviar un mensaje CONFIG separado justo despues del ACK, aprovechando la ventana de tiempo en la que el nodo esta despierto y escuchando.

  En esta tarea usaremos un mensaje CONFIG separado enviado justo despues del ACK.

  Pasos:
  1. **En el gateway** (`espnow_manager.c`):
     - Crear una estructura para almacenar configuracion pendiente por nodo:
       ```c
       typedef struct {
           bool pending;                  // Hay config pendiente?
           uint16_t new_sleep_interval;   // Nuevo intervalo de sleep
       } pending_config_t;
       ```
     - Añadir un campo `pending_config_t pending_config` a `registered_node_t`
     - Crear funcion `espnow_manager_set_node_config(const uint8_t *mac, uint16_t new_sleep_interval)` que marca la config como pendiente para ese nodo
     - En el callback de recepcion, despues de enviar el ACK, verificar si hay config pendiente para el nodo emisor. Si la hay:
       a. Construir un `msg_config_t` con el nuevo intervalo
       b. Serializar y enviar por ESP-NOW al nodo
       c. Marcar `pending_config.pending = false`
     - Nota: no hay garantia de que el nodo reciba el CONFIG (no hay ACK del CONFIG). Es "best effort"
  2. **En el nodo** (`espnow_node.c`):
     - En el callback de recepcion, ademas de manejar ACK, añadir un caso para `MSG_TYPE_CONFIG`:
       ```c
       case MSG_TYPE_CONFIG: {
           msg_config_t config;
           protocol_deserialize_config(data, data_len, &config);
           node_state_t *state = power_manager_get_state();
           state->sleep_interval_sec = config.new_sleep_interval_sec;
           ESP_LOGI(TAG, "Config recibida: nuevo sleep interval = %d segundos",
                    config.new_sleep_interval_sec);
           break;
       }
       ```
     - El nuevo intervalo se aplica en el proximo `power_manager_enter_deep_sleep()`
     - Como esta en RTC memory, persiste entre ciclos de deep sleep
  3. **Limitacion a documentar**: el CONFIG se envia justo despues del ACK, pero el nodo solo esta escuchando durante los 500ms de espera de ACK. Si el CONFIG tarda mas de lo que queda de esa ventana, se pierde. En la practica, ESP-NOW es rapido (<10ms), asi que deberia llegar a tiempo
  4. **Para probar**: añadir un comando por UART o por la API REST del gateway que permita invocar `espnow_manager_set_node_config(mac, nuevo_intervalo)`. Por ejemplo, un handler HTTP `POST /api/nodes/{mac}/config` con el nuevo intervalo en el body

- **Archivos a crear/modificar**:
  - `firmware/gateway/main/espnow_manager.c` (modificar - logica CONFIG pendiente y envio)
  - `firmware/gateway/main/espnow_manager.h` (modificar - declarar `set_node_config`, `pending_config_t`)
  - `firmware/node/main/espnow_node.c` (modificar - manejar MSG_TYPE_CONFIG en callback de recepcion)
  - Opcionalmente: `firmware/gateway/main/http_server.c` (modificar - añadir endpoint de configuracion)

- **Criterio de aceptacion**:
  - [ ] Desde el gateway se puede marcar una config pendiente para un nodo especifico (por MAC)
  - [ ] Cuando ese nodo envia el siguiente DATA, el gateway responde con ACK + CONFIG
  - [ ] El nodo recibe el CONFIG y actualiza su `sleep_interval_sec`
  - [ ] En el siguiente ciclo de deep sleep, el nodo duerme con el nuevo intervalo (verificable en logs)
  - [ ] Si no hay config pendiente, solo se envia el ACK (comportamiento normal)
  - [ ] El nuevo intervalo persiste entre ciclos de deep sleep (RTC memory)

- **Dependencias**: T2.4.2, T2.3.1

- **Pistas**:
  - El tiempo entre el envio del ACK y el envio del CONFIG es critico. Envialos con el minimo delay posible. ESP-NOW tarda ~1ms en enviar un paquete
  - Para probar sin API REST: puedes hardcodear en el codigo del gateway un cambio de intervalo despues de recibir N paquetes de un nodo
  - El nodo no envia ACK del CONFIG. Si el CONFIG se pierde, el gateway puede volver a intentarlo en el siguiente ciclo (manteniendo `pending = true` hasta que confirme que el nodo cambio el intervalo)
  - El `flags` en `msg_config_t` esta reservado para futuras opciones (por ejemplo, "forzar discovery", "reiniciar nodo", etc.)

- **Errores comunes**:
  1. **Enviar CONFIG antes del ACK**: el nodo esta esperando un ACK. Si recibe un CONFIG primero, podria confundirse o ignorarlo (depende de tu implementacion). Envia siempre el ACK primero, CONFIG despues
  2. **No considerar la ventana de tiempo**: el nodo solo escucha durante los ~500ms que espera el ACK. Si el gateway tarda demasiado en procesar y enviar el CONFIG, el nodo podria haberse dormido ya. Envia el CONFIG lo mas rapido posible despues del ACK

- **Tiempo estimado**: 4-5 horas

---

### Checkpoint 2.4

Antes de dar por completada la Fase 2, verifica lo siguiente:

- [ ] Un nodo nuevo (cold boot) envia DISCOVERY y queda registrado en el gateway
- [ ] El gateway muestra la lista de nodos registrados con MAC, tipo de sensor, y version de firmware
- [ ] En ciclos normales (wake por timer), el nodo solo envia DATA (no DISCOVERY)
- [ ] El gateway puede enviar configuracion remota (cambio de intervalo de sleep) a un nodo especifico
- [ ] El nodo aplica la nueva configuracion y duerme con el nuevo intervalo

**Test de integracion end-to-end con 2 nodos**: Si tienes 2 ESP32-C3, flashea ambos con el firmware del nodo y verifica que:
1. Ambos envian DISCOVERY al arrancar y quedan registrados en el gateway
2. Ambos envian datos con sequence numbers independientes
3. El gateway responde con ACK a cada uno
4. Puedes cambiar el intervalo de sleep de uno sin afectar al otro

---

## Preguntas de autoevaluacion

Responde estas preguntas antes de pasar a la Fase 3. Si no puedes responder alguna, revisa el sub-grupo correspondiente.

1. **Por que usamos `RTC_DATA_ATTR` en vez de NVS para el sequence number?**
   (Pista: piensa en el desgaste de la flash y la frecuencia de escritura)

2. **Que ocurre si el gateway recibe un paquete con un sequence number que ya recibio antes? Como detectarias un paquete duplicado?**
   (Pista: el gateway guarda `last_sequence` por nodo)

3. **Por que el nodo usa `WIFI_MODE_STA` si no se conecta a ningun router?**
   (Pista: ESP-NOW necesita que la radio WiFi este activa, pero no necesita asociacion)

4. **Que pasa si el gateway cambia de canal WiFi mientras un nodo esta en deep sleep? Como afecta esto a la comunicacion ESP-NOW?**
   (Pista: el nodo tiene el canal hardcoded. Piensa en que pasaria y como solucionarlo)

5. **Por que usamos `__attribute__((packed))` en las estructuras del protocolo? Que pasaria sin el?**
   (Pista: padding del compilador, sizeof diferente, datos corruptos al deserializar)

6. **Cual es la diferencia entre el callback de envio (`send_cb`) y el mecanismo de ACK que implementamos?**
   (Pista: `send_cb` confirma que el hardware envio el frame, no que el receptor lo proceso correctamente)

7. **Por que limitamos a 3 reintentos en vez de reintentar indefinidamente?**
   (Pista: consumo de bateria. Cada intento son ~500ms despierto con la radio activa)

8. **Si un nodo esta en modo ahorro (30 min de sleep) y el gateway quiere enviarle una configuracion, cuando podra hacerlo?**
   (Pista: el gateway solo puede enviar CONFIG cuando el nodo esta despierto y envia un DATA)

---

## Lectura recomendada

Antes y durante esta fase, consulta estos recursos:

- **ESP-NOW en ESP-IDF**: [ESP-NOW Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/network/esp_now.html)
- **Deep sleep en ESP32-C3**: [Sleep Modes](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/sleep_modes.html)
- **Ejemplo oficial ESP-NOW**: `$IDF_PATH/examples/wifi/espnow/` - estudia la estructura del ejemplo, especialmente como inicializa WiFi y ESP-NOW
- **Ejemplo oficial deep sleep**: `$IDF_PATH/examples/system/deep_sleep/` - estudia como detecta la causa del wake-up y usa RTC memory
- **FreeRTOS Event Groups**: [Event Groups API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/freertos_idf.html) - para entender `xEventGroupWaitBits()`
- **ESP32-C3 Technical Reference Manual**: Capitulo sobre RTC memory y power management

---

## Errores frecuentes de esta fase

Estos son los errores mas comunes que cometen los programadores durante la Fase 2. Revisalos antes de empezar y tenlos presentes mientras trabajas.

1. **Variables locales que "desaparecen" entre ciclos de deep sleep**: toda la RAM normal se borra durante deep sleep. Solo `RTC_DATA_ATTR` y NVS persisten. Si tu sequence number siempre es 0, comprueba que esta declarada con `RTC_DATA_ATTR`.

2. **Canal WiFi desincronizado**: ESP-NOW solo funciona si nodo y gateway estan en el mismo canal. Si no recibes paquetes, verifica con `esp_wifi_get_channel()` en ambos dispositivos.

3. **Olvidar NVS antes de WiFi**: `esp_wifi_init()` falla con error criptico si no llamaste a `nvs_flash_init()` antes. Siempre inicializa NVS como primer paso.

4. **Confundir callback de envio con ACK real**: el callback `esp_now_register_send_cb()` se dispara cuando el hardware envia el frame (capa MAC). Esto NO significa que el receptor lo proceso. El ACK de nuestro protocolo es un mensaje explicitico de vuelta.

5. **Deep sleep sin deinit**: si no llamas a `esp_now_deinit()` y `esp_wifi_stop()` antes de `esp_deep_sleep_start()`, el consumo durante deep sleep puede ser mayor al esperado (la radio no se apaga correctamente en algunos casos).

6. **Timeout de ACK demasiado corto**: 500ms es un valor razonable. Valores menores a 100ms pueden causar falsos negativos si hay congestion en el canal WiFi.

7. **Event group no limpiado entre reintentos**: si no llamas a `xEventGroupClearBits()` antes de cada intento de envio, un ACK de un intento anterior puede hacer que el siguiente `xEventGroupWaitBits()` retorne inmediatamente.

8. **Enviar CONFIG antes del ACK**: el nodo esta bloqueado esperando el ACK. Si el gateway envia CONFIG primero, el nodo puede procesarlo como un ACK invalido y descartarlo. Siempre envia el ACK primero.

9. **No manejar `ESP_ERR_NVS_NO_FREE_PAGES`**: la primera vez que inicializas NVS en un chip nuevo, puede fallar con este error. Solucion: borrar la particion NVS con `nvs_flash_erase()` y reintentar `nvs_flash_init()`.

10. **Arrays de tamaño variable en structs packed**: ESP-NOW necesita saber el tamaño exacto del paquete al enviar. Si usas un array flexible (`float values[]` sin tamaño), `sizeof(struct)` no incluye los valores reales. Usa arrays de tamaño fijo (`float values[4]`) con un campo `value_count`.
