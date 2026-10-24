# `brand_profile` — la capa white-label

Permite personalizar el producto por marca **sin bifurcar el firmware**: el
mismo árbol de fuentes produce imágenes con identidad visual distinta.

## Idea

Un *perfil de marca* es una selección de compilación. La *choice* de Kconfig
`BRAND_PROFILE` elige uno de los perfiles de
`components/brand_profile/profiles/<id>/`, y cada perfil trae:

- sus valores públicos por defecto (nombre, modelo, colores, URL de soporte,
  SSID inicial, canal OTA), y
- sus assets versionados (el logo y su manifiesto).

```
firmware/gateway/components/brand_profile/
├── Kconfig.projbuild          # la configuración pública
├── include/brand_profile.h    # API
├── brand_profile.c            # lógica portable (sin ESP-IDF)
├── brand_profile_esp.c        # glue ESP-IDF (Kconfig + logo embebido)
├── cmake/
│   ├── brand_profile.cmake    # resolución del perfil + presupuesto de flash
│   └── brand_build_info.h.in  # cabecera generada (revisión, geometría)
└── profiles/
    ├── iiot-kit/              # perfil de referencia
    └── acme-industrial/       # perfil de ejemplo (cliente)
```

## Dos mitades que no se mezclan

| | Configuración **pública** | Secretos de **fabricación** |
|---|---|---|
| Dónde | `Kconfig.projbuild` | `include/brand_secrets.local.h` |
| Versionado | sí, en Git | **no**, gitignored |
| Contenido | nombre, modelo, colores, URL de soporte, SSID inicial, canal OTA, versión | clave de aprovisionamiento, token de soporte, contraseña de fábrica del AP |
| Generado por | el Kconfig | `tools/brand/gen-brand-secrets.sh` desde `/.env` |

El firmware **nunca** imprime un secreto: `brand_profile_log()` dice si los
secretos están presentes, no qué valen.

## La API

| Función | Para qué |
|---|---|
| `brand_profile_get()` | la identidad compilada (perfil, nombre, modelo, URL, SSID, versión, canal, revisión, colores, logo) |
| `brand_profile_validate()` | coherencia de la identidad: URL absoluta, SSID ≤ 32, logo coherente con sus dimensiones |
| `brand_profile_format_json()` | payload JSON determinista de la API de marca |
| `brand_profile_build_artifact_name()` | nombre reproducible del binario |
| `brand_profile_slugify()` | normalización a `[-a-z0-9.]` |
| `brand_profile_parse_color()` / `_format_color()` | colores `RRGGBB` ⇄ `0xRRGGBB` |
| `brand_secrets_get()` / `brand_secrets_present()` | acceso a los secretos, sin exponerlos |

`brand_profile.c` no llama a la API de ESP-IDF: por eso la lógica
determinista se puede ejecutar en el host (`tools/host-tests`).

## Dónde aparece la identidad

| Superficie | Mecanismo |
|---|---|
| **Logs** | `brand_profile_log()` en el arranque de `main.c` |
| **LCD** | `assets_brand_splash_text()` → `ui_boot_screen()`; la paleta, en `assets_brand_get_palette()` |
| **API** | `http_server_brand_payload()`, el payload de `/api/brand` |

Las tres superficies leen la misma estructura, así que no pueden discrepar.

## Presupuesto de flash de los assets

`CONFIG_BRAND_ASSET_BUDGET_BYTES` (32 KiB por defecto) es el techo de los
assets de marca: **logo + reserva de fuente**. El build lo comprueba y **falla**
si se supera, de modo que un logo demasiado grande nunca llega al dispositivo.

`CONFIG_BRAND_FONT_RESERVE_BYTES` (16 KiB por defecto) reserva el hueco de la
fuente: LVGL sigue siendo *contract-only* (issue #10), pero la reserva ya
cuenta contra el presupuesto para que la fuente futura tenga sitio conocido.

## Metadatos y nombre del artefacto

- **Versión**: `CONFIG_BRAND_FIRMWARE_VERSION`. Es la única fuente de verdad:
  el build la copia a `PROJECT_VER` antes de `project()`, de modo que el
  descriptor de la aplicación de ESP-IDF lleva la misma versión que el
  firmware.
- **Revisión**: SHA corto del commit (`-dirty` si el árbol está modificado),
  calculada en el build.
- **Canal**: `CONFIG_BRAND_OTA_CHANNEL`.

Convención de nombre (idéntica en C y en CMake):

```
<brand>-<model>-v<version>-<channel>-<revision>.bin
```

Ejemplo: `iiot-kit-gw-s3-1-v1.0.0-stable-d0dcb5bb.bin`.

Junto al binario, el build escribe `<nombre>.bin.manifest.json` con la
identidad, el tamaño y el `sha256` de la imagen.

## Compilar otro perfil

```bash
tools/brand/build-profile.sh acme-industrial
```

Cada perfil usa su propio directorio de build y su propio `sdkconfig`, así que
dos perfiles conviven sin pisarse.

## Añadir un perfil

1. Crear `profiles/<id>/profile.manifest` con la geometría del logo.
2. Añadir la *choice* `BRAND_PROFILE_<ID>` al Kconfig, con sus valores
   públicos por defecto.
3. Mapear la *choice* al directorio en `cmake/brand_profile.cmake`
   (`brand_profile_resolve`).
4. Crear `firmware/gateway/sdkconfig.brand.<id>` con la selección.
5. Regenerar el logo: `tools/brand/gen_logo.py`.

Ver `profiles/README.md` para el detalle.

## Referencias

- Issue #12: https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/12
- `odd/tasks/issue-12-brand-profiles.md` — plan y evidencia
- ESP-IDF, Kconfig: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/kconfig.html
- ESP-IDF, incrustar binarios: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/build-system.html
