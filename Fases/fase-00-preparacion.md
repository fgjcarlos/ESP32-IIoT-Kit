# Fase 0: Preparacion y Aprendizaje

> **CORRECCIONES APLICADAS** (2026-05-23): T0.8.2 usa el protocolo unificado de `docs/replanificacion/02-protocolo-unificado.md`. Se agrega T0.8.3 (test manual del protocolo). Ver detalles en `docs/replanificacion/03-fases-corregidas.md`.

## Resumen

- **Objetivo**: Validar hardware, establecer entorno de desarrollo y familiarizarse con ESP-IDF. Al finalizar esta fase, el desarrollador debe ser capaz de compilar, flashear y depurar proyectos basicos en ESP32-S3 y ESP32-C3, leer sensores, comunicar dispositivos via ESP-NOW y configurar WiFi en modo simultaneo AP+STA.
- **Duracion estimada**: 3 semanas
- **Prerequisitos**: Ninguno (es la primera fase del proyecto)
- **Hardware necesario**:
  - 1x ESP32-S3-DevKitC (gateway)
  - 1-3x ESP32-C3-DevKitM (nodos sensores)
  - 1x Sensor de temperatura (DS18B20 o BMP280)
  - 1x Potenciometro (10K recomendado)
  - Cables jumper macho-macho y macho-hembra
  - 1-2x Breadboard
  - 1x Multimetro digital (para medir corriente en mA y uA)
  - Cables USB-C (para programacion y alimentacion)

## Dependencias externas

| Dependencia | Version | Enlace |
|---|---|---|
| ESP-IDF | v5.x (recomendado v5.4) | https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/ |
| VS Code | Ultima estable | https://code.visualstudio.com/ |
| ESP-IDF Extension para VS Code | Ultima estable | https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension |
| Git | >= 2.x | https://git-scm.com/ |
| Python | >= 3.8 (requerido por ESP-IDF) | https://www.python.org/ |

---

## Sub-tarea 0.1: Entorno de desarrollo

**Objetivo**: Tener un entorno funcional capaz de compilar y flashear codigo en ambos chips (ESP32-S3 y ESP32-C3).

---

### Tarea T0.1.1: Instalar ESP-IDF v5.x
- **Dificultad**: Basico
- **Descripcion**: Descargar e instalar el framework ESP-IDF version 5.x en tu maquina de desarrollo. Pasos:
  1. Abrir una terminal.
  2. Clonar el repositorio oficial: `git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf`
  3. Ejecutar el script de instalacion: `cd ~/esp/esp-idf && ./install.sh esp32s3,esp32c3`
  4. Cargar las variables de entorno: `source ~/esp/esp-idf/export.sh`
  5. Verificar la instalacion: `idf.py --version`
  6. El comando debe imprimir algo como `ESP-IDF v5.4`.
- **Archivos a crear/modificar**: Ninguno (instalacion del sistema)
- **Criterio de aceptacion**:
  - `idf.py --version` devuelve una version 5.x
  - `xtensa-esp32s3-elf-gcc --version` funciona (toolchain Xtensa para S3)
  - `riscv32-esp-elf-gcc --version` funciona (toolchain RISC-V para C3)
- **Dependencias**: Ninguna
- **Pistas**: El script `install.sh` acepta una lista de targets separados por coma. Usar `install.sh esp32s3,esp32c3` instala solo los toolchains necesarios y es mas rapido que `install.sh all`. El script `export.sh` configura PATH, IDF_PATH y otras variables.
- **Errores comunes**:
  - Olvidar ejecutar `source export.sh` en cada terminal nueva. Solucion: añadir `. ~/esp/esp-idf/export.sh` al fichero `~/.bashrc` o `~/.zshrc`.
  - No usar `--recursive` al clonar, lo que provoca que falten submodulos. Solucion: ejecutar `git submodule update --init --recursive` despues de clonar.
- **Tiempo estimado**: 1-2 horas (depende de la velocidad de descarga)

---

### Tarea T0.1.2: Configurar VS Code con extension ESP-IDF
- **Dificultad**: Basico
- **Descripcion**: Instalar y configurar Visual Studio Code con la extension oficial de Espressif para tener autocompletado, compilacion y flasheo integrados. Pasos:
  1. Abrir VS Code.
  2. Ir a Extensions (Ctrl+Shift+X) y buscar "ESP-IDF".
  3. Instalar la extension "Espressif IDF" (publisher: espressif).
  4. Al iniciar la extension, seleccionar "Use existing setup" y apuntar al directorio `~/esp/esp-idf`.
  5. Verificar que la barra inferior de VS Code muestra los botones de ESP-IDF (build, flash, monitor, target).
  6. Configurar el target a `esp32s3` desde la barra inferior.
- **Archivos a crear/modificar**: Ninguno (configuracion del IDE)
- **Criterio de aceptacion**:
  - La extension muestra los botones de build/flash/monitor en la barra inferior de VS Code
  - El autocompletado funciona para funciones de ESP-IDF (probar escribiendo `esp_` y ver sugerencias)
  - Se puede seleccionar el target (esp32s3/esp32c3) desde la barra inferior
- **Dependencias**: Requiere T0.1.1
- **Pistas**: La extension busca ESP-IDF en `IDF_PATH`. Si no lo encuentra automaticamente, configurar manualmente en Settings > ESP-IDF: Idf Path. Tambien se puede ejecutar el comando "ESP-IDF: Configure ESP-IDF Extension" desde la paleta de comandos (Ctrl+Shift+P).
- **Errores comunes**:
  - La extension no encuentra el toolchain si no se configuro el PATH correctamente en T0.1.1. Solucion: verificar que `IDF_PATH` apunta al directorio correcto.
  - Tener multiples versiones de Python instaladas puede confundir a la extension. Solucion: configurar explicitamente la ruta de Python en la configuracion de la extension.
- **Tiempo estimado**: 30-60 minutos

---

### Tarea T0.1.3: Conectar ESP32-S3 por USB y verificar comunicacion
- **Dificultad**: Basico
- **Descripcion**: Conectar fisicamente la placa ESP32-S3-DevKitC al ordenador por USB y verificar que el sistema operativo la reconoce. Pasos:
  1. Conectar la placa ESP32-S3-DevKitC al ordenador usando un cable USB-C (asegurarse de que es un cable de datos, no solo de carga).
  2. En Linux, ejecutar `ls /dev/ttyUSB*` o `ls /dev/ttyACM*` para identificar el puerto. El ESP32-S3 normalmente aparece como `/dev/ttyACM0` (USB nativo) o `/dev/ttyUSB0` (si usa chip UART externo).
  3. Verificar con `dmesg | tail -20` que el kernel ha detectado el dispositivo.
  4. Si da error de permisos al acceder al puerto, añadir tu usuario al grupo `dialout`: `sudo usermod -aG dialout $USER` y cerrar/abrir sesion.
  5. Probar la comunicacion basica: `python -m serial.tools.miniterm /dev/ttyACM0 115200` (Ctrl+] para salir).
- **Archivos a crear/modificar**: Ninguno
- **Criterio de aceptacion**:
  - El comando `ls /dev/ttyACM*` o `ls /dev/ttyUSB*` muestra al menos un dispositivo al conectar la placa
  - `dmesg | tail` muestra un mensaje de deteccion del dispositivo USB
  - El miniterm se conecta sin error de permisos
- **Dependencias**: Ninguna (solo hardware)
- **Pistas**: El ESP32-S3-DevKitC tiene USB nativo (USB-OTG) ademas del conector UART. El puerto USB nativo suele aparecer como `/dev/ttyACM0`. Usa `dmesg | grep tty` para filtrar solo los mensajes relevantes.
- **Errores comunes**:
  - Usar un cable USB de solo carga (sin lineas de datos). Solucion: probar con otro cable si no aparece nada en `dmesg`.
  - No tener permisos de acceso al puerto serie. Solucion: `sudo usermod -aG dialout $USER` y reiniciar sesion (cerrar y volver a abrir terminal no es suficiente, hay que cerrar sesion del sistema).
- **Tiempo estimado**: 15-30 minutos

---

### Tarea T0.1.4: Compilar y flashear hello_world en ESP32-S3
- **Dificultad**: Basico
- **Descripcion**: Compilar el ejemplo basico `hello_world` incluido en ESP-IDF y flashearlo en la placa ESP32-S3. Este es el primer contacto con el ciclo completo de desarrollo. Pasos:
  1. Copiar el ejemplo a una carpeta de trabajo: `cp -r ~/esp/esp-idf/examples/get-started/hello_world ~/esp/hello_world_s3`
  2. Entrar en la carpeta: `cd ~/esp/hello_world_s3`
  3. Cargar variables de entorno: `source ~/esp/esp-idf/export.sh`
  4. Configurar el target: `idf.py set-target esp32s3`
  5. Compilar: `idf.py build`
  6. Flashear y abrir monitor serie: `idf.py -p /dev/ttyACM0 flash monitor`
  7. Observar la salida: debe mostrar "Hello world!" seguido de una cuenta regresiva y reinicio.
  8. Para salir del monitor: pulsar `Ctrl+]`.
- **Archivos a crear/modificar**: Ninguno (se usa el ejemplo tal cual)
- **Criterio de aceptacion**:
  - La compilacion termina sin errores (`Project build complete.`)
  - El flasheo termina exitosamente (`Hard resetting via RTS pin...`)
  - El monitor serie muestra "Hello world!" y la informacion del chip (modelo, cores, revision)
  - El dispositivo se reinicia automaticamente tras la cuenta regresiva
- **Dependencias**: Requiere T0.1.1, T0.1.3
- **Pistas**: `idf.py set-target` limpia el build directory y configura el toolchain adecuado. `idf.py flash monitor` ejecuta ambos comandos secuencialmente. El flag `-p` especifica el puerto serie. Si el monitor no muestra texto legible, verificar que el baudrate es 115200 (por defecto en ESP-IDF).
- **Errores comunes**:
  - No ejecutar `set-target` antes de compilar, lo que causa errores del toolchain. Solucion: siempre ejecutar `idf.py set-target esp32s3` en un proyecto nuevo.
  - El flash falla con "Failed to connect": mantener pulsado el boton BOOT de la placa mientras se inicia el flasheo, y soltar al ver "Connecting...".
- **Tiempo estimado**: 30-60 minutos

---

### Tarea T0.1.5: Compilar y flashear hello_world en ESP32-C3
- **Dificultad**: Basico
- **Descripcion**: Repetir el proceso de T0.1.4 pero para la placa ESP32-C3-DevKitM. La diferencia principal es que el C3 usa arquitectura RISC-V en lugar de Xtensa. Pasos:
  1. Copiar el ejemplo: `cp -r ~/esp/esp-idf/examples/get-started/hello_world ~/esp/hello_world_c3`
  2. Entrar en la carpeta: `cd ~/esp/hello_world_c3`
  3. Cargar variables: `source ~/esp/esp-idf/export.sh`
  4. Configurar el target: `idf.py set-target esp32c3`
  5. Compilar: `idf.py build`
  6. Flashear y monitorizar: `idf.py -p /dev/ttyUSB0 flash monitor` (el puerto puede ser diferente al S3)
  7. Verificar que muestra "Hello world!" y que indica un solo core (el C3 es single-core).
- **Archivos a crear/modificar**: Ninguno
- **Criterio de aceptacion**:
  - La compilacion usa el toolchain RISC-V (`riscv32-esp-elf-gcc`)
  - El monitor muestra "Hello world!" con informacion del chip ESP32-C3
  - La informacion del chip indica 1 CPU core (a diferencia del S3 que tiene 2)
- **Dependencias**: Requiere T0.1.1, T0.1.3
- **Pistas**: El ESP32-C3 usa toolchain RISC-V (`riscv32-esp-elf-gcc`), no Xtensa. El puerto serie del C3 puede ser diferente al del S3 si ambos estan conectados simultaneamente. Usar `ls /dev/tty*` para identificar cada uno. Puedes desconectar uno y ver cual desaparece.
- **Errores comunes**:
  - No cambiar el target antes de compilar (si antes compilaste para S3). Solucion: `idf.py set-target esp32c3` limpia el build anterior automaticamente.
  - Confundir los puertos si ambos dispositivos estan conectados. Solucion: conectar solo un dispositivo a la vez al principio, o etiquetar los cables USB.
- **Tiempo estimado**: 20-40 minutos

---

### Tarea T0.1.6: Crear repositorio Git con estructura del proyecto
- **Dificultad**: Basico
- **Descripcion**: Inicializar un repositorio Git y crear la estructura de carpetas que usara el proyecto completo. Esto establece la organizacion del codigo desde el principio. Pasos:
  1. Crear el directorio raiz del proyecto y entrar en el: `mkdir -p ~/Dev/ESP32-fg && cd ~/Dev/ESP32-fg`
  2. Inicializar Git: `git init`
  3. Crear la estructura de carpetas:
     ```
     mkdir -p firmware/gateway/main
     mkdir -p firmware/node/main
     mkdir -p firmware/common/protocol
     mkdir -p server
     mkdir -p dashboard
     mkdir -p docs
     mkdir -p tools
     ```
  4. Crear ficheros placeholder (`README.md` vacio o con titulo) en cada carpeta para que Git las rastree.
  5. Crear un `.gitignore` con las exclusiones tipicas de ESP-IDF: `build/`, `sdkconfig.old`, `managed_components/`, `dependencies.lock`.
  6. Hacer el primer commit: `git add . && git commit -m "feat: estructura inicial del proyecto"`
- **Archivos a crear/modificar**:
  - `.gitignore`
  - `firmware/gateway/main/.gitkeep`
  - `firmware/node/main/.gitkeep`
  - `firmware/common/protocol/.gitkeep`
  - `server/.gitkeep`
  - `dashboard/.gitkeep`
  - `docs/.gitkeep`
  - `tools/.gitkeep`
- **Criterio de aceptacion**:
  - `git status` muestra un repositorio limpio tras el commit
  - La estructura de carpetas existe y es correcta: `tree -d` muestra firmware/gateway, firmware/node, firmware/common, server, dashboard, docs, tools
  - El `.gitignore` excluye correctamente los directorios de build
- **Dependencias**: Ninguna
- **Pistas**: Los ficheros `.gitkeep` son una convencion (Git no rastrea carpetas vacias). El `.gitignore` de ESP-IDF tipicamente incluye: `build/`, `sdkconfig.old`, `managed_components/`, `dependencies.lock`, `*.pyc`, `__pycache__/`.
- **Errores comunes**:
  - Olvidar el `.gitignore` y subir accidentalmente la carpeta `build/` (que puede pesar cientos de MB). Solucion: crear el `.gitignore` antes del primer commit.
  - No usar `--recursive` si luego se añaden submodulos. Solucion: documentar en el README como clonar el proyecto.
- **Tiempo estimado**: 30-45 minutos

---

> **Checkpoint 0.1**: En este punto debes poder abrir una terminal, cargar ESP-IDF con `source export.sh`, compilar un proyecto para ESP32-S3 o ESP32-C3, flashearlo y ver la salida en el monitor serie. Tambien debes tener un repositorio Git con la estructura del proyecto. Si algo falla, revisa las tareas anteriores antes de continuar.

---

## Sub-tarea 0.2: Blink y GPIO

**Objetivo**: Aprender a controlar pines GPIO de salida. Crear un proyecto propio desde cero (sin copiar el ejemplo).

---

### Tarea T0.2.1: Crear proyecto blink para ESP32-S3 desde cero
- **Dificultad**: Basico
- **Descripcion**: Crear un proyecto nuevo (no copiar el ejemplo blink) que haga parpadear el LED integrado del ESP32-S3-DevKitC. Esto enseña como se estructura un proyecto ESP-IDF minimo. Pasos:
  1. Crear el archivo `main.c` dentro de `firmware/gateway/main/`.
  2. Incluir las cabeceras necesarias: `driver/gpio.h`, `freertos/FreeRTOS.h`, `freertos/task.h`.
  3. En `app_main()`:
     - Resetear el pin GPIO: `gpio_reset_pin(GPIO_NUM_48)` (el LED del DevKitC-S3 tipicamente esta en GPIO48, verificar con el esquematico de tu placa).
     - Configurar como salida: `gpio_set_direction(GPIO_NUM_48, GPIO_MODE_OUTPUT)`.
     - En un bucle infinito, alternar el nivel del pin con `gpio_set_level()` y esperar con `vTaskDelay(pdMS_TO_TICKS(500))`.
  4. Crear `firmware/gateway/main/CMakeLists.txt` con `idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")`.
  5. Crear `firmware/gateway/CMakeLists.txt` con el boilerplate de proyecto ESP-IDF.
  6. Compilar, flashear y verificar que el LED parpadea.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c`
  - `firmware/gateway/main/CMakeLists.txt`
  - `firmware/gateway/CMakeLists.txt`
- **Criterio de aceptacion**:
  - El proyecto compila sin warnings (usar `-Wall` si es posible)
  - El LED de la placa parpadea a un ritmo constante de 500ms encendido / 500ms apagado
  - El codigo usa las funciones `gpio_reset_pin`, `gpio_set_direction`, `gpio_set_level` (no atajos como escribir directamente en registros)
- **Dependencias**: Requiere T0.1.4, T0.1.6
- **Pistas**: `gpio_reset_pin()` desconecta el pin de cualquier funcion especial y lo prepara para uso general. `pdMS_TO_TICKS()` convierte milisegundos a ticks de FreeRTOS (necesario para `vTaskDelay`). La funcion `app_main()` es el punto de entrada de ESP-IDF (equivalente a `main()` en C estandar). El `CMakeLists.txt` del proyecto raiz debe contener:
  ```cmake
  cmake_minimum_required(VERSION 3.16)
  include($ENV{IDF_PATH}/tools/cmake/project.cmake)
  project(gateway)
  ```
- **Errores comunes**:
  - Usar `delay()` en lugar de `vTaskDelay()`. ESP-IDF usa FreeRTOS, y `delay()` no existe. Siempre usar `vTaskDelay(pdMS_TO_TICKS(ms))`.
  - Numero de GPIO incorrecto para el LED. El GPIO del LED varia segun la version de la placa. Consultar el esquematico o probar con GPIO48 (comun en ESP32-S3-DevKitC-1).
- **Tiempo estimado**: 1-2 horas

---

### Tarea T0.2.2: Configurar intervalo de blink via menuconfig (Kconfig)
- **Dificultad**: Intermedio
- **Descripcion**: Crear una opcion de configuracion personalizada en Kconfig para que el intervalo de parpadeo sea configurable sin modificar el codigo fuente. Pasos:
  1. Crear el archivo `firmware/gateway/main/Kconfig.projbuild`.
  2. Definir un menu con una opcion de tipo `int`:
     ```
     menu "Configuracion Blink"
         config BLINK_PERIOD_MS
             int "Periodo de parpadeo en milisegundos"
             default 500
             range 100 5000
             help
                 Tiempo en milisegundos entre cambios de estado del LED.
     endmenu
     ```
  3. En `main.c`, reemplazar el valor hardcoded `500` por `CONFIG_BLINK_PERIOD_MS`.
  4. Ejecutar `idf.py menuconfig`, navegar al menu "Configuracion Blink" y cambiar el valor a 1000.
  5. Recompilar y flashear. Verificar que el LED ahora parpadea mas lento.
  6. Cambiar de nuevo a 200ms desde menuconfig y verificar que parpadea mas rapido.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/Kconfig.projbuild` (crear)
  - `firmware/gateway/main/main.c` (modificar para usar `CONFIG_BLINK_PERIOD_MS`)
- **Criterio de aceptacion**:
  - `idf.py menuconfig` muestra el menu "Configuracion Blink" con la opcion de periodo
  - Cambiar el valor en menuconfig y recompilar cambia efectivamente la velocidad de parpadeo
  - El rango esta validado: no se puede poner un valor menor a 100 ni mayor a 5000
  - El codigo fuente `main.c` no contiene ningun valor de intervalo hardcoded
- **Dependencias**: Requiere T0.2.1
- **Pistas**: Los archivos `Kconfig.projbuild` en el directorio `main/` se incluyen automaticamente en el menu de configuracion. Las variables definidas en Kconfig se acceden en C como `CONFIG_<NOMBRE>`. El archivo `sdkconfig` almacena la configuracion actual (no editarlo manualmente, usar siempre `menuconfig`). La directiva `range` en Kconfig valida los limites.
- **Errores comunes**:
  - Editar `sdkconfig` directamente en lugar de usar `menuconfig`. Los cambios manuales en `sdkconfig` pueden perderse o causar inconsistencias.
  - Olvidar recompilar despues de cambiar la configuracion. `idf.py build` detecta cambios en `sdkconfig` automaticamente, pero hay que ejecutarlo.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T0.2.3: Crear proyecto blink para ESP32-C3
- **Dificultad**: Basico
- **Descripcion**: Repetir el ejercicio de blink pero para el ESP32-C3-DevKitM, prestando atencion a las diferencias de hardware. Pasos:
  1. Crear la misma estructura de proyecto en `firmware/node/`.
  2. Copiar el codigo de `firmware/gateway/main/main.c` como punto de partida.
  3. Cambiar el numero de GPIO del LED. En ESP32-C3-DevKitM, el LED RGB addressable suele estar en GPIO8. Si tu placa no tiene LED integrado, conectar un LED externo con resistencia de 330 ohm a cualquier GPIO disponible.
  4. Configurar el target: `idf.py set-target esp32c3`.
  5. Compilar y flashear en el C3.
  6. Verificar que el LED parpadea.
- **Archivos a crear/modificar**:
  - `firmware/node/main/main.c`
  - `firmware/node/main/CMakeLists.txt`
  - `firmware/node/CMakeLists.txt`
- **Criterio de aceptacion**:
  - El proyecto compila correctamente para target `esp32c3`
  - El LED de la placa ESP32-C3 parpadea al intervalo configurado
  - El Kconfig del intervalo tambien funciona en este proyecto
- **Dependencias**: Requiere T0.1.5, T0.2.1
- **Pistas**: El ESP32-C3-DevKitM usa un LED RGB tipo WS2812 en GPIO8. Para controlarlo como GPIO simple (sin colores), `gpio_set_level(GPIO_NUM_8, 1)` enciende el LED en blanco. Si quieres control de colores, necesitaras el driver RMT (que se vera mas adelante). Algunos kits C3 tienen un LED simple en GPIO8 en lugar del WS2812.
- **Errores comunes**:
  - Intentar flashear firmware compilado para S3 en un C3 (o viceversa). Siempre verificar el target con `idf.py set-target`.
  - El LED RGB WS2812 no responde a `gpio_set_level` de la misma forma que un LED simple. Si no funciona, probar con un LED externo conectado a otro GPIO.
- **Tiempo estimado**: 30-60 minutos

---

> **Checkpoint 0.2**: Ahora debes tener dos proyectos funcionales (gateway y node), ambos haciendo parpadear un LED. El intervalo es configurable via menuconfig. Si necesitas mas practica con GPIO, intenta añadir un segundo LED o un boton como entrada (usando `gpio_set_direction` con `GPIO_MODE_INPUT` y `gpio_get_level`).

---

## Sub-tarea 0.3: UART y Logging

**Objetivo**: Dominar el sistema de logging de ESP-IDF para depuracion. El logging es la herramienta principal de diagnostico durante todo el proyecto.

---

### Tarea T0.3.1: Practicar sistema de logging con diferentes niveles y TAGs
- **Dificultad**: Basico
- **Descripcion**: Aprender a usar las macros de logging de ESP-IDF para imprimir mensajes categorizados por nivel de severidad y modulo. Pasos:
  1. En el proyecto gateway (`firmware/gateway/main/main.c`), añadir al inicio del archivo:
     ```c
     static const char *TAG = "gateway_main";
     ```
  2. En `app_main()`, antes del bucle del blink, añadir mensajes de cada nivel:
     ```c
     ESP_LOGE(TAG, "Esto es un ERROR - algo ha fallado");
     ESP_LOGW(TAG, "Esto es un WARNING - situacion anormal");
     ESP_LOGI(TAG, "Esto es INFO - flujo normal del programa");
     ESP_LOGD(TAG, "Esto es DEBUG - detalle para depuracion");
     ESP_LOGV(TAG, "Esto es VERBOSE - maximo detalle");
     ```
  3. Dentro del bucle, añadir un log de info con el estado del LED:
     ```c
     ESP_LOGI(TAG, "LED estado: %s", led_on ? "ON" : "OFF");
     ```
  4. Incluir la cabecera `esp_log.h`.
  5. Compilar, flashear y observar los mensajes coloreados en el monitor serie.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar)
- **Criterio de aceptacion**:
  - El monitor serie muestra mensajes con colores diferentes para cada nivel (rojo para ERROR, amarillo para WARNING, verde para INFO)
  - Cada mensaje incluye el TAG entre corchetes, el nivel y el timestamp
  - Los mensajes DEBUG y VERBOSE pueden no aparecer (depende de la configuracion por defecto, que suele ser INFO)
- **Dependencias**: Requiere T0.2.1
- **Pistas**: La cabecera es `#include "esp_log.h"`. Las macros son: `ESP_LOGE` (Error, rojo), `ESP_LOGW` (Warning, amarillo), `ESP_LOGI` (Info, verde), `ESP_LOGD` (Debug, sin color), `ESP_LOGV` (Verbose, sin color). El TAG es un string que identifica el modulo o componente que genera el mensaje. Convencion: usar el nombre del archivo o modulo.
- **Errores comunes**:
  - No incluir `esp_log.h`. El compilador dara error de "implicit declaration of function".
  - Usar `printf` en lugar de las macros ESP_LOGx. El `printf` funciona pero no incluye timestamp, TAG ni nivel, y no se puede filtrar. Siempre preferir ESP_LOGx.
- **Tiempo estimado**: 30-60 minutos

---

### Tarea T0.3.2: Controlar niveles de log en compilacion y en runtime
- **Dificultad**: Basico
- **Descripcion**: Aprender a filtrar los mensajes de log por nivel y por TAG, tanto en la configuracion de compilacion (menuconfig) como en tiempo de ejecucion. Pasos:
  1. **Cambiar nivel por defecto en compilacion**:
     - Ejecutar `idf.py menuconfig`.
     - Navegar a `Component config > Log output > Default log verbosity`.
     - Cambiar de `Info` a `Debug`. Recompilar y flashear.
     - Ahora los mensajes `ESP_LOGD` deben ser visibles.
  2. **Cambiar nivel en runtime para un TAG especifico**:
     - En `app_main()`, al inicio, añadir:
       ```c
       esp_log_level_set("gateway_main", ESP_LOG_VERBOSE);
       esp_log_level_set("wifi", ESP_LOG_WARN);
       ```
     - Esto hace que el TAG "gateway_main" muestre todos los mensajes (incluido VERBOSE), pero el TAG "wifi" solo muestre WARNING y ERROR.
  3. Verificar:
     - Los mensajes VERBOSE de "gateway_main" aparecen.
     - Los mensajes INFO de componentes WiFi (si los hay) no aparecen.
  4. Experimentar con `esp_log_level_set("*", ESP_LOG_ERROR)` para silenciar todo excepto errores.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar)
- **Criterio de aceptacion**:
  - Al cambiar el nivel en menuconfig a DEBUG, los mensajes `ESP_LOGD` se muestran
  - `esp_log_level_set()` en runtime cambia efectivamente los mensajes visibles para un TAG especifico
  - Se puede demostrar que un TAG muestra VERBOSE mientras otro solo muestra ERROR
  - El wildcard `"*"` afecta a todos los TAGs
- **Dependencias**: Requiere T0.3.1
- **Pistas**: `esp_log_level_set(TAG, nivel)` cambia el nivel en runtime. Los niveles son: `ESP_LOG_NONE`, `ESP_LOG_ERROR`, `ESP_LOG_WARN`, `ESP_LOG_INFO`, `ESP_LOG_DEBUG`, `ESP_LOG_VERBOSE`. El nivel de compilacion es el maximo posible: si compilas con nivel INFO, `esp_log_level_set` no puede activar DEBUG en runtime (el codigo para DEBUG ni siquiera esta compilado). Para maxima flexibilidad, compilar con `VERBOSE` y filtrar en runtime.
- **Errores comunes**:
  - Compilar con nivel INFO e intentar activar DEBUG en runtime. No funciona porque el codigo del `ESP_LOGD` fue eliminado por el preprocesador en compilacion.
  - Confundir el orden de los niveles. De menos a mas detalle: NONE < ERROR < WARN < INFO < DEBUG < VERBOSE.
- **Tiempo estimado**: 30-60 minutos

---

> **Checkpoint 0.3**: Ahora puedes usar el sistema de logging para depurar tu codigo de forma efectiva. Debes poder filtrar mensajes por nivel y por modulo. A partir de aqui, usa `ESP_LOGI`/`ESP_LOGW`/`ESP_LOGE` en todo tu codigo en lugar de `printf`.

---

## Sub-tarea 0.4: Lectura de sensor I2C/ADC

**Objetivo**: Leer datos de sensores reales (temperatura y valor analogico) usando los perifericos I2C y ADC del ESP32.

---

### Tarea T0.4.1: Conectar sensor de temperatura al ESP32-S3
- **Dificultad**: Basico
- **Descripcion**: Realizar la conexion fisica del sensor de temperatura a la placa ESP32-S3. Dependiendo del sensor que tengas, seguir el esquema correspondiente. Pasos:
  1. **Si usas DS18B20 (1-Wire)**:
     - Conectar VDD del sensor a 3.3V del ESP32.
     - Conectar GND del sensor a GND del ESP32.
     - Conectar DQ (datos) del sensor a un GPIO libre (por ejemplo GPIO4).
     - Colocar una resistencia pull-up de 4.7K ohm entre DQ y VDD (3.3V).
  2. **Si usas BMP280 (I2C)**:
     - Conectar VCC a 3.3V, GND a GND.
     - Conectar SDA a GPIO1 (o cualquier GPIO que soporte I2C), SCL a GPIO2.
     - Colocar resistencias pull-up de 4.7K ohm en SDA y SCL (a 3.3V). Muchos modulos breakout ya las incluyen.
  3. Verificar con multimetro que:
     - El voltaje entre VDD y GND es ~3.3V.
     - Las lineas de datos tienen pull-up (voltaje en alto ~3.3V cuando estan en reposo).
  4. Documentar el esquema de conexion (foto o diagrama) en `docs/conexiones-sensor.md`.
- **Archivos a crear/modificar**:
  - `docs/conexiones-sensor.md` (crear con esquema de conexion)
- **Criterio de aceptacion**:
  - El sensor esta conectado fisicamente al ESP32-S3 segun el esquema
  - El multimetro confirma voltaje correcto en VDD (3.3V +/- 0.2V)
  - Las lineas de datos tienen pull-up verificado (voltaje alto en reposo)
  - Existe documentacion del esquema de conexion
- **Dependencias**: Ninguna (solo hardware)
- **Pistas**: Los sensores de temperatura suelen venir en modulos breakout con los pull-ups incluidos. Verificar la documentacion del modulo antes de añadir resistencias externas (pull-up duplicado puede causar problemas). El DS18B20 solo necesita un pin de datos. El BMP280 necesita dos (SDA, SCL). Algunos modulos BMP280 soportan tanto I2C como SPI; asegurarse de que esta configurado en modo I2C (ver jumpers del modulo).
- **Errores comunes**:
  - Olvidar la resistencia pull-up de 4.7K en 1-Wire o I2C. Sin pull-up, la comunicacion falla silenciosamente (no hay error, simplemente no lee datos).
  - Conectar el sensor a 5V en lugar de 3.3V. Los pines del ESP32 son de 3.3V y no toleran 5V. Esto puede dañar el chip permanentemente.
- **Tiempo estimado**: 30-60 minutos

---

### Tarea T0.4.2: Leer temperatura con el driver apropiado
- **Dificultad**: Intermedio
- **Descripcion**: Escribir codigo para leer la temperatura del sensor conectado en T0.4.1 usando el driver correspondiente de ESP-IDF. Pasos:
  1. **Si usas DS18B20 (1-Wire con driver RMT)**:
     - Añadir la dependencia del componente 1-Wire en `idf_component.yml`:
       ```yaml
       dependencies:
         espressif/ds18b20: "^0.1.0"
         espressif/onewire_bus: "^1.0.0"
       ```
     - Inicializar el bus 1-Wire con `onewire_bus_rmt`:
       ```c
       onewire_bus_config_t bus_config = { .bus_gpio_num = GPIO_NUM_4 };
       onewire_bus_rmt_config_t rmt_config = { .max_rx_bytes = 10 };
       onewire_bus_handle_t bus;
       onewire_new_bus_rmt(&bus_config, &rmt_config, &bus);
       ```
     - Buscar dispositivos en el bus y leer temperatura.
  2. **Si usas BMP280 (I2C)**:
     - Configurar el bus I2C master:
       ```c
       i2c_master_bus_config_t i2c_config = {
           .clk_source = I2C_CLK_SRC_DEFAULT,
           .i2c_port = I2C_NUM_0,
           .sda_io_num = GPIO_NUM_1,
           .scl_io_num = GPIO_NUM_2,
           .glitch_ignore_cnt = 7,
       };
       i2c_master_bus_handle_t bus_handle;
       i2c_new_master_bus(&i2c_config, &bus_handle);
       ```
     - Añadir el dispositivo al bus con `i2c_master_bus_add_device`.
     - Leer los registros del BMP280 para obtener temperatura.
  3. Imprimir la temperatura leida con `ESP_LOGI`.
  4. Repetir la lectura en un bucle cada 2 segundos.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar)
  - `firmware/gateway/main/idf_component.yml` (crear si se usa componente del registry)
- **Criterio de aceptacion**:
  - La temperatura se lee correctamente (valor coherente con la temperatura ambiente, +-2 grados C)
  - Las lecturas se repiten periodicamente cada 2 segundos
  - No hay errores en el log durante la comunicacion con el sensor
  - Si se calienta el sensor (tocandolo con el dedo), el valor sube visiblemente
- **Dependencias**: Requiere T0.4.1, T0.2.1
- **Pistas**: El ESP-IDF Component Registry (https://components.espressif.com/) tiene drivers listos para DS18B20 y muchos otros sensores. Buscar "ds18b20" en el registry. Para BMP280, puede que necesites escribir el driver a mano o buscar un componente de terceros. La direccion I2C del BMP280 es tipicamente 0x76 o 0x77 (depende del estado del pin SDO). La nueva API I2C de ESP-IDF v5.x usa `i2c_master_bus_handle_t` (no la API legacy `i2c_driver_install`).
- **Errores comunes**:
  - Usar la API I2C legacy (`i2c_driver_install`, `i2c_cmd_link_create`) en lugar de la nueva API master. La API legacy esta deprecada en ESP-IDF v5.x.
  - Direccion I2C incorrecta del sensor. Solucion: hacer un escaneo I2C (bucle de 0x01 a 0x7F intentando `i2c_master_probe`) para descubrir la direccion real.
- **Tiempo estimado**: 2-4 horas

---

### Tarea T0.4.3: Leer valor analogico con ADC (potenciometro)
- **Dificultad**: Intermedio
- **Descripcion**: Conectar un potenciometro a un pin ADC del ESP32-S3 y leer el valor analogico. Esto simula la lectura de sensores analogicos como sondas de pH o turbidez. Pasos:
  1. Conectar el potenciometro:
     - Terminal izquierdo a GND.
     - Terminal derecho a 3.3V.
     - Terminal central (wiper) a GPIO3 (u otro pin ADC1, ver documentacion del ESP32-S3).
  2. Configurar el ADC en el codigo:
     ```c
     adc_oneshot_unit_handle_t adc_handle;
     adc_oneshot_unit_init_cfg_t init_config = {
         .unit_id = ADC_UNIT_1,
     };
     adc_oneshot_new_unit(&init_config, &adc_handle);

     adc_oneshot_chan_cfg_t channel_config = {
         .atten = ADC_ATTEN_DB_12,
         .bitwidth = ADC_BITWIDTH_DEFAULT,
     };
     adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_2, &channel_config);
     ```
  3. Leer el valor en un bucle:
     ```c
     int raw_value;
     adc_oneshot_read(adc_handle, ADC_CHANNEL_2, &raw_value);
     ESP_LOGI(TAG, "ADC raw: %d", raw_value);
     ```
  4. Calibrar el ADC para obtener voltaje real:
     ```c
     adc_cali_handle_t cali_handle;
     adc_cali_curve_fitting_config_t cali_config = {
         .unit_id = ADC_UNIT_1,
         .atten = ADC_ATTEN_DB_12,
         .bitwidth = ADC_BITWIDTH_DEFAULT,
     };
     adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);

     int voltage_mv;
     adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage_mv);
     ESP_LOGI(TAG, "Voltaje: %d mV", voltage_mv);
     ```
  5. Girar el potenciometro y verificar que el valor cambia de ~0 a ~3300 mV.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar)
- **Criterio de aceptacion**:
  - El valor raw del ADC cambia al girar el potenciometro (rango ~0 a ~4095 para 12 bits)
  - El valor calibrado en mV va de ~0 a ~3300 mV
  - Los extremos del potenciometro corresponden a valores cercanos a 0 mV y 3300 mV
  - Las lecturas son estables (variacion menor a +-50 mV cuando el potenciometro esta fijo)
- **Dependencias**: Requiere T0.2.1
- **Pistas**: Los canales ADC estan mapeados a GPIOs especificos. En ESP32-S3: ADC1_CHANNEL_0 = GPIO1, ADC1_CHANNEL_1 = GPIO2, ADC1_CHANNEL_2 = GPIO3, etc. Consultar la tabla de mapeo en la documentacion. `ADC_ATTEN_DB_12` permite leer hasta ~3.3V (atenuacion de 12dB). Sin atenuacion, el rango es solo ~0-1V. La cabecera para la calibracion es `esp_adc/adc_cali_scheme.h` (para curve fitting) o `esp_adc/adc_cali.h` (API general).
- **Errores comunes**:
  - No configurar la atenuacion del ADC. Sin atenuacion (`ADC_ATTEN_DB_0`), el rango es solo 0-1V, y voltajes mayores saturan la lectura. Siempre usar `ADC_ATTEN_DB_12` para señales de 0-3.3V.
  - Usar un pin ADC2. ADC2 esta compartido con WiFi y no se puede usar cuando WiFi esta activo. Siempre usar ADC1 para lecturas fiables.
- **Tiempo estimado**: 1-3 horas

---

### Tarea T0.4.4: Imprimir lecturas formateadas con timestamp
- **Dificultad**: Basico
- **Descripcion**: Formatear la salida de las lecturas del sensor de temperatura y del ADC de forma clara y consistente, incluyendo el timestamp desde el arranque. Pasos:
  1. Crear una funcion auxiliar que obtenga el timestamp en milisegundos:
     ```c
     static uint32_t get_timestamp_ms(void) {
         return (uint32_t)(esp_timer_get_time() / 1000);
     }
     ```
  2. Formatear las lecturas asi:
     ```
     [12345ms] Temperatura: 23.5 °C
     [12345ms] ADC: 2048 raw | 1650 mV
     ```
  3. Asegurarse de que el formato sea consistente en todas las lecturas.
  4. Imprimir una lectura de cada sensor cada 2 segundos.
  5. Usar `ESP_LOGI` con formato string para las impresiones.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar)
- **Criterio de aceptacion**:
  - Cada linea de lectura incluye timestamp en milisegundos entre corchetes
  - La temperatura muestra un decimal (e.g., `23.5 °C`)
  - El ADC muestra tanto el valor raw como el voltaje calibrado
  - El formato es consistente y legible en el monitor serie
  - El timestamp incrementa correctamente entre lecturas (~2000ms de diferencia)
- **Dependencias**: Requiere T0.4.2, T0.4.3
- **Pistas**: `esp_timer_get_time()` devuelve microsegundos desde el arranque como `int64_t`. Dividir por 1000 para obtener milisegundos. Para formatear floats, usar `%.1f` en el string de formato. En ESP-IDF, `printf` y `ESP_LOGI` soportan floats por defecto (a diferencia de algunos toolchains embebidos que los desactivan por tamaño). El caracter `°` se imprime correctamente si el terminal soporta UTF-8.
- **Errores comunes**:
  - Overflow de `uint32_t` al dividir microsegundos. `esp_timer_get_time()` devuelve `int64_t` (microsegundos). Convertir a `uint32_t` milisegundos permite ~49 dias antes del overflow, lo cual es suficiente para pruebas.
  - Intentar imprimir floats y obtener "nan" o "0.000000". Esto ocurre si el valor de temperatura es un entero y no se castea a float correctamente. Asegurarse de que la variable de temperatura es `float` o hacer cast explicito.
- **Tiempo estimado**: 30-60 minutos

---

> **Checkpoint 0.4**: Ahora puedes leer datos de sensores reales (temperatura digital y valor analogico) y mostrarlos de forma formateada. El monitor serie debe mostrar lecturas continuas con timestamps. Verifica que la temperatura cambia al calentar el sensor y que el voltaje cambia al girar el potenciometro.

---

## Sub-tarea 0.5: ESP-NOW basico

**Objetivo**: Establecer comunicacion inalambrica entre dos ESP32 usando el protocolo ESP-NOW. Este sera el protocolo principal de comunicacion nodo-gateway en el proyecto.

---

### Tarea T0.5.1: Crear proyecto sender en ESP32-C3
- **Dificultad**: Intermedio
- **Descripcion**: Crear un proyecto que envie datos simulados via ESP-NOW desde el ESP32-C3 (nodo). ESP-NOW es un protocolo ligero de Espressif que permite comunicacion directa entre dispositivos sin necesidad de router WiFi. Pasos:
  1. En `firmware/node/main/main.c`, crear una estructura para los datos a enviar:
     ```c
     typedef struct {
         uint16_t sensor_id;
         float temperatura;
         uint16_t adc_value;
         uint32_t timestamp_ms;
     } sensor_data_t;
     ```
  2. Inicializar NVS (requerido por WiFi):
     ```c
     esp_err_t ret = nvs_flash_init();
     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
         nvs_flash_erase();
         nvs_flash_init();
     }
     ```
  3. Inicializar WiFi en modo STA (necesario para ESP-NOW, aunque no nos conectemos a ningun AP):
     ```c
     esp_netif_init();
     esp_event_loop_create_default();
     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     esp_wifi_init(&cfg);
     esp_wifi_set_mode(WIFI_MODE_STA);
     esp_wifi_start();
     ```
  4. Inicializar ESP-NOW y registrar callback de envio:
     ```c
     esp_now_init();
     esp_now_register_send_cb(send_cb);
     ```
  5. Añadir el peer (gateway) con su MAC address (por ahora usar broadcast `FF:FF:FF:FF:FF:FF`):
     ```c
     esp_now_peer_info_t peer = {
         .channel = 0,
         .ifidx = WIFI_IF_STA,
         .encrypt = false,
     };
     memset(peer.peer_addr, 0xFF, ESP_NOW_ETH_ALEN); // broadcast
     esp_now_add_peer(&peer);
     ```
  6. En un bucle, llenar la estructura con datos simulados y enviar cada 2 segundos:
     ```c
     sensor_data_t data = { .sensor_id = 1, .temperatura = 23.5, ... };
     esp_now_send(peer.peer_addr, (uint8_t *)&data, sizeof(data));
     ```
- **Archivos a crear/modificar**:
  - `firmware/node/main/main.c` (modificar)
- **Criterio de aceptacion**:
  - El proyecto compila y flashea correctamente en ESP32-C3
  - El callback de envio se ejecuta cada 2 segundos indicando que el paquete fue enviado
  - El log muestra los datos que se estan enviando (sensor_id, temperatura, etc.)
  - No hay errores de inicializacion de WiFi o ESP-NOW en el log
- **Dependencias**: Requiere T0.2.3
- **Pistas**: ESP-NOW requiere que WiFi este inicializado y arrancado, pero no necesita estar conectado a un AP. `esp_now_send` es asincrona: retorna inmediatamente y el resultado se notifica via callback. Las cabeceras necesarias son: `esp_now.h`, `esp_wifi.h`, `nvs_flash.h`, `esp_netif.h`, `esp_event.h`. El tamaño maximo de un paquete ESP-NOW es 250 bytes.
- **Errores comunes**:
  - No inicializar WiFi antes de ESP-NOW. `esp_now_init()` devolvera `ESP_ERR_ESPNOW_NOT_INIT` si WiFi no esta inicializado. Siempre llamar a `esp_wifi_init()`, `esp_wifi_set_mode()` y `esp_wifi_start()` antes.
  - No inicializar NVS. WiFi requiere NVS para almacenar calibracion del PHY. Sin NVS, `esp_wifi_init()` falla.
- **Tiempo estimado**: 2-3 horas

---

### Tarea T0.5.2: Crear proyecto receiver en ESP32-S3
- **Dificultad**: Intermedio
- **Descripcion**: Crear el receptor ESP-NOW en el ESP32-S3 (gateway) que reciba los datos enviados por el nodo C3. Pasos:
  1. En `firmware/gateway/main/main.c`, añadir la misma definicion de `sensor_data_t` que en el sender.
  2. Inicializar NVS, WiFi (modo STA) y ESP-NOW igual que en el sender.
  3. Registrar el callback de recepcion:
     ```c
     static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
         if (len == sizeof(sensor_data_t)) {
             sensor_data_t *sensor = (sensor_data_t *)data;
             ESP_LOGI(TAG, "Recibido de " MACSTR ": sensor=%d, temp=%.1f, adc=%d",
                      MAC2STR(info->src_addr),
                      sensor->sensor_id,
                      sensor->temperatura,
                      sensor->adc_value);
         }
     }

     esp_now_register_recv_cb(recv_cb);
     ```
  4. En este caso NO es necesario añadir un peer ni enviar datos (solo recibimos).
  5. Flashear ambos dispositivos (C3 como sender, S3 como receiver) y verificar comunicacion.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar)
- **Criterio de aceptacion**:
  - El ESP32-S3 recibe los datos enviados por el ESP32-C3
  - El log del receiver muestra la MAC del sender y los datos correctos (sensor_id, temperatura, adc_value)
  - Los datos recibidos coinciden con los datos enviados
  - La recepcion ocurre cada ~2 segundos (coincidiendo con la frecuencia de envio)
- **Dependencias**: Requiere T0.5.1, T0.2.1
- **Pistas**: La macro `MACSTR` formatea una MAC address como "XX:XX:XX:XX:XX:XX" y `MAC2STR()` descompone un array de 6 bytes en los argumentos del formato. El callback de recepcion se ejecuta en el contexto de la tarea WiFi, asi que debe ser rapido (no bloquear). En ESP-IDF v5.x, el callback de recepcion usa `esp_now_recv_info_t` que incluye la MAC de origen y destino ademas de RSSI.
- **Errores comunes**:
  - Definir `sensor_data_t` de forma diferente en sender y receiver (distinto orden de campos, padding diferente). Solucion: usar `__attribute__((packed))` en la struct o mejor aun, compartir la definicion en un header comun.
  - El receiver no recibe nada: verificar que ambos dispositivos estan en el mismo canal WiFi. Por defecto, canal 1.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T0.5.3: Añadir callbacks de estado al sender
- **Dificultad**: Basico
- **Descripcion**: Mejorar el sender para verificar si cada envio fue exitoso o fallo, usando el callback de estado de ESP-NOW. Pasos:
  1. En el sender (`firmware/node/main/main.c`), implementar el callback de envio con manejo de estado:
     ```c
     static void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
         if (status == ESP_NOW_SEND_SUCCESS) {
             ESP_LOGI(TAG, "Envio exitoso a " MACSTR, MAC2STR(mac_addr));
         } else {
             ESP_LOGW(TAG, "Envio FALLIDO a " MACSTR, MAC2STR(mac_addr));
         }
     }
     ```
  2. Llevar un contador de envios exitosos y fallidos:
     ```c
     static uint32_t send_ok = 0;
     static uint32_t send_fail = 0;
     ```
  3. Imprimir estadisticas cada 10 envios:
     ```c
     if ((send_ok + send_fail) % 10 == 0) {
         ESP_LOGI(TAG, "Estadisticas: OK=%lu, FAIL=%lu, ratio=%.1f%%",
                  send_ok, send_fail,
                  (float)send_ok / (send_ok + send_fail) * 100);
     }
     ```
  4. Probar apagando el receiver y verificar que los envios ahora reportan FAIL (en broadcast siempre reporta SUCCESS, asi que para esta prueba necesitaras usar la MAC unicast del receiver en lugar de broadcast).
- **Archivos a crear/modificar**:
  - `firmware/node/main/main.c` (modificar)
- **Criterio de aceptacion**:
  - El callback reporta correctamente SUCCESS o FAIL para cada envio
  - El contador de estadisticas se incrementa correctamente
  - Al usar unicast (MAC especifica del receiver) y apagar el receiver, el sender reporta envios fallidos
  - Con broadcast, los envios siempre se reportan como SUCCESS (esto es comportamiento esperado de broadcast)
- **Dependencias**: Requiere T0.5.1, T0.5.2
- **Pistas**: `esp_now_send_status_t` tiene dos valores: `ESP_NOW_SEND_SUCCESS` y `ESP_NOW_SEND_FAIL`. En modo broadcast, el envio siempre se reporta como exitoso porque no hay confirmacion (ACK). En modo unicast (MAC especifica), ESP-NOW espera ACK del receptor. Para obtener la MAC real del receiver, imprimirla con `esp_read_mac()` en el codigo del receiver. Ten cuidado con acceder a variables compartidas entre callbacks y el bucle principal (concurrencia).
- **Errores comunes**:
  - Asumir que `SEND_SUCCESS` en broadcast significa que el receptor recibio el dato. En broadcast, no hay confirmacion. Solo unicast tiene ACK real.
  - Acceder a contadores compartidos desde el callback (contexto WiFi) y desde el bucle principal sin proteccion. Para un contador simple, `_Atomic` o `volatile` puede ser suficiente, pero para datos mas complejos usar un mutex o una cola FreeRTOS.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T0.5.4: Prueba de alcance y medicion de RSSI
- **Dificultad**: Basico
- **Descripcion**: Realizar pruebas de alcance de ESP-NOW moviendo los dispositivos a diferentes distancias y midiendo la intensidad de señal (RSSI). Pasos:
  1. Modificar el callback de recepcion en el receiver para imprimir el RSSI:
     ```c
     static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
         ESP_LOGI(TAG, "RSSI: %d dBm, de " MACSTR,
                  info->rx_ctrl->rssi,
                  MAC2STR(info->src_addr));
     }
     ```
  2. Colocar ambos dispositivos juntos (1 metro) y anotar el RSSI.
  3. Mover el sender a 5 metros y anotar.
  4. Mover a 10 metros y anotar.
  5. Mover a 20 metros (o maximo alcance posible) y anotar.
  6. Probar con obstaculos (pared, puerta) entre los dispositivos.
  7. Registrar los resultados en una tabla:
     | Distancia | Obstaculos | RSSI (dBm) | Paquetes perdidos |
     |---|---|---|---|
     | 1m | Ninguno | -25 | 0% |
     | 5m | Ninguno | -45 | 0% |
     | ... | ... | ... | ... |
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar callback de recepcion)
  - `docs/benchmark-alcance.md` (crear con tabla de resultados)
- **Criterio de aceptacion**:
  - El RSSI se imprime correctamente para cada paquete recibido
  - La tabla de resultados tiene al menos 4 distancias diferentes
  - Se observa que el RSSI disminuye (se hace mas negativo) con la distancia
  - Se documenta el maximo alcance con recepcion fiable (tasa de perdida < 5%)
- **Dependencias**: Requiere T0.5.2
- **Pistas**: RSSI (Received Signal Strength Indicator) se mide en dBm. Valores tipicos: -20 a -30 dBm (muy cerca), -50 a -60 dBm (mismo cuarto), -70 a -80 dBm (habitaciones adyacentes), -90 dBm (limite de recepcion). El campo `rx_ctrl` del `esp_now_recv_info_t` contiene la estructura `wifi_pkt_rx_ctrl_t` con el RSSI del paquete recibido. ESP-NOW puede alcanzar hasta ~200 metros en linea de vista sin obstaculos, pero en interior el alcance se reduce significativamente.
- **Errores comunes**:
  - Confundir señal WiFi con señal ESP-NOW. Ambos comparten la misma radio 2.4GHz, asi que el RSSI es comparable, pero ESP-NOW no depende de un AP.
  - No esperar suficientes paquetes para tener una medicion fiable. Anotar el promedio de al menos 20 paquetes por distancia.
- **Tiempo estimado**: 1-2 horas

---

> **Checkpoint 0.5**: Ya tienes comunicacion inalambrica funcionando entre el nodo C3 (sender) y el gateway S3 (receiver). Debes poder ver los datos recibidos en el monitor del S3, con estadisticas de envio en el C3. Ahora es un buen momento para refactorizar: mover `sensor_data_t` a un header compartido en `firmware/common/protocol/`.

---

## Sub-tarea 0.6: WiFi AP+STA simultaneo

**Objetivo**: Configurar el ESP32-S3 (gateway) para funcionar simultaneamente como Access Point y como estacion WiFi, que es el modo que usara en produccion.

---

### Tarea T0.6.1: Configurar ESP32-S3 en modo APSTA
- **Dificultad**: Intermedio
- **Descripcion**: Configurar el ESP32-S3 para que funcione simultaneamente como Access Point (AP) y como estacion (STA). El modo AP permitira que dispositivos se conecten directamente al gateway (por ejemplo, un movil para configuracion), mientras que el modo STA permite al gateway conectarse a un router para tener acceso a internet. Pasos:
  1. Crear un nuevo archivo o modificar `firmware/gateway/main/main.c`.
  2. Inicializar NVS y la interfaz de red:
     ```c
     nvs_flash_init();
     esp_netif_init();
     esp_event_loop_create_default();
     esp_netif_create_default_wifi_ap();
     esp_netif_create_default_wifi_sta();
     ```
  3. Inicializar WiFi y configurar modo APSTA:
     ```c
     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     esp_wifi_init(&cfg);
     esp_wifi_set_mode(WIFI_MODE_APSTA);
     ```
  4. Configurar el AP:
     ```c
     wifi_config_t ap_config = {
         .ap = {
             .ssid = "Piscifactoria-GW",
             .ssid_len = strlen("Piscifactoria-GW"),
             .password = "piscifactoria123",
             .channel = 1,
             .max_connection = 4,
             .authmode = WIFI_AUTH_WPA2_PSK,
         },
     };
     esp_wifi_set_config(WIFI_IF_AP, &ap_config);
     ```
  5. Configurar el STA (conectarse a tu router):
     ```c
     wifi_config_t sta_config = {
         .sta = {
             .ssid = "TU_SSID_ROUTER",
             .password = "TU_PASSWORD_ROUTER",
         },
     };
     esp_wifi_set_config(WIFI_IF_STA, &sta_config);
     ```
  6. Registrar event handlers para IP_EVENT y WIFI_EVENT para monitorizar el estado de ambas interfaces.
  7. Arrancar WiFi: `esp_wifi_start()`.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar)
- **Criterio de aceptacion**:
  - El ESP32-S3 crea una red WiFi llamada "Piscifactoria-GW" visible desde un movil
  - El ESP32-S3 se conecta simultaneamente al router (obtiene IP por DHCP)
  - El log muestra ambos eventos: "AP started" y "STA got IP: x.x.x.x"
  - No hay errores de inicializacion en el log
- **Dependencias**: Requiere T0.1.4
- **Pistas**: `WIFI_MODE_APSTA` activa ambas interfaces simultaneamente. Cada interfaz necesita su propio `esp_netif`: `esp_netif_create_default_wifi_ap()` y `esp_netif_create_default_wifi_sta()`. Los event handlers se registran con `esp_event_handler_instance_register()`. Eventos importantes: `WIFI_EVENT_AP_START`, `WIFI_EVENT_STA_CONNECTED`, `IP_EVENT_STA_GOT_IP`, `WIFI_EVENT_AP_STACONNECTED` (un cliente se conecta al AP).
- **Errores comunes**:
  - No registrar event handlers para ambas interfaces. Sin el handler de STA, no se inicia el proceso de obtencion de IP (DHCP). Registrar handlers para `WIFI_EVENT` e `IP_EVENT`.
  - Poner una contraseña de AP menor a 8 caracteres con WPA2. WPA2 requiere minimo 8 caracteres. Solucion: usar contraseña de 8+ caracteres o `WIFI_AUTH_OPEN` para pruebas (no recomendado en produccion).
- **Tiempo estimado**: 2-3 horas

---

### Tarea T0.6.2: Verificar ambas interfaces de red
- **Dificultad**: Basico
- **Descripcion**: Verificar que ambas interfaces (AP y STA) funcionan correctamente de forma simultanea. Pasos:
  1. Flashear el firmware con la configuracion APSTA.
  2. Verificar que el log muestra que el AP ha arrancado y que STA ha obtenido IP del router.
  3. Desde un movil:
     - Buscar redes WiFi y conectarse a "Piscifactoria-GW" con la contraseña configurada.
     - Verificar que el movil obtiene IP (tipicamente 192.168.4.x).
     - Verificar en el log del ESP32 que muestra el evento `WIFI_EVENT_AP_STACONNECTED` con la MAC del movil.
  4. Verificar la STA:
     - El ESP32 debe haber obtenido IP del router (evento `IP_EVENT_STA_GOT_IP`).
     - Desde un ordenador en la misma red que el router, intentar hacer ping a la IP del ESP32.
  5. (Opcional) Imprimir las IP de ambas interfaces con `esp_netif_get_ip_info()`.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar si es necesario para imprimir IPs)
- **Criterio de aceptacion**:
  - Un movil puede conectarse a la red "Piscifactoria-GW" y obtiene IP del DHCP del ESP32
  - El ESP32 muestra en el log cuando un cliente se conecta al AP
  - La interfaz STA obtiene IP del router
  - `ping` desde el movil conectado al AP llega al ESP32 (IP por defecto 192.168.4.1)
  - Se pueden imprimir las IPs de ambas interfaces: AP (192.168.4.1) y STA (IP asignada por router)
- **Dependencias**: Requiere T0.6.1
- **Pistas**: La interfaz AP del ESP32 asigna IPs por DHCP a los clientes (por defecto subnet 192.168.4.x con gateway 192.168.4.1). La interfaz STA obtiene IP del router externo. Para obtener la IP de una interfaz: `esp_netif_get_ip_info(esp_netif_sta, &ip_info)` donde `ip_info.ip` contiene la direccion. `esp_netif_create_default_wifi_ap()` y `esp_netif_create_default_wifi_sta()` devuelven los handles `esp_netif_t*` necesarios.
- **Errores comunes**:
  - El movil se desconecta porque detecta que no hay internet en la red del AP. Solucion: en Android, desactivar "Smart Network Switch" o "Cambiar a datos moviles automaticamente". En iOS, ignorar el aviso de "Sin conexion a internet".
  - La STA no se conecta al router si el canal del AP fija el canal de toda la radio. En APSTA, ambas interfaces comparten la misma radio y por tanto el mismo canal. El canal lo determina el router al que se conecta STA.
- **Tiempo estimado**: 30-60 minutos

---

### Tarea T0.6.3: Probar coexistencia de WiFi APSTA con ESP-NOW
- **Dificultad**: Avanzado
- **Descripcion**: Verificar que ESP-NOW sigue funcionando correctamente mientras el gateway esta en modo APSTA. Esta es una validacion critica porque en produccion el gateway debe recibir datos de los nodos via ESP-NOW al mismo tiempo que mantiene WiFi activo. Pasos:
  1. En el gateway (ESP32-S3), combinar el codigo de APSTA (T0.6.1) con el receptor ESP-NOW (T0.5.2).
  2. IMPORTANTE: ESP-NOW y WiFi comparten la misma radio y deben operar en el mismo canal. Cuando STA se conecta a un router, el canal lo fija el router. El sender ESP-NOW debe usar el mismo canal.
  3. En el gateway, despues de conectarse al router, obtener el canal actual:
     ```c
     uint8_t primary_channel;
     wifi_second_chan_t second;
     esp_wifi_get_channel(&primary_channel, &second);
     ESP_LOGI(TAG, "Canal WiFi actual: %d", primary_channel);
     ```
  4. En el nodo C3 (sender), fijar el mismo canal:
     ```c
     esp_wifi_set_channel(CANAL_DEL_ROUTER, WIFI_SECOND_CHAN_NONE);
     ```
     (En pruebas, hardcodear el canal. En produccion, se comunicara dinamicamente.)
  5. Verificar simultaneamente:
     - ESP-NOW recibe datos del nodo C3.
     - Un movil esta conectado al AP "Piscifactoria-GW".
     - STA esta conectado al router.
  6. Documentar si hay perdida de paquetes ESP-NOW al tener APSTA activo vs solo ESP-NOW.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar)
  - `firmware/node/main/main.c` (modificar para fijar canal)
- **Criterio de aceptacion**:
  - ESP-NOW recibe datos del nodo C3 mientras APSTA esta activo
  - No hay aumento significativo de perdida de paquetes ESP-NOW con APSTA activo (< 5% adicional)
  - Las tres funciones coexisten: recepcion ESP-NOW, AP activo, STA conectado al router
  - El log muestra recepciones ESP-NOW, clientes AP conectados, e IP de STA simultaneamente
- **Dependencias**: Requiere T0.5.2, T0.6.2
- **Pistas**: ESP-NOW transmite en el canal WiFi actual del dispositivo. Si sender y receiver estan en canales diferentes, no se comunicaran. `esp_wifi_set_channel()` fija el canal en el sender. En modo STA conectado a un AP, el canal lo determina el router y no se puede cambiar. `esp_wifi_get_channel()` permite consultar el canal actual. La coexistencia funciona porque ESP-NOW y WiFi comparten la capa MAC y se turnan para transmitir.
- **Errores comunes**:
  - Sender y receiver en canales diferentes. Si el router del gateway usa canal 6, el sender C3 tambien debe estar en canal 6. Si no coinciden, ESP-NOW no recibira nada.
  - Intentar cambiar el canal del gateway cuando STA esta conectado. El canal lo fija el router al que STA esta conectado. Si necesitas un canal especifico, configurar el router para usar ese canal.
- **Tiempo estimado**: 2-3 horas

---

> **Checkpoint 0.6**: El gateway ESP32-S3 ahora funciona en modo APSTA (crea red WiFi propia y se conecta al router simultaneamente) y recibe datos via ESP-NOW del nodo C3. Esta es la configuracion de red que usara el sistema en produccion. Si la coexistencia causa problemas, documenta las limitaciones encontradas.

---

## Sub-tarea 0.7: Benchmarks de consumo

**Objetivo**: Medir el consumo energetico real del ESP32-C3 (nodo) en diferentes modos de operacion para estimar la autonomia con bateria.

---

### Tarea T0.7.1: Medir corriente del ESP32-C3 en diferentes modos
- **Dificultad**: Basico
- **Descripcion**: Medir el consumo de corriente del ESP32-C3 en los principales modos de operacion usando un multimetro. Pasos:
  1. Configurar el multimetro en modo amperimetro (rango mA para modos activos, uA para sleep).
  2. Conectar el multimetro EN SERIE con la alimentacion del ESP32-C3:
     - Desconectar el USB.
     - Alimentar la placa con una fuente externa de 3.3V.
     - Insertar el multimetro entre la fuente y el pin 3.3V de la placa (o entre VIN y fuente de 5V si la placa tiene regulador).
  3. Medir consumo en cada modo:
     - **Modo activo (WiFi TX)**: ejecutar el sender ESP-NOW. Anotar corriente.
     - **Modem sleep**: usar `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)`. Medir cuando no transmite.
     - **Light sleep**: configurar con `esp_light_sleep_start()`. Medir durante el sleep.
     - **Deep sleep**: configurar con `esp_deep_sleep_start()`. Medir. Este es el modo de menor consumo.
  4. Para deep sleep, crear un programa simple:
     ```c
     void app_main(void) {
         ESP_LOGI(TAG, "Despertando...");
         // Aqui iria lectura de sensor y envio ESP-NOW
         vTaskDelay(pdMS_TO_TICKS(1000));
         ESP_LOGI(TAG, "Entrando en deep sleep por 10 segundos...");
         esp_deep_sleep(10 * 1000000); // argumento en microsegundos
     }
     ```
  5. Anotar todos los valores medidos.
- **Archivos a crear/modificar**:
  - `firmware/node/main/main.c` (modificar para diferentes modos de sleep)
- **Criterio de aceptacion**:
  - Se han medido al menos 4 modos: activo con WiFi, modem sleep, light sleep, deep sleep
  - Los valores son coherentes con la documentacion de Espressif (ej: deep sleep del C3 ~5uA, activo ~130mA)
  - Los valores estan anotados con precision (al menos en mA para activo, uA para sleep)
  - El multimetro esta correctamente configurado (rango adecuado para cada medicion)
- **Dependencias**: Requiere T0.5.1 (para tener firmware base)
- **Pistas**: Valores de referencia del ESP32-C3 (aproximados, varian segun condiciones): Activo WiFi TX ~130-150mA, Modem sleep ~15-20mA, Light sleep ~130uA, Deep sleep ~5uA. `esp_deep_sleep(us)` acepta microsegundos. En deep sleep, la RAM se pierde (excepto RTC memory). Al despertar, se reinicia `app_main()`. Para guardar datos entre deep sleeps, usar `RTC_DATA_ATTR`.
- **Errores comunes**:
  - Medir la corriente con el multimetro en paralelo (como voltimetro) en lugar de en serie. Esto puede dañar el multimetro o fundir el fusible. Siempre medir corriente EN SERIE.
  - No cambiar el rango del multimetro al pasar de mA a uA. Las mediciones de deep sleep estan en el rango de microsAmperios y no se ven en el rango de mA.
- **Tiempo estimado**: 2-3 horas

---

### Tarea T0.7.2: Calcular autonomia teorica con bateria
- **Dificultad**: Basico
- **Descripcion**: Usando los valores medidos en T0.7.1, calcular la autonomia teorica del nodo con una bateria de 3400mAh (tipica Li-Ion 18650). Pasos:
  1. Calcular autonomia en modo continuo para cada modo:
     ```
     Autonomia (horas) = Capacidad (mAh) / Consumo (mA)
     ```
     Ejemplo: 3400mAh / 130mA = 26.2 horas (solo WiFi activo)
  2. Calcular autonomia con duty cycle (caso real):
     - Escenario: despertar cada 60 segundos, leer sensor (100ms), enviar ESP-NOW (200ms), dormir el resto.
     - Tiempo activo: 300ms, tiempo dormido: 59700ms.
     ```
     Consumo medio = (t_activo * I_activo + t_sleep * I_sleep) / (t_activo + t_sleep)
     Consumo medio = (0.3s * 130mA + 59.7s * 0.005mA) / 60s
     Consumo medio = (39 + 0.2985) / 60 = 0.655 mA
     Autonomia = 3400mAh / 0.655mA = 5191 horas = 216 dias
     ```
  3. Repetir el calculo para intervalos de 30s, 60s, 120s y 300s.
  4. Crear una tabla comparativa con los resultados.
  5. Considerar un factor de eficiencia del 80% (la bateria no entrega el 100% de su capacidad nominal).
- **Archivos a crear/modificar**:
  - `docs/benchmark-consumo.md` (crear, parcial; se completara en T0.7.3)
- **Criterio de aceptacion**:
  - Se han calculado las autonomias para al menos 4 escenarios diferentes
  - Los calculos incluyen el duty cycle realista (no solo modo continuo)
  - Se aplica el factor de eficiencia del 80%
  - Los resultados estan en una tabla clara con columnas: Escenario, Intervalo, Consumo medio, Autonomia
  - Las unidades son consistentes (mA, mAh, horas/dias)
- **Dependencias**: Requiere T0.7.1
- **Pistas**: La formula de duty cycle es: `I_medio = (D * I_activo) + ((1-D) * I_sleep)` donde D es el duty cycle (fraccion del tiempo activo). Una bateria 18650 de 3400mAh proporciona ~3.7V nominal. Si usas regulador LDO, la eficiencia es ~90%. Si usas buck converter, ~85-95%. Para ser conservador, usar 80% de la capacidad nominal. No olvidar que la corriente de arranque del WiFi puede ser mayor (~400mA pico) aunque sea breve.
- **Errores comunes**:
  - Olvidar convertir unidades (mezclar segundos con horas, mA con uA). Mantener todo en las mismas unidades durante el calculo.
  - No considerar el consumo durante el arranque del WiFi (startup). Aunque es breve (~100ms), la corriente pico es alta (~300-400mA). Para calculos conservadores, incluir este pico.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T0.7.3: Escribir informe de benchmarks de consumo
- **Dificultad**: Basico
- **Descripcion**: Documentar todos los resultados de las mediciones y calculos de consumo en un informe estructurado. Pasos:
  1. Crear el archivo `docs/benchmark-consumo.md`.
  2. Incluir las siguientes secciones:
     - **Metodologia**: como se midio (equipo, configuracion, condiciones).
     - **Resultados de medicion**: tabla con los valores medidos en T0.7.1.
     - **Calculos de autonomia**: tabla con los calculos de T0.7.2.
     - **Graficos** (opcional): se pueden crear con herramientas externas o en ASCII.
     - **Conclusiones**: modo recomendado para produccion, intervalo optimo, autonomia esperada.
     - **Limitaciones**: factores no considerados (temperatura, envejecimiento de bateria, variabilidad entre chips).
  3. Incluir la configuracion exacta de firmware para cada medicion (target, config de WiFi, etc.).
- **Archivos a crear/modificar**:
  - `docs/benchmark-consumo.md` (crear o completar)
- **Criterio de aceptacion**:
  - El documento contiene todas las secciones listadas (metodologia, resultados, calculos, conclusiones)
  - Las tablas son claras y los valores tienen unidades
  - Las conclusiones responden a la pregunta: "¿Cuanto durara un nodo con bateria en produccion?"
  - Se mencionan las limitaciones del benchmark
  - El informe es reproducible: otro desarrollador podria repetir las mediciones con la informacion proporcionada
- **Dependencias**: Requiere T0.7.1, T0.7.2
- **Pistas**: Formato de tabla Markdown:
  ```
  | Modo | Corriente | Autonomia (continuo) |
  |------|-----------|---------------------|
  | Activo WiFi | 130 mA | 26 horas |
  | Deep sleep | 5 uA | 7.8 años |
  ```
  Incluir la version de ESP-IDF, la version de la placa y la fecha de las mediciones. Los resultados pueden variar un 10-20% entre chips del mismo modelo.
- **Errores comunes**:
  - No especificar las condiciones de medicion (temperatura ambiente, firmware exacto, etc.). Un benchmark sin contexto no es reproducible.
  - Presentar solo el mejor caso. Incluir tambien el peor caso y el caso tipico para dar una vision realista.
- **Tiempo estimado**: 1-2 horas

---

> **Checkpoint 0.7**: Ahora tienes datos reales de consumo del ESP32-C3 y una estimacion de cuanto durara cada nodo con bateria. Estos datos seran fundamentales para decidir el intervalo de lectura y el modo de sleep en las fases siguientes. Guarda bien el informe, sera una referencia durante todo el proyecto.

---

## Sub-tarea 0.8: Estructura del proyecto

**Objetivo**: Establecer la estructura de build del proyecto con CMake y crear el componente compartido entre gateway y node.

---

### Tarea T0.8.1: Crear CMakeLists.txt para gateway y node
- **Dificultad**: Basico
- **Descripcion**: Configurar correctamente los archivos CMakeLists.txt para que tanto el gateway como el node sean proyectos ESP-IDF independientes que se pueden compilar por separado. Pasos:
  1. Crear `firmware/gateway/CMakeLists.txt`:
     ```cmake
     cmake_minimum_required(VERSION 3.16)

     set(EXTRA_COMPONENT_DIRS "../common")

     include($ENV{IDF_PATH}/tools/cmake/project.cmake)
     project(gateway)
     ```
  2. Crear `firmware/gateway/main/CMakeLists.txt`:
     ```cmake
     idf_component_register(
         SRCS "main.c"
         INCLUDE_DIRS "."
         REQUIRES protocol
     )
     ```
  3. Crear `firmware/node/CMakeLists.txt`:
     ```cmake
     cmake_minimum_required(VERSION 3.16)

     set(EXTRA_COMPONENT_DIRS "../common")

     include($ENV{IDF_PATH}/tools/cmake/project.cmake)
     project(node)
     ```
  4. Crear `firmware/node/main/CMakeLists.txt`:
     ```cmake
     idf_component_register(
         SRCS "main.c"
         INCLUDE_DIRS "."
         REQUIRES protocol
     )
     ```
  5. Verificar que ambos proyectos compilan independientemente:
     ```bash
     cd firmware/gateway && idf.py set-target esp32s3 && idf.py build
     cd firmware/node && idf.py set-target esp32c3 && idf.py build
     ```
- **Archivos a crear/modificar**:
  - `firmware/gateway/CMakeLists.txt` (crear o verificar)
  - `firmware/gateway/main/CMakeLists.txt` (crear o verificar)
  - `firmware/node/CMakeLists.txt` (crear o verificar)
  - `firmware/node/main/CMakeLists.txt` (crear o verificar)
- **Criterio de aceptacion**:
  - `idf.py build` en `firmware/gateway/` compila exitosamente para esp32s3
  - `idf.py build` en `firmware/node/` compila exitosamente para esp32c3
  - Ambos proyectos referencian el componente compartido en `../common`
  - La linea `REQUIRES protocol` no causa error (el componente existe tras T0.8.2)
- **Dependencias**: Requiere T0.1.6
- **Pistas**: `EXTRA_COMPONENT_DIRS` dice a ESP-IDF donde buscar componentes adicionales (fuera del directorio `components/` por defecto). La ruta es relativa al `CMakeLists.txt` del proyecto. `REQUIRES` en `idf_component_register` declara una dependencia de componente (necesario para que el header se incluya correctamente). `cmake_minimum_required(VERSION 3.16)` es el minimo requerido por ESP-IDF v5.x.
- **Errores comunes**:
  - Usar rutas absolutas en `EXTRA_COMPONENT_DIRS`. Siempre usar rutas relativas para que el proyecto sea portable entre maquinas.
  - Olvidar `REQUIRES protocol` en el CMakeLists.txt del main. Sin esta directiva, `#include "protocol.h"` fallara porque CMake no sabe que debe añadir el include path del componente.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T0.8.2: Crear componente protocol compartido
- **Dificultad**: Intermedio
- **Descripcion**: Crear un componente ESP-IDF reutilizable que contenga las definiciones de estructuras y constantes compartidas entre gateway y node. Esto evita duplicar codigo y asegura que ambos lados usen las mismas definiciones. Pasos:
  1. Crear la estructura del componente:
     ```
     firmware/common/protocol/
     ├── CMakeLists.txt
     ├── include/
     │   └── protocol.h
     └── protocol.c
     ```
  2. Crear `firmware/common/protocol/CMakeLists.txt`:
     ```cmake
     idf_component_register(
         SRCS "protocol.c"
         INCLUDE_DIRS "include"
     )
     ```
  3. Crear `firmware/common/protocol/include/protocol.h`:
     ```c
     #pragma once

     #include <stdint.h>

     #define PROTOCOL_VERSION 1
     #define MAX_SENSOR_DATA_SIZE 250  // Limite de ESP-NOW

     // Tipos de mensaje
     typedef enum {
         MSG_TYPE_SENSOR_DATA = 0x01,
         MSG_TYPE_HEARTBEAT   = 0x02,
         MSG_TYPE_CONFIG      = 0x03,
         MSG_TYPE_ACK         = 0x04,
     } msg_type_t;

     // Estructura principal de datos de sensor
     typedef struct __attribute__((packed)) {
         uint8_t  version;        // Version del protocolo
         uint8_t  msg_type;       // msg_type_t
         uint16_t node_id;        // Identificador del nodo
         uint32_t timestamp_ms;   // Milisegundos desde arranque
         float    temperatura;    // Grados Celsius
         float    ph;             // pH (placeholder)
         float    oxigeno;        // mg/L (placeholder)
         uint16_t turbidez_raw;   // Valor raw ADC
         int8_t   rssi;           // RSSI del ultimo envio
         uint8_t  bateria_pct;    // Porcentaje bateria (0-100)
     } sensor_data_t;

     // Estructura de heartbeat
     typedef struct __attribute__((packed)) {
         uint8_t  version;
         uint8_t  msg_type;
         uint16_t node_id;
         uint32_t uptime_s;       // Segundos desde arranque
         uint8_t  bateria_pct;
         int8_t   rssi;
     } heartbeat_t;
     ```
  4. Crear `firmware/common/protocol/protocol.c` con funciones auxiliares placeholder:
     ```c
     #include "protocol.h"

     // Funcion para validar un mensaje recibido
     bool protocol_validate_message(const uint8_t *data, size_t len) {
         if (len < 2) return false;
         if (data[0] != PROTOCOL_VERSION) return false;
         return true;
     }
     ```
  5. En los `main.c` de gateway y node, reemplazar las definiciones locales de `sensor_data_t` por `#include "protocol.h"`.
  6. Compilar ambos proyectos y verificar que no hay errores.
- **Archivos a crear/modificar**:
  - `firmware/common/protocol/CMakeLists.txt` (crear)
  - `firmware/common/protocol/include/protocol.h` (crear)
  - `firmware/common/protocol/protocol.c` (crear)
  - `firmware/gateway/main/main.c` (modificar para usar `#include "protocol.h"`)
  - `firmware/node/main/main.c` (modificar para usar `#include "protocol.h"`)
- **Criterio de aceptacion**:
  - Ambos proyectos (gateway y node) compilan usando `#include "protocol.h"` del componente compartido
  - No hay definiciones duplicadas de `sensor_data_t` (solo existe en `protocol.h`)
  - `sizeof(sensor_data_t)` devuelve el mismo valor en S3 y C3 (gracias a `__attribute__((packed))`)
  - `protocol_validate_message()` retorna `true` para un mensaje valido y `false` para datos invalidos
- **Dependencias**: Requiere T0.8.1, T0.5.2
- **Pistas**: `__attribute__((packed))` elimina el padding entre campos de la struct, lo que es crucial cuando los datos se envian por red (sin packed, el compilador puede insertar bytes de padding diferentes en arquitecturas diferentes). `INCLUDE_DIRS "include"` en el CMakeLists.txt hace que `#include "protocol.h"` funcione directamente (sin necesidad de `#include "include/protocol.h"`). `#pragma once` es una directiva de guarda de inclusion (alternativa moderna a `#ifndef`/`#define`/`#endif`).
- **Errores comunes**:
  - Olvidar `__attribute__((packed))` en las structs compartidas. Sin packed, el ESP32-S3 (Xtensa, 32-bit) y el ESP32-C3 (RISC-V, 32-bit) podrian añadir padding diferente, corrompiendo los datos recibidos.
  - No incluir el componente en `REQUIRES` del CMakeLists.txt del main. Si no se declara la dependencia, el include path no se configura y `#include "protocol.h"` falla en compilacion.
- **Tiempo estimado**: 1-2 horas

---

> **Checkpoint 0.8**: El proyecto tiene ahora una estructura limpia con dos firmwares independientes (gateway para ESP32-S3 y node para ESP32-C3) que comparten un componente comun con las definiciones del protocolo. Esta estructura sera la base para todo el desarrollo futuro. Verifica que ambos proyectos compilan limpiamente.

---

## Preguntas de autoevaluacion

Antes de pasar a la Fase 1, asegurate de poder responder estas preguntas:

1. **¿Cual es la diferencia entre ESP32-S3 y ESP32-C3 en terminos de arquitectura de CPU?** (Pista: uno es Xtensa dual-core y otro es RISC-V single-core)

2. **¿Por que es necesario inicializar WiFi antes de usar ESP-NOW, aunque no nos conectemos a ningun Access Point?** (Pista: ESP-NOW usa la capa MAC de WiFi)

3. **¿Que ocurre si el sender ESP-NOW esta en canal 1 y el receiver esta en canal 6?** (Pista: no se comunicaran; deben estar en el mismo canal)

4. **¿Por que usamos `__attribute__((packed))` en las estructuras del protocolo?** (Pista: evitar que el compilador añada padding diferente en arquitecturas diferentes)

5. **¿Cual es la diferencia entre `ESP_LOGD` y `ESP_LOGI` y por que los mensajes DEBUG pueden no aparecer?** (Pista: el nivel de compilacion por defecto es INFO, que excluye DEBUG y VERBOSE)

6. **¿Por que el ADC2 no se debe usar cuando WiFi esta activo en el ESP32?** (Pista: ADC2 comparte hardware con el modulo WiFi)

7. **¿Que modo de sleep ofrece el menor consumo en el ESP32-C3 y cual es su principal limitacion?** (Pista: deep sleep consume ~5uA pero pierde toda la RAM)

8. **¿Cual es el tamaño maximo de un paquete ESP-NOW y como afecta esto al diseño del protocolo?** (Pista: 250 bytes maximo; las estructuras de datos deben caber en un solo paquete)

---

## Lectura recomendada

### Guias oficiales de Espressif
- [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/get-started/index.html) - Guia de inicio con ESP-IDF
- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/build-system.html) - Sistema de build con CMake
- [ESP-NOW Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/network/esp_now.html) - Documentacion oficial de ESP-NOW

### Perifericos
- [GPIO & RTC GPIO](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/gpio.html) - Control de pines GPIO
- [ADC Oneshot Mode](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/adc_oneshot.html) - Lectura ADC (API nueva)
- [I2C Master Driver](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/i2c.html) - Driver I2C (API nueva v5.x)

### Bajo consumo
- [Deep Sleep Wake Stubs](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/deep-sleep-stub.html) - Deep sleep y wake stubs
- [Power Management](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/power_management.html) - Gestion de energia

### Datasheets
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) - Hoja de datos del ESP32-S3
- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf) - Hoja de datos del ESP32-C3

### Component Registry
- [ESP-IDF Component Registry](https://components.espressif.com/) - Registro de componentes (drivers de sensores, librerias, etc.)

---

## Errores frecuentes de esta fase

A continuacion, un resumen consolidado de los errores mas comunes que un desarrollador junior puede encontrar durante la Fase 0:

### Entorno y toolchain
1. **Olvidar `source export.sh`**: Cada terminal nueva necesita cargar las variables de ESP-IDF. Solucion: añadir al `.bashrc` o usar un alias.
2. **No usar `--recursive` al clonar ESP-IDF**: Los submodulos no se descargan. Solucion: `git submodule update --init --recursive`.
3. **No cambiar el target antes de compilar**: Compilar firmware de S3 para C3 (o viceversa) produce errores confusos. Siempre ejecutar `idf.py set-target` primero.

### Hardware y conexiones
4. **Cable USB de solo carga**: Si `dmesg` no detecta nada al conectar la placa, probar con otro cable que tenga lineas de datos.
5. **Permisos de puerto serie**: Añadir el usuario al grupo `dialout` y reiniciar sesion completa (no solo la terminal).
6. **Olvidar resistencias pull-up**: I2C y 1-Wire requieren pull-ups. Sin ellos, la comunicacion falla silenciosamente.
7. **Conectar sensores a 5V**: Los pines del ESP32 son de 3.3V. Conectar 5V puede dañar el chip permanentemente.

### Programacion ESP-IDF
8. **Usar `delay()` en lugar de `vTaskDelay()`**: ESP-IDF usa FreeRTOS; `delay()` no existe.
9. **Usar `printf` en lugar de `ESP_LOGx`**: Las macros de log proporcionan timestamp, TAG, nivel y son filtrables.
10. **No inicializar NVS antes de WiFi**: WiFi requiere NVS para calibracion del PHY.
11. **No inicializar WiFi antes de ESP-NOW**: ESP-NOW usa la capa MAC de WiFi.
12. **Usar API I2C legacy**: En ESP-IDF v5.x, usar la nueva API con `i2c_master_bus_handle_t`.

### ADC
13. **No configurar atenuacion del ADC**: Sin `ADC_ATTEN_DB_12`, el rango es solo 0-1V y voltajes mayores saturan.
14. **Usar ADC2 con WiFi activo**: ADC2 comparte hardware con WiFi. Siempre usar ADC1.

### ESP-NOW y WiFi
15. **Sender y receiver en canales WiFi diferentes**: ESP-NOW solo funciona si ambos dispositivos estan en el mismo canal.
16. **Asumir que broadcast ESP-NOW confirma recepcion**: En broadcast, `SEND_SUCCESS` solo significa que el paquete fue transmitido, no que fue recibido.
17. **Structs sin `packed` en el protocolo**: Sin `__attribute__((packed))`, el padding puede diferir entre arquitecturas, corrompiendo los datos.

### Mediciones y benchmarks
18. **Medir corriente con multimetro en paralelo**: La corriente se mide EN SERIE. Medir en paralelo puede dañar el multimetro.
19. **No cambiar rango del multimetro**: Deep sleep es ~uA, activo es ~mA. Usar el rango adecuado para cada medicion.

---

**Al completar la Fase 0**, debes tener:
- Entorno de desarrollo funcional para ESP32-S3 y ESP32-C3
- Familiaridad con GPIO, logging, I2C, ADC y ESP-NOW
- Comunicacion basica nodo-gateway funcionando
- WiFi APSTA configurado en el gateway
- Datos de consumo medidos y documentados
- Estructura de proyecto con componente compartido

**Siguiente fase**: [Fase 1 - Gateway Nucleo](fase-01-gateway-nucleo.md)
