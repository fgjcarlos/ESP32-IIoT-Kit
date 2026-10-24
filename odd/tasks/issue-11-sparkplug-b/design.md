# Design — Issue #11: MQTT Sparkplug B opcional en el gateway

> Documento de diseño técnico. NO es spec. La spec SDD con requirements + scenarios se generará aparte si el usuario aprueba el camino OpenSpec.
>
> Estado: **draft — pendiente de aprobación antes de ODD-T3**.

## 0. Resumen ejecutivo

El gateway ESP32-S3-WROOM-1-N16R8 actúa como **Edge of Network (EoN) Node** de Sparkplug B v3.0.0. Cada nodo sensor ESP-NOW se representa como un **Device** del EoN-Node. La traducción se hace en el gateway; los nodos no hablan MQTT.

Cuando la feature está habilitada (CONFIG_SPARKPLUG_B_ENABLED=y), el gateway mantiene un cliente MQTT persistente hacia un broker configurado en NVS, publica mensajes Sparkplug Birth/Data/Death, y traduce comandos NCMD/DCMD a ESP-NOW.

Cuando la feature está deshabilitada (default), `mqtt_bridge_start()` queda en no-op (compatible con el placeholder de Fase 4).

## 1. Decisiones cerradas (validadas con el usuario)

| # | Decisión | Justificación |
|---|----------|---------------|
| D1 | Spec Sparkplug 3.0.0 (OASIS/ISO/IEC) | Alineado con Ignition 8.1+ y Cirrus Link. metric_aliases y bdSeq obligatorios. |
| D2 | Identidades `group_id`, `edge_node_id`, `device_id_template` configurables en NVS con defaults seguros | Default: `group_id="iiot-kit"`, `edge_node_id="GW-<últimos 4 hex MAC>"`, `device_id_template="N-<últimos 4 hex MAC>"`. Sin secretos en código. |
| D3 | Verificación de conformidad con broker Ignition 8.1+ (o Cirrus Link compatible) + publisher Python | El harness es un docker-compose con broker + cliente de prueba. |
| D4 | TLS obligatorio en alcance (sin secretos en repo). CA bundle y client cert desde SPIFFS/NVS | Carga en runtime. Repo solo lleva código que sabe cargar. |
| D5 | Materializar `firmware/common/protocol/protocol.h/.c` (T0.8.2) dentro de esta issue | Cierra una dependencia bloqueante para la traducción ESP-NOW → Sparkplug. |
| D6 | Política DDEATH = expiración fija configurable (N min sin MSG_TYPE_DATA), default 30 min | Simple, determinista. La alternativa adaptativa requiere parsear MSG_TYPE_DISCOVERY (T0.8.3). |
| D7 | Un único componente `firmware/gateway/components/sparkplug_b` con `.proto` + codegen + state machine + cliente MQTT | Menos boilerplate; tests centrados; un solo CMakeLists. |
| D8 | MQTT 3.1.1 only (sin MQTT 5) | Spec no lo exige. Mejor interoperabilidad y menos código. |

## 2. Alcance y no-alcance

### 2.1 Incluido

- Materialización de `firmware/common/protocol/protocol.h` y `protocol.c` desde `02-protocolo-unificado.md` (cierra T0.8.2).
- Componente `firmware/gateway/components/sparkplug_b/`:
  - `.proto` (copia de eclipse-tahu 3.x `sparkplug_b.proto`, congelado a la revisión de la spec v3.0.0).
  - Codegen nanopb en tiempo de build.
  - Máquina de estados: NBIRTH, NDEATH (vía LWT), DBIRTH, DDEATH (por temporizador), NDATA, DDATA, NCMD, DCMD.
  - Manejo de `seq` (uint64 wrap 256), `bdSeq` (incrementa al reconnect), LWT topic pre-configurado en connect.
  - Cliente MQTT (esp-mqtt built-in), TLS opcional (esp-tls built-in), reconexión con backoff.
  - Traducción `MSG_TYPE_DATA` (ESP-NOW) → DDATA (Sparkplug).
  - Traducción NCMD/DCMD → MSG_TYPE_CONFIG / MSG_TYPE_OTA_PREPARE (ESP-NOW).
- `Kconfig` en `sparkplug_b/` con `CONFIG_SPARKPLUG_B_ENABLED` (default n) y sub-opciones (TLS, MQTT_URI por defecto).
- Enlace de `mqtt_bridge` al main: si la feature está habilitada, `mqtt_bridge_start()` arranca `sparkplug_b`; si no, sigue siendo no-op.
- Layout SPIFFS para TLS:
  - `/spiffs/mqtt/ca.pem` (CA bundle)
  - `/spiffs/mqtt/cert.pem` (client cert, PEM)
  - `/spiffs/mqtt/key.pem` (client key, PEM)
  - Nada en el repo. Solo documentación en `docs/security/mqtt-tls.md` explicando cómo poblar SPIFFS.
- Tests Unity on-device del componente (mocks mínimos del cliente MQTT).
- Harness de conformidad en `tools/sparkplug-conformance/` (Python, paho + Eclipse Tahu reference):
  - docker-compose: Mosquitto 2.0 (soporta MQTT 3.1.1) + cliente Python.
  - Cliente Python que:
    - Se conecta como Host Application.
    - Verifica NBIRTH al arranque del gateway.
    - Simula un nodo ESP-NOW virtual que envía MSG_TYPE_DATA, verifica DBIRTH y DDATA.
    - Inyecta un NCMD "reboot" y verifica que el gateway lo reencola como MSG_TYPE_CONFIG.
    - Espera expiración del DDEATH timer y verifica que se publica DDEATH.
- Medición de flash/heap en `idf.py size-components` y `idf.py -p PORT monitor` con `app_main()` que imprime `ESP.getFreeHeap()`. Reporte en `docs/sparkplug_3.0.0-measurements.md`.

### 2.2 NO incluido

- MQTT plano coexistiendo con Sparkplug B (la propuesta #11 lo excluye; la coexistencia queda para Fase 4 si más adelante se decide).
- Protobuf en el LCD o en la SPA Preact.
- Generación de claves: el operador las inyecta en SPIFFS por canal serial o por OTA. Documentado, no automatizado.
- Persistencia de certificados en eFuse (eso es DC-08, NVS encryption, Fase 5).
- Soporte MQTT 5 (ver D8).
- Algoritmos adaptativos de DDEATH (ver D6).
- Múltiples EoN-Nodes en un mismo gateway (un gateway = un EoN-Node).

## 3. Arquitectura

```
                 ┌──────────────────────────────────────────────┐
                 │ ESP32-S3-WROOM-1-N16R8 (EoN-Node)             │
                 │                                                │
  Nodos ESP-NOW ─► espnow_manager ─► sparkplug_b (state machine) ─► esp-mqtt ─► Broker Sparkplug
                                          │                                  ▲
                                          ▼                                  │
                                  nanopb encoder/decoder                     │
                                                                         Host Application
```

### 3.1 Topic namespace (Sparkplug 3.0.0)

```
spBv1.0/{group_id}/{msg_type}/{edge_node_id}[/{device_id}]
```

- `group_id`: de NVS `mqtt_group_id`, default `iiot-kit`.
- `edge_node_id`: de NVS `mqtt_edge_node_id`, default `GW-<mac4hex>`.
- `device_id`: derivado del MAC del nodo, formato `N-<mac4hex>` (configurable vía template).

`msg_type` ∈ {`NBIRTH`, `DBIRTH`, `NDATA`, `DDATA`, `NDEATH`, `DDEATH`, `NCMD`, `DCMD`}.

Topic NDEATH pre-configurado en connect (LWT) por esp-mqtt: `spBv1.0/{group_id}/NDEATH/{edge_node_id}`.

### 3.2 Identidades y defaults

| Clave NVS | Default | Notas |
|-----------|---------|-------|
| `mqtt_group_id` | `iiot-kit` | String, validación SPIFFS |
| `mqtt_edge_node_id` | `GW-<mac4hex>` | Derivado en primer arranque si no está set |
| `mqtt_device_id_template` | `N-<mac4hex>` | `printf`-style con `%s` para MAC |
| `mqtt_uri` | (vacío) | `mqtt://host:1883` o `mqtts://host:8883` |
| `mqtt_user` | (vacío) | Username |
| `mqtt_password` | (vacío) | Password — NVS lo almacena en claro hasta DC-08 |
| `mqtt_tls` | `0` | `0`=TCP, `1`=TLS |
| `mqtt_ca_path` | `/spiffs/mqtt/ca.pem` | Solo si TLS=1 |
| `mqtt_client_cert_path` | `/spiffs/mqtt/cert.pem` | Solo si mTLS |
| `mqtt_client_key_path` | `/spiffs/mqtt/key.pem` | Solo si mTLS |
| `mqtt_ddeath_timeout_sec` | `1800` | Default 30 min |
| `mqtt_keepalive_sec` | `60` | |

### 3.3 Máquina de estados

Estados globales del EoN-Node:
- `OFFLINE`: MQTT no conectado, no publica.
- `WAITING_BIRTH`: MQTT conectado, aún no se publicó NBIRTH.
- `ONLINE`: NBIRTH publicado, MQTT conectado.
- `REBIRTH_PENDING`: conexión perdida, esperando reconnect → re-publicar NBIRTH con `bdSeq++`.

Estados por Device (uno por nodo ESP-NOW conocido):
- `UNKNOWN`: nodo anunciado en DISCOVERY, no visto en DATA.
- `ALIVE`: último DATA recibido dentro de `mqtt_ddeath_timeout_sec`.
- `STALE`: último DATA fuera de timeout → publicar DDEATH, esperar DISCOVERY para revivir.

### 3.4 Manejo de `seq` y `bdSeq`

- `seq`: uint64 de Sparkplug payload. Incrementa por mensaje (NBIRTH, NDATA, DBIRTH, DDATA, NCMD-out, DCMD-out). Wrap a 0 en 2^32-1 (no, la spec dice 0..255 wrap en el transport layer; usamos uint64 para evitar truncar). Ver spec 6.4.3.
- `bdSeq`: uint64. Incrementa **solo** cuando el EoN-Node se reconecta tras una desconexión y republica NBIRTH. En reconnect normal sin pérdida de sesión, no incrementa.
- Ambos persistidos en NVS para sobrevivir reset del gateway. `bdSeq` solo se persiste tras un NBIRTH exitoso.

### 3.5 Mapeo ESP-NOW → Sparkplug

`MSG_TYPE_DATA` (ESP-NOW, 37 bytes) → `DDATA` (Sparkplug, Protobuf):

| Campo ESP-NOW | Métrica Sparkplug | Tipo Sparkplug | Alias |
|---------------|-------------------|----------------|-------|
| `sensor_type = TEMPERATURE` (0x01) | `Inputs/Temperature` | Double | 0 |
| `sensor_type = PH` (0x02) | `Inputs/Ph` | Double | 1 |
| `sensor_type = DO` (0x03) | `Inputs/DissolvedOxygen` | Double | 2 |
| `sensor_type = ORP` (0x04) | `Inputs/Orp` | Double | 3 |
| `sensor_type = LEVEL` (0x05) | `Inputs/Level` | Double | 4 |
| `sensor_type = TURBIDITY` (0x06) | `Inputs/Turbidity` | Double | 5 |
| `sensor_type = CUSTOM` (0xFF) | `Inputs/Custom/0..3` | Double × N | 6..9 |
| `uptime_ms` | `Diagnostics/UptimeMs` | Int64 | 10 |
| `battery_pct` (≠ 0xFF) | `Diagnostics/BatteryPercent` | Float | 11 |
| `rssi_last` | `Diagnostics/RssiLast` | Int8 | 12 |

Aliases asignados en orden creciente en NBIRTH (alias 0 reservado según spec v3.0.0).

`MSG_TYPE_DISCOVERY` (ESP-NOW) → publica DBIRTH con métricas idénticas + `Properties/HwVersion` y `Properties/FwVersion`.

### 3.6 Traducción NCMD/DCMD → ESP-NOW

- `NCMD { "Reboot": true }` → MSG_TYPE_CONFIG con flags bit 1 (reboot) al gateway (no al nodo). El gateway reinicia.
- `DCMD { "Inputs/Temperature": 25.0 }` (escritura) → actualmente no soportado por el protocolo ESP-NOW: el ESP-NOW v1.0 no tiene "escritura" de sensor data. **Acción**: aceptar DCMD como configuración remota del gateway (ej. `mqtt_ddeath_timeout_sec`) y propagar al nodo vía MSG_TYPE_CONFIG (`new_sleep_interval`).
- `DCMD { "Reboot": true }` → MSG_TYPE_CONFIG con flags bit 1 (reboot) al nodo.
- Cualquier otro DCMD → log warning, descartado. Mensaje `DDEATH` no usado para DCMD inválido (el nodo sigue vivo).

## 4. Estructura de archivos

```
firmware/
├── common/
│   └── protocol/
│       ├── CMakeLists.txt                 (idf_component_register)
│       ├── protocol.h                     (WIP sync a 02-protocolo-unificado.md)
│       └── protocol.c                     (serialize/deserialize + validate)
└── gateway/
    ├── idf_component.yml                  (declara deps gestionadas: nanopb)
    ├── sdkconfig.defaults                 (+MQTT/TLS/SPARKPLUG flags)
    ├── main/
    │   ├── CMakeLists.txt                 (REQUIRES + sparkplug_b)
    │   └── main.c                         (mqtt_bridge_start si feature on)
    └── components/
        ├── mqtt_bridge/                   (placeholder + facade hacia sparkplug_b)
        │   ├── include/mqtt_bridge.h      (extendido: mqtt_bridge_set_mode, ...)
        │   ├── mqtt_bridge.c
        │   └── test/test_mqtt_bridge.c    (extendido)
        └── sparkplug_b/                   (NUEVO)
            ├── CMakeLists.txt
            ├── Kconfig                    (CONFIG_SPARKPLUG_B_ENABLED, ...)
            ├── sparkplug_b.proto          (copia congelada de eclipse-tahu 3.x)
            ├── idf_component.yml          (override para nanopb)
            ├── include/
            │   ├── sparkplug_b.h          (API pública: start/stop/publish_sensor_data/on_cmd)
            │   ├── sparkplug_b_state.h    (máquina de estados)
            │   └── sparkplug_b_payload.h  (helpers nanopb encode/decode)
            ├── sparkplug_b.c
            ├── sparkplug_b_state.c
            ├── sparkplug_b_payload.c
            ├── sparkplug_b_mqtt.c         (cliente MQTT + TLS + LWT)
            ├── sparkplug_b_espnow_bridge.c (consumidor de MSG_TYPE_DATA/DISCOVERY)
            ├── test/
            │   ├── test_sparkplug_b.c          (state machine + topic builder)
            │   ├── test_sparkplug_b_payload.c  (round-trip Protobuf)
            │   └── test_sparkplug_b_state.c    (seq, bdSeq, expiry, rebirth)
            └── pb/                       (gitkeep; lo genera nanopb_generator en build)

tools/
└── sparkplug-conformance/
    ├── README.md
    ├── docker-compose.yml                (Mosquitto + cliente Python)
    ├── mosquitto/
    │   └── mosquitto.conf                (allow_anonymous true para test)
    ├── client/
    │   ├── Dockerfile                    (Python 3.11 + paho-mqtt + eclipse-tahu python)
    │   ├── conformance.py                (aserciones de NBIRTH/DBIRTH/DDATA/DDEATH/NCMD/DCMD)
    │   └── requirements.txt
    └── scripts/
        └── run-conformance.sh            (cd tools/sparkplug-conformance && docker compose up)

docs/
├── sparkplug_3.0.0-design.md             (mirror de este design.md, listo para commit)
├── sparkplug_3.0.0-measurements.md       (resultados de flash/heap; generado tras primer build)
└── security/
    └── mqtt-tls.md                       (cómo poblar /spiffs/mqtt/*.pem)
```

## 5. Plan de implementación (orden de commits)

Cada commit es una **work-unit** (Revisión de scope pequeño, build verde al cierre, tests in-line):

1. **`build: add MQTT/TLS/Sparkplug feature flags to sdkconfig.defaults`**
   - Activa `CONFIG_MQTT_PROTOCOL_311=y`, `CONFIG_ESP_TLS_USING_MBEDTLS=y`, `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`.
   - Añade `CONFIG_SPARKPLUG_B_ENABLED=n` (default off).
   - Criterio: `idf.py set-target esp32s3 && idf.py build` cierra en verde **sin** código nuevo (no debe romper la Fase 4).

2. **`feat(common): materialize ESP-NOW protocol header (T0.8.2)`**
   - Crea `firmware/common/protocol/protocol.h/.c` con tipos y serialización del doc.
   - Tests Unity: serialize/deserialize round-trip + validate.
   - Criterio: `cd firmware/gateway/test && idf.py test` pasa.

3. **`feat(gateway): wire mqtt_bridge into main as feature-gated facade`**
   - `mqtt_bridge.c` decide: si `CONFIG_SPARKPLUG_B_ENABLED`, delega a `sparkplug_b_start()`; si no, log + return OK.
   - `main.c` llama `mqtt_bridge_start()` después de `wifi_manager_init`.
   - Criterio: build verde con `CONFIG_SPARKPLUG_B_ENABLED=n` (no rompe nada) y con `=y` (compila sparkplug_b como placeholder que solo loguea).

4. **`feat(gateway): add sparkplug_b component with .proto + nanopb codegen`**
   - Componente nuevo, idf_component.yml con `nanopb>=0.4`.
   - Copia `sparkplug_b.proto` de eclipse-tahu 3.x, congelada en `third_party-notices.txt`.
   - Codegen se ejecuta en build vía `nanopb_generator`.
   - Test round-trip de Payload encode/decode sin tocar MQTT.
   - Criterio: `idf.py build` verde; tests on-device pasan.

5. **`feat(gateway): sparkplug_b state machine (NBIRTH/NDEATH/seq/bdSeq/rebirth)`**
   - Lógica de estados del EoN-Node (sin MQTT todavía, usando un mock del cliente).
   - Tests: seq wrap, bdSeq incrementa en reconnect, LWT topic correcto.
   - Criterio: tests on-device pasan con mock del cliente.

6. **`feat(gateway): sparkplug_b MQTT client with TLS + LWT`**
   - esp-mqtt + LWT pre-configurado en NBIRTH connect.
   - TLS opcional desde SPIFFS.
   - Criterio: build verde; tests de carga de CA/cert/key (con SPIFFS fake).

8. ** **`feat(gateway): ESP-NOW → Sparkplug translation (MSG_TYPE_DATA → DDATA)`**
   - Hook en `espnow_manager` para invocar `sparkplug_b_publish_sensor_data()`.
   - DBIRTH en MSG_TYPE_DISCOVERY.
   - Criterio: build verde; tests con mensaje ESP-NOW simulado.

9. **`feat(gateway): DCMD/NCMD → ESP-NOW bridge`**
   - Suscripción a NCMD/{edge_node_id} y DCMD/{edge_node_id}/{device_id}.
   - Mapeo a MSG_TYPE_CONFIG / MSG_TYPE_OTA_PREPARE.
   - Criterio: tests con DCMD simulado en mock.

10. **`feat(gateway): DDEATH expiry timer + rebirth on MQTT reconnect`**
   - Timer por dispositivo; publica DDEATH al expirar.
   - En reconnect: NBIRTH con bdSeq++.
   - Criterio: tests de expiración + rebirth.

11. **`test: sparkplug-conformance harness with Mosquitto + Python`**
   - docker-compose + cliente Python con aserciones de los 8 mensajes.
   - Criterio: `bash tools/sparkplug-conformance/scripts/run-conformance.sh` cierra verde contra un gateway simulado o contra mocks.

12. **`docs: sparkplug_3.0.0-measurements.md with idf.py size-components output`**
   - Reporte de flash/heap delta vs baseline de #9.
   - Criterio: archivo versionado en el repo.

## 6. Dependencias

Nuevas declaradas en `firmware/gateway/idf_component.yml`:

```yaml
dependencies:
  nanopb:
    version: ">=0.4.0"
```

`esp-mqtt`, `esp-tls`, `mbedtls` son built-in en ESP-IDF v5.x; se activan vía Kconfig.

## 7. Riesgos y mitigaciones

| Riesgo | Mitigación |
|--------|------------|
| Codegen nanopb añade demasiada flash (>30 KB) | Usar `nanopb_generator.py` con `--strip` solo en símbolos internos; medir en commit 4 antes de seguir. |
| TLS con SPIFFS añade latencia al connect (~5-10 ms) | Aceptable para MQTT keepalive 60s; documentado. |
| bdSeq no se persiste en NVS si el gateway se resetea entre connect y persist | Persistir **después** de recibir PUBACK del NBIRTH. Si se pierde, broker verá bdSeq repetido y el Host sabrá re-sincronizar. |
| Nodos ESP-NOW sin DISCOVERY reciente no entran en DBIRTH | DBIRTH se retrasa hasta ver DISCOVERY o un DATA. Si nunca llega, no hay DBIRTH (es comportamiento válido). |
| DDEATH sin NDEATH puede romper conformidad | Implementar primero LWT y verificar connect → NDEATH en test broker. |
| Secrets en repo (CA, cert, key) | .gitignore explícito para `/spiffs/mqtt/` y verificación con `git secrets` en CI. |

## 8. Criterios de aceptación (mapeados a la issue)

- [ ] **CA-S1** Spec y subconjunto Sparkplug B v3.0.0 documentados en `docs/sparkplug_3.0.0-design.md`.
- [ ] **CA-S2** Gateway publica NBIRTH, DBIRTH, NDATA, DDATA, NDEATH, DDEATH correctos según spec.
- [ ] **CA-S3** Política DDEATH por expiración fija (N min configurable, default 30) con tests.
- [ ] **CA-S4** NCMD/DCMD autorizados se traducen a MSG_TYPE_CONFIG / MSG_TYPE_OTA_PREPARE verificables.
- [ ] **CA-S5** Pruebas de `seq` (incremento + wrap), `bdSeq` (incremento en reconnect), LWT (NDEATH al disconnect), rebirth (NBIRTH al reconnect).
- [ ] **CA-S6** Harness de conformidad con Mosquitto + cliente Python que aserta los 8 mensajes.
- [ ] **CA-S7** Medición flash/heap en `docs/sparkplug_3.0.0-measurements.md` (delta vs baseline #9).

## 9. Pendiente de aprobación

Antes de pasar a ODD-T3 (codegen + tests Protobuf), confirma:

1. El plan de 12 commits encaja con tu forma de reinar/mergear.
2. El layout de archivos en `firmware/common/protocol/` + `firmware/gateway/components/sparkplug_b/` + `tools/sparkplug-conformance/` es aceptable.
3. La política DDEATH por expiración fija (no adaptativa) está bien para v1.0.
4. El harness con Mosquitto + Python (no Ignition comercial) es suficiente para CA-S6.

Si todo OK, paso a ODD-T3 delegando la implementación del componente `sparkplug_b` a un `gentle-ai-worker` con scope restringido, y me quedo yo orquestando la revisión.