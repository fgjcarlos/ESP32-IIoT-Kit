# Issue #16 — Construir un gateway IIoT sobre ESP32-S3-WROOM-1-N16R8 con ST7789V2 y firmware personalizable

> Plan de trabajo. La issue #16 es la propuesta que agrupa el producto: sus
> ocho issues dependientes (#8 a #15) estan cerradas y su codigo esta en
> `main`. Este plan no implementa un subsistema: audita la evidencia de cada
> criterio de aceptacion, arregla el defecto que la auditoria destapo y deja
> escrito que queda pendiente y por que.

## Alcance

- La evidencia de cada criterio de aceptacion de la issue #16: build, tests,
  ejecucion sobre la placa, conformance de Sparkplug B, rollback de OTA,
  perfiles de marca y documentacion.
- El defecto que la auditoria destapo: la suite Unity se ejecuta dentro de
  `app_main()`, el stack de la tarea principal se agota y la CI reporta verde
  sobre una aplicacion que revienta.
- El registro que hace falta: un ledger criterio-evidencia, la documentacion
  de la puerta nueva y la memoria de la tarea principal.

## Restricciones

- No se asigna ningun GPIO. La retencion de hardware de
  `openspec/config.yaml` (`open_constraints`) sigue vigente: el mapa de
  contactos de la pantalla de 8 pines no es una asignacion de GPIO y GPIO22
  como retroiluminacion sigue siendo candidato.
- El pitch retirado `0.1155 mm` no se reintroduce ni se deriva de el ninguna
  geometria.
- No se implementa runtime de `espnow_manager`, `http_server`,
  `mqtt_bridge`, `lcd_driver`, `lvgl_port`, `ui_screens` ni
  `display_manager`.
- No cambian las particiones, el protocolo, el esquema de NVS ni ninguna API
  publica.
- `firmware/node/` y `firmware/gateway/web/` no estan en disco y este cambio
  no los crea.
- Las carpetas de cambios archivadas son de solo lectura. La unica edicion a
  un artefacto existente son los marcadores de conflicto de
  `openspec/changes/issue-9-gateway-firmware-foundation/apply-progress.md`,
  cuya resolucion se contrasta con la copia archivada.

## Diagnostico

| Hallazgo | Evidencia |
|----------|-----------|
| La suite on-target acaba con `***ERROR*** A stack overflow in task main has been detected` | 114 Tests 0 Failures y luego el desbordamiento, en el perfil de placa y en el de QEMU |
| La CI reporta verde sobre esa ejecucion | `tools/ci/run-device-tests.sh` solo miraba el resumen y el veredicto de Unity |
| El stack de la tarea principal se quedo corto al crecer la suite | 8192 B dejaron 0 bytes de holgura; 16384 B dejan 7504 B |
| El producto no mide su propia holgura | `app_main()` no la reporta; el valor de `CONFIG_ESP_MAIN_TASK_STACK_SIZE` era una estimacion |
| Marcadores de conflicto en el registro del repositorio | Los tres marcadores de un conflicto de merge sin resolver, comprometidos en `openspec/changes/issue-9-gateway-firmware-foundation/apply-progress.md` |

## Trabajo

1. **El defecto**
   - Subir `CONFIG_ESP_MAIN_TASK_STACK_SIZE` a 16384 en
     `firmware/gateway/test/sdkconfig.defaults`, con el comentario que
     recoge la historia (3584 revienta en `nvs_flash_init()`, 8192 dejaron
     de bastar cuando la suite crecio) y la medicion (114 tests, 7504 B).
   - Medir la holgura de la tarea principal al final de la cadena de
     inicializacion de `app_main()`, para que el valor sea un dato y no una
     estimacion.
   - Endurecer `tools/ci/run-device-tests.sh`: fallar si la aplicacion
     revienta despues del resumen, y si la holgura falta o baja del suelo
     (1024 B, ajustable con `HEADROOM_FLOOR_BYTES`).
2. **La evidencia**
   - Build del gateway y del proyecto de pruebas, informe de tamano, puerta
     de secretos y puerta de build reproducible.
   - Tests de host y carga de 10 nodos sinteticos.
   - Suite Unity en los dos perfiles: logica bajo QEMU y placa en la placa
     fisica.
   - Conformance de Sparkplug B contra un broker Mosquitto real.
   - Boot smoke en la placa: cadena de inicializacion, hechos de hardware y
     AP.
   - Rollback de OTA reproducido en la placa: imagen defectuosa armada en el
     slot inactivo, fallo observado y `otadata` releido.
   - Los dos perfiles de marca construidos desde las mismas fuentes y sus
     identidades y digests comparados.
   - Build estricto del sitio, comprobacion de enlaces y estado de la CI.
3. **El registro**
   - `verify-report.md` con una fila por criterio de la issue #16.
   - `docs/testing/hardware-tests.md`: la puerta de crash y el suelo de
     holgura en el catalogo de pruebas que necesitan placa.
   - `AGENTS.md` y `CLAUDE.md`: la leccion en la lista de errores a evitar.
   - Quitar los marcadores de conflicto del registro del cambio #9,
     contrastando con la copia archivada.

## Fuera de alcance

- El runtime que falta: ESP-NOW, servidor HTTP, driver ST7789, LVGL real y
  pantallas reales. El display esta bajo retencion de hardware y el ESP-NOW
  pertenece a la Fase 2.
- `firmware/node/` y la SPA embebida: no estan en disco.
- Los dos criterios que dependen de verificacion fisica: no se pueden
  cumplir desde un terminal, se registran como diferidos.

## Criterios de aceptacion

| Criterio | Como se comprueba |
|----------|-------------------|
| La suite on-target termina con holgura y sin desbordamiento | En la placa: `114 Tests 0 Failures` y `headroom after the run: 7504 bytes of 16384`, sin `***ERROR***` |
| La CI no reporta verde sobre una aplicacion que revienta | Prueba negativa: un build con stack de 4096 B hace que la puerta salga con codigo distinto de cero sobre la misma ejecucion que reportaba `114 Tests 0 Failures` |
| El producto mide su propia holgura | El log de arranque lleva la linea de holgura con un valor mayor que cero |
| Cada criterio de la issue #16 tiene evidencia | `verify-report.md`, una fila por criterio |
| Los criterios diferidos no se cuentan como cumplidos | El ledger los marca como diferidos y explica por que |
| La documentacion recoge la puerta nueva | `docs/testing/hardware-tests.md` y la lista de errores de `CLAUDE.md` |
| El registro del repositorio queda limpio | Ningun fichero de `openspec/` contiene ya los tres marcadores de un conflicto de merge |
