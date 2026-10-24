# tools/brand

Herramientas de la capa white-label (issue #12): perfiles de marca, secretos
de fabricación y empaquetado del artefacto.

## `build-profile.sh <perfil> [--clean]`

Compila el gateway con un perfil de marca concreto, en su propio directorio de
build y con su propio `sdkconfig`, de modo que dos perfiles conviven sin
pisarse y **sin tocar nada más del proyecto**.

```bash
tools/brand/build-profile.sh iiot-kit
tools/brand/build-profile.sh acme-industrial
tools/brand/build-profile.sh acme-industrial --clean
```

El script es la forma ejecutable del criterio "dos perfiles de ejemplo
producen firmware con identidad visual diferente desde el mismo código
fuente": lo único que cambia es el perfil seleccionado.

## `gen-brand-secrets.sh [--dry-run]`

Genera `firmware/gateway/components/brand_profile/include/brand_secrets.local.h`
—**gitignored**— a partir de `/.env`, que también está gitignored.

Claves que lee de `/.env`:

| Clave | Para qué |
|---|---|
| `BRAND_SECRET_PROVISIONING_KEY` | secreto de aprovisionamiento por unidad |
| `BRAND_SECRET_SUPPORT_TOKEN` | token de la API de soporte |
| `BRAND_SECRET_AP_PASSWORD` | contraseña de fábrica del soft-AP |

En `--dry-run` los valores se sustituyen por `<redacted>`, así que la
salida se puede pegar en un ticket sin filtrar nada. El script nunca imprime
el valor de un secreto.

Si `/.env` no existe o no tiene ninguna de esas claves, se genera una
cabecera centinela sin secretos: el firmware arranca igual y el log dice
`manufacturing secrets absent`.

```bash
tools/brand/gen-brand-secrets.sh --dry-run
tools/brand/gen-brand-secrets.sh
```

## `gen_logo.py`

Genera el logo RGB565 de un perfil a partir de la paleta del Kconfig y de la
geometría del manifiesto. Ver
`firmware/gateway/components/brand_profile/profiles/README.md`.

## `package.cmake`

No se invoca a mano: el build del gateway lo llama para renombrar la imagen
según el perfil y escribir su manifiesto de metadatos.

Convención de nombre:

```
<brand>-<model>-v<version>-<channel>-<revision>.bin
```

Junto a la imagen escribe `<nombre>.bin.manifest.json` con la identidad, el
tamaño y el `sha256`:

```json
{
  "artifact": "iiot-kit-gw-s3-1-v1.0.0-stable-d0dcb5bb.bin",
  "profile": "iiot-kit",
  "brand": "IIoT-Kit",
  "model": "GW-S3-1",
  "version": "1.0.0",
  "channel": "stable",
  "revision": "d0dcb5bb",
  "support_url": "https://example.org/iiot-kit/support",
  "ap_ssid": "IIoT-Kit-GW",
  "size_bytes": 1011344,
  "sha256": "…",
  "built_at": "2026-10-24T20:47:52Z"
}
```

## Reproducibilidad

El binario es reproducible: el build desactiva la marca de tiempo de
compilación (`CONFIG_APP_COMPILE_TIME_DATE=n`) y activa
`CONFIG_APP_REPRODUCIBLE_BUILD=y`, que elimina rutas absolutas de la imagen.
Mismo commit + mismo perfil ⇒ mismos bytes.

El nombre lleva el SHA corto del commit, con el sufijo `-dirty` cuando el
árbol tiene cambios locales: un artefacto no puede presumir de ser
reproducible cuando no lo es.
