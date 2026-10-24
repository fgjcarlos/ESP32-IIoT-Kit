# Issue #12 — Perfiles de marca sin bifurcar el firmware

## Objetivo

Permitir personalizar el producto por marca mediante **perfiles de compilación**
(Kconfig + assets versionados) sin mantener un fork del firmware por cliente.

## Problema y justificación

El repositorio no tiene capa white-label. Hoy la identidad del producto está
dispersa en constantes (`ASSETS_BRAND_PLACEHOLDER_LABEL "IIoT-Kit"`,
`WIFI_MANAGER_AP_DEFAULT_SSID "IIoT-Gateway"`) y en el nombre del proyecto CMake
(`gateway`). Personalizar para un cliente obligaría a tocar esas constantes, y
mantener un fork por cliente multiplicaría el coste de cada corrección común.

El proyecto ya tiene las piezas que este issue debe respetar:

- `board_profile` (issue #9): lee el perfil del chip conectado.
- `assets_brand` (issue #10): *placeholder* de contrato para los assets de marca.
- `wifi_manager` (issue #21): credenciales inyectadas desde una cabecera
  gitignored (`wifi_credentials.local.h`) generada desde `/.env`.

Este issue convierte ese placeholder en un perfil real de marca y fija la
política de secretos, el presupuesto de flash de los assets y la convención de
nombres de binario.

## Alcance

Incluye:

- Componente nuevo `firmware/gateway/components/brand_profile`:
  - `Kconfig.projbuild` con la *choice* de perfil y los campos públicos
    (nombre, modelo, URL de soporte, SSID inicial, canal OTA, colores).
  - `profiles/<id>/` con los assets versionados de cada perfil
    (`profile.manifest` + `logo.rgb565`).
  - API C: `brand_profile_get()`, `brand_profile_validate()`,
    `brand_profile_format_json()`, `brand_profile_build_artifact_name()`,
    `brand_profile_parse_color()`, `brand_secrets_get()`.
  - Separación **configuración pública** (Kconfig, versionada) frente a
    **secretos de fabricación** (`brand_secrets.local.h`, gitignored).
- Política de presupuesto de flash para los assets de marca
  (`CONFIG_BRAND_ASSET_BUDGET_BYTES`), verificada **en tiempo de build**: el
  build falla si el logo + la reserva de fuente lo superan.
- Metadatos de versión y revisión:
  - versión → `CONFIG_BRAND_FIRMWARE_VERSION` (única fuente de verdad) y
    descriptor de la aplicación ESP-IDF (`PROJECT_VER`).
  - revisión → SHA corto del commit (+ `-dirty`), generado en el build.
  - canal OTA → `CONFIG_BRAND_OTA_CHANNEL`.
- Identidad visible en las tres superficies que pide la issue:
  - **logs** → `brand_profile_log()` en el arranque.
  - **API** → `http_server_brand_payload()` (payload JSON determinista).
  - **LCD** → `assets_brand_splash_text()` + `ui_boot_screen()`.
- Convención reproducible de nombres de binario:
  `<brand>-<model>-v<version>-<channel>-<revision>.bin` + manifiesto JSON con
  metadatos y `sha256`.
- Dos perfiles de ejemplo (`iiot-kit`, `acme-industrial`) con logos distintos
  generados por `tools/brand/gen_logo.py` (procedencia reproducible).

No incluye:

- Claves privadas, certificados ni credenciales reales en el repositorio.
- Un fork por cliente.
- Renderizado LVGL real (LVGL sigue siendo *contract-only* tras issue #10).
- Servidor HTTP completo (Fase 4 es la dueña de `esp_http_server`); este issue
  entrega el *payload* y su contrato, no el arranque del servidor.

## Restricciones y evidencia

- ESP-IDF v5.4, xtensa. Toolchain en `$HOME/esp/esp-idf`. `source export.sh`
  antes de `idf.py`.
- `delivery_strategy: ask-on-risk`. Pausa antes de cada ciclo push+PR.
- `chain_strategy: feature-branch-chain`. Una slice por PR, ≤400 líneas.
- TDD: compilación limpia + etiqueta de Unity + símbolo exportado es la
  evidencia equivalente cuando no hay hardware; el dispositivo
  (`/dev/ttyACM0`) está disponible para el *boot smoke*.
- Sin `Kconfig` en componentes que no lo necesiten: sólo `brand_profile` (y
  `wifi_manager`, que ya lo tenía) declaran opciones.
- Los secretos nunca en Git: `.env` (gitignored) →
  `tools/brand/gen-brand-secrets.sh` → `brand_secrets.local.h` (gitignored).

## Mapa de la separación público / secreto

| Dato | Dónde vive | Versionado | Secreto |
|---|---|---|---|
| Nombre, modelo, URL de soporte, SSID, canal OTA, colores | `Kconfig.projbuild` | sí | no |
| Logo y dimensiones | `profiles/<id>/` | sí | no |
| Versión de firmware | `CONFIG_BRAND_FIRMWARE_VERSION` | sí | no |
| Revisión | SHA del commit (build) | sí | no |
| Clave de aprovisionamiento, token de soporte, contraseña AP de fábrica | `brand_secrets.local.h` | **no** | **sí** |

## Plan de trabajo

### [x] ODD-1 — Componente `brand_profile` (Kconfig + API)

**Archivos**:
- `firmware/gateway/components/brand_profile/Kconfig.projbuild`
- `firmware/gateway/components/brand_profile/include/brand_profile.h`
- `firmware/gateway/components/brand_profile/brand_profile.c`
- `firmware/gateway/components/brand_profile/CMakeLists.txt`
- `firmware/gateway/components/brand_profile/cmake/brand_profile.cmake`
- `firmware/gateway/components/brand_profile/cmake/brand_build_info.h.in`
- `firmware/gateway/components/brand_profile/test/CMakeLists.txt`
- `firmware/gateway/components/brand_profile/test/test_brand_profile.c`

**Aceptación**:
- La *choice* `BRAND_PROFILE` ofrece `iiot-kit` y `acme-industrial`.
- `brand_profile_get()` devuelve la identidad del perfil activo.
- `brand_profile_validate()` rechaza nombre/SSID/URL/color inválidos.
- `brand_profile_format_json()` produce un JSON determinista.
- `brand_profile_build_artifact_name()` produce el nombre reproducible.
- Test bajo la etiqueta `[brand_profile]`.

**Commit**: `feat(gateway): add the brand_profile component (white-label layer)`.

### [x] ODD-2 — Assets versionados y presupuesto de flash

**Archivos**:
- `firmware/gateway/components/brand_profile/profiles/README.md`
- `firmware/gateway/components/brand_profile/profiles/iiot-kit/profile.manifest`
- `firmware/gateway/components/brand_profile/profiles/iiot-kit/logo.rgb565`
- `firmware/gateway/components/brand_profile/profiles/acme-industrial/profile.manifest`
- `firmware/gateway/components/brand_profile/profiles/acme-industrial/logo.rgb565`
- `tools/brand/gen_logo.py`

**Aceptación**:
- Los dos logos son distintos y proceden del generador.
- El build falla si logo + reserva de fuente > presupuesto.
- El presupuesto por defecto (32 KiB) cubre el logo de 8 KiB.

**Commit**: `feat(gateway): add versioned brand assets and the flash budget gate`.

### [x] ODD-3 — Secretos de fabricación

**Archivos**:
- `tools/brand/gen-brand-secrets.sh`
- `firmware/gateway/components/brand_profile/include/brand_secrets.local.h` (gitignored)
- `.gitignore`

**Aceptación**:
- El script nunca imprime el valor de un secreto (`(set)` / `(empty)`).
- `git check-ignore` cubre la cabecera y `git ls-files` no la lista.

**Commit**: `feat(gateway): separate the manufacturing secrets from the public config`.

### [x] ODD-4 — Identidad en logs, LCD y API

**Archivos**:
- `firmware/gateway/components/assets_brand/*` (de placeholder a real)
- `firmware/gateway/components/ui_screens/ui_screens.c`
- `firmware/gateway/components/http_server/http_server.c` + `.h`
- `firmware/gateway/main/main.c`

**Aceptación**:
- El log de arranque muestra nombre, modelo, versión y revisión.
- `http_server_brand_payload()` devuelve el mismo JSON que la API publicará.
- `ui_boot_screen()` muestra la misma identidad en el LCD.
- El SSID del AP es el del perfil de marca.

**Commit**: `feat(gateway): surface the brand identity in the logs, the LCD and the API`.

### [x] ODD-5 — Nombre reproducible del binario + manifiesto

**Archivos**:
- `tools/brand/package.cmake`
- `tools/brand/README.md`
- `firmware/gateway/CMakeLists.txt`
- `firmware/gateway/sdkconfig.defaults`

**Aceptación**:
- El build produce `<brand>-<model>-v<version>-<channel>-<revision>.bin` y su
  `.manifest.json` con `sha256` y metadatos.
- El binario no lleva marca de tiempo (`APP_COMPILE_TIME_DATE=n`).

**Commit**: `build(gateway): emit reproducible branded binaries and a metadata manifest`.

### [x] ODD-6 — Evidencia acumulada y cierre

**Trabajo**:
- Build de los dos perfiles; comprobar que la identidad y el logo difieren.
- `idf.py -C firmware/gateway/test build`; comprobar la etiqueta `[brand_profile]`.
- Tests en host (`tools/host-tests`) para la lógica portable.
- *Boot smoke* en `/dev/ttyACM0` con los dos perfiles.
- Verificar que los secretos no están en el índice de Git.

**Aceptación**:
- Los dos perfiles compilan y muestran identidades distintas.
- Los tests pasan.
- El smoke en dispositivo muestra la identidad correcta.

**Commit**: `docs(odd): record the brand profile evidence of issue 12`.

## Ledger de estado

| ODD | Alcance | Estado | Evidencia |
|---|---|---|---|
| ODD-1 | `brand_profile` (Kconfig + API) | hecho | componente `firmware/gateway/components/brand_profile/`, etiqueta `[brand_profile]` en el ELF de test |
| ODD-2 | Assets versionados + presupuesto | hecho | `profiles/{iiot-kit,acme-industrial}/`, `tools/brand/gen_logo.py`, el build falla si el presupuesto se supera |
| ODD-3 | Secretos de fabricación | hecho | `tools/brand/gen-brand-secrets.sh`, `brand_secrets.local.h` gitignored y no versionado |
| ODD-4 | Identidad en logs, LCD y API | hecho | `brand_profile_log()`, `ui_boot_screen()`, `http_server_brand_payload()` |
| ODD-5 | Nombre reproducible + manifiesto | hecho | `tools/brand/package.cmake`, `brand_package` como target del build |
| ODD-6 | Evidencia acumulada | hecho | builds de los dos perfiles, 95 tests en dispositivo, 57 en host, smoke en `/dev/ttyACM0` |

## Evidencia acumulada (ODD-6)

### Dos perfiles, un mismo código fuente

`tools/brand/build-profile.sh <perfil>` compila cada perfil en su propio
directorio de build y con su propio `sdkconfig`.

| | `iiot-kit` | `acme-industrial` |
|---|---|---|
| Artefacto | `iiot-kit-gw-s3-1-v1.0.0-stable-d0dcb5bb-dirty.bin` | `acme-industrial-ax-200-v1.0.0-beta-d0dcb5bb-dirty.bin` |
| Tamaño | 1.011.344 B | 789.744 B |
| `sha256` | `2f2d5b61…f68f939d` | `9bb47d1a…df625d08` |
| Logo | 64×64 RGB565, 8.192 B, `0f5472b7…` | 64×64 RGB565, 8.192 B, `0703ad7d…` |

Los dos logos son ficheros distintos: el `sha256` de la imagen completa
difiere incluso con la misma revisión.

### Smoke en `/dev/ttyACM0` (mismo equipo, mismo binario compilado dos veces)

Perfil `iiot-kit`:

```
I (1212) brand_profile: brand: name=IIoT-Kit model=GW-S3-1 version=1.0.0 revision=d0dcb5bb-dirty channel=stable
I (1222) brand_profile: brand: profile=iiot-kit support_url=https://example.org/iiot-kit/support ap_ssid=IIoT-Kit-GW logo=64x64 (8192 bytes of a 32768 byte budget)
I (1242) brand_profile: brand: manufacturing secrets absent
I (1239) ui_screens: show: boot — IIoT-Kit GW-S3-1 v1.0.0 d0dcb5bb-dirty
I (1292) assets_brand: brand assets: profile=iiot-kit logo=64x64 (8192 bytes) palette=0B1020/E8EEF7/2E8BFF
I (1462) wifi_manager: wifi_manager_init OK: AP='IIoT-Kit-GW' channel=1 max_conn=4 STA=(disabled)
```

Perfil `acme-industrial`:

```
I (1169) brand_profile: brand: name=Acme Industrial model=AX-200 version=1.0.0 revision=d0dcb5bb-dirty channel=beta
I (1179) brand_profile: brand: profile=acme-industrial support_url=https://support.acme.example ap_ssid=Acme-AX200 logo=64x64 (8192 bytes of a 32768 byte budget)
I (1189) brand_profile: brand: manufacturing secrets absent
I (1239) ui_screens: show: boot — Acme Industrial AX-200 v1.0.0 d0dcb5bb-dirty
I (1249) assets_brand: brand assets: profile=acme-industrial logo=64x64 (8192 bytes) palette=101820/FFFFFF/FF7A00
I (1419) wifi_manager: wifi_manager_init OK: AP='Acme-AX200' channel=1 max_conn=4 STA=(disabled)
```

La identidad cambia en las tres superficies (logs, splash de la LCD, SSID del
AP) y el presupuesto de assets se respeta en ambos.

### Pruebas

- En el dispositivo (`/dev/ttyACM0`, `gateway_test.bin`): **95 tests, 0 fallos,
  0 ignorados**. 17 de ellos son de la capa de marca
  (`[brand_profile]`, `[assets_brand]`, `[http_server]`).
- En el host (`tools/host-tests`): **57 tests, 0 fallos**. Los 10 tests de
  `brand_profile` se compilan contra las fuentes reales del componente.

### Presupuesto de assets

```
-- Brand profile 'iiot-kit': 8192 B of assets + 16384 B of font reserve = 24576 B of a 32768 B budget
```

El build aborta si el total supera el presupuesto; el presupuesto se comprueba
por perfil, no por media.

### Secretos

- `git check-ignore -v firmware/gateway/components/brand_profile/include/brand_secrets.local.h`
  → `.gitignore:63:…`
- `git ls-files -- …` → vacío: la cabecera no está en el índice.
- `tools/brand/gen-brand-secrets.sh --dry-run` sustituye los valores por
  `<redacted>`: la salida se puede compartir sin filtrar nada.

### Reproducibilidad

- `CONFIG_APP_COMPILE_TIME_DATE=n` y `CONFIG_APP_REPRODUCIBLE_BUILD=y` quitan
  la marca de tiempo y las rutas absolutas de la imagen.
- El descriptor de la aplicación lleva la versión de la marca:
  `App "gateway" version: 1.0.0`.

## Guardarraíles

- No se toca `wifi_manager` (sólo se le pasa el SSID del perfil).
- No se toca `partitions.csv`.
- Los secretos nunca se versionan.
- Ningún perfil incluye credenciales reales.
- Los assets de marca no se decodifican como imagen en esta issue: el logo es
  RGB565 crudo y el renderizado real es de `lvgl_port` (Fase posterior).

## Referencias

- GitHub issue: https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/12
- Issue #9 (`board_profile`): `odd/tasks/issue-9-gateway-firmware-foundation/`
- Issue #10 (`assets_brand`, `ui_screens`, `lcd_driver`): `odd/tasks/issue-10-st7789-lvgl.md`
- Issue #21 (`wifi_manager`, credenciales gitignored): `odd/tasks/issue-21-s3-apsta.md`
- ESP-IDF Kconfig: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/kconfig.html
- ESP-IDF embeding data: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/build-system.html#embedding-binary-files
