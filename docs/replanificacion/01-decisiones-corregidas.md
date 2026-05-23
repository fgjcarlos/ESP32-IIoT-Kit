# Decisiones Corregidas

## DC-01: ESP-NOW sin cifrado en v1.0

**Original**: 20 peers cifrados con AES-128
**Corregido**: 20 peers SIN cifrar en v1.0

**Motivo**: El hardware ESP-NOW soporta maximo 6 peers cifrados (con LMK/PMK). El plan original asumia 20 cifrados, lo cual es incorrecto. Con 6 peers cifrados, el sistema solo soportaria 6 nodos — insuficiente para una instalacion IIoT.

**Mitigacion de seguridad sin cifrado**:
- El rango de ESP-NOW es limitado (~200m linea de vista), lo que reduce la superficie de ataque
- En v1.0, la seguridad se basa en la validacion de protocolo (version, msg_type valido, MAC conocida)
- Para v1.1+: evaluar cifrado selectivo (6 nodos criticos cifrados + resto sin cifrar)
- Para v2.0: implementar cifrado a nivel de payload (AES-256 en software, independiente del limite de peers)

**Impacto en fases**:
- Fase 2: `peer_info.encrypt = false` en todas las configuraciones
- Fase 5: Agregar validacion de MAC whitelist como alternativa a cifrado
- Fase 6: Planificar cifrado por software como mejora futura

---

## DC-02: Protocolo definido UNA sola vez en Fase 0

**Original**: Fase 0 (T0.8.2) define `sensor_data_t` con campos fijos. Fase 2 (T2.2.1) redefine como `msg_sensor_data_t` con header + values[].
**Corregido**: El protocolo se define en Fase 0 (T0.8.2) usando la estructura de Fase 2 (que es superior). Fase 2 lo USA, no lo redefine.

**Motivo**: Dos definiciones contradictorias del mismo protocolo. La version de Fase 2 es mejor (header generico + tipos de mensaje extensibles), asi que se adopta esa como unica.

**Ver**: [02-protocolo-unificado.md](02-protocolo-unificado.md)

---

## DC-03: Nodos con WiFi dormido para OTA

**Original**: Nodos solo usan WiFi para la radio ESP-NOW, nunca se conectan a un AP.
**Corregido**: Nodos incluyen codigo para conectarse al AP del gateway, pero solo se activa bajo comando ESP-NOW del gateway (para OTA).

**Motivo**: Fase 5 requiere que los nodos se conecten al AP del gateway para recibir OTA via HTTP. Sin esta capacidad, OTA de nodos es imposible.

**Implementacion**:
- Fase 2: Agregar tarea T2.4.4 "WiFi AP connect bajo demanda"
  - El nodo almacena el SSID/password del AP del gateway en NVS (recibido via ESP-NOW CONFIG)
  - Al recibir un comando MSG_TYPE_OTA_PREPARE, el nodo se conecta al AP en vez de dormir
  - Timeout de 5 minutos: si no recibe OTA, desconecta y vuelve al ciclo normal
- Fase 5: Usa esta capacidad para ejecutar OTA

---

## DC-04: Gateway envia ACK desde Fase 1

**Original**: ACK se implementa en Fase 2 (T2.2.4) modificando el gateway.
**Corregido**: ACK se implementa en Fase 1 como parte de espnow_manager.

**Motivo**: Fase 2 tiene dependencia circular: el nodo espera ACK (T2.2.5) que requiere que el gateway lo envie (T2.2.4), pero ambas son de la misma fase y el gateway se supone "terminado" en Fase 1.

**Implementacion**:
- Fase 1: Agregar tarea T1.1.5 "Gateway envia ACK por ESP-NOW"
  - Al recibir un paquete con header valido, responder con msg_ack_t
  - Incluir sequence_ack y timestamp del gateway
- Fase 2: T2.2.4 se elimina (ya esta en Fase 1), T2.2.5 depende de T1.1.5

---

## DC-05: Watchdog timeout a 90 segundos

**Original**: 30 segundos
**Corregido**: 90 segundos

**Motivo**: El backoff de reconexion WiFi puede llegar a 60s, SNTP sync puede tardar 30s. Con WDT de 30s, operaciones normales pueden disparar un reset.

**Calculo**:
- Operacion mas larga posible: WiFi reconnect (60s) + SNTP sync (30s) = 90s secuencial
- Con WDT de 90s hay margen. Cada subsistema debe alimentar el WDT en su propio loop
- Alternativa: usar Task WDT por tarea en vez de WDT global (mas granular)

---

## DC-06: Canal WiFi con discovery, no hardcoded

**Original**: Canal ESP-NOW hardcoded en el nodo (ej. canal 1)
**Corregido**: Nodo descubre el canal del gateway en cada cold boot

**Motivo**: Si el gateway se conecta a un router en canal 6, su AP y ESP-NOW cambian a canal 6 automaticamente. El nodo con canal 1 hardcoded queda incomunicado.

**Implementacion**:
1. En cold boot, el nodo hace un scan WiFi rapido buscando el SSID del AP del gateway
2. Del scan obtiene el canal actual del AP
3. Configura ESP-NOW en ese canal
4. Almacena el canal en RTC memory para no escanear en cada wake (solo en cold boot)
5. Si falla el scan (AP no encontrado), usa el ultimo canal conocido (NVS) o el default (canal 1)

**Trade-off**: El scan consume tiempo (~2s) y energia, pero solo ocurre en cold boot, no en cada wake.

---

## DC-07: SNTP validado antes de usar timestamps

**Original**: `time(NULL)` se usa sin verificar que SNTP sincronizo
**Corregido**: Agregar funcion `bool sntp_is_synced(void)` y validar antes de usar timestamps

**Implementacion**:
- En Fase 1 (T1.1.4): exponer `sntp_is_synced()` que retorna true si `sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED`
- Antes de escribir timestamps en MQTT o DB: verificar sync
- Si no esta sincronizado: usar uptime (millis desde boot) como timestamp temporal, y marcarlo con flag `synced: false` en el JSON

---

## DC-08: NVS encryption en Fase 5

**Original**: NVS almacena credenciales en texto plano
**Corregido**: Agregar tarea de NVS encryption en Fase 5

**Implementacion**:
- Fase 5 (nueva tarea T5.4.5): Habilitar NVS encryption
  - Generar clave de encriptacion en eFuse (one-time, irreversible)
  - Configurar particion NVS como encrypted
  - Re-escribir credenciales existentes en formato cifrado
  - Documentar proceso de recovery si se pierde la clave

---

## DC-09: DB schema con gateway_id

**Original**: Tabla `sensor_readings` sin columna `gateway_id`
**Corregido**: Agregar `gateway_id TEXT NOT NULL` a la tabla

**Motivo**: El topic MQTT ya incluye gateway_id (`{mqtt_ns}/{gateway_id}/nodo/...`). Si a futuro hay multiples gateways, la DB debe distinguirlos. Agregarlo ahora es gratis.

---

## DC-10: Validacion basica de peers ESP-NOW

**Original**: Gateway acepta cualquier MAC como peer al primer mensaje
**Corregido**: Gateway mantiene whitelist en NVS (modificable via API/web)

**Implementacion**:
- Modo OPEN (v1.0 default): acepta cualquier nodo pero lo marca como "pendiente de aprobacion"
- El dashboard web muestra nodos pendientes para que el usuario los apruebe
- Los nodos aprobados se guardan en NVS (whitelist)
- Nodos no aprobados no se procesan pero no se bloquean (se loguean)
- Modo STRICT (configurable): solo procesa nodos en la whitelist

---

## DC-11: Eliminacion de relay fantasma

**Original**: Fase 6 referencia "relay estatico" como feature existente en v1.0
**Corregido**: Eliminado. Fase 6 describe relay como feature NUEVA a implementar.

---

## DC-12: sensor_manager.c se crea en Fase 2

**Original**: Fase 3 referencia sensor_manager.c como existente sin que ninguna fase lo cree
**Corregido**: Fase 2 (nueva tarea T2.1.4) crea sensor_manager.c con interfaz generica

**Implementacion**:
- T2.1.4 crea la interfaz: `sensor_manager_init()`, `sensor_manager_read()`, `sensor_manager_get_type()`
- En Fase 2, la implementacion es simulada (valores aleatorios)
- En Fase 3, se reemplaza la implementacion simulada por drivers reales

---

## DC-13: Migracion de MQTT plano a MQTT Sparkplug B en Fase 6

**Original**: MQTT plano con topics custom como solucion definitiva
**Corregido**: MQTT plano como paso formativo (Fase 4), migracion a Sparkplug B como evolucion (Fase 6)

**Motivo**: Sparkplug B es el estandar de facto en IIoT/SCADA. La arquitectura gateway/nodos mapea directamente al modelo Edge of Network Node / Devices de Sparkplug. Sin embargo, implementarlo sin entender MQTT base primero elimina el valor didactico. El tutorial sigue un camino formativo: primero los fundamentos (MQTT plano), luego el estandar industrial (Sparkplug B).

**Mapeo arquitectonico**:
- EoN Node (Edge of Network) = Gateway ESP32-S3
- Devices = Nodos sensores (representados por el gateway en el broker)
- NBIRTH/NDEATH = estado del gateway
- DBIRTH/DDEATH/DDATA = estado y datos de cada nodo sensor

**Implementacion**:
- Fase 4: MQTT plano con topics custom — se aprende publish/subscribe, QoS, retain, LWT
- Fase 6 (nueva tarea T6.3): Migracion a Sparkplug B
  - Integracion de nanopb (Protobuf lite para C) en el gateway
  - State machine Sparkplug: NBIRTH, NDEATH, DBIRTH, DDEATH, NDATA, DDATA, NCMD, DCMD
  - Topics estandarizados: `spBv1.0/{group_id}/{msg_type}/{edge_node_id}/{device_id}`
  - Payloads Protobuf con metricas tipadas
  - Birth/Death certificates automaticos
  - Adaptacion del servidor y dashboard para consumir Sparkplug B
- Los nodos sensores NO se ven afectados (hablan ESP-NOW, no MQTT)

**Impacto en recursos del gateway**:
- Flash: nanopb anade ~20-30KB al firmware
- RAM: buffers de serializacion Protobuf (~1-2KB por mensaje)
- ESP32-S3 con 8MB flash y 2MB PSRAM soporta esto sin problema
