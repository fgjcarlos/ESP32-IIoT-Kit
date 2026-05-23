# ESP-IDF

#concepto #fase/0

## Que es
Espressif IoT Development Framework — el SDK oficial de Espressif para programar ESP32 en C. Basado en [[FreeRTOS]]. Incluye drivers, protocolos de red, filesystem, crypto, OTA, y mas.

## Por que importa en este proyecto
Es el framework sobre el que se construye TODO el firmware, tanto del gateway (ESP32-S3) como de los nodos (ESP32-C3).

## Ciclo de desarrollo
```bash
source ~/esp/esp-idf/export.sh    # Cargar variables (cada terminal nueva!)
idf.py set-target esp32s3         # Configurar target (una vez por proyecto)
idf.py menuconfig                 # Configurar opciones (Kconfig)
idf.py build                      # Compilar
idf.py -p /dev/ttyACM0 flash      # Flashear
idf.py monitor                    # Ver UART output (Ctrl+] para salir)
idf.py -p /dev/ttyACM0 flash monitor  # Flashear y monitorizar
```

## Estructura de un proyecto
```
mi_proyecto/
├── CMakeLists.txt          ← Boilerplate de proyecto
├── main/
│   ├── CMakeLists.txt      ← Lista de sources y dependencias
│   ├── main.c              ← app_main() va aqui
│   └── Kconfig.projbuild   ← Opciones de menuconfig custom
├── components/             ← Componentes reutilizables
│   └── mi_componente/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── mi_componente.h
│       └── mi_componente.c
└── sdkconfig               ← Generado por menuconfig (no editar a mano)
```

## Gotchas
- `source export.sh` es NECESARIO en cada terminal nueva
- `idf.py set-target` limpia el build — ejecutar antes del primer build
- `sdkconfig` no se edita a mano — usar `idf.py menuconfig`
- El punto de entrada es `app_main()`, no `main()`
- Usar `ESP_LOGx()` en vez de `printf()` — tiene tags, niveles y timestamps

## Enlaces
- [[FreeRTOS]] - El RTOS debajo de ESP-IDF
- [[CMake-ESP-IDF]] - Sistema de build
- [[Kconfig]] - Sistema de configuracion
- [[NVS]] - Almacenamiento persistente
- [Doc oficial](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/)
