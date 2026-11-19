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
| T1 | S2, S6 | openspec change `issue-69-local-portal` + este plan + decisión del mecanismo de primera provisión con aprobación del usuario (puerta antes de escribir auth) + ADR | `e52f98e` |
| T2 | S1 | Inline: `wifi_manager` getters de estado (AP/STA/IP/canal) + Unity tests | `87c87df` |
| T3 | S2 | Inline: `portal_auth` — credencial única en NVS (SHA-256, namespace `"portal"`, X-macro keys) + tests host | `64aeb03` |
| T4 | S1 | Inline: pipeline de assets — `vite.config.js` con nombres estables, `dist/` commiteado, generador `portal_assets_data.c` + prueba de integridad | `73716f9` (andamiaje), `9861de6` (embebido en el componente) |
| T5 | S2 | Worker: `http_server` real — `esp_http_server`, rutas assets + API, sesión cookie HttpOnly, rechazo 401 + tests (device/QEMU) | `9861de6` |
| T6 | S3 | Worker (murió por cuota a mitad; completado inline): web W01→W02 — cliente API real, login/estado contra firmware, ConfigView diferida a W03, `node --test` | `fdd76c1` |
| T7 | S1, S3 | Inline: `app_main` inicia el servidor cuando el AP está listo | `9861de6` |
| T8 | S3 | Verificación completa: `idf.py build` limpio, host tests, QEMU device tests; verify independiente `gentle-ai-verify` (riesgo alto: seguridad/auth — item 2) | — |
| T9 | S3, S4 | Docs en fuente canónica, cierre de issue #69, PRs encadenados ≤400 líneas | — |

## Log

- L1: "resuelve la issue #69"
- L2 (2026-11-19, decisión del propietario): mecanismo de primera provisión = opción A — credencial aleatoria al primer arranque, digest SHA-256 en NVS, reveal por serie, cambio forzado en primer login. Registrado en `knowledge/Decisiones/006-portal-provision-inicial.md` (ADR-006). Puerta de S2/S6 levantada: autorizada la implementación de auth.
- L3 (2026-11-19, entorno): la máquina no tenía ESP-IDF; se instaló v5.4 (la versión del CI) en `~/esp/esp-idf` y QEMU-xtensa con `libslirp` extraído a mano (sin sudo).
- L4 (2026-11-19, worker de la web): el subagente que adaptó la SPA murió por cuota a mitad; se completó inline (tests de `status-panel` y `credentials` corregidos, README reescrito, `src/data/` de demo eliminado).
- L5 (2026-11-19, verificación E2E): el QEMU disponible emula `esp32` y `esp32s3`, pero no el PSRAM octal que el gateway exige (`Failed to init external RAM`), así que el portal no se pudo recorrer end-to-end en QEMU. La suite de device corre sobre el target `esp32` (134 tests, incluido el arranque real del `http_server`) y el recorrido desde un navegador sobre el AP físico queda como prueba `[hw]` pendiente de verificación física, tal como fija la especificación.
