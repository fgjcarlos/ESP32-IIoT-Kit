# Issue #69 — Portal local protegido servido por el gateway (W02 — Base)

Vertical delivery: firmware, embedded assets, auth, status API, tests and docs
for the protected local portal over the gateway AP. Plan of record for the goal
"resuelve la issue #69". Change openspec: `openspec/changes/issue-69-local-portal/`.

## Specs

- S1 (criterio de aceptación #1, issue #69): "Entregar el recorrido aprobado de acceso y una página de estado funcional, con HTML/CSS/JS servidos desde flash y sin recursos externos obligatorios."
- S2 (criterio de aceptación #2, issue #69): "Rechazar acceso no autorizado a operaciones y datos protegidos; acordar el mecanismo de primera provisión antes de implementarlo, sin publicar secretos."
- S3 (criterio de aceptación #3, issue #69): "Verificar desde un navegador conectado al AP, sin STA/Internet, que el portal funciona y comunica los errores; incluir pruebas de API, interfaz y recursos embebidos."
- S4 (alcance, issue #69): "Incluye el resultado funcional descrito y su manejo de errores. Entrega vertical: código de nodo/gateway, protocolo, API, interfaz, persistencia, pruebas y documentación sólo en la medida necesaria para esta función. No separar frontend, API y tests en tickets inconexos."
- S5 (fuera de alcance, issue #69): "No incluye nodos, historial, cuentas múltiples ni un backend Bun/Node."
- S6 (dependencias, issue #69): "Las vistas nuevas o modificadas requieren wireframes aprobados antes de implementar su interfaz. Las decisiones de seguridad/hardware pendientes se resuelven antes de escribir la parte afectada."
  - W01 (issue #68) cerrado: wireframes W01-v0.2 aprobados; las vistas de W02 son las de W01-v0.2 sin vistas nuevas.
  - La vista de configuración Wi-Fi (ConfigView del demo) se activa en W03, no en W02: roadmap `docs/replanificacion/04-roadmap-historias.md:9` ("W02 entrega un portal protegido alojado realmente en el gateway; W03 permite configurar Wi-Fi conservando acceso local").

## Tasks

| ID | Specs | Route | Commit |
|----|-------|-------|--------|
| T1 | S2, S6 | openspec change `issue-69-local-portal` + este plan + decisión del mecanismo de primera provisión con aprobación del usuario (puerta antes de escribir auth) + ADR | slices 01 (`01-openspec`) y 02 (`02-plan-adr`)|
| T2 | S1 | Inline: `wifi_manager` getters de estado (AP/STA/IP/canal) + Unity tests | slice 06 (`06-wifi-status`)|
| T3 | S2 | Inline: `portal_auth` — credencial única en NVS (SHA-256, namespace `"portal"`, X-macro keys) + tests host | slices 03-05 (`03-auth-core`, `04-auth-tests`, `05-host-shims`)|
| T4 | S1 | Inline: pipeline de assets — `vite.config.js` con nombres estables, `dist/` commiteado, generador `portal_assets_data.c` + prueba de integridad | slice 07 (`07-assets`)|
| T5 | S2 | Worker: `http_server` real — `esp_http_server`, rutas assets + API, sesión cookie HttpOnly, rechazo 401 + tests (device/QEMU) | slices 08-10 (`08-portal-api`, `09-portal-api-tests`, `10-server`)|
| T6 | S3 | Worker (murió por cuota a mitad; completado inline): web W01→W02 — cliente API real, login/estado contra firmware, ConfigView diferida a W03, `node --test` | slices 11-12 (`11-web-api`, `12-web-app`)|
| T7 | S1, S3 | Inline: `app_main` inicia el servidor cuando el AP está listo | slice 10 (`10-server`)|
| T8 | S3 | Verificación completa: `idf.py build` limpio, host tests, QEMU device tests; verify independiente `gentle-ai-verify` (riesgo alto: seguridad/auth — item 2) | slices 09-10 y la verificación del tracker|
| T9 | S3, S4 | Docs en fuente canónica, cierre de issue #69, PRs encadenados ≤400 líneas | slice 13 (`13-docs`)|

## Cadena de publicación

La entrega supera el límite de 400 líneas por PR, así que va encadenada como
`Feature Branch Chain`: un PR tracker (draft, sin merge) y 13 PRs hijos, cada
uno una unidad de trabajo completa y verificable por separado. El orden sigue
las tareas T1 a T9.

| Slice | Rama | Unidad | Líneas |
|---|---|---|---|
| 01 | `feature/issue-69-01-openspec` | change de OpenSpec (propuesta, diseño, tareas, spec) | 341 |
| 02 | `feature/issue-69-02-plan-adr-r2` | este plan + ADR-006 | 112 |
| 03 | `feature/issue-69-03-auth-core-r2` | `portal_auth`: reglas de credencial y sesión | 385 |
| 04 | `feature/issue-69-04-auth-tests-r2` | glue de dispositivo y suite Unity del auth | 271 |
| 05 | `feature/issue-69-05-host-shims-r2` | shims de NVS y ports deterministas del host | 223 |
| 06 | `feature/issue-69-06-wifi-status-r2` | getters de estado de `wifi_manager` | 121 |
| 07 | `feature/issue-69-07-assets-r2` | bundle estable, generador y tabla de assets | 313 |
| 08 | `feature/issue-69-08-portal-api-r2` | reglas portables de JSON y cookie | 303 |
| 09 | `feature/issue-69-09-portal-api-tests-r2` | contrato de bytes de la API y su suite | 233 |
| 10 | `feature/issue-69-10-server-r2` | servidor HTTP real y arranque en `app_main` | 337 |
| 11 | `feature/issue-69-11-web-api-r2` | el cliente de la API de la interfaz | 112 |
| 12 | `feature/issue-69-12-web-app-r2` | la interfaz contra la API real, con sus tests | 733 (excepción de pantalla propia) |
| 13 | `feature/issue-69-13-docs-r2` | fuentes canónicas y README de la web | 128 |

## Log

- L1: "resuelve la issue #69"
- L2 (2026-11-19, decisión del propietario): mecanismo de primera provisión = opción A — credencial aleatoria al primer arranque, digest SHA-256 en NVS, reveal por serie, cambio forzado en primer login. Registrado en `knowledge/Decisiones/006-portal-provision-inicial.md` (ADR-006). Puerta de S2/S6 levantada: autorizada la implementación de auth.
- L3 (2026-11-19, entorno): la máquina no tenía ESP-IDF; se instaló v5.4 (la versión del CI) en `~/esp/esp-idf` y QEMU-xtensa con `libslirp` extraído a mano (sin sudo).
- L4 (2026-11-19, worker de la web): el subagente que adaptó la SPA murió por cuota a mitad; se completó inline (tests de `status-panel` y `credentials` corregidos, README reescrito, `src/data/` de demo eliminado).
- L5 (2026-11-19, verificación E2E): el QEMU disponible emula `esp32` y `esp32s3`, pero no el PSRAM octal que el gateway exige (`Failed to init external RAM`), así que el portal no se pudo recorrer end-to-end en QEMU. La suite de device corre sobre el target `esp32` (134 tests, incluido el arranque real del `http_server`) y el recorrido desde un navegador sobre el AP físico queda como prueba `[hw]` pendiente de verificación física, tal como fija la especificación.
