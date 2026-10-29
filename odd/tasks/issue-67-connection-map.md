# Feature: Mapa de conexión verificado placa ↔ pantalla (issue #67)

Documento de feature ODD para la historia H01 del roadmap de fases restantes.
Publica el esquema de conexión, el protocolo de comprobación física y el estado
de la evidencia en el perfil canónico. No implementa la pantalla ni cierra el
bloqueo físico: el bloqueo se mantiene mientras falte prueba.

## Specs

- **S1** (petición del usuario, verbatim): "Vamos con la issue #67"
- **S2** (objetivo de la issue #67, verbatim): "Como integrador, quiero disponer de un mapa de conexión verificado para conectar la pantalla sin dañar la placa ni perder USB o PSRAM."
- **S3** (criterio 1 de la issue #67, verbatim): "Identificar físicamente carrier y pantalla; registrar qué está confirmado y qué sigue pendiente, sin convertir datos del vendedor en medidas."
- **S4** (criterio 2 de la issue #67, verbatim): "Verificar alimentación, niveles lógicos, retroiluminación y correspondencia entre contactos de pantalla y GPIO; conservar las reservas del perfil canónico."
- **S5** (criterio 3 de la issue #67, verbatim): "Publicar en el perfil canónico el esquema de conexión y las evidencias necesarias para autorizar el primer encendido; si falta prueba, mantener el bloqueo."
- **S6** (exclusión de la issue #67, verbatim): "No incluye implementar pantallas, asignar GPIO por suposición ni reutilizar valores retirados del vendedor."
- **S7** (decisión ODD-3 del usuario, 2026-10-28): en el banco de pruebas hay "Placa + pantalla + multímetro", y se publica "Sí, candidatos con justificación": el perfil canónico publica GPIO candidatos con su razonamiento y estado explícito "provisional, pendiente de verificación física".
- **S8** (restricción heredada del repo): el valor retirado `0.1155 × 0.1155 mm` de pitch del vendedor no se reutiliza; las cifras del vendedor no se convierten en medidas; `GPIO35/36/37` (PSRAM Octal), `GPIO19/20` (USB nativo), `GPIO43/44` (UART0), `GPIO0/3/45/46` (strapping) y `GPIO48` (WS2812 onboard) siguen reservados.
- **S9** (hallazgo verificado en el datasheet oficial ESP32-S3-WROOM-1, v1.4, tablas de pines del módulo): el módulo **no expone GPIO22** (ni GPIO22–GPIO34); expone GPIO0–GPIO21, GPIO35–GPIO42, GPIO45–GPIO48 más TXD0/RXD0. En consecuencia, el candidato de retroiluminación `GPIO22` heredado de la issue #10 es imposible en este hardware y estaba copiado del carrier C6 retirado.

## Tasks

- [x] T1 (S1–S9), inline: documento de feature + espejo Engram (`odd/issue-67-connection-map/tasks.md`). Commit: `de5bea3` (protocol slice).
- [x] T2 (S3, S4, S5), delegated: `docs/hardware/display-bringup-protocol.md` — protocolo por etapas con puntos de medida, criterio de aprobación por paso, reglas de seguridad y tabla de registro de evidencia.
- [x] T3 (S2, S4, S5), delegated: perfil canónico `docs/hardware/esp32-s3-wroom-1-n16r8.md` — esquema de conexión (contacto → señal de carrier → GPIO candidato), reservas vigentes, tabla de estado de evidencia y bloqueo explícito.
- [x] T4 (S6, S9), delegated: corregir el candidato imposible `LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE` (22 → candidato vigente) en `firmware/gateway/components/lcd_driver/`, su test Unity, y las menciones obsoletas de GPIO22 en `AGENTS.md` y `CLAUDE.md`.
- [x] T5 (S1–S9), inline: verificación — build del proyecto de test y ejecución del caso Unity de `lcd_driver`, lectura del diff y commits de unidad de trabajo; protocol: `de5bea3`, map/contract: this slice (identity recorded in Engram after commit). Physical T6 remains blocked.

- [ ] T6 (S3–S5), external BLOCK: exact-unit identification, documentary ratings, measurements and named per-stage release from the integrator. No connection or power authorization.

## Log

- L1 (2026-10-28): "Vamos con la issue #67"
- L2 (2026-10-28): issue #67 leída desde GitHub (`fgjcarlos/ESP32-IIoT-Kit`), estado OPEN, label `enhancement`, sin dependencias. Encaja como H01 en `docs/replanificacion/04-roadmap-historias.md`.
- L3 (2026-10-28): lectura del perfil canónico `docs/hardware/esp32-s3-wroom-1-n16r8.md` (250 líneas): el carrier y la pantalla siguen sin validar; GPIO35–GPIO37 reservados por PSRAM Octal; GPIO19/20 USB; GPIO48 WS2812 confirmado por smoke; retroiluminación como "candidato a GPIO22" pendiente de verificación física.
- L4 (2026-10-28): datasheet oficial ESP32-S3-WROOM-1 v1.4 consultado. Confirmado: GPIO35/36/37 corresponden al bus Octal de PSRAM; el módulo no expone GPIO22–GPIO34; la tabla de pines del módulo expone GPIO0–GPIO21, GPIO35–GPIO42, GPIO45–GPIO48, TXD0/RXD0. Nota: para las variantes N16R16V, IO47/IO48 trabajan en dominio 1,8 V; **no** aplica a N16R8.
- L5 (2026-10-28): decisión ODD-3 del usuario — hay placa, pantalla y multímetro disponibles, y se publican GPIO candidatos con justificación y estado provisional. El bloqueo físico se mantiene hasta que el protocolo se ejecute y se registre la evidencia.
- L6 (2026-10-28): el candidato de retroiluminación `GPIO22` que arrastra `lcd_driver` desde la issue #10 es imposible en este módulo (S9). Se corrige en T4 para que el repositorio no conserve un valor que induciría a cablear un pin inexistente; el nuevo valor sigue siendo candidato no vinculante.
- L7 (2026-10-28): no hay RED/GREEN de firmware para una historia de validación física: la prueba que decide es eléctrica y visual sobre el hardware real. La comprobación aplicable es el test Unity que fija el contrato del driver, más la lectura del diff y del esquema publicado.
- L8 (2026-10-28): alcance deliberadamente limitado a documentación y contrato. No se asignan GPIOs en el firmware de display, no se inicializa `esp_lcd_*`, no se conecta la pantalla. La issue #87 (L02) y la #86 (L01) siguen siendo las historias de implementación.
- L9 (2026-10-28): la ruta del carrier es la que el usuario photographió y las cifras del vendedor siguen siendo "reported, not physically verified". Ninguna cifra del vendedor se copia como medida; el protocolo exige leer el valor en el multímetro y registrarlo.

- L10: "Continua con la resolucion de la issue #67". Reconciled actual branch and untracked drafts; T2 needs safety correction before completion. GPIO22 replacement is GPIO7, a proposal only; no GPIO is configured. Risk: item 6 (physical wiring safety cannot be proved by software tests).
- L11: Resume review found unsafe BLK inference, circular header checks and powering an unidentified display. T2 will gate power on exact-module ratings/schematic, identify headers with the display disconnected and leave BLK unconnected until its circuit is established. A multimeter alone cannot validate logic thresholds or transient limits.
- L12: Runtime context backstop fired; T2–T4 delegated together as one cohesive bounded unit to gentle-ai-worker, task mvsjzr41-1-mzpk. Parent retains tracking and T5 checks. Physical evidence remains external and blocked; no wiring authorized.
- L13: Writer mvsjzr41-1-mzpk delivered T2–T4 drafts, observed Unity RED (Expected 7 Was 22), GREEN 2/2, host 78/78 and stress zero failures. Parent read back protocol/map/header; completion NOT checked off pending independent safety verification mvsk6byx-2-4k0h, gateway/test builds job-1 and correction of two stale GPIO22 comments. Physical acceptance remains blocked.
- L14: Parent observed gateway and device-test builds exit 0, MkDocs strict exit 0 (upstream advisory/deprecation only), and QEMU exit 0: 111 Tests 0 Failures 0 Ignored; main-task headroom 7504/16384 bytes. Parent spot-check confirmed both real lcd_driver Unity cases PASS, including provisional GPIO7. Hardware-tagged tests excluded; no hardware safety proof inferred. T4 implementation/test complete; commit evidence pending final safety verification.
- L15: Independent verifier mvsk6byx-2-4k0h requires one correction batch: B1 unify pre-power human release and all applicable prior checks; B2 documented circuit GND plus verified common return for isolated display supply; B3 safe input-voltage/current/unpowered limits and documented dual-supply sequencing (VIH/VIL alone insufficient). Reset-polarity documentary request also aligned. Physical S3/S4 remain external BLOCK. Correction delegated to mvskck39-3-cx9w; verifier regression for exposed/unreserved candidate will join owning Unity test.
- L16: Worker mvskck39-3-cx9w delivered B1–B3 correction and candidate exposure/reservation regression; targeted LCD3/3 and maintained host78/78 reported. Parent read back staged release/evidence model; one targeted independent recheck mvskh17h-4-cyiv and final builds/QEMU job-3 running. No completion or physical release inferred.
- L17: Final parent gateway/test builds and QEMU job-3 exited 0: 112 Tests 0 Failures 0 Ignored, headroom7504/16384 bytes. New exposure/reservation regression included; hardware-tagged tests excluded. Independent targeted correction verdict still pending.
- L18: Targeted recheck resolved B2 but same B1/B3 persisted: all-row P1 circularity, mere overlap vs full-envelope compatibility, and Stage6 powered-rewiring order. Parent applied final surgical correction to those same blockers only; final targeted recheck requested. P1 explicit pre-power IDs exclude P2/E/F; E2 wires with sources off, E3 enables after; full voltage/current envelope plus VIH/VIL required or B blocked. No further broad verification or correction rounds authorized by this plan.
- L19: Final targeted verifier mvskkdhf-5-w7tq resolved B1, B3 and Stage6 sequencing; B2 previously resolved. Documentary T2/T3 accepted, not physical S3/S4. Final checks: firmware+Unity build, QEMU112/112, LCD3/3, host78/78, stress0, MkDocs strict and diff check passed. Hardware tests excluded; physical evidence unavailable.
- L20: Delivery slices: (1) protocol + feature tracker, (2) canonical map + candidate contract/test + active/historical guidance and local task mirror. Each below400 authored lines; no push/PR/issue closure. Risk: item 6 (physical safety cannot be proved by software).
- L21: Protocol/tracker committed as de5bea3 (394 authored lines). Native assessment remained unavailable while the local task mirror was undeclared/untracked; no native review approval claimed. Second slice includes intended mirror, removing that declaration ambiguity; assess each exact committed range after normalization.
