# Issue #14 — CI y pruebas de conformidad del firmware

## Objetivo

Que cada cambio del firmware se construya, se mida y se pruebe sin una placa
encendida, y que las pruebas que sí necesitan una placa estén separadas,
documentadas y ejecutables.

## Problema y justificación

El repositorio publicaba documentación con GitHub Actions y nada más. No
había builds de firmware, ni tests Unity en la CI, ni validación Protobuf,
ni medidas de tamaño, ni pruebas de estrés. El firmware podía romperse —o
desbordar una partición— sin que nada lo dijera.

La consecuencia práctica: el primer cambio de firmware de cada fase se
enteraba de los errores en la placa, que es el ciclo más lento y más caro
del proyecto.

## Nota sobre la premisa de la issue

La issue se redactó sobre el **ESP32-C6**, la placa candidata de la
primera ola. La issue #8 sustituyó esa placa por el **ESP32-S3-WROOM-1-N16R8**
(el C6 no arrancaba: el GPIO46 estaba retenido a nivel bajo, ver
`docs/hardware/esp32-s3-wroom-1-n16r8.md`). Construir para el C6 sería
construir para una placa que no existe en el producto.

La matriz de la CI sigue, por tanto, el hardware que existe de verdad:

| En la issue | En esta implementación | Por qué |
|---|---|---|
| Build reproducible para ESP32-C6 | Build reproducible para **ESP32-S3** (`esp32s3`) | El C6 fue sustituido por la issue #8 |
| — | Build de **ESP32-C3** del nodo | El firmware del nodo todavía no existe; se añade cuando `firmware/node` aterrice |
| Tests del protocolo y del codec | Igual, y además el **conjunto completo** del proyecto bajo QEMU | La separación real es "necesita placa" / "no necesita placa", no "S3" / "C3" |

La propiedad que la issue pide —"cada cambio de firmware ejecuta un build
reproducible"— se conserva intacta: la CI construye dos veces desde cero y
compara los bytes.

## Alcance

### Dentro

- Workflow de CI con cinco trabajos: tests de host, build del firmware,
  build reproducible, tests del dispositivo bajo QEMU y conformidad
  Sparkplug B.
- Tests de host que ejecutan el **código real** de los componentes (protocolo
  ESP-NOW, codec Protobuf, estado y comandos de Sparkplug B, perfil de marca
  y manifiesto de OTA).
- Escenarios de regresión de sesión Sparkplug B: birth, death, data, command,
  `seq` y `bdSeq`.
- Informe de tamaños de binario, particiones y headroom, con puerta de
  overflow.
- Plan de estrés ejecutable con diez nodos o más.
- Catálogo de pruebas que necesitan placa, con la separación mecánica por
  etiqueta `[hw]`.
- Artefactos de firmware de desarrollo, con comprobación de que no llevan
  secretos.

### Fuera

- Certificaciones regulatorias.
- Publicación de firmware de producción sin firma (la firma ya existe desde
  la issue #13; la CI no la usa).
- Laradio real: la CI no tiene radio. El soak de diez nodos con hardware está
  en `docs/testing/stress-plan.md` y en el catálogo de pruebas de hardware.

## Restricciones y evidencia

- ESP-IDF v5.4, el mismo toolchain que usa el desarrollo local.
- El build debe ser reproducible **byte a byte**: el proyecto ya activa
  `CONFIG_APP_REPRODUCIBLE_BUILD`, que quita la fecha, la hora y las rutas
  absolutas de la imagen.
- QEMU emula el ESP32-S3 con ESP-IDF v5.4, pero **no** emula el PSRAM Octal
  ni el RMT. Por eso el perfil de CI se construye sin PSRAM y las pruebas que
  lo necesitan llevan la etiqueta `[hw]`.
- El acelerador SHA de QEMU está roto (`[SHA] Error reading from GDMA
  buffer` y el heap queda corrupto después), así que el perfil de CI usa la
  implementación software de mbedTLS. La misma verificación de manifiesto se
  cubre igual.

## Separación público / secreto

Este issue no mueve ningún secreto. La CI construye sin los dos headers
locales (credenciales de Wi-Fi y secretos de fabricación, ambos en
`.gitignore`), y `tools/ci/check-no-secrets.sh` lo comprueba en el artefacto
publicado.

## Plan de trabajo

### [x] ODD-1 — CI del firmware

Workflow `.github/workflows/firmware-ci.yml` con cinco trabajos:

| Trabajo | Qué demuestra |
|---|---|
| `host-tests` | La lógica portable funciona, en el host, sin placa |
| `firmware` | La imagen del S3 compila, y su tamaño y particiones se miden |
| `reproducible` | Dos builds desde cero producen los mismos bytes |
| `device-tests` | El conjunto del proyecto pasa bajo QEMU |
| `conformance` | Sparkplug B contra un broker Mosquitto real |

### [x] ODD-2 — Cobertura de los tests de host

El runner de host (`tools/host-tests`) compilaba los tests de Sparkplug B,
pero **no** los del protocolo ESP-NOW, y el conjunto del dispositivo no se
ejecutaba en ningún sitio. Ahora:

- `firmware/common/protocol/test/test_protocol.c` entra en el runner de host;
- `firmware/gateway/components/sparkplug_b/test/test_lifecycle.c` es nuevo y
  cubre la sesión completa.

### [x] ODD-3 — Regresión de sesión Sparkplug B

`test_lifecycle.c` recorre `NBIRTH → NDATA → DBIRTH → DDATA → DDEATH →
NDEATH` y comprueba, leyendo la **cable** con nanopb y no el estado interno:

- la birth abre el espacio de secuencia con `seq` 0;
- el nodo y sus dispositivos comparten **un** espacio de secuencia;
- la muerte del nodo **no** lleva `seq`, y la del dispositivo sí;
- la muerte lleva el `bdSeq` de la birth que abrió la sesión;
- un comando **no** consume número de secuencia;
- un mensaje de datos sólo lleva alias, nunca el nombre.

### [x] ODD-4 — Tamaño, particiones y overflow

`tools/ci/report-size.py` compara la imagen con la partición de aplicación
más pequeña (la que impone el bootloader), publica el headroom en el
resumen del job y **falla** cuando la imagen no cabe o cuando el headroom
baja del 10 %.

### [x] ODD-5 — Build reproducible

`tools/ci/check-reproducible.sh` construye dos veces desde cero y compara el
SHA-256 de la aplicación, del bootloader y de la tabla de particiones.

### [x] ODD-6 — Estrés con diez nodos

`tools/host-tests/stress_nodes.c` (`iot_stress`) inyecta N nodos sintéticos
en el puente real y mide pérdida, errores de identidad, continuidad de la
secuencia, tamaño del registro y memoria residente. El plan completo, con
la parte de radio, está en `docs/testing/stress-plan.md`.

### [x] ODD-7 — Separación de las pruebas de hardware

La etiqueta `[hw]` y dos perfiles de la aplicación de test (`board` y
`logic`) separan de forma mecánica lo que necesita placa de lo que no. El
catálogo está en `docs/testing/hardware-tests.md`.

### [x] ODD-8 — Documentación y evidencia acumulada

Este documento, el README de `tools/ci`, el plan de estrés y el catálogo de
hardware.

## Ledger de estado

| ODD | Estado | Evidencia |
|---|---|---|
| ODD-1 | [x] | `.github/workflows/firmware-ci.yml` |
| ODD-2 | [x] | `tools/host-tests/CMakeLists.txt`: 78 tests de host |
| ODD-3 | [x] | `firmware/gateway/components/sparkplug_b/test/test_lifecycle.c` |
| ODD-4 | [x] | `tools/ci/report-size.py` |
| ODD-5 | [x] | `tools/ci/check-reproducible.sh` |
| ODD-6 | [x] | `tools/host-tests/stress_nodes.c` |
| ODD-7 | [x] | `firmware/gateway/test/main/test_main.c`, `docs/testing/hardware-tests.md` |
| ODD-8 | [x] | `docs/testing/stress-plan.md`, `tools/ci/README.md` |

## Evidencia acumulada

### Criterios de aceptación de la issue

| Criterio | Estado | Evidencia |
|---|---|---|
| Cada cambio de firmware ejecuta un build reproducible | [x] | Trabajo `reproducible`: dos builds, mismos SHA-256 |
| El protocolo ESP-NOW y el codec Protobuf tienen tests automatizados | [x] | `test_protocol.c` y `test_payload.c` en el runner de host |
| Birth, Death, Data, Command, `seq` y `bdSeq` tienen escenarios de regresión | [x] | `test_lifecycle.c` (host) y el arnés de conformidad (broker real) |
| La CI informa de los tamaños de binario y de los desbordamientos de partición | [x] | `report-size.py` publica el headroom y falla al desbordar |
| Existe un plan ejecutable de estrés con al menos diez nodos | [x] | `iot_stress --nodes 10` y `docs/testing/stress-plan.md` |
| Las pruebas que requieren placa real están separadas y documentadas | [x] | Etiqueta `[hw]`, perfiles `board`/`logic`, `docs/testing/hardware-tests.md` |

### Build reproducible

Dos builds desde cero, el mismo commit:

```
38e74bd9f978c4ccf28ee591d54b654877b4da2e6d9c1f415e579d02a97a54ee  build-a/gateway.bin
38e74bd9f978c4ccf28ee591d54b654877b4da2e6d9c1f415e579d02a97a54ee  build-b/gateway.bin
4590caef80ca85c132c717a270c44ce392d72255d5d12b47c7eca2241d3cf01a  build-a/bootloader/bootloader.bin
4590caef80ca85c132c717a270c44ce392d72255d5d12b47c7eca2241d3cf01a  build-b/bootloader/bootloader.bin
e66046b86176d18f7d6617d8bb70d48801868a22ea2defea8a068d2c063dc0c3  build-a/partition_table/partition-table.bin
e66046b86176d18f7d6617d8bb70d48801868a22ea2defea8a068d2c063dc0c3  build-b/partition_table/partition-table.bin
```

### Tamaño y particiones

| Métrica | Valor |
|---|---|
| Imagen de aplicación | 1 021 120 B (997,19 KiB) |
| Partición de aplicación más pequeña | 3 145 728 B (3 MiB) |
| Libre | 2 124 608 B (67,54 %) |
| Puerta | al menos un 10 % libre |

### Pruebas

| Suite | Dónde | Resultado |
|---|---|---|
| Tests de host | `iot_host_tests` | 78 tests, 0 fallos |
| Tests del dispositivo | QEMU (`iot_stress` no aplica) | 111 tests, 0 fallos |
| Estrés con 10 nodos | `iot_stress --nodes 10` | 200 tramas, 0 pérdida, 0 errores |
| Estrés con 20 nodos | `iot_stress --nodes 20` | 1000 tramas, 0 pérdida, 0 errores |
| Conformidad Sparkplug B | `tools/sparkplug-conformance` | El arnés existente de la issue #11 |

### Secretos

`tools/ci/check-no-secrets.sh` encuentra la clave privada de prueba y no
confunde los marcadores de mbedTLS con una clave real.

## Guardarraíles

- La CI no publica firmware de producción: sólo artefactos de desarrollo,
  con 14 días de retención.
- La puerta de partición es una puerta, no un informe: un firmware que
  desborda rompe el build.
- La separación de las pruebas de hardware es una etiqueta en el código, no
  una lista en un documento: una prueba de hardware sin etiquetar falla en
  la CI.
- El perfil de CI construye **sin** PSRAM y **con** SHA software. Ninguna de
  las dos cosas cambia el código que se prueba; las dos están documentadas
  arriba.

## Referencias

- `.github/workflows/firmware-ci.yml`
- `tools/ci/README.md`
- `docs/testing/stress-plan.md`
- `docs/testing/hardware-tests.md`
- `firmware/gateway/test/main/test_main.c`
- `firmware/gateway/test/sdkconfig.qemu.defaults`
- ESP-IDF v5.4: `components/unity/unity_runner.c` (etiquetas),
  `components/esp_app_format/Kconfig.projbuild` (build reproducible)
