# Fase 1: Gateway - Nucleo

> **CORRECCIONES APLICADAS** (2026-05-23): Se agrega T1.1.5 (gateway envia ACK por ESP-NOW), T1.5.4 (whitelist de nodos). T1.6.2 cambia WDT de 30s a 90s. T1.1.4 agrega validacion SNTP. Duracion ajustada a 5 semanas. Ver detalles en `docs/replanificacion/03-fases-corregidas.md`.

## Resumen

- **Objetivo**: Construir un gateway ESP32-S3 funcional con triple conectividad (ESP-NOW + WiFi AP + WiFi STA), servidor web embebido y API REST basica.
- **Duracion estimada**: 4 semanas
- **Prerequisitos**: Fase 0 completada (entorno de desarrollo configurado, toolchain ESP-IDF instalado, proyecto base compilando y flasheando correctamente)
- **Hardware necesario**: ESP32-S3-DevKitC, al menos 1x ESP32-C3 para pruebas ESP-NOW, router WiFi para pruebas modo STA

## Dependencias externas

- ESP-IDF v5.x instalado y configurado en el PATH
- Navegador web (Chrome/Firefox para probar el servidor embebido)
- Router WiFi (para probar modo STA)
- Herramienta de pruebas HTTP: `curl` o Postman
- Monitor serial: `idf.py monitor` o PlatformIO Monitor

---

## Sub-tarea 1.1: Inicializacion del sistema

Esta sub-tarea establece los cimientos del gateway. Sin una inicializacion correcta, nada mas funcionara. El orden de inicializacion es critico: NVS primero, luego WiFi, luego ESP-NOW.

---

### Tarea T1.1.1: Inicializar NVS con manejo de errores

- **Dificultad**: Basico
- **Descripcion**: Crear el modulo de configuracion NVS (Non-Volatile Storage) que sera la base para almacenar credenciales WiFi y configuraciones persistentes del gateway. Paso a paso:
  1. Crear los archivos `nvs_config.c` y `nvs_config.h` en el directorio de fuentes del gateway.
  2. En el header, declarar la funcion `esp_err_t nvs_config_init(void)`.
  3. En la implementacion, llamar a `nvs_flash_init()`.
  4. Si el retorno es `ESP_ERR_NVS_NO_FREE_PAGES` o `ESP_ERR_NVS_NEW_VERSION_FOUND`, llamar a `nvs_flash_erase()` y luego reintentar `nvs_flash_init()`.
  5. Logear con `ESP_LOGI` el resultado de la inicializacion.
  6. Llamar a `nvs_config_init()` como primera operacion en `app_main()`.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/nvs_config.h` (crear)
  - `firmware/gateway/main/nvs_config.c` (crear)
  - `firmware/gateway/main/main.c` (modificar - agregar llamada en app_main)
  - `firmware/gateway/main/CMakeLists.txt` (modificar - agregar nvs_config.c a SRCS)
- **Criterio de aceptacion**:
  - La funcion `nvs_config_init()` retorna `ESP_OK` en el monitor serial
  - Si se corrompe la NVS (simular con `nvs_flash_erase()` manual), el sistema se recupera automaticamente
  - No hay memory leaks ni errores en la consola serial al arrancar
  - El log muestra claramente "NVS inicializado correctamente" o el error correspondiente
- **Dependencias**: Ninguna (es la primera tarea)
- **Pistas**:
  - `#include "nvs_flash.h"` para las funciones NVS
  - `#include "esp_log.h"` para logging
  - Usar `static const char *TAG = "NVS"` para el tag de log
  - La funcion `nvs_flash_init()` retorna `esp_err_t`
  - Macro util: `ESP_ERROR_CHECK()` para abortar si hay error fatal (pero aqui queremos manejar el error, no abortar)
- **Errores comunes**:
  - Olvidar incluir `nvs_flash` en los requerimientos de CMake (`REQUIRES nvs_flash`). Sin esto, el linker dara errores de simbolos no encontrados.
  - Usar `ESP_ERROR_CHECK()` directamente en `nvs_flash_init()` sin manejar los codigos de error recuperables. Esto causa un abort innecesario cuando la NVS simplemente necesita un erase.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T1.1.2: Crear wifi_manager con modo APSTA

- **Dificultad**: Intermedio
- **Descripcion**: Crear el modulo de gestion WiFi que configure el ESP32-S3 en modo APSTA (Access Point + Station simultaneo). Esto permite que el gateway cree su propia red WiFi para configuracion mientras se conecta al router existente para acceso a internet. Paso a paso:
  1. Crear los archivos `wifi_manager.c` y `wifi_manager.h`.
  2. En el header, declarar `esp_err_t wifi_manager_init(void)`.
  3. En la implementacion:
     a. Llamar a `esp_netif_init()` para inicializar la pila TCP/IP.
     b. Crear el event loop por defecto con `esp_event_loop_create_default()`.
     c. Crear interfaces de red: `esp_netif_create_default_wifi_ap()` y `esp_netif_create_default_wifi_sta()`.
     d. Inicializar WiFi con `esp_wifi_init()` usando `WIFI_INIT_CONFIG_DEFAULT()`.
     e. Configurar el modo con `esp_wifi_set_mode(WIFI_MODE_APSTA)`.
     f. Configurar AP: SSID "Piscifactoria-GW", password "piscifactoria2024", canal 1, max 4 conexiones.
     g. Configurar STA: SSID y password hardcoded por ahora (se cambiara a NVS en T1.2.3).
     h. Llamar a `esp_wifi_start()`.
  4. Registrar event handlers basicos (se expandiran en T1.2.1).
  5. Llamar a `wifi_manager_init()` en `app_main()` despues de NVS.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/wifi_manager.h` (crear)
  - `firmware/gateway/main/wifi_manager.c` (crear)
  - `firmware/gateway/main/main.c` (modificar - agregar llamada despues de nvs_config_init)
  - `firmware/gateway/main/CMakeLists.txt` (modificar - agregar wifi_manager.c a SRCS)
- **Criterio de aceptacion**:
  - Al arrancar el ESP32, aparece la red WiFi "Piscifactoria-GW" visible desde un telefono o PC
  - Un dispositivo puede conectarse al AP con la password configurada
  - El log serial muestra la IP asignada al AP (normalmente 192.168.4.1)
  - Si se configuran credenciales STA validas, el ESP32 obtiene IP del router
  - El log muestra claramente el estado de ambas interfaces (AP y STA)
- **Dependencias**: T1.1.1 (NVS debe estar inicializado antes de WiFi)
- **Pistas**:
  - `#include "esp_wifi.h"` y `#include "esp_netif.h"`
  - Estructura de config AP: `wifi_config_t` con campo `.ap`
  - Estructura de config STA: `wifi_config_t` con campo `.sta`
  - `esp_wifi_set_config(WIFI_IF_AP, &ap_config)` para configurar AP
  - `esp_wifi_set_config(WIFI_IF_STA, &sta_config)` para configurar STA
  - El SSID y password son arrays de `uint8_t`, usar `strcpy()` o asignacion directa
  - `wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT()` para config por defecto
- **Errores comunes**:
  - Olvidar llamar a `esp_netif_init()` y `esp_event_loop_create_default()` antes de inicializar WiFi. Esto causa crashes silenciosos o comportamiento erratico.
  - Configurar el canal del AP sin considerar que ESP-NOW necesita el mismo canal. En modo APSTA, el canal del AP y STA deben coincidir. Si STA se conecta a un router en canal 6, el AP cambiara automaticamente al canal 6.
- **Tiempo estimado**: 3-5 horas

---

### Tarea T1.1.3: Inicializar ESP-NOW en el gateway

- **Dificultad**: Intermedio
- **Descripcion**: Crear el modulo de gestion ESP-NOW que permitira al gateway recibir datos de los nodos sensores sin necesidad de que estos se conecten a WiFi. ESP-NOW es un protocolo ligero de Espressif que funciona sobre la capa de enlace WiFi. Paso a paso:
  1. Crear los archivos `espnow_manager.c` y `espnow_manager.h`.
  2. En el header, declarar `esp_err_t espnow_manager_init(void)`.
  3. En la implementacion:
     a. Llamar a `esp_now_init()` para inicializar ESP-NOW.
     b. Registrar el callback de recepcion con `esp_now_register_recv_cb()`.
     c. Implementar un callback de recepcion basico que por ahora solo logee los datos recibidos.
     d. Verificar el canal WiFi actual con `esp_wifi_get_channel()` y logearlo.
  4. Llamar a `espnow_manager_init()` en `app_main()` DESPUES de `wifi_manager_init()`.
  5. Verificar que ESP-NOW esta activo logeando el estado.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/espnow_manager.h` (crear)
  - `firmware/gateway/main/espnow_manager.c` (crear)
  - `firmware/gateway/main/main.c` (modificar - agregar llamada despues de wifi_manager_init)
  - `firmware/gateway/main/CMakeLists.txt` (modificar - agregar espnow_manager.c a SRCS, agregar `esp_wifi` a REQUIRES)
- **Criterio de aceptacion**:
  - `esp_now_init()` retorna `ESP_OK` en el log
  - El canal WiFi reportado coincide con el canal del AP/STA
  - Si se envia un paquete ESP-NOW desde otro ESP32, el callback se ejecuta y logea la MAC del remitente
  - No hay errores de inicializacion en la consola serial
- **Dependencias**: T1.1.2 (WiFi debe estar inicializado y activo antes de ESP-NOW)
- **Pistas**:
  - `#include "esp_now.h"`
  - El callback de recepcion tiene la firma: `void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)`
  - Para logear la MAC: `ESP_LOGI(TAG, "Recibido de: "MACSTR, MAC2STR(recv_info->src_addr))`
  - `MACSTR` y `MAC2STR` son macros definidas en `esp_mac.h`
  - `esp_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second)` para verificar el canal
  - ESP-NOW tiene un limite de 250 bytes por paquete
- **Errores comunes**:
  - Inicializar ESP-NOW ANTES de WiFi. ESP-NOW depende de la capa WiFi, asi que `esp_wifi_start()` debe haberse ejecutado primero. Si no, `esp_now_init()` fallara con `ESP_ERR_ESPNOW_NOT_INIT`.
  - No verificar que el canal WiFi del gateway coincide con el canal de los nodos ESP-NOW. Si estan en canales diferentes, los paquetes nunca llegaran. El canal del AP en modo APSTA puede cambiar cuando STA se conecta a un router.
- **Tiempo estimado**: 2-4 horas

---

### Tarea T1.1.4: Configurar sincronizacion horaria SNTP

- **Dificultad**: Basico
- **Descripcion**: Configurar la sincronizacion de reloj via SNTP (Simple Network Time Protocol) para que el gateway tenga la hora real. Esto es critico para timestamps en los datos de sensores y logs. La sincronizacion solo funciona cuando el STA esta conectado a internet. Paso a paso:
  1. En `main.c`, crear una funcion `static void sntp_time_init(void)`.
  2. Configurar la zona horaria con `setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1)` y `tzset()` (ajustar segun zona horaria del proyecto).
  3. Configurar el modo de sincronizacion: `esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL)`.
  4. Configurar el servidor: `esp_sntp_setservername(0, "pool.ntp.org")`.
  5. Llamar a `esp_sntp_init()`.
  6. Esperar a que se sincronice el tiempo con un bucle que compruebe `sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED`.
  7. Logear la hora actual una vez sincronizada.
  8. Llamar a `sntp_time_init()` despues de que el STA obtenga IP (dentro del event handler o en main despues de un breve delay).
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar - agregar funcion sntp_time_init y llamarla tras conexion STA)
- **Criterio de aceptacion**:
  - El log serial muestra la hora actual correcta tras sincronizacion (formato legible)
  - Si no hay conexion STA (sin internet), el sistema no se bloquea indefinidamente (timeout de espera)
  - La hora se mantiene correcta durante la ejecucion (verificar con logs periodicos)
  - La zona horaria esta correctamente configurada
- **Dependencias**: T1.1.2 (WiFi STA debe estar conectado para acceder a internet)
- **Pistas**:
  - `#include "esp_sntp.h"` (o `#include "esp_netif_sntp.h"` en versiones mas nuevas de ESP-IDF v5.x)
  - `#include <time.h>` y `#include <sys/time.h>` para funciones de tiempo
  - Para obtener la hora: `time_t now; time(&now); struct tm timeinfo; localtime_r(&now, &timeinfo);`
  - `strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo)` para formatear
  - No bloquear indefinidamente: usar un contador de reintentos (ej. esperar max 30 segundos)
  - `sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED)` para sincronizacion inmediata
- **Errores comunes**:
  - Llamar a `esp_sntp_init()` antes de que el STA tenga IP. Sin conectividad a internet, la resolucion DNS de "pool.ntp.org" fallara silenciosamente y el reloj nunca se sincronizara.
  - Esperar la sincronizacion en un bucle infinito sin timeout. Si no hay internet, el programa se bloquea para siempre. Siempre poner un limite de intentos o tiempo maximo de espera.
- **Tiempo estimado**: 1-3 horas

---

> **Checkpoint 1.1**: Al completar esta sub-tarea, el gateway debe:
> - Arrancar sin errores en la consola serial
> - Mostrar la red WiFi "Piscifactoria-GW" visible desde otros dispositivos
> - Conectarse al router WiFi si se configuran credenciales STA validas
> - Tener ESP-NOW inicializado y listo para recibir paquetes
> - Mostrar la hora correcta si hay conexion a internet
> - El orden de inicializacion en `app_main()` debe ser: NVS -> WiFi -> ESP-NOW -> SNTP
> - **Prueba rapida**: Conectarse al AP "Piscifactoria-GW" con un telefono y verificar que se asigna una IP (192.168.4.x)

---

## Sub-tarea 1.2: Gestion de conexiones WiFi

Esta sub-tarea mejora la robustez de la conexion WiFi, implementando manejo completo de eventos, reconexion automatica y persistencia de credenciales.

---

### Tarea T1.2.1: Event handler completo para WiFi

- **Dificultad**: Intermedio
- **Descripcion**: Implementar un sistema completo de manejo de eventos WiFi para que el gateway reaccione correctamente ante conexiones, desconexiones y cambios de estado. Esto es fundamental para un sistema embebido que debe funcionar 24/7. Paso a paso:
  1. En `wifi_manager.c`, crear una funcion `static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)`.
  2. Manejar los siguientes eventos de `WIFI_EVENT`:
     - `WIFI_EVENT_STA_START`: Logear e intentar conexion con `esp_wifi_connect()`.
     - `WIFI_EVENT_STA_CONNECTED`: Logear SSID y canal del router.
     - `WIFI_EVENT_STA_DISCONNECTED`: Logear razon de desconexion e iniciar reconexion.
     - `WIFI_EVENT_AP_STACONNECTED`: Logear MAC del cliente que se conecto al AP.
     - `WIFI_EVENT_AP_STADISCONNECTED`: Logear MAC del cliente que se desconecto.
  3. Manejar los siguientes eventos de `IP_EVENT`:
     - `IP_EVENT_STA_GOT_IP`: Logear IP obtenida, marcar flag `sta_connected = true`.
  4. Registrar el handler con `esp_event_handler_instance_register()` para ambos grupos de eventos.
  5. Crear variable global o estatica `bool wifi_sta_is_connected(void)` para consultar el estado.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/wifi_manager.c` (modificar - agregar event handler completo)
  - `firmware/gateway/main/wifi_manager.h` (modificar - exponer funcion de estado `wifi_sta_is_connected()`)
- **Criterio de aceptacion**:
  - Al conectar STA al router, el log muestra: IP obtenida, SSID y canal
  - Al desconectar el router (apagarlo), el log muestra la razon de desconexion
  - Al conectar un telefono al AP, el log muestra la MAC del cliente
  - Al desconectar el telefono del AP, el log muestra que se fue
  - La funcion `wifi_sta_is_connected()` retorna el estado correcto
- **Dependencias**: T1.1.2 (wifi_manager base debe existir)
- **Pistas**:
  - `esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_wifi)`
  - `esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_ip)`
  - Para la razon de desconexion: `wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data; event->reason`
  - Para la IP obtenida: `ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data; IPSTR, IP2STR(&event->ip_info.ip)`
  - Para MAC del cliente AP: `wifi_event_ap_staconnected_t *event = ...; MAC2STR(event->mac)`
- **Errores comunes**:
  - Llamar a `esp_wifi_connect()` dentro del handler de `WIFI_EVENT_STA_DISCONNECTED` directamente en un bucle rapido sin delay. Esto puede saturar el sistema si el router no esta disponible. Es mejor delegar la reconexion a una tarea separada o usar un timer.
  - Olvidar que los event handlers se ejecutan en el contexto del event loop task, no en el de `app_main()`. Operaciones lentas dentro del handler bloquean otros eventos.
- **Tiempo estimado**: 2-4 horas

---

### Tarea T1.2.2: Reconexion automatica con backoff exponencial

- **Dificultad**: Intermedio
- **Descripcion**: Implementar un mecanismo de reconexion automatica que no sature el sistema cuando el router no esta disponible. El backoff exponencial incrementa el tiempo entre reintentos para evitar intentos constantes que consumen recursos. Paso a paso:
  1. En `wifi_manager.c`, crear variables estaticas:
     - `static int s_retry_count = 0;`
     - `static int s_backoff_ms = 1000;` (1 segundo inicial)
     - `static const int MAX_BACKOFF_MS = 60000;` (maximo 60 segundos)
  2. En el evento `WIFI_EVENT_STA_DISCONNECTED`:
     a. Incrementar `s_retry_count`.
     b. Logear el intento: "Reconexion intento #%d, esperando %d ms".
     c. Crear un FreeRTOS timer de un solo disparo que llame a `esp_wifi_connect()` tras `s_backoff_ms`.
     d. Duplicar `s_backoff_ms` para el proximo intento (sin exceder `MAX_BACKOFF_MS`).
  3. En el evento `IP_EVENT_STA_GOT_IP`:
     a. Resetear `s_retry_count = 0` y `s_backoff_ms = 1000`.
     b. Logear "Conectado exitosamente tras %d intentos".
  4. Alternativa mas simple: usar `vTaskDelay(pdMS_TO_TICKS(s_backoff_ms))` en una tarea dedicada a reconexion en lugar de un timer.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/wifi_manager.c` (modificar - agregar logica de backoff en event handler)
- **Criterio de aceptacion**:
  - Al perder conexion STA, los intentos de reconexion se espacian: 1s, 2s, 4s, 8s, 16s, 32s, 60s, 60s...
  - El log muestra claramente el numero de intento y el tiempo de espera
  - Al reconectar exitosamente, el backoff se resetea a 1 segundo
  - El sistema no se congela ni consume 100% de CPU durante la reconexion
  - Otras funcionalidades (AP, ESP-NOW, servidor web) siguen funcionando durante la reconexion
- **Dependencias**: T1.2.1 (event handler debe estar implementado)
- **Pistas**:
  - `vTaskDelay(pdMS_TO_TICKS(ms))` para delays no bloqueantes en una tarea
  - `xTimerCreate()`, `xTimerStart()` para timers de FreeRTOS
  - Backoff formula: `s_backoff_ms = MIN(s_backoff_ms * 2, MAX_BACKOFF_MS)`
  - Macro util: `#define MIN(a,b) ((a) < (b) ? (a) : (b))`
  - Considerar usar un EventGroup o semaforo para coordinar la reconexion
  - NO llamar a `vTaskDelay()` dentro del event handler, bloquea el event loop
- **Errores comunes**:
  - Usar `vTaskDelay()` directamente dentro del event handler de WiFi. Los event handlers se ejecutan en el contexto del system event task, y bloquearlos impide que otros eventos se procesen. Usar un timer o una tarea separada.
  - No resetear el backoff al conectar exitosamente. El proximo ciclo de desconexion empezaria con el delay maximo de 60 segundos.
- **Tiempo estimado**: 2-3 horas

---

### Tarea T1.2.3: Almacenar y cargar credenciales WiFi desde NVS

- **Dificultad**: Basico
- **Descripcion**: Implementar funciones para guardar y leer las credenciales WiFi STA desde la memoria no volatil (NVS), permitiendo que el gateway recuerde la configuracion WiFi entre reinicios. Paso a paso:
  1. En `nvs_config.h`, declarar las siguientes funciones:
     - `esp_err_t nvs_config_set_wifi_ssid(const char *ssid);`
     - `esp_err_t nvs_config_get_wifi_ssid(char *ssid, size_t max_len);`
     - `esp_err_t nvs_config_set_wifi_password(const char *password);`
     - `esp_err_t nvs_config_get_wifi_password(char *password, size_t max_len);`
  2. En `nvs_config.c`, implementar cada funcion:
     a. Abrir NVS con namespace "wifi": `nvs_open("wifi", NVS_READWRITE, &handle)`.
     b. Para set: usar `nvs_set_str(handle, "ssid", ssid)` y luego `nvs_commit(handle)`.
     c. Para get: usar `nvs_get_str(handle, "ssid", ssid, &len)`.
     d. Siempre cerrar con `nvs_close(handle)`.
  3. En `wifi_manager_init()`, antes de configurar STA:
     a. Intentar leer SSID y password desde NVS.
     b. Si existen, usarlos; si no, usar valores por defecto hardcoded.
     c. Logear que credenciales se estan usando (sin mostrar la password completa por seguridad).
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/nvs_config.h` (modificar - agregar declaraciones de funciones WiFi)
  - `firmware/gateway/main/nvs_config.c` (modificar - implementar funciones WiFi)
  - `firmware/gateway/main/wifi_manager.c` (modificar - cargar credenciales desde NVS al iniciar)
- **Criterio de aceptacion**:
  - Al guardar credenciales y reiniciar el ESP32, se cargan automaticamente las credenciales guardadas
  - Si no hay credenciales guardadas, se usan los valores por defecto sin error
  - El log muestra si se estan usando credenciales de NVS o por defecto
  - Las funciones retornan `ESP_OK` al guardar y leer correctamente
  - Las funciones retornan error apropiado (`ESP_ERR_NVS_NOT_FOUND`) si la clave no existe
- **Dependencias**: T1.1.1 (NVS inicializado), T1.1.2 (wifi_manager existente)
- **Pistas**:
  - `#include "nvs.h"` para funciones NVS (diferente de `nvs_flash.h`)
  - `nvs_handle_t` es el tipo para el handle de NVS
  - `nvs_get_str()` con buffer `NULL` retorna el tamanio necesario en `&len`
  - Maximo SSID: 32 bytes; maximo password: 64 bytes
  - Siempre verificar el retorno de `nvs_open()` y cerrar el handle en todos los paths (exito y error)
  - Namespace maximo: 15 caracteres
- **Errores comunes**:
  - Olvidar llamar a `nvs_commit()` despues de `nvs_set_str()`. Sin commit, los datos se escriben en cache pero no se persisten en flash, y se pierden al reiniciar.
  - No cerrar el handle NVS con `nvs_close()` en todas las rutas de ejecucion (incluidas las de error). Esto causa leaks de handles que eventualmente agotan los recursos NVS.
- **Tiempo estimado**: 2-3 horas

---

> **Checkpoint 1.2**: Al completar esta sub-tarea, el gateway debe:
> - Manejar correctamente conexiones y desconexiones WiFi con logs claros
> - Reconectarse automaticamente al router con backoff exponencial (1s, 2s, 4s... hasta 60s)
> - Recordar las credenciales WiFi entre reinicios gracias a NVS
> - Mantener el AP funcional incluso cuando el STA esta desconectado
> - **Prueba rapida**: Apagar el router, observar los logs de reconexion con tiempos crecientes. Encender el router, verificar que se reconecta y el backoff se resetea. Reiniciar el ESP32 y verificar que usa las credenciales guardadas.

---

## Sub-tarea 1.3: Servidor web embebido

Esta sub-tarea implementa el servidor HTTP embebido que permite configurar y monitorear el gateway desde un navegador web. El servidor se ejecuta directamente en el ESP32 y sirve paginas HTML/CSS/JS.

---

### Tarea T1.3.1: Crear servidor HTTP basico

- **Dificultad**: Basico
- **Descripcion**: Crear el modulo de servidor HTTP que sera la base para la interfaz web y la API REST del gateway. El servidor HTTP de ESP-IDF es ligero y maneja las peticiones con handlers registrados para cada URI. Paso a paso:
  1. Crear los archivos `http_server.c` y `http_server.h`.
  2. En el header, declarar `httpd_handle_t http_server_start(void)` y `void http_server_stop(httpd_handle_t server)`.
  3. En la implementacion:
     a. Crear la configuracion del servidor: `httpd_config_t config = HTTPD_DEFAULT_CONFIG()`.
     b. Iniciar el servidor con `httpd_start(&server, &config)`.
     c. Registrar un handler para GET "/" que devuelva un HTML basico.
     d. Implementar la funcion handler: `static esp_err_t root_get_handler(httpd_req_t *req)`.
     e. En el handler, usar `httpd_resp_set_type(req, "text/html")` y `httpd_resp_send(req, html_content, HTTPD_RESP_USE_STRLEN)`.
  4. El HTML inicial puede ser simple: `<html><body><h1>Gateway Piscifactoria</h1><p>Sistema activo</p></body></html>`.
  5. Llamar a `http_server_start()` en `app_main()` despues de WiFi.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/http_server.h` (crear)
  - `firmware/gateway/main/http_server.c` (crear)
  - `firmware/gateway/main/main.c` (modificar - agregar llamada para iniciar servidor)
  - `firmware/gateway/main/CMakeLists.txt` (modificar - agregar http_server.c a SRCS, agregar `esp_http_server` a REQUIRES)
- **Criterio de aceptacion**:
  - Al conectarse al AP del gateway (192.168.4.1) desde un navegador, se muestra la pagina "Gateway Piscifactoria"
  - El servidor responde con codigo HTTP 200 y Content-Type "text/html"
  - El log serial muestra "Servidor HTTP iniciado en puerto 80"
  - El servidor no crashea al recibir multiples peticiones consecutivas
- **Dependencias**: T1.1.2 (WiFi debe estar funcionando para acceder al servidor)
- **Pistas**:
  - `#include "esp_http_server.h"`
  - `httpd_uri_t` es la estructura para registrar URIs: contiene `.uri`, `.method`, `.handler`, `.user_ctx`
  - `httpd_register_uri_handler(server, &uri_definition)` para registrar cada ruta
  - `HTTPD_DEFAULT_CONFIG()` usa puerto 80 y permite max 12 URI handlers por defecto
  - El handler debe retornar `ESP_OK` para indicar que la respuesta se envio correctamente
  - `httpd_resp_send()` con `HTTPD_RESP_USE_STRLEN` calcula el tamanio del string automaticamente
- **Errores comunes**:
  - Olvidar agregar `esp_http_server` a REQUIRES en CMakeLists.txt. Sin esto, el compilador encontrara los headers pero el linker fallara con "undefined reference to httpd_start".
  - No verificar el retorno de `httpd_start()`. Si el puerto 80 ya esta en uso (por otro componente) o la memoria es insuficiente, la funcion falla silenciosamente y el puntero `server` queda invalido.
- **Tiempo estimado**: 2-3 horas

---

### Tarea T1.3.2: Pagina de configuracion WiFi

- **Dificultad**: Intermedio
- **Descripcion**: Crear una pagina web con un formulario HTML que permita al usuario cambiar las credenciales WiFi STA del gateway sin necesidad de reflashear el firmware. Paso a paso:
  1. Crear un handler GET para "/config" que devuelva un formulario HTML con:
     - Campo de texto para SSID
     - Campo password para la clave WiFi
     - Boton "Guardar y Conectar"
  2. El formulario debe enviar un POST a "/config/wifi" con los datos.
  3. Crear un handler POST para "/config/wifi" que:
     a. Lea el cuerpo de la peticion con `httpd_req_recv()`.
     b. Parsee los datos URL-encoded (formato: `ssid=MiRed&password=MiClave`).
     c. Guarde las credenciales en NVS usando las funciones de T1.2.3.
     d. Desconecte y reconecte el STA con las nuevas credenciales.
     e. Responda con una pagina de confirmacion.
  4. Crear una funcion auxiliar para decodificar URL-encoded strings (reemplazar `+` por espacio, `%XX` por el caracter).
  5. Registrar ambos handlers en el servidor HTTP.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/http_server.c` (modificar - agregar handlers GET /config y POST /config/wifi)
  - `firmware/gateway/main/wifi_manager.h` (modificar - agregar funcion para reconectar con nuevas credenciales)
  - `firmware/gateway/main/wifi_manager.c` (modificar - implementar funcion de reconexion)
- **Criterio de aceptacion**:
  - Al navegar a `http://192.168.4.1/config` se muestra el formulario con campos SSID y password
  - Al enviar el formulario con credenciales validas, el gateway se conecta al nuevo router
  - Las credenciales se persisten en NVS (verificar reiniciando el ESP32)
  - Si las credenciales son invalidas, el gateway no se queda bloqueado (usa backoff de T1.2.2)
  - Se muestra una pagina de confirmacion tras guardar
- **Dependencias**: T1.3.1 (servidor HTTP), T1.2.3 (funciones NVS para WiFi), T1.2.2 (reconexion automatica)
- **Pistas**:
  - `httpd_req_recv(req, buf, buf_len)` lee el cuerpo del POST; retorna el numero de bytes leidos
  - `httpd_req_get_url_query_str(req, buf, buf_len)` para query strings en GET
  - Para parsear URL-encoded: buscar `&` para separar pares, `=` para separar clave-valor
  - Para reconectar STA: `esp_wifi_disconnect()`, actualizar config, `esp_wifi_connect()`
  - El buffer para `httpd_req_recv()` puede necesitar multiples lecturas si `req->content_len` es mayor que tu buffer
  - El Content-Length de la peticion esta en `req->content_len`
- **Errores comunes**:
  - No verificar `req->content_len` antes de leer el cuerpo. Si el cliente envia un POST sin body (o con body mas grande de lo esperado), el handler puede leer basura o desbordar el buffer.
  - Olvidar la decodificacion URL-encoding. Los caracteres especiales en SSID o password (espacios, simbolos) llegaran codificados (ej. "Mi%20Red" en lugar de "Mi Red") y se guardaran incorrectamente en NVS.
- **Tiempo estimado**: 3-5 horas

---

### Tarea T1.3.3: Embeber archivos HTML/CSS/JS con CMake

- **Dificultad**: Intermedio
- **Descripcion**: En lugar de tener el HTML como strings dentro del codigo C, mover los archivos web a un directorio separado y embeberlos en el binario del firmware usando el sistema de build de CMake. Esto facilita editar el HTML/CSS/JS sin tocar el codigo C. Paso a paso:
  1. Crear el directorio `firmware/gateway/web/`.
  2. Crear `firmware/gateway/web/index.html` con la pagina principal del gateway (dashboard basico con titulo, estado, enlaces a configuracion).
  3. Crear `firmware/gateway/web/style.css` con estilos basicos (fuente, colores, layout responsive).
  4. Crear `firmware/gateway/web/config.html` con el formulario de configuracion WiFi (mejorar el HTML de T1.3.2).
  5. En `firmware/gateway/main/CMakeLists.txt`, agregar la directiva `EMBED_FILES` para incluir los archivos web:
     ```
     idf_component_register(
         SRCS "main.c" "nvs_config.c" "wifi_manager.c" "espnow_manager.c" "http_server.c"
         INCLUDE_DIRS "."
         REQUIRES nvs_flash esp_wifi esp_http_server esp_now esp_netif
         EMBED_FILES ../web/index.html ../web/style.css ../web/config.html
     )
     ```
  6. En el codigo C, acceder a los archivos embebidos con las variables externas generadas por CMake:
     ```c
     extern const uint8_t index_html_start[] asm("_binary_index_html_start");
     extern const uint8_t index_html_end[] asm("_binary_index_html_end");
     ```
  7. Modificar los handlers HTTP para usar los archivos embebidos en lugar de strings hardcoded.
- **Archivos a crear/modificar**:
  - `firmware/gateway/web/index.html` (crear)
  - `firmware/gateway/web/style.css` (crear)
  - `firmware/gateway/web/config.html` (crear)
  - `firmware/gateway/main/CMakeLists.txt` (modificar - agregar EMBED_FILES)
  - `firmware/gateway/main/http_server.c` (modificar - usar archivos embebidos en handlers)
- **Criterio de aceptacion**:
  - El proyecto compila sin errores con los archivos embebidos
  - Al navegar a `http://192.168.4.1/` se muestra la pagina index.html con estilos CSS aplicados
  - El HTML/CSS se carga correctamente y se ve bien en un navegador de telefono movil
  - Modificar un archivo .html y recompilar refleja los cambios sin tocar el codigo C
  - El tamanio del binario solo aumenta proporcionalmente al tamanio de los archivos web
- **Dependencias**: T1.3.1 (servidor HTTP basico), T1.3.2 (pagina de configuracion)
- **Pistas**:
  - `EMBED_FILES` convierte cada archivo en un array de bytes en el binario
  - El nombre del simbolo se genera a partir de la ruta relativa: `../web/index.html` -> `_binary_index_html_start`
  - Los puntos en nombres de archivo se reemplazan por guiones bajos: `style.css` -> `style_css`
  - El tamanio del archivo embebido: `size_t len = index_html_end - index_html_start`
  - `httpd_resp_send(req, (const char *)index_html_start, len)` para enviar el contenido
  - Alternativa: `EMBED_TXTFILES` para archivos de texto (agrega null terminator automaticamente)
- **Errores comunes**:
  - Usar rutas incorrectas en EMBED_FILES. Las rutas son relativas al directorio donde esta el CMakeLists.txt (en este caso `firmware/gateway/main/`), no relativas a la raiz del proyecto.
  - Confundir los nombres de simbolos generados. Los caracteres especiales en las rutas (puntos, guiones, barras) se convierten en guiones bajos. Verificar con `nm build/gateway.elf | grep binary` si no funciona.
- **Tiempo estimado**: 2-4 horas

---

### Tarea T1.3.4: Servir archivos estaticos

- **Dificultad**: Basico
- **Descripcion**: Crear handlers HTTP para servir correctamente los archivos estaticos (CSS, JavaScript) con los Content-Type apropiados. Sin el Content-Type correcto, el navegador no interpretara los archivos adecuadamente. Paso a paso:
  1. Crear un handler GET para "/style.css" que:
     a. Configure `Content-Type: text/css` con `httpd_resp_set_type()`.
     b. Envie el contenido del archivo embebido `style.css`.
  2. Si se tiene un archivo JS, crear handler GET para "/app.js" con Content-Type `application/javascript`.
  3. Crear una funcion auxiliar que determine el Content-Type basado en la extension:
     - `.html` -> `text/html`
     - `.css` -> `text/css`
     - `.js` -> `application/javascript`
     - `.json` -> `application/json`
     - `.ico` -> `image/x-icon`
  4. Opcionalmente, crear un handler generico de archivos estaticos que sirva cualquier archivo embebido segun su ruta.
  5. Registrar todos los handlers de archivos estaticos en el servidor.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/http_server.c` (modificar - agregar handlers para archivos estaticos)
- **Criterio de aceptacion**:
  - Al solicitar `http://192.168.4.1/style.css` el navegador recibe Content-Type "text/css"
  - Los estilos CSS se aplican correctamente en la pagina HTML (verificar visualmente)
  - Las DevTools del navegador (pestaña Network) muestran los Content-Type correctos para cada recurso
  - No hay errores 404 para los recursos referenciados desde el HTML
- **Dependencias**: T1.3.3 (archivos embebidos deben estar disponibles)
- **Pistas**:
  - `httpd_resp_set_type(req, "text/css")` debe llamarse ANTES de `httpd_resp_send()`
  - Cada archivo necesita su propio handler registrado con `httpd_register_uri_handler()`
  - El handler puede ser el mismo para todos los archivos, diferenciando por `req->uri`
  - Para un handler generico: comparar `req->uri` con cada ruta conocida usando `strcmp()`
  - Considerar agregar header de cache: `httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600")`
- **Errores comunes**:
  - Olvidar configurar el Content-Type. Sin Content-Type, el navegador asume "text/html" para todo, y el CSS no se aplica porque el navegador espera recibir CSS pero recibe algo sin tipo definido (o con tipo incorrecto).
  - Exceder el limite de URI handlers del servidor HTTP. Por defecto, `HTTPD_DEFAULT_CONFIG()` permite 12 handlers. Si se necesitan mas, incrementar `config.max_uri_handlers` antes de llamar a `httpd_start()`.
- **Tiempo estimado**: 1-2 horas

---

> **Checkpoint 1.3**: Al completar esta sub-tarea, el gateway debe:
> - Servir una pagina web completa con HTML y CSS desde `http://192.168.4.1/`
> - Mostrar un formulario de configuracion WiFi funcional en `/config`
> - Permitir cambiar las credenciales WiFi desde el navegador sin reflashear
> - Los archivos web estan separados del codigo C (en `firmware/gateway/web/`)
> - **Prueba rapida**: Conectarse al AP "Piscifactoria-GW", abrir `http://192.168.4.1/` en el navegador, verificar que la pagina se muestra con estilos. Navegar a `/config`, ingresar credenciales de un router, verificar que el gateway se conecta.

---

## Sub-tarea 1.4: API REST minima

Esta sub-tarea implementa los endpoints JSON que permitiran a la interfaz web (y a clientes externos) consultar y modificar el estado del gateway de forma programatica.

---

### Tarea T1.4.1: GET /api/status - Endpoint de estado del sistema

- **Dificultad**: Intermedio
- **Descripcion**: Crear un endpoint REST que devuelva el estado completo del gateway en formato JSON. Este endpoint es fundamental para el dashboard web y para monitoreo externo. Paso a paso:
  1. En `http_server.c`, crear un handler para GET "/api/status".
  2. Recopilar la informacion del sistema:
     - Uptime en segundos: `esp_timer_get_time() / 1000000`
     - Memoria heap libre: `esp_get_free_heap_size()`
     - Estado WiFi STA: usar `wifi_sta_is_connected()` de T1.2.1
     - IP del STA: `esp_netif_get_ip_info()` sobre la interfaz STA
     - Clientes conectados al AP: `esp_wifi_ap_get_sta_list()`
     - Estado ESP-NOW: variable booleana de `espnow_manager`
     - Hora actual: `time()` y `strftime()` en formato ISO 8601
  3. Construir el JSON usando la libreria cJSON (incluida en ESP-IDF):
     a. Crear el objeto raiz con `cJSON_CreateObject()`.
     b. Agregar cada campo con `cJSON_AddNumberToObject()`, `cJSON_AddStringToObject()`, `cJSON_AddBoolToObject()`.
     c. Serializar con `cJSON_PrintUnformatted()`.
     d. Enviar la respuesta con Content-Type "application/json".
     e. Liberar la memoria de cJSON con `cJSON_Delete()` y `free()` para el string.
  4. Registrar el handler en el servidor.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/http_server.c` (modificar - agregar handler GET /api/status)
  - `firmware/gateway/main/wifi_manager.h` (modificar - exponer funciones de estado si no estan)
  - `firmware/gateway/main/espnow_manager.h` (modificar - exponer funcion `espnow_is_active()`)
  - `firmware/gateway/main/CMakeLists.txt` (modificar - agregar `cjson` a REQUIRES si es necesario)
- **Criterio de aceptacion**:
  - `curl http://192.168.4.1/api/status` devuelve JSON valido (verificar con `jq` o JSON validator online)
  - El JSON contiene todos los campos: uptime, free_heap, wifi_sta_connected, wifi_sta_ip, wifi_ap_clients, espnow_active, time
  - El uptime incrementa con cada peticion
  - El free_heap reporta un valor razonable (>100KB tipicamente)
  - Si SNTP no esta sincronizado, el campo time muestra "not synced" o similar
  - Content-Type es "application/json"
- **Dependencias**: T1.3.1 (servidor HTTP), T1.2.1 (estado WiFi), T1.1.3 (estado ESP-NOW), T1.1.4 (hora SNTP)
- **Pistas**:
  - `#include "cJSON.h"` - cJSON esta incluido en ESP-IDF, no hace falta instalarlo
  - `cJSON *root = cJSON_CreateObject()` crea el objeto raiz
  - `cJSON_AddNumberToObject(root, "uptime", uptime_seconds)`
  - `char *json_str = cJSON_PrintUnformatted(root)` serializa sin espacios (mas compacto)
  - `cJSON_Delete(root)` libera el arbol cJSON
  - `free(json_str)` libera el string generado por cJSON_Print (usa malloc internamente)
  - Para IP como string: `char ip_str[16]; esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str))`
  - Para ISO 8601: `strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo)`
- **Errores comunes**:
  - Olvidar liberar la memoria de cJSON. Tanto el objeto `cJSON *root` (con `cJSON_Delete()`) como el string generado por `cJSON_PrintUnformatted()` (con `free()`) deben liberarse. Si no, se produce un memory leak que eventualmente agota el heap.
  - No configurar Content-Type como "application/json". Sin esto, el navegador no interpreta la respuesta como JSON y las aplicaciones frontend no podran consumir la API correctamente.
- **Tiempo estimado**: 3-4 horas

---

### Tarea T1.4.2: GET /api/nodes - Endpoint de nodos registrados (placeholder)

- **Dificultad**: Basico
- **Descripcion**: Crear un endpoint que devuelva la lista de nodos sensores registrados. En esta fase sera un array vacio o con datos de prueba, pero la estructura debe estar preparada para la Fase 2 donde los nodos reales se registraran via ESP-NOW. Paso a paso:
  1. En `http_server.c`, crear un handler para GET "/api/nodes".
  2. Crear la estructura de datos para nodos. En `espnow_manager.h`:
     ```c
     typedef struct {
         uint8_t mac[6];
         char tipo_sensor[16];
         time_t ultimo_contacto;
         int8_t rssi;
         uint32_t paquetes_recibidos;
     } node_info_t;
     ```
  3. En `espnow_manager.c`, crear un array estatico: `static node_info_t s_nodes[MAX_NODES]` con `#define MAX_NODES 20`.
  4. Crear funciones de acceso: `int espnow_get_node_count(void)` y `const node_info_t *espnow_get_nodes(void)`.
  5. En el handler, construir un JSON array con los nodos registrados (por ahora vacio).
  6. Formato esperado:
     ```json
     {
       "count": 0,
       "max_nodes": 20,
       "nodes": []
     }
     ```
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/http_server.c` (modificar - agregar handler GET /api/nodes)
  - `firmware/gateway/main/espnow_manager.h` (modificar - agregar struct node_info_t y funciones de acceso)
  - `firmware/gateway/main/espnow_manager.c` (modificar - agregar array de nodos y funciones)
- **Criterio de aceptacion**:
  - `curl http://192.168.4.1/api/nodes` devuelve JSON valido con array "nodes" (vacio inicialmente)
  - El JSON incluye "count" (0) y "max_nodes" (20)
  - Content-Type es "application/json"
  - La estructura `node_info_t` esta definida y es accesible desde otros modulos
  - El array de nodos tiene tamanio fijo de 20 elementos
- **Dependencias**: T1.3.1 (servidor HTTP), T1.1.3 (espnow_manager base)
- **Pistas**:
  - `cJSON *nodes_array = cJSON_CreateArray()` para crear el array JSON
  - `cJSON_AddItemToObject(root, "nodes", nodes_array)` para agregar el array al objeto raiz
  - Para formatear MAC como string: `snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5])`
  - El array estatico garantiza que no hay allocaciones dinamicas para la lista de nodos
  - `MAX_NODES 20` es suficiente para una piscifactoria pequena; se puede parametrizar via menuconfig
- **Errores comunes**:
  - Exponer el array directamente como `extern` en lugar de usar funciones de acceso. Esto rompe el encapsulamiento y puede causar condiciones de carrera si multiples tareas acceden simultaneamente.
  - No considerar la concurrencia. El callback ESP-NOW y el handler HTTP pueden acceder al array de nodos simultaneamente. Agregar un mutex aunque sea un placeholder por ahora.
- **Tiempo estimado**: 2-3 horas

---

### Tarea T1.4.3: POST /api/config - Endpoint de configuracion

- **Dificultad**: Intermedio
- **Descripcion**: Crear un endpoint REST que reciba configuracion en formato JSON via POST, permitiendo configurar el gateway desde herramientas externas (curl, Postman, o la propia interfaz web con JavaScript). Paso a paso:
  1. En `http_server.c`, crear un handler para POST "/api/config".
  2. En el handler:
     a. Leer el cuerpo de la peticion con `httpd_req_recv()`.
     b. Verificar que Content-Length no exceda un limite razonable (ej. 1024 bytes).
     c. Parsear el JSON recibido con `cJSON_Parse()`.
     d. Verificar que el JSON es valido (retornar 400 si no lo es).
     e. Buscar el campo "wifi" que contenga "ssid" y "password".
     f. Si existen, guardar en NVS y reconectar STA.
     g. Responder con JSON de confirmacion: `{"status": "ok", "message": "Configuracion actualizada"}`.
     h. Si hay error, responder con `{"status": "error", "message": "Descripcion del error"}`.
  3. Formato de entrada esperado:
     ```json
     {
       "wifi": {
         "ssid": "MiRedWiFi",
         "password": "MiPassword123"
       }
     }
     ```
  4. Validar los datos: SSID no vacio y longitud maxima 32 chars, password longitud entre 8-64 chars (o vacia para red abierta).
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/http_server.c` (modificar - agregar handler POST /api/config)
- **Criterio de aceptacion**:
  - `curl -X POST -H "Content-Type: application/json" -d '{"wifi":{"ssid":"test","password":"12345678"}}' http://192.168.4.1/api/config` retorna JSON con status "ok"
  - Con JSON invalido, retorna HTTP 400 con mensaje de error descriptivo
  - Con SSID vacio, retorna error de validacion
  - Las credenciales se guardan en NVS y el STA reconecta
  - El endpoint no crashea con input malformado, vacio o excesivamente grande
- **Dependencias**: T1.3.1 (servidor HTTP), T1.2.3 (NVS credenciales WiFi), T1.2.1 (reconexion WiFi)
- **Pistas**:
  - `cJSON *json = cJSON_Parse(buffer)` parsea el string JSON; retorna NULL si es invalido
  - `cJSON *wifi = cJSON_GetObjectItem(json, "wifi")` obtiene un campo del objeto
  - `cJSON_GetObjectItem(wifi, "ssid")->valuestring` obtiene el valor string
  - Siempre verificar que `cJSON_GetObjectItem()` no retorne NULL antes de acceder a `valuestring`
  - `httpd_resp_set_status(req, "400 Bad Request")` para errores de validacion
  - `httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN)` para enviar la respuesta
  - Buffer para el body: declarar en el stack con tamanio fijo, ej. `char buf[1024]`
- **Errores comunes**:
  - No verificar que cada campo cJSON exista antes de acceder a su valor. Si el JSON no contiene "wifi" y se intenta `cJSON_GetObjectItem(json, "wifi")->valuestring`, se produce un null pointer dereference y el sistema crashea.
  - No liberar el objeto cJSON parseado con `cJSON_Delete(json)` despues de usarlo. cJSON usa malloc internamente, y sin liberacion se pierde memoria en cada peticion POST.
- **Tiempo estimado**: 3-4 horas

---

> **Checkpoint 1.4**: Al completar esta sub-tarea, el gateway debe:
> - Responder a `GET /api/status` con JSON conteniendo uptime, heap, estado WiFi, hora, etc.
> - Responder a `GET /api/nodes` con JSON conteniendo la estructura de nodos (vacia por ahora)
> - Aceptar `POST /api/config` con JSON para cambiar credenciales WiFi
> - Manejar correctamente errores de parseo JSON (400 Bad Request)
> - **Prueba rapida**: Ejecutar `curl http://192.168.4.1/api/status | jq .` y verificar que todos los campos estan presentes. Ejecutar `curl -X POST -H "Content-Type: application/json" -d '{"wifi":{"ssid":"MiRed","password":"MiClave123"}}' http://192.168.4.1/api/config` y verificar la reconexion.

---

## Sub-tarea 1.5: Recepcion ESP-NOW y registro de nodos

Esta sub-tarea implementa la logica para recibir datos via ESP-NOW y mantener un registro de los nodos sensores que se comunican con el gateway.

---

### Tarea T1.5.1: Callback de recepcion ESP-NOW

- **Dificultad**: Basico
- **Descripcion**: Implementar el callback de recepcion ESP-NOW que se ejecuta cada vez que el gateway recibe un paquete de un nodo sensor. Este callback debe logear la informacion del paquete para depuracion. Paso a paso:
  1. En `espnow_manager.c`, implementar la funcion de callback registrada en T1.1.3:
     ```c
     static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
     ```
  2. En el callback:
     a. Logear la MAC del remitente con `MAC2STR()`.
     b. Logear el tamanio del paquete.
     c. Logear el RSSI si esta disponible: `recv_info->rx_ctrl->rssi`.
     d. Hacer un hex dump de los primeros bytes del paquete (maximo 32 bytes) para depuracion.
     e. Logear un contador de paquetes recibidos totales.
  3. Para el hex dump, crear una funcion auxiliar que convierta bytes a string hexadecimal:
     ```c
     static void hex_dump(const uint8_t *data, int len, char *out, int out_len);
     ```
  4. Probar enviando un paquete ESP-NOW desde otro ESP32 (el ESP32-C3 de pruebas).
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/espnow_manager.c` (modificar - implementar callback con logging)
- **Criterio de aceptacion**:
  - Al recibir un paquete ESP-NOW, el log muestra: MAC del remitente, tamanio del paquete y RSSI
  - El hex dump muestra los datos del paquete en formato legible
  - El contador de paquetes incrementa con cada recepcion
  - El callback no bloquea el sistema (ejecuta rapido y retorna)
  - No hay crashes al recibir paquetes de cualquier tamanio (0-250 bytes)
- **Dependencias**: T1.1.3 (ESP-NOW inicializado)
- **Pistas**:
  - El callback se ejecuta en el contexto de la WiFi task, debe ser rapido (no usar delays ni operaciones lentas)
  - `MACSTR` es `"%02x:%02x:%02x:%02x:%02x:%02x"` y `MAC2STR(mac)` expande a los 6 bytes
  - `recv_info->src_addr` contiene la MAC del remitente (6 bytes)
  - `recv_info->rx_ctrl` contiene informacion de control RF, incluyendo RSSI
  - Para hex dump: `snprintf(out + i*3, 4, "%02x ", data[i])` para cada byte
  - Limite ESP-NOW: maximo 250 bytes por paquete, pero data_len puede ser menor
  - Usar `ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, data_len, ESP_LOG_INFO)` como alternativa para hex dump
- **Errores comunes**:
  - Realizar operaciones lentas (como escribir en flash o enviar HTTP) dentro del callback. Esto bloquea la WiFi task y puede causar perdida de paquetes o watchdog timeout. Si se necesita procesar los datos, copiarlos a una cola (xQueue) y procesarlos en otra tarea.
  - Acceder a `recv_info->rx_ctrl` sin verificar que no sea NULL. En algunas versiones de ESP-IDF, este campo puede no estar disponible.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T1.5.2: Registro de nodos en memoria

- **Dificultad**: Intermedio
- **Descripcion**: Implementar la logica para mantener un registro en memoria de los nodos sensores que se comunican con el gateway. Cuando llega un paquete ESP-NOW, el sistema debe buscar si el nodo ya esta registrado (por MAC) y actualizar su informacion, o registrarlo si es nuevo. Paso a paso:
  1. En `espnow_manager.c`, usar la estructura `node_info_t` definida en T1.4.2:
     - Agregar campo `bool activo` para marcar nodos en uso.
     - Inicializar todo el array a ceros con `memset()` al inicio.
  2. Crear funcion `static node_info_t *find_node_by_mac(const uint8_t *mac)`:
     - Recorrer el array buscando un nodo cuya MAC coincida.
     - Usar `memcmp(node->mac, mac, 6)` para comparar MACs.
     - Retornar puntero al nodo o NULL si no se encuentra.
  3. Crear funcion `static node_info_t *register_node(const uint8_t *mac)`:
     - Si el nodo ya existe (find_node_by_mac), retornar el existente.
     - Si no existe, buscar la primera posicion libre (activo == false) en el array.
     - Si no hay espacio, logear warning y retornar NULL.
     - Copiar la MAC, marcar como activo, inicializar campos.
  4. Modificar el callback ESP-NOW para:
     a. Buscar o registrar el nodo por MAC.
     b. Actualizar `ultimo_contacto` con `time(NULL)`.
     c. Actualizar `rssi` con el RSSI del paquete.
     d. Incrementar `paquetes_recibidos`.
  5. Agregar un mutex (`SemaphoreHandle_t`) para proteger el acceso al array de nodos desde multiples contextos (callback ESP-NOW y handler HTTP).
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/espnow_manager.c` (modificar - agregar logica de registro de nodos)
  - `firmware/gateway/main/espnow_manager.h` (modificar - actualizar struct si es necesario, agregar funciones de acceso thread-safe)
- **Criterio de aceptacion**:
  - Al recibir el primer paquete de un nodo nuevo, el log muestra "Nuevo nodo registrado: XX:XX:XX:XX:XX:XX"
  - Al recibir paquetes posteriores del mismo nodo, solo se actualizan los campos (sin duplicar)
  - `espnow_get_node_count()` retorna el numero correcto de nodos registrados
  - Si se alcanzan 20 nodos, el siguiente nodo nuevo genera un warning sin crashear
  - El ultimo_contacto se actualiza con cada paquete recibido
  - No hay condiciones de carrera al acceder al array desde diferentes contextos
- **Dependencias**: T1.5.1 (callback ESP-NOW funcional), T1.4.2 (struct node_info_t definida)
- **Pistas**:
  - `#include "freertos/semphr.h"` para el mutex
  - `SemaphoreHandle_t s_nodes_mutex = xSemaphoreCreateMutex()` en init
  - `xSemaphoreTake(s_nodes_mutex, portMAX_DELAY)` antes de acceder al array
  - `xSemaphoreGive(s_nodes_mutex)` despues de terminar
  - `memcmp(a, b, 6) == 0` retorna true si las MACs son iguales
  - `time(NULL)` retorna el timestamp UNIX actual (requiere SNTP sincronizado para hora real)
  - Considerar no usar `portMAX_DELAY` en el callback ESP-NOW para no bloquear la WiFi task
- **Errores comunes**:
  - No proteger el acceso al array con mutex. El callback ESP-NOW (ejecutado en la WiFi task) y el handler HTTP (ejecutado en la HTTP task) pueden acceder simultaneamente al array, causando corrupcion de datos.
  - Olvidar usar `memcmp()` en lugar de `==` para comparar arrays de MAC. El operador `==` compara punteros, no contenidos, y siempre sera false.
- **Tiempo estimado**: 3-4 horas

---

### Tarea T1.5.3: Actualizar GET /api/nodes

- **Dificultad**: Basico
- **Descripcion**: Modificar el endpoint GET /api/nodes para que devuelva la lista real de nodos registrados con toda su informacion, en lugar del array vacio del placeholder. Paso a paso:
  1. En `http_server.c`, modificar el handler de GET "/api/nodes".
  2. Obtener la lista de nodos con `espnow_get_nodes()` y el conteo con `espnow_get_node_count()`.
  3. Para cada nodo activo, crear un objeto JSON con:
     - `mac`: string formateada "XX:XX:XX:XX:XX:XX"
     - `tipo_sensor`: string con el tipo (por ahora "desconocido" hasta Fase 2)
     - `ultimo_contacto`: timestamp UNIX o string ISO 8601
     - `rssi`: numero entero
     - `paquetes_recibidos`: numero entero
     - `activo`: boolean (tiempo desde ultimo contacto < 5 minutos)
  4. Agregar cada objeto al array JSON.
  5. Incluir metadatos: "count", "max_nodes" y opcionalmente "last_update".
  6. Asegurar que el acceso a los nodos es thread-safe (usar las funciones de acceso con mutex de T1.5.2).
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/http_server.c` (modificar - actualizar handler GET /api/nodes)
- **Criterio de aceptacion**:
  - Despues de recibir paquetes ESP-NOW de nodos, `GET /api/nodes` muestra los nodos registrados
  - Cada nodo tiene todos los campos: mac, tipo_sensor, ultimo_contacto, rssi, paquetes_recibidos
  - El campo "count" coincide con el numero de objetos en el array "nodes"
  - La MAC se muestra en formato legible (XX:XX:XX:XX:XX:XX)
  - Si no hay nodos, el array esta vacio y count es 0 (mismo comportamiento que T1.4.2)
  - No hay crashes ni memory leaks al consultar el endpoint repetidamente
- **Dependencias**: T1.4.2 (endpoint placeholder), T1.5.2 (registro de nodos funcional)
- **Pistas**:
  - Iterar sobre el array de nodos y solo incluir los que tengan `activo == true`
  - `cJSON *node_obj = cJSON_CreateObject()` para cada nodo
  - `cJSON_AddItemToArray(nodes_array, node_obj)` para agregar al array
  - `snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(node->mac))` para formatear la MAC
  - `cJSON_AddNumberToObject(node_obj, "rssi", node->rssi)` para campos numericos
  - Considerar agregar un campo "tiempo_sin_contacto" calculado como `difftime(time(NULL), node->ultimo_contacto)`
  - Liberar toda la memoria cJSON al final del handler
- **Errores comunes**:
  - Acceder directamente al array de nodos sin pasar por las funciones thread-safe. Aunque funcione la mayoria del tiempo, puede causar lecturas inconsistentes si un paquete ESP-NOW llega justo mientras se esta generando el JSON.
  - Crear objetos cJSON dentro del bucle sin verificar que `cJSON_CreateObject()` no retorne NULL. Si el heap se agota, la funcion retorna NULL y agregar campos a un NULL crashea el sistema.
- **Tiempo estimado**: 2-3 horas

---

> **Checkpoint 1.5**: Al completar esta sub-tarea, el gateway debe:
> - Recibir paquetes ESP-NOW y mostrar informacion detallada en el log
> - Registrar automaticamente nuevos nodos por MAC address
> - Mantener estadisticas por nodo (RSSI, paquetes, ultimo contacto)
> - Exponer la lista de nodos real via GET /api/nodes
> - Manejar hasta 20 nodos simultaneos sin problemas
> - **Prueba rapida**: Desde un ESP32-C3, enviar paquetes ESP-NOW al gateway. Verificar que aparece en el log. Luego consultar `curl http://192.168.4.1/api/nodes | jq .` y verificar que el nodo aparece con su MAC, RSSI y contador de paquetes.

---

## Sub-tarea 1.6: Logging y watchdog

Esta sub-tarea mejora la observabilidad y confiabilidad del gateway, implementando logging estructurado y un watchdog que reinicie el sistema si alguna tarea se bloquea.

---

### Tarea T1.6.1: Sistema de logging estructurado

- **Dificultad**: Basico
- **Descripcion**: Organizar y estandarizar el logging del gateway para facilitar la depuracion y el monitoreo. Cada modulo debe tener su propio TAG y el nivel de log debe ser configurable. Paso a paso:
  1. Verificar que cada archivo .c tiene su propio TAG definido:
     - `main.c`: `static const char *TAG = "MAIN";`
     - `wifi_manager.c`: `static const char *TAG = "WIFI";`
     - `espnow_manager.c`: `static const char *TAG = "ESPNOW";`
     - `http_server.c`: `static const char *TAG = "HTTP";`
     - `nvs_config.c`: `static const char *TAG = "NVS";`
  2. Configurar niveles de log por defecto en `menuconfig`:
     - Component config -> Log output -> Default log verbosity: INFO
  3. Crear un modulo auxiliar `firmware/gateway/main/log_config.c/.h` con:
     - Funcion `void log_config_init(void)` que configure niveles iniciales.
     - Funcion `void log_set_module_level(const char *tag, esp_log_level_t level)` wrapper de `esp_log_level_set()`.
     - Funcion `void log_set_all_verbose(void)` para modo debug.
     - Funcion `void log_set_all_minimal(void)` para produccion (solo WARN y ERROR).
  4. Agregar un comando por consola serial o endpoint API para cambiar niveles en runtime (opcional, pero recomendado).
  5. Asegurar que todos los logs usan los niveles correctos:
     - `ESP_LOGE`: Errores criticos
     - `ESP_LOGW`: Advertencias
     - `ESP_LOGI`: Informacion operativa
     - `ESP_LOGD`: Depuracion detallada
     - `ESP_LOGV`: Verbose (tracing)
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/log_config.h` (crear)
  - `firmware/gateway/main/log_config.c` (crear)
  - `firmware/gateway/main/main.c` (modificar - llamar a log_config_init al inicio)
  - `firmware/gateway/main/CMakeLists.txt` (modificar - agregar log_config.c a SRCS)
  - Todos los archivos .c existentes (verificar TAGs y niveles de log)
- **Criterio de aceptacion**:
  - Cada modulo usa su propio TAG en los mensajes de log
  - El log serial muestra mensajes con formato: `I (timestamp) WIFI: Conectado a MiRed`
  - Se puede cambiar el nivel de log de un modulo en runtime sin recompilar
  - En modo "minimal", solo se ven errores y advertencias (no info ni debug)
  - En modo "verbose", se ve toda la informacion detallada de depuracion
- **Dependencias**: Todas las tareas previas (todos los modulos deben existir)
- **Pistas**:
  - `#include "esp_log.h"` para todas las funciones de logging
  - `esp_log_level_set("WIFI", ESP_LOG_DEBUG)` para cambiar nivel de un TAG especifico
  - `esp_log_level_set("*", ESP_LOG_WARN)` para cambiar todos los TAGs
  - Los niveles son: `ESP_LOG_NONE`, `ESP_LOG_ERROR`, `ESP_LOG_WARN`, `ESP_LOG_INFO`, `ESP_LOG_DEBUG`, `ESP_LOG_VERBOSE`
  - El nivel configurado en menuconfig es el maximo compilado; no se puede subir mas alla en runtime
  - Para que ESP_LOGD y ESP_LOGV funcionen, el nivel maximo compilado debe ser al menos DEBUG o VERBOSE
- **Errores comunes**:
  - Configurar el nivel maximo de compilacion en WARN en menuconfig y luego intentar activar DEBUG en runtime. Los mensajes de DEBUG ni siquiera se compilan en el binario, asi que `esp_log_level_set()` no puede activarlos. Configurar el nivel de compilacion en VERBOSE para tener maxima flexibilidad en runtime.
  - Usar el mismo TAG en multiples archivos. Si dos modulos usan `TAG = "MAIN"`, no se puede diferenciar el origen de los mensajes ni configurar niveles independientemente.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T1.6.2: Configurar task watchdog

- **Dificultad**: Intermedio
- **Descripcion**: Configurar el Task Watchdog Timer (TWDT) para detectar y recuperarse automaticamente de bloqueos en el sistema. En un entorno de produccion (piscifactoria), el gateway debe ser capaz de reiniciarse si alguna tarea deja de responder. Paso a paso:
  1. En `main.c`, despues de todas las inicializaciones:
     a. Configurar el TWDT con `esp_task_wdt_config_t`:
        ```c
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = 30000,       // 30 segundos
            .idle_core_mask = (1 << 0) | (1 << 1),  // Ambos cores en S3
            .trigger_panic = true       // Reiniciar el sistema si el WDT dispara
        };
        esp_task_wdt_reconfigure(&wdt_config);
        ```
     b. Suscribir la tarea principal al TWDT: `esp_task_wdt_add(NULL)` (NULL = tarea actual).
  2. Crear un bucle principal en `app_main()` que:
     a. Alimente el watchdog: `esp_task_wdt_reset()`.
     b. Realice tareas periodicas (ej. verificar salud del sistema, logear estadisticas).
     c. Espere un tiempo razonable: `vTaskDelay(pdMS_TO_TICKS(10000))` (cada 10 segundos).
  3. Asegurar que el timeout del WDT (30s) es mayor que el delay del loop (10s), dejando margen.
  4. Opcionalmente, suscribir otras tareas criticas al WDT si se crean tareas separadas.
  5. Probar el WDT insertando un `while(1) {}` temporal (sin delay) y verificando que el sistema se reinicia.
- **Archivos a crear/modificar**:
  - `firmware/gateway/main/main.c` (modificar - agregar configuracion TWDT y bucle principal)
  - `firmware/gateway/main/CMakeLists.txt` (modificar - agregar `esp_system` a REQUIRES si falta)
- **Criterio de aceptacion**:
  - El sistema se reinicia automaticamente si una tarea se bloquea durante mas de 30 segundos
  - En operacion normal, el WDT nunca dispara (se alimenta cada 10 segundos)
  - El log muestra periodicamente informacion de salud del sistema (heap libre, uptime)
  - Al insertar un `while(1){}` de prueba, el log muestra "Task watchdog got triggered" y el sistema reinicia
  - Despues del reinicio por WDT, el sistema arranca normalmente
- **Dependencias**: Todas las tareas previas (el WDT debe configurarse al final de la inicializacion)
- **Pistas**:
  - `#include "esp_task_wdt.h"` para funciones del watchdog
  - En ESP-IDF v5.x, usar `esp_task_wdt_reconfigure()` en lugar del deprecated `esp_task_wdt_init()`
  - `esp_task_wdt_add(NULL)` suscribe la tarea actual; `esp_task_wdt_add(task_handle)` suscribe otra tarea
  - `esp_task_wdt_reset()` alimenta el WDT para la tarea actual
  - Si `trigger_panic = true`, el WDT causa un reinicio; si `false`, solo imprime un warning
  - El ESP32-S3 tiene 2 cores; configurar `idle_core_mask` para monitorear ambos
  - El WDT se alimenta automaticamente para las tareas IDLE si se configuran en idle_core_mask
- **Errores comunes**:
  - Configurar un timeout de WDT demasiado corto (ej. 5 segundos) y tener operaciones legitimas que tardan mas (como la sincronizacion SNTP o la reconexion WiFi con backoff). Esto causa reinicios innecesarios. 30 segundos es un buen valor inicial.
  - Olvidar alimentar el WDT en TODAS las ramas del bucle principal. Si hay un `if/else` donde una rama no llama a `esp_task_wdt_reset()`, el WDT disparara en esa condicion.
- **Tiempo estimado**: 2-3 horas

---

> **Checkpoint 1.6**: Al completar esta sub-tarea, el gateway debe:
> - Tener logging estructurado con TAGs por modulo y niveles configurables
> - Poderse cambiar el nivel de verbosidad de los logs en runtime
> - Reiniciarse automaticamente si alguna tarea se bloquea por mas de 30 segundos
> - Mostrar informacion periodica de salud del sistema (heap, uptime) en el log
> - **Prueba rapida**: Cambiar el nivel de log de "WIFI" a DEBUG y verificar mas detalle. Insertar temporalmente un `while(1){}` en el loop principal y verificar que el WDT reinicia el sistema tras ~30 segundos.

---

## Preguntas de autoevaluacion

Responde estas preguntas para verificar tu comprension de los conceptos implementados en esta fase. Si no puedes responder alguna con confianza, revisa la tarea correspondiente.

1. **Orden de inicializacion**: ¿Por que es critico inicializar NVS antes que WiFi, y WiFi antes que ESP-NOW? ¿Que errores ocurririan si se cambia el orden?

2. **Modo APSTA**: ¿Que sucede con el canal WiFi del AP cuando el STA se conecta a un router en un canal diferente? ¿Como afecta esto a la comunicacion ESP-NOW con los nodos?

3. **Programacion basada en eventos**: ¿Por que no se deben realizar operaciones largas (como escritura a flash o HTTP requests) dentro de un event handler de WiFi? ¿Que alternativas hay?

4. **NVS**: ¿Cual es la diferencia entre `nvs_flash_init()` y `nvs_open()`? ¿Por que se necesitan ambas? ¿Que pasa si no se llama a `nvs_commit()` despues de escribir?

5. **Servidor HTTP**: ¿Cual es el limite por defecto de URI handlers en `HTTPD_DEFAULT_CONFIG()`? ¿Que sucede si se excede este limite? ¿Como se incrementa?

6. **cJSON y memoria**: Si se tiene un endpoint API que se llama 100 veces por minuto, y en cada llamada se olvida `cJSON_Delete()`, ¿cuanto tiempo tardaria en agotarse el heap del ESP32-S3 (512KB SRAM)? Calcular aproximadamente.

7. **Watchdog**: ¿Cual es la diferencia entre el Interrupt Watchdog Timer (IWDT) y el Task Watchdog Timer (TWDT)? ¿Cuando conviene usar cada uno?

8. **Backoff exponencial**: ¿Por que es mejor usar backoff exponencial en lugar de reintentar inmediatamente? ¿Que problemas podria causar un reintento constante cada 100ms en un sistema con WiFi y ESP-NOW activos?

---

## Lectura recomendada

Documentacion oficial de ESP-IDF v5.x (consultar la version exacta que se este usando):

- **WiFi**: [ESP-IDF WiFi Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html) - Configuracion APSTA, eventos WiFi, channel management.
- **Servidor HTTP**: [ESP HTTP Server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/protocols/esp_http_server.html) - Handlers, URI matching, send/receive.
- **ESP-NOW**: [ESP-NOW Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_now.html) - Inicializacion, callbacks, limitaciones de tamanio de paquete y canal.
- **NVS**: [Non-Volatile Storage](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html) - Namespaces, tipos de datos, particiones, errores comunes.
- **SNTP**: [SNTP Time Synchronization](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/system_time.html) - Configuracion, timezone, sync modes.
- **Task Watchdog**: [Watchdog Timers](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html) - TWDT vs IWDT, configuracion, panic handler.
- **FreeRTOS en ESP-IDF**: [FreeRTOS Overview](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html) - Tasks, semaphores, timers, queues.
- **cJSON**: [cJSON en ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/cjson.html) - Creacion, parseo, serializacion, gestion de memoria.

---

## Errores frecuentes de esta fase

Lista recopilatoria de los errores mas comunes que suelen cometer los programadores juniors en esta fase. Revisarla antes de empezar y consultarla cuando algo no funcione:

1. **Orden de inicializacion incorrecto**: NVS debe ir ANTES de WiFi, y WiFi ANTES de ESP-NOW. Cambiar el orden causa crashes cripticos. El orden correcto en `app_main()` es: NVS -> WiFi -> ESP-NOW -> HTTP Server -> SNTP -> Watchdog.

2. **Canal WiFi y ESP-NOW desincronizados**: ESP-NOW usa el canal WiFi activo. Si el STA se conecta a un router en canal 6, el AP y ESP-NOW automaticamente cambian al canal 6. Los nodos ESP-NOW deben estar en el mismo canal. Si los nodos no reciben, verificar `esp_wifi_get_channel()` en ambos lados.

3. **Gestion de memoria cJSON**: Cada `cJSON_CreateObject()`, `cJSON_CreateArray()`, etc., reserva memoria con malloc. Se DEBE liberar con `cJSON_Delete()` en el objeto raiz. Ademas, `cJSON_Print()` y `cJSON_PrintUnformatted()` reservan memoria adicional con malloc que se debe liberar con `free()`. Olvidar cualquiera de estas liberaciones causa memory leaks.

4. **Limite de URI handlers HTTP**: `HTTPD_DEFAULT_CONFIG()` permite solo 12 URI handlers. Con los endpoints de esta fase (/, /config, /config/wifi, /style.css, /app.js, /api/status, /api/nodes, /api/config) ya se usan 8. Si en futuras fases se necesitan mas, incrementar `config.max_uri_handlers` ANTES de llamar a `httpd_start()`.

5. **Bloquear event handlers**: Los event handlers de WiFi y ESP-NOW se ejecutan en tasks del sistema. Si se realizan operaciones lentas (escritura flash, HTTP, delays largos) dentro de ellos, se bloquean otros eventos y el sistema puede volverse inestable. Usar colas (xQueue) para delegar trabajo a otras tareas.

6. **NVS sin commit**: Despues de `nvs_set_str()` o cualquier escritura, se DEBE llamar a `nvs_commit()`. Sin esto, los datos quedan en cache RAM y se pierden al reiniciar. Tambien se debe cerrar el handle con `nvs_close()` en todas las rutas de ejecucion, incluidas las de error.

7. **Buffer overflow en HTTP POST**: Al leer el body de un POST con `httpd_req_recv()`, siempre verificar `req->content_len` y limitar el tamanio del buffer. Un cliente malicioso podria enviar un body enorme. Siempre agregar null terminator al buffer leido antes de pasarlo a `cJSON_Parse()`.

8. **Watchdog timeout demasiado corto**: Si el WDT timeout es menor que el delay mas largo del sistema (ej. backoff de reconexion WiFi de 60s), el WDT disparara durante operaciones normales. Configurar el timeout mayor que la operacion mas larga, o alimentar el WDT dentro de los bucles de espera.

9. **Comparar MACs con ==**: En C, `mac1 == mac2` compara punteros, no contenidos. Siempre usar `memcmp(mac1, mac2, 6) == 0` para comparar direcciones MAC.

10. **Content-Type incorrecto en API REST**: Sin `httpd_resp_set_type(req, "application/json")`, el servidor envia "text/html" por defecto. Esto hace que los clientes frontend no puedan consumir la API correctamente y herramientas como `fetch()` en JavaScript no interpreten la respuesta como JSON.
