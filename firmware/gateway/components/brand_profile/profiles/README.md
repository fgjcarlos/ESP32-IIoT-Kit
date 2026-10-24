# Perfiles de marca

Cada directorio es un perfil: sus assets versionados y la geometría de su
logo. El perfil activo se elige en tiempo de compilación con la *choice* de
Kconfig `BRAND_PROFILE`, nunca editando el código.

## Contenido de un perfil

```
profiles/<id>/
├── profile.manifest   # geometría y versión de los assets
└── logo.rgb565        # el logo, RGB565 crudo, sin cabecera
```

### `profile.manifest`

Formato `clave=valor`, una por línea. Las líneas vacías y las que empiezan por
`#` se ignoran.

| Clave | Significado |
|---|---|
| `id` | identificador del perfil; coincide con el nombre del directorio |
| `asset_version` | versión del paquete de assets, para trazar cambios |
| `logo` | nombre del fichero del logo dentro del directorio |
| `logo_width`, `logo_height` | dimensiones en píxeles |
| `logo_shape`, `logo_glyph` | geometría del logo generado (ver abajo) |

El build comprueba que el tamaño del fichero coincide con
`logo_width × logo_height × 2` bytes (RGB565) y que el conjunto respeta el
presupuesto de flash.

### El logo

El logo es **RGB565 crudo**: exactamente el formato que consume el panel, sin
decodificador, sin conversión en el arranque y sin RAM extra. El tamaño del
fichero es known en tiempo de build, así que el presupuesto de flash es
exacto.

Los logos de este repositorio se **generan**, no se pintan a mano:
`tools/brand/gen_logo.py` lee la paleta del perfil del Kconfig y la geometría
del manifiesto, y dibuja la marca. Así el logo no puede divergir de los
colores que el firmware usa de verdad.

```bash
tools/brand/gen_logo.py \
    --kconfig firmware/gateway/components/brand_profile/Kconfig.projbuild \
    --profile iiot-kit \
    --manifest firmware/gateway/components/brand_profile/profiles/iiot-kit/profile.manifest \
    --out firmware/gateway/components/brand_profile/profiles/iiot-kit/logo.rgb565
```

Formas disponibles:

| `logo_shape` | Figura |
|---|---|
| `circle` | disco |
| `square` | cuadrado |

| `logo_glyph` | Figura |
|---|---|
| `wave` | onda sinusoidal |
| `chevron` | galón `>` |

Un logo de un cliente real se sustituye por su fichero y se deja el
manifiesto con sus dimensiones; el generador deja de ser necesario para ese
perfil.

## Perfiles incluidos

| Perfil | Marca | Modelo | Canal | Logo |
|---|---|---|---|---|
| `iiot-kit` | IIoT-Kit | GW-S3-1 | stable | disco con onda |
| `acme-industrial` | Acme Industrial | AX-200 | beta | cuadrado con galón |

`acme-industrial` es un cliente **ficticio**: existe para demostrar que dos
productos distintos salen del mismo código fuente.

## Añadir un perfil nuevo

1. `profiles/<id>/profile.manifest` con la geometría.
2. La *choice* `BRAND_PROFILE_<ID>` en el Kconfig, con los valores públicos
   por defecto del cliente.
3. El mapeo de la *choice* al directorio en
   `cmake/brand_profile.cmake` (`brand_profile_resolve`).
4. `firmware/gateway/sdkconfig.brand.<id>` con la selección del perfil.
5. Generar el logo y compilar:

   ```bash
   tools/brand/build-profile.sh <id>
   ```
