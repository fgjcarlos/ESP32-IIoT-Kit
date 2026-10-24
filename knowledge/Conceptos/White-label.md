# White-label (perfiles de marca)

#concepto #fase/0

## Que es
Personalizar el producto por cliente **sin mantener un fork del firmware**. Un *perfil de marca* es una seleccion de compilacion: el mismo arbol de fuentes produce imagenes con nombre, modelo, logo, colores, URL de soporte, SSID y canal OTA distintos.

## Por que importa en este proyecto
- El gateway es un producto comercializable: cada cliente quiere "su" marca sin que el proyecto se multiplique en N copias
- Duplicar el firmware por cliente multiplica el coste de cada correccion comun (un bug del stack se arregla N veces)
- La identidad de marca aparece en la LCD, en la API y en los logs: si vive dispersa en constantes, cada cliente necesita su propia rama
- Los secretos de fabricacion (clave de aprovisionamiento, token de soporte, contrasena de fabrica) **no** pueden estar en el repositorio, pero el nombre y los colores **si**

## Como funciona
1. La *choice* de Kconfig `BRAND_PROFILE` selecciona el perfil en tiempo de compilacion
2. Cada perfil tiene un directorio en `firmware/gateway/components/brand_profile/profiles/<id>/` con sus assets versionados (logo + manifiesto)
3. Los valores publicos por defecto (nombre, modelo, colores, URL de soporte, SSID, canal) estan en el Kconfig, con un `default` por rama de la *choice*
4. El build embebe el logo del perfil, comprueba el presupuesto de flash y genera la cabecera con la revision
5. El firmware expone la identidad por tres superficies (logs, LCD, API) que leen la misma estructura

## Reparto publico / secreto
| Dato | Donde | En Git |
|---|---|---|
| Nombre, modelo, colores, URL, SSID, canal, version | Kconfig | si |
| Logo y sus dimensiones | `profiles/<id>/` | si |
| Clave de aprovisionamiento, token de soporte, contrasena del AP | `brand_secrets.local.h` | **no** |

Los secretos se inyectan en el build desde `/.env` con `tools/brand/gen-brand-secrets.sh`. El firmware solo dice si estan presentes, nunca que valen.

## API clave
```c
brand_profile_get()                    // identidad compilada (puntero estatico)
brand_profile_validate(&profile)      // coherencia: URL absoluta, SSID <= 32, logo
brand_profile_format_json(&p, buf, n) // payload determinista de la API
brand_profile_build_artifact_name()   // <brand>-<model>-v<ver>-<canal>-<rev>.bin
brand_profile_parse_color("0B1020", &rgb)
brand_secrets_get()                    // secretos; NULL si no hay
```

## Gotchas
- **El Kconfig no puede leer ficheros**: los assets van en el directorio del perfil, no en el Kconfig. Los valores de texto si pueden tener `default` por rama de la *choice`
- **Un `choice` con dos opciones a `y` es un error de Kconfig**: el fragmento que selecciona un perfil tiene que desactivar explicitamente los demas (`# CONFIG_OTRO is not set`)
- **La *choice* y el directorio deben coincidir**: el mapeo esta en `cmake/brand_profile.cmake` (`brand_profile_resolve`), no en el C
- **El descriptor de la aplicacion lleva su propia version**: `PROJECT_VER` se fija *antes* de `project()`, leyendo el `sdkconfig` generado; si la clave no esta todavia (primer build tras anadir la opcion), se usa el `default` del Kconfig
- **El nombre del binario se calcula dos veces** (en C y en CMake). Ambas implementaciones aplican las mismas reglas de *slug*; el smoke en dispositivo compara el nombre que anuncia el firmware con el fichero que produce el build
- **Un logo RGB565 crudo** evita el decodificador y hace el presupuesto de flash exacto; a cambio, hay que regenerarlo si cambian los colores

## Convencion del proyecto
- `brand_profile` — identidad, version, canal, secretos, presupuesto de assets
- `assets_brand` — assets listos para la pantalla: logo, paleta, linea de splash
- `ui_screens` — las seis pantallas; el splash de arranque lleva la identidad
- `http_server` — el payload de `/api/brand`

## Enlaces
- [[Kconfig]] - la *choice* que selecciona el perfil
- [[CMake-ESP-IDF]] - `configure_file`, `target_add_binary_data`, `PROJECT_VER`
- [[OTA]] - el canal OTA etiqueta las imagenes
- [[NVS]] - los secretos en RAM/NVS, no en el codigo
- `firmware/gateway/components/brand_profile/README.md`
- `tools/brand/README.md`
