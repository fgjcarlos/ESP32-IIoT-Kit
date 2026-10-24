# Kconfig

#concepto #fase/0

## Que es
El sistema de configuracion de ESP-IDF. Cada opcion se declara en un fichero `Kconfig` o `Kconfig.projbuild` y su valor acaba en `sdkconfig` (el fichero que el usuario versiona) y en `sdkconfig.h` (las macros `CONFIG_*` que ve el codigo C). `menuconfig` es el editor grafico; `idf.py menuconfig` lo lanza.

## Por que importa en este proyecto
- Es la forma de que el firmware tenga mas de una variante sin ramificar el codigo
- Los valores por defecto se versionan en `sdkconfig.defaults`; el `sdkconfig` local es personal y esta gitignored
- La *choice* `BRAND_PROFILE` (issue #12) es el caso real del proyecto: un solo codigo, dos productos

## Como funciona
1. Un componente declara sus opciones en `Kconfig.projbuild`; ESP-IDF las recoge automaticamente
2. `idf.py menuconfig` genera `sdkconfig`; `sdkconfig.defaults` aporta los valores por defecto de la repo
3. El build genera `build/config/sdkconfig.h` con las macros `CONFIG_*` y `build/config/sdkconfig.cmake` con las mismas variables para CMake
4. En el codigo C se leen con `#if CONFIG_X` / `CONFIG_X`; en CMake, con la variable `CONFIG_X`

### El patron de la *choice* con valores por perfil
```kconfig
choice BRAND_PROFILE
    prompt "Brand profile"
    default BRAND_PROFILE_IIOT_KIT

    config BRAND_PROFILE_IIOT_KIT
        bool "IIoT-Kit (reference)"
    config BRAND_PROFILE_ACME_INDUSTRIAL
        bool "Acme Industrial (example customer)"
endchoice

config BRAND_NAME
    string "Brand name"
    default "IIoT-Kit" if BRAND_PROFILE_IIOT_KIT
    default "Acme Industrial" if BRAND_PROFILE_ACME_INDUSTRIAL
```
Cada rama de la *choice* trae sus propios `default`: el perfil decide los valores publicos sin duplicarlos.

## API clave
```kconfig
config BRAND_ASSET_BUDGET_BYTES
    int "Flash budget for the brand assets (bytes)"
    default 32768
    range 4096 262144
    help
        Texto de ayuda multilinea.
```
- `bool`, `int`, `string`, `hex`, `choice`, `menu`, `source`
- `default <valor> if <CONDICION>` — el valor depende de otra opcion
- `depends on` — la opcion solo existe si se cumple la condicion
- `range` — acota el valor de un entero
- `select` / `imply` — habilitan otras opciones

## Gotchas
- **Una *choice* con dos opciones a `y` es un error**: al seleccionar un perfil hay que desactivar los demas a mano (`# CONFIG_OTRO is not set`)
- **`sdkconfig.defaults` no pisa un `sdkconfig` existente**: solo rellena lo que falta. Si cambias un default, el valor antiguo sigue en el `sdkconfig` local
- **El `sdkconfig` esta gitignored**: el default que se versiona es `sdkconfig.defaults`
- **Kconfig no puede leer ficheros**: los assets van en el repositorio, no en el Kconfig
- **Las macros `CONFIG_*` solo existen si la opcion esta activa**: un `string` vacio llega como `""`, no como macro inexistente
- **El orden de las opciones en el menu es el del fichero**, y las opciones de un componente aparecen bajo su componente

## Convencion del proyecto
- Cada componente que declare opciones lleva su `Kconfig.projbuild` junto al `CMakeLists.txt`
- `sdkconfig.brand.<perfil>` es el fragmento que selecciona un perfil para `tools/brand/build-profile.sh`
- Los secretos **no** van en Kconfig: van en `wifi_credentials.local.h` y `brand_secrets.local.h`, gitignored

## Enlaces
- [[White-label]] - el caso real de Kconfig en este proyecto
- [[CMake-ESP-IDF]] - como se cruzan los valores de Kconfig con el build
- [[ESP-IDF]] - el framework completo
