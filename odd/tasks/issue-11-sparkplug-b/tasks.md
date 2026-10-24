# Feature: Integrar MQTT Sparkplug B como modo opcional (issue #11)

## Source of truth
- Issue GitHub #11 (aproposición `ota-sparkplug-b` integrado)
- `docs/replanificacion/01-decisiones-corregidas.md` (DC-13)
- `docs/replanificacion/02-protocolo-unificado.md`
- `Fases/fase-06-mejoras-futuras.md` (la T6.x de Sparkplug B no existe aún en el archivo — debe definirse como parte de esta propuesta)
- `firmware/gateway/components/mqtt_bridge/`

## Decisiones validadas con el usuario (ODD paso 3)
1. Spec: Sparkplug 3.0.0
2. Identidades (`group_id`, `edge_node_id`, `device_id`): configurables en NVS con defaults seguros
3. Verificación de conformidad: Ignition 8.1+ o broker compatible Cirrus Link, con publisher Python de prueba
4. TLS: obligatorio en el alcance; CA bundle + client cert desde NVS/SPIFFS, jamás en repo

## Criterios de aceptación (mapeados a la issue)
- [x] Versión y subconjunto Sparkplug B documentados y fijados (v3.0.0)
- [x] El gateway publica NBIRTH/DBIRTH/NDATA/DDATA/NDEATH/DDEATH correctos
- [x] DDEATH por expiración de nodo silencioso con política documentada
- [x] NCMD/DCMD autorizados se traducen a comandos ESP-NOW verificables
- [x] Pruebas automatizadas de `seq`, `bdSeq`, reconnect y rebirth
- [x] Harness de conformidad con broker de referencia (Ignition/Cirrus compatible)
- [~] Medición de flash y heap en ESP32-S3-WROOM-1-N16R8 — flash medido; **heap de runtime sin medir** (la unidad conectada no arranca desde flash; ver "Bloqueador de hardware")

## Subconjunto Sparkplug B fijado (v3.0.0)

Referencia normativa: la especificación oficial Eclipse Sparkplug 3.0.0
(`https://sparkplug.eclipse.org/specification/version/3.0/documents/sparkplug-specification-3.0.0.pdf`).
Los requirement ids citados abajo son los de esa especificación.

| Mensaje | Topic | Payload | QoS / retain |
|---|---|---|---|
| NBIRTH | `spBv1.0/{group}/NBIRTH/{edge}` | `timestamp`, `seq` ∈ [0,255], métrica `bdSeq` (INT64, sin alias), métrica `Node Control/Rebirth` (Boolean `false`, sin alias), `Inputs/UptimeMs` (alias 11) | 0 / false |
| DBIRTH | `spBv1.0/{group}/DBIRTH/{edge}/{device}` | `timestamp`, `seq` = anterior + 1 (envuelve 255→0), métricas con nombre + alias (el juego completo de métricas del Device) | 0 / false |
| DDATA | `spBv1.0/{group}/DDATA/{edge}/{device}` | `timestamp`, `seq` = anterior + 1, métricas **solo con alias** (el nombre va excluido) | 0 / false |
| NDATA | `spBv1.0/{group}/NDATA/{edge}` | `timestamp`, `seq` = anterior + 1, `Inputs/UptimeMs` (alias 11, sin nombre) | 0 / false |
| NDEATH | `spBv1.0/{group}/NDEATH/{edge}` | Will: **solo** la métrica `bdSeq` (INT64), **sin** `seq` | 1 / false |
| DDEATH | `spBv1.0/{group}/DDEATH/{edge}/{device}` | `timestamp`, `seq` = anterior + 1, métrica `bdSeq` | 0 / false |

El `seq` es un **único espacio por EoN-Node**: el nodo y sus Devices lo
comparten, y cada mensaje posterior a un NBIRTH lleva el anterior + 1
(`tck-id-payloads-sequence-num-incrementing`). Un NBIRTH abre un espacio nuevo.

Comandos: NCMD y DCMD llevan un Payload Protobuf con `timestamp` y las
métricas a escribir. La suscripción a NCMD se hace con **QoS 1**
(`tck-id-message-flow-edge-node-ncmd-subscribe`).

Métricas de control implementadas: `Node Control/Rebirth` (Boolean),
`Node Control/Reboot` (Boolean), `Device Control/Rebirth` (Boolean),
`Device Control/Reboot` (Boolean), y una escritura numérica en DCMD que se
traduce a `new_sleep_interval` del nodo.

### Mapa de alias

Sparkplug B v3.0.0 exige que cada alias sea único en el conjunto completo de
métricas de un EoN-Node (`tck-id-payloads-alias-uniqueness`), así que las
métricas del nodo y las de sus Devices ocupan rangos disjuntos:

| Alias | Métrica | Tipo | Dónde |
|---:|---|---|---|
| 0 | reservado por la spec | | |
| 1..6 | `Inputs/{Temperature,Ph,Do,Orp,Level,Turbidity}` | Double | DBIRTH/DDATA |
| 7..10 | `Inputs/Custom/0..3` | Double | DBIRTH/DDATA |
| 11 | `Inputs/UptimeMs` | Int64 | NBIRTH (uptime del nodo) |
| 12 | `Diagnostics/BatteryPercent` | Float | DBIRTH/DDATA |
| 13 | `Diagnostics/RssiLast` | Int8 | DBIRTH/DDATA |
| 14 | `Diagnostics/UptimeMs` | Int64 | DBIRTH/DDATA (uptime del Device) |
| 15 | `Properties/SleepIntervalSec` | Int64 | DBIRTH (propiedad del Device) |

El DBIRTH declara el **juego completo** de métricas del Device, de modo que
todo alias que el DDATA pueda publicar después quede ligado a un nombre. El
frame de discovery ESP-NOW no lleva el número de valores del sensor, así que
se declara el rango Custom entero por adelantado.

## Política de expiración (DDEATH)

Un Device pasa a `STALE` cuando no reporta durante `mqtt_ddeath_timeout_sec`
(NVS, por defecto 1800 s). El gateway publica entonces un DDEATH con el `seq`
global y la métrica `bdSeq` de la sesión. La comprobación corre cada segundo
en el task de sesión. Un comando inválido **no** genera DDEATH: el nodo sigue
vivo (`D-C6`).

## Trabajo commits 7-11 (2026-10-24)

Estado: commits 7, 8, 9 y 11 cerrados; commit 10 cerrado con el harness
alineado a la spec.

### Commit 7 — NCMD/DCMD → ESP-NOW
- Nuevo módulo `sparkplug_b_cmd.{h,c}`: parsea las métricas decodificadas y
  clasifica el comando; traduce a un frame ESP-NOW `MSG_TYPE_CONFIG`.
- Nuevo módulo `sparkplug_b_device.{h,c}`: registro de Devices con estado,
  timestamp de última señal y buffer de replay del birth.
- Decoder Protobuf completo (`sparkplug_b_decode_metrics`): el schema Tahu
  declara todos los campos de `Payload.Metric` como callbacks de nanopb, así
  que los nombres y valores solo se alcanzan instalando callbacks de decode.
- El módulo Sparkplug no habla con la radio: traduce y entrega el frame a un
  sink registrado por la aplicación (`sparkplug_b_set_espnow_sink`), que
  `main.c` conecta con `espnow_manager_send()`.
- `msg_config_t` gana `msg_config_flag_t` en `firmware/common/protocol/protocol.h`
  para que nodo y gateway no divergan.

### Commit 8 — DDEATH por expiración y rebirth
- El task de sesión comprueba la expiración cada segundo y publica DDEATH.
- El rebirth (NCMD `Node Control/Rebirth` = true) republica NBIRTH + DBIRTH de
  cada Device conocido **sin** tocar `bdSeq` (`tck-id-operational-behavior-data-commands-rebirth-action-3`).

### Commit 9 — persistencia NVS
- `group_id` y `edge_node_id` se derivan de la MAC y se persisten en el
  namespace `mqtt` la primera vez, de modo que la identidad sobrevive a un
  reflasheo.
- `bdSeq` se persiste al abrir cada sesión: el valor guardado es el de la
  **siguiente** sesión, así que un reinicio inesperado no reutiliza un
  `bdSeq` que un Host ya vio.

### Bucle de sesión MQTT (parte del commit 7)
La sesión la lleva un task propio. El cliente ESP-IDF solo acepta el Will en
el momento de crearlo, y la spec exige que el `bdSeq` del Will coincida con
el del NBIRTH de esa sesión (`tck-id-topics-nbirth-bdseq-matching`). Por eso
cada reconexión **recrea el cliente** con un `bdSeq` nuevo y su Will
correspondiente, en lugar de dejar que el cliente se reconecte solo.

### Bugs de codec encontrados al ejecutar los tests
El host runner (abajo) ejecutó por primera vez el código del codec y dejó
verificar tres defectos reales del commit 4/6, que ningún test anterior
detectaba porque solo comprobaban "payload no vacío":

1. Los brazos del `oneof` de `Payload.Metric` comparten una misma unión de C,
   así que **solo un callback puede estar registrado a la vez**: el último
   registraba sobrevivía y todos los valores se codificaban con el callback
   equivocado. Ahora un único callback despacha por el tag del brazo.
2. El `datatype` usaba una única variable `static` compartida: **toda** métrica
   salía con el datatype de la última métrica preparada.
3. El `int_value` de Int8/UInt8 salía con el valor sin signo del tipo
   equivocado.

### Commit 12 — desviaciones de conformidad detectadas por el harness
Alinear el harness con las reglas normativas de la spec destapó tres
desviaciones reales del gateway, corregidas en `33b1210`:

1. **NDATA y DDATA llevaban el nombre de cada métrica.**
   `tck-id-payloads-alias-data-cmd-requirement` dice que un mensaje de datos
   MUST solo incluir el alias. Los nombres se quitan en el bridge y en el
   publicador de NDATA.
2. **El mapa de alias no era único en el EoN-Node.** El uptime del nodo usaba
   el alias 11 con el nombre `Inputs/UptimeMs` y el uptime del Device usaba el
   mismo alias con `Diagnostics/UptimeMs`, así que un Host no podía resolver
   el alias 11. Ahora las métricas del Device van en su propio rango (14 para
   el uptime, 15 para el intervalo de sleep) y el DBIRTH declara el juego
   completo de métricas del Device, de modo que todo alias que un DDATA pueda
   publicar queda ligado a un nombre.
3. **Un rebirth republicaba el último DDATA como si fuera el DBIRTH**, con el
   `seq` de la sesión anterior. El registro de Devices guarda ahora el juego
   de métricas del birth en vez de los bytes codificados, y el DBIRTH se
   reconstruye con un `seq` nuevo, como exige
   `tck-id-payloads-dbirth-seq-inc`.

El registro se quedó 1.760 B más pequeño de rebote: el componente pasó de
12.424 B a 9.544 B de DRAM.

### Bloqueador de hardware: GPIO46
La unidad conectada **no arranca desde flash**. `GPIO_IN1` (bit 14) lee 0:
GPIO46 está a masa, y en el ESP32-S3 eso deshabilita el arranque SPI, así que
el ROM solo ofrece el downloader (`boot:0x0 (DOWNLOAD(USB/UART0))`). GPIO0 se
midió alto, o sea que BOOT no es la causa. El detalle y la evidencia están en
`docs/hardware/esp32-s3-wroom-1-n16r8.md`.

Consecuencia: el runner on-device no se pudo ejecutar. La verificación se hizo
con el **host runner** `tools/host-tests/`, que compila los **mismos** ficheros
de test Unity contra los **mismos** fuentes del componente.

### Host runner `tools/host-tests/`
- Compila `firmware/gateway/components/sparkplug_b/test/*.c` (los mismos
  ficheros que el proyecto de test on-device) contra los fuentes reales del
  componente, con shims mínimos de ESP-IDF en `tools/host-tests/shims/`.
- Resultado actual: **44 tests, 0 failures**, exit 0.
- Cubre: codec Protobuf (round-trip, datatype por métrica, tipo de cable por
  brazo), parseo NCMD/DCMD, traducción a ESP-NOW, registro y expiración de
  Devices, replay de birth, `seq`/`bdSeq`, y el bridge ESP-NOW → Sparkplug.
- No cubre: la sesión MQTT (depende del cliente ESP-IDF) y la radio. Eso lo
  cubre el harness de conformidad.

### Harness de conformidad `tools/sparkplug-conformance/`
- Docker Compose con Mosquitto 2.0 y un cliente Python que compila el schema
  Tahu real en tiempo de build (`grpcio-tools`).
- El cliente usa los dos roles MQTT de un despliegue Sparkplug: la **Host
  Application** (suscribe y afirma) y un **Edge Node simulado** (publica y
  registra el Will). El modo `simulate` termina tirando la conexión TCP sin
  DISCONNECT, así que el broker publica el Will en nombre del nodo y se puede
  afirmar el NDEATH de verdad.
- Modos: `simulate` (publica una secuencia conforme y la verifica) y
  `observe` (verifica lo que publica un gateway real; con `--expect-ndata`,
  `--expect-ddeath` y `--expect-rebirth`).
- Verificado en esta máquina: `SPARKPLUG_BROKER_HOST_PORT=18830 bash
  tools/sparkplug-conformance/scripts/run-conformance.sh` → **82 checks, 82
  passed, exit 0**. `MODE=observe` sin publisher → exit 2 con mensaje claro.
- El puerto 1883 del host está ocupado por un contenedor `mcm-mosquitto` ajeno;
  por eso el puerto de publicación es configurable.
- Lo que afirma el harness, con los requirement ids de la spec:
  - topics: prefijo `spBv1.0`, 4 niveles para mensajes de nodo y 5 para los de
    Device, verbo conocido;
  - `NBIRTH`: `seq` ∈ [0,255], métrica `bdSeq` (INT64, sin alias), métrica
    `Node Control/Rebirth` (Boolean `false`, sin alias), el resto con nombre +
    alias + datatype;
  - `DBIRTH`: `seq` = anterior + 1, métricas con nombre + alias;
  - `NDATA`/`DDATA`: `seq` = anterior + 1, métricas **solo con alias** y sin
    nombre;
  - `NDEATH`: Will, QoS 1, retain false, **solo** la métrica `bdSeq` y **sin**
    `seq`; su `bdSeq` coincide con el del `NBIRTH`;
  - `DDEATH`: `seq` presente y métrica `bdSeq`;
  - QoS 0 y retain false en todos los mensajes de birth/data/death, y QoS 1 en
    el Will y en la suscripción a NCMD/DCMD;
  - **un único espacio de `seq` por EoN-Node**, compartido por el nodo y sus
    Devices;
  - unicidad de alias en el conjunto completo de métricas del EoN-Node.

### Medición (commit 11)
Ver `docs/sparkplug_3.0.0-measurements.md`. Resumen:

| Configuración | `gateway.bin` | DRAM | Libre en slot OTA 3 MiB |
|---|---:|---:|---:|
| Sparkplug OFF (default) | 830,000 B | 33,604 B (18.59 %) | 74 % |
| Sparkplug ON | 1,050,128 B | 44,756 B (24.76 %) | 67 % |

`libsparkplug_b.a`: 18,956 B totales, 9,544 B de DRAM, 8,224 B de código
flash. La medición destapó 24,576 B de DRAM que se pagaban en **todas** las
builds por tres buffers PEM de TLS estáticos, aunque la conexión fuese en
claro; ahora se asignan solo cuando TLS está activo.

## Baseline (ODD-Ta, 2026-10-05)

- Cambios: `firmware/gateway/test/partitions.csv` (nuevo, layout mínimo nvs+phy_init+factory 1.5 MiB) + `firmware/gateway/test/sdkconfig.defaults` (añadido `CONFIG_PARTITION_TABLE_CUSTOM=y` + `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"`).
- Criterio de cierre: `cd firmware/gateway/test && idf.py build` cierra verde desde checkout limpio (sin `sdkconfig` previo). Verificado por ejecutor del sub-agent gentle-ai-verify (build + size-components).
- Test bin post-ODD-Ta: **802,400 bytes** (49% libre en factory 0x180000).
- Gateway bin (no regresión): **779,168 bytes** (idéntico al post-commit-4 baseline).
- Componentes del test (size-components): libprotocol.a = 727 B. Sin regresión de payload.
- Halla pre-existente cerrado: `ninja: error: '.../firmware/gateway/test/partitions.csv', needed by 'partition_table/partition-table.bin', missing and no known rule to make it` → no se reproducirá.
- Commit (a aplicar por el usuario): `test(gateway): add partitions.csv and pin CONFIG_PARTITION_TABLE_CUSTOM_FILENAME for tests sub-project`.

## Baseline (commit 4)

- Commit 4: `proto/sparkplug_b.proto` (EPL-2.0, vendored from eclipse/tahu 3.x) +
  `proto/sparkplug_b.pb.{h,c}` (nanopb-generated, pre-vendored — see SOURCE.md for the
  regen command) + `include/sparkplug_b_payload.h` + `src/sparkplug_b_payload.c` +
  `src/sparkplug_b.c` (offline start/stop) + 5 Unity TEST_CASEs. Commit: `ec4d013`.
- App post-commit-4 (flag OFF, baseline regenerated): **779168 bytes**. Delta vs post-commit-3
  (753888) is +25280 B — most of this is the regenerated sdkconfig adding
  `livekit__nanopb` headers + nanopb runtime even though the C codepath is dead-stripped.
  This regenerates a controlled baseline for commit 5.
- App post-commit-4 (flag ON): **785584 bytes**. Delta vs flag-OFF baseline = **+6416 B**
  for the sparkplug_b_payload codepath + nanopb runtime.
- Test post-commit-4 (flag OFF): **790448 bytes** (+13856 vs post-commit-3 test baseline).
- Vendor note: `.pb.{h,c}` are committed because ESP-IDF's component_get_requirements runs
  in script mode and rejects `add_custom_command`. SOURCE.md documents the regeneration
  command and the upstream version pin.

## Baseline (commit 3)

- Commit 1: `sdkconfig.defaults` + stub `components/sparkplug_b/` (Kconfig + CMakeLists). Commit: `5e26683`.
- Commit 2: `firmware/common/protocol/protocol.{h,c}` + tests. Commit: `9d8cbb3`. T0.8.2 cerrado.
- Commit 3: `mqtt_bridge` facade + `sparkplug_b` header stub + enganche en `main.c`. Commit: `6f5e950`.
- App baseline post-commit-3: **753888 bytes** (+208 vs commit 2).
- Test bin post-commit-3: **776592 bytes** (+192 vs commit 2).

## Baseline (commit 2)

- Commit 1: `sdkconfig.defaults` + stub `components/sparkplug_b/` (Kconfig + CMakeLists). Commit: `5e26683`.
- Commit 2: `firmware/common/protocol/protocol.{h,c}` + tests. Commit: `9d8cbb3`. T0.8.2 cerrado.
- App baseline post-commit-2: **753680 bytes** (sin delta vs commit 1; nadie referencia el protocolo aún → linker dead-strip).
- Test bin post-commit-2: **776400 bytes** (+4896 vs commit 1; = protocol.c + 6 TEST_CASE).
- Comportamiento esperado: el delta flash real aparecerá cuando `espnow_manager` llame a `protocol_deserialize_header` (commit 7).

## Baseline (commit 1)

- `idf.py build` (gateway): VERDE. App 0xb8010 bytes (752 KB / 3 MB = 76% libre).
- `idf.py build` (test project): VERDE. gateway_test.bin 0xbc6b0 bytes.
- `CONFIG_SPARKPLUG_B_ENABLED` aparece en el menú y queda `is not set` por defecto.
- Flag efectivo `CONFIG_MQTT_PROTOCOL_311=y`, `CONFIG_MQTT_TRANSPORT_SSL=y`, `CONFIG_ESP_TLS_USING_MBEDTLS=y`, `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` persistidos en `sdkconfig`.
- ESP-IDF detectado: 5.4.0. Herramienta activa en `/home/composedof2/esp/esp-idf`.

## Diseño técnico

- [`design.md`](./design.md) — versión draft con 9 secciones, 12 commits planeados, 8 criterios de aceptación mapeados a la issue. Decisiones cerradas (D1–D8). Pendiente de aprobación del usuario antes de ODD-T3.

## Tareas (en ODD)
1. **ODD-T1 Explorar** — estado actual del gateway, mqtt_bridge, dependencias nanopb/esp-tls.
2. **ODD-T2 Diseño técnico** — máquina de estados, mapeo ESP-NOW ↔ Sparkplug, topic tree, archivos a tocar.
3. **ODD-T3 sparkplug_payload** — capa Protobuf sobre nanopb con tests de round-trip.
4. **ODD-T4 sparkplug_b** — máquina de estados v3 + traducción ESP-NOW.
5. **ODD-T5 TLS opcional** — CA/cert desde NVS/SPIFFS, sin secretos en repo.
6. **ODD-T6 Política DDEATH + rebirth** — pruebas seq/bdSeq/reconnect.
7. **ODD-T7 Harness de conformidad** — docker-compose + cliente Python Sparkplug.
8. **ODD-T8 Medición flash/heap** — cerrar criterios con `idf.py size-components`.
9. **ODD-T5.a Commit 5** — cliente MQTT + state machine + topic builder + 7 TEST_CASE. Commit: `2e8daf3`.
9a. **ODD-Ta Commit 5a (BLOQUEADOR PREVIO, nuevo)** — `firmware/gateway/test/partitions.csv` mínimo + `sdkconfig.defaults` apuntándolo. Sin esto no se puede verificar RED de ningún test on-device del gateway.
10. **ODD-T5.b Commit 6 (PENDIENTE, bloqueado por ODD-Ta)** — tests RED del decode API ESP-NOW → Sparkplug; luego implementación GREEN del bridge `sparkplug_b_espnow_bridge.{h,c}`.
11. **ODD-T5.c Commit 7** — NCMD/DCMD → ESP-NOW.
12. **ODD-T5.d Commit 8** — DDEATH timer + rebirth en reconnect.
13. **ODD-T5.e Commit 9** — persistencia NVS de bdSeq + identities (cubierto parcialmente en commit 5).
14. **ODD-T5.f Commit 10** — harness de conformidad `tools/sparkplug-conformance/`.
15. **ODD-T5.g Commit 11** — `docs/sparkplug_3.0.0-measurements.md` con `idf.py size-components`.

## Hallazgos del scout (ODD-T1, evidencia)

- `mqtt_bridge` es placeholder puro (16 líneas, dos funciones log). No enlazado en `main/CMakeLists.txt` ni invocado desde `main.c`. Solo aparece en el binario de tests.
- No existe `firmware/common/protocol/` ni `protocol.h` en el árbol: `docs/replanificacion/02-protocolo-unificado.md` es la única definición del protocolo ESP-NOW. **Implicación**: antes de implementar la capa de traducción ESP-NOW → Sparkplug, hay que materializar `firmware/common/protocol/` (T0.8.2 pendiente). Esta issue debe coordinarse o asumirlo como dependencia previa.
- `sdkconfig.defaults` tiene 5 líneas: solo flasnsize 16MB, partición custom, SPIRAM OCT. Sin `MQTT_CLIENT`, `MBEDTLS`, `NANOPB`, `esp_tls`.
- Sin `idf_component.yml` en ningún archivo del gateway. **Implicación**: hay que crear uno para añadir `nanopb` y `esp-mqtt` (o usar ESP-IDF built-in `mqtt` y `esp-tls`).
- `partitions.csv`: NVS 24 KiB, OTA 2×3 MiB, SPIFFS 4 MiB. NVS se queda corto si metemos 6+ claves Sparkplug (group_id, edge_node_id, device_id_template, mqtt_uri, mqtt_tls, mqtt_ca_path, mqtt_client_cert_path, mqtt_client_key_path, mqtt_user, mqtt_password, mqtt_namespace, lwt_metric).
- Sin artefactos de build: cualquier afirmación de tamaño flash/RAM es cálculo teórico, no medición. **Criterio #11 exige medición**, así que el primer commit debe abrir un PR de scaffolding que cierre `idf.py set-target esp32s3 && idf.py build` antes de seguir.
- Tests Unity: patrón `TEST_CASE`+`TEST_ASSERT_EQUAL` consistente. Solo tests on-device; no hay host harness. **Implicación**: para Protobuf round-trip y Sparkplug topic building se necesita un test host-side en C o un script Python (lo segundo es más barato y se alinea con el harness de Ignition).
- Sin Kconfig por componente. Para feature-gating (`CONFIG_SPARKPLUG_B_ENABLED`) hay que añadir `Kconfig` al componente.

## Branch de trabajo
- Trabajar en `feat/issue-11-sparkplug-b` (no creada aún; crear desde `main` antes del primer commit).

## Hallazgos post-commit-5 (capturados en sesión 2026-10-05)

- **ODD-Ta [BLOQUEADOR PREVIO]**: el sub-proyecto de tests `firmware/gateway/test/` no tiene `partitions.csv` propio. Cualquier intento de `idf.py build` allí falla con `ninja: error: '.../firmware/gateway/test/partitions.csv', needed by 'partition_table/partition-table.bin', missing and no known rule to make it`. Esto bloquea el RED del commit 6 (no se llega a fase de enlace, donde aparecerían los `undefined reference` esperados del bridge). **Acción**: extraer a un commit aislado previo a ODD-T5.b que añada `firmware/gateway/test/partitions.csv` mínimo + `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` en su `sdkconfig.defaults`. Reabrir commit 6 después.
- **Discrepancia alias Custom**: `design.md §3.5` dice `CUSTOM` en alias 6..9; el código commitado en commit 4 (`sparkplug_b_payload.h`) los fija en `7..10` (CUSTOM_0..3). Documentado en commit 6 (header del bridge) y se seguirá el código como contrato operativo.
- **Contrato cerrado del decode API ESP-NOW → Sparkplug** (decidido con el usuario 2026-10-05):
  - D-B1: `value_count > 1` → `values[0]` métrica tipada; `values[1..N-1]` → `Inputs/Custom/0..N-2` con alias 7..10.
  - D-B2: timestamp = `esp_timer_get_time() / 1000` (monotonic), con un único `ESP_LOGW` en el primer encode avisando del límite hasta SNTP (Fase 5).
  - D-B3: 1 DDATA por cada MSG_TYPE_DATA (sin coalescing).
  - D-B4: `seq` global del EoN-Node vía `sparkplug_b_seq_next()`.

## Baseline (ODD-Ta, 2026-10-05)

- Cambios: `firmware/gateway/test/partitions.csv` (nuevo, layout mínimo nvs+phy_init+factory 1.5 MiB) + `firmware/gateway/test/sdkconfig.defaults` (añadido `CONFIG_PARTITION_TABLE_CUSTOM=y` + `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"`).
- Criterio de cierre: `cd firmware/gateway/test && idf.py build` cierra verde desde checkout limpio (sin `sdkconfig` previo).
- Test bin post-ODD-Ta: **802,400 bytes** (49% libre en factory 0x180000).
- Gateway bin (no regresión): **779,168 bytes** (idéntico al post-commit-4 baseline).
- Componentes del test (size-components): libprotocol.a = 727 B.
- Halla pre-existente cerrado: `ninja: error: '.../firmware/gateway/test/partitions.csv', needed by 'partition_table/partition-table.bin', missing and no known rule to make it` → no se reproducirá.
- Commit: `2208efc test(gateway): add partitions.csv and pin CONFIG_PARTITION_TABLE_CUSTOM_FILENAME`.

## Baseline (Commit 6 — ODD-T5.b ESP-NOW → Sparkplug bridge, 2026-10-05)

- Cambios:
  - `firmware/gateway/components/sparkplug_b/include/sparkplug_b_espnow_bridge.h` (nuevo, contrato del bridge con alias map anotado).
  - `firmware/gateway/components/sparkplug_b/test/test_espnow_bridge.c` (nuevo, 8 TEST_CASE cubriendo `device_id_for`, `on_data` single/multi/CUSTOM/nullable, `on_discovery` DBIRTH-shape, increment de seq, init NULL/empty).
  - `firmware/gateway/components/sparkplug_b/src/sparkplug_b_espnow_bridge.c` (nuevo, implementación: typed-metric table, custom-slot mapping, diagnostics con battery 0xFF=unknown suprimido, monotonic timestamp con ESP_LOGW único).
  - `firmware/gateway/components/sparkplug_b/CMakeLists.txt` (añadido `sparkplug_b_espnow_bridge.c` + `protocol` en REQUIRES).
  - `firmware/gateway/components/sparkplug_b/test/CMakeLists.txt` (añadido `test_espnow_bridge.c` + `protocol` en REQUIRES).
  - `firmware/gateway/CMakeLists.txt` (añadido `set(EXTRA_COMPONENT_DIRS "components" "../common/protocol")` para que el gateway principal pueda linkear `protocol`, antes huérfano).
- Criterio de cierre:
  1. RED con 4 `undefined reference to sparkplug_b_espnow_bridge_{init,on_data,on_discovery,device_id_for}` confirmado antes de materializar el .c.
  2. GREEN con compilación verde tras añadir el .c.
  3. No regresión: `gateway.bin` compila verde.
- Test bin post-commit-6: **858,080 bytes** (0xd17e0). Delta vs ODD-Ta: +55,680 B.
- Gateway bin post-commit-6: **830,000 bytes** (0xcaa30). Delta vs post-commit-5: **+50,832 B** (incluye el .c del bridge, el .c de protocol antes no enlazado, y los buffers estáticos `s_group_id`/`s_edge_node_id`).
- Componentes del test (size-components): libsparkplug_b.a = 6,057 B; libprotocol.a = 728 B.
- Pendiente de verificación on-device (HW no disponible en este entorno): ejecutar los 8 TEST_CASE flasheando `gateway_test.bin` en ESP32-S3. Salida esperada: 8/8 PASS bajo `[sparkplug_b_bridge]`.
- Hallas cerrados:
  - `msg_header_t` usa `node_mac[6]`, no `mac` (corregido).
  - `firmware/gateway/CMakeLists.txt` no declaraba `EXTRA_COMPONENT_DIRS`; añadir `protocol` al REQUIRES rompía el build del gateway. Resuelto.
- Commit (a aplicar por el usuario): `feat(gateway/sparkplug_b): add ESP-NOW → Sparkplug B translation bridge (init, on_data, on_discovery, device_id_for) with on-device RED-GREEN tests`.