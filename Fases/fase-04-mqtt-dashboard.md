# Fase 4: Integracion MQTT + Dashboard Web

> **CORRECCIONES APLICADAS** (2026-05-23): QoS corregido (QoS 1 para datos, QoS 2 solo para comandos). Tabla sensor_readings incluye gateway_id. Ver detalles en `docs/replanificacion/03-fases-corregidas.md`.

## Resumen general

- **Objetivo**: Conectar el gateway ESP32-S3 al mundo exterior mediante el protocolo MQTT, crear un backend servidor con API REST y WebSocket, y un dashboard web con visualizacion en tiempo real de los datos de la piscifactoria.
- **Duracion**: 4 semanas
- **Prerequisitos**: Fases 0, 1, 2 y 3 completadas (gateway con WiFi, ESP-NOW funcionando, sensores leyendo datos, alertas configuradas)
- **Hardware necesario**: ESP32-S3 gateway, ESP32-C3 nodos con sensores, Raspberry Pi o PC para servidor (broker + backend + frontend)

## Dependencias externas

| Componente | Version minima | Proposito |
|---|---|---|
| Mosquitto | 2.0+ | Broker MQTT |
| Bun | 1.0+ | Runtime para backend TypeScript |
| Node.js (alternativa) | 18+ | Runtime alternativo si no se usa Bun |
| npm/bun | - | Gestor de paquetes para dependencias |
| SQLite | 3.x | Base de datos local para historicos |
| Vite | 5.x | Bundler para frontend React |
| React | 18+ | Framework frontend |

## Arquitectura general de la Fase 4

```
┌─────────────┐    ESP-NOW    ┌──────────────┐     MQTT      ┌────────────┐
│  Nodo C3    │──────────────>│  Gateway S3  │──────────────>│  Mosquitto │
│  (sensores) │               │  (mqtt_bridge)│<──────────────│  Broker    │
└─────────────┘               └──────────────┘               └─────┬──────┘
                                                                   │
                                                                   │ MQTT
                                                                   v
                                                            ┌──────────────┐
                                                            │  Backend Bun │
                                                            │  (mqtt.ts)   │
                                                            │  (api.ts)    │
                                                            │  (ws.ts)     │
                                                            └──────┬───────┘
                                                                   │
                                                          REST + WebSocket
                                                                   │
                                                                   v
                                                            ┌──────────────┐
                                                            │  Dashboard   │
                                                            │  React+Vite  │
                                                            └──────────────┘
```

---

## Sub-tarea 4.1: MQTT Bridge en el Gateway (4 tareas)

### Tarea T4.1.1: Inicializar cliente MQTT en el gateway

- **Dificultad**: Intermedio
- **Descripcion**: Crear el modulo `mqtt_bridge` que inicializa un cliente MQTT en el ESP32-S3 gateway. Paso a paso:
  1. Crear los archivos `mqtt_bridge.c` y `mqtt_bridge.h` en el directorio de componentes del gateway.
  2. Incluir la cabecera `mqtt_client.h` de ESP-IDF.
  3. Definir una estructura de configuracion que contenga: broker URL (ej. `mqtt://192.168.1.100`), puerto (por defecto 1883), usuario y password.
  4. Leer estos parametros desde NVS (Non-Volatile Storage) al arrancar, usando las funciones `nvs_get_str()` ya implementadas en fases anteriores.
  5. Crear la funcion `mqtt_bridge_init()` que configure `esp_mqtt_client_config_t` con los parametros leidos de NVS.
  6. Registrar un handler de eventos con `esp_mqtt_client_register_event()` para los eventos: `MQTT_EVENT_CONNECTED`, `MQTT_EVENT_DISCONNECTED`, `MQTT_EVENT_DATA`, `MQTT_EVENT_ERROR`.
  7. En el handler de `MQTT_EVENT_CONNECTED`, loguear la conexion exitosa y marcar un flag global `mqtt_connected = true`.
  8. En el handler de `MQTT_EVENT_DISCONNECTED`, marcar `mqtt_connected = false` y loguear el motivo.
  9. Llamar a `esp_mqtt_client_start()` para iniciar la conexion.
  10. Llamar a `mqtt_bridge_init()` desde `app_main()` despues de que el WiFi este conectado.
- **Archivos a crear/modificar**:
  - Crear: `components/mqtt_bridge/mqtt_bridge.c`
  - Crear: `components/mqtt_bridge/mqtt_bridge.h`
  - Crear: `components/mqtt_bridge/CMakeLists.txt`
  - Modificar: `main/app_main.c` (llamar a `mqtt_bridge_init()`)
  - Modificar: `main/CMakeLists.txt` (agregar dependencia de `mqtt_bridge`)
- **Criterio de aceptacion**:
  - El gateway se conecta al broker Mosquitto al arrancar.
  - En el monitor serial aparece un log: `MQTT_BRIDGE: Conectado al broker mqtt://X.X.X.X:1883`.
  - Si el broker se cae, aparece el log de desconexion y el gateway intenta reconectar automaticamente.
  - Los parametros del broker se leen correctamente desde NVS.
- **Dependencias**: Fase 1 (WiFi funcionando), Fase 2 (NVS configurado)
- **Pistas**:
  - Componente ESP-IDF: `esp_mqtt` (habilitar en `menuconfig` -> Component config -> ESP-MQTT)
  - La reconexion automatica ya viene incluida en `esp_mqtt_client` por defecto.
  - Usar `ESP_LOGI(TAG, ...)` para todos los logs con tag `"MQTT_BRIDGE"`.
  - Documentacion: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/mqtt.html
- **Errores comunes**:
  - Intentar iniciar MQTT antes de que WiFi este conectado. Siempre esperar al evento `WIFI_EVENT_STA_CONNECTED` o `IP_EVENT_STA_GOT_IP` antes de llamar a `mqtt_bridge_init()`.
  - No incluir `esp_mqtt` en las dependencias del `CMakeLists.txt` del componente, lo que causa errores de enlazado.
- **Tiempo estimado**: 3-4 horas

---

### Tarea T4.1.2: Publicar datos de sensores en topics MQTT

- **Dificultad**: Intermedio
- **Descripcion**: Cuando el gateway recibe un dato de un nodo sensor via ESP-NOW, publicarlo en el broker MQTT con un topic jerarquico. Paso a paso:
  1. Definir la estructura de topics MQTT. Formato: `piscifactoria/{gateway_id}/nodo/{nodo_mac}/{tipo_sensor}`. Ejemplo: `piscifactoria/GW01/nodo/AA:BB:CC:DD:EE:FF/temperatura`.
  2. Crear la funcion `mqtt_bridge_publish_sensor_data()` que reciba: MAC del nodo, tipo de sensor (temperatura, ph, oxigeno_disuelto), valor, unidad.
  3. Construir el payload JSON con los campos: `value` (float), `unit` (string), `timestamp` (epoch en segundos obtenido con `time()`), `node_id` (MAC del nodo).
  4. Usar `cJSON` para construir el JSON (ya disponible en ESP-IDF).
  5. Publicar con `esp_mqtt_client_publish()` usando QoS 1 (at least once) y retain = 0.
  6. Verificar que `mqtt_connected == true` antes de intentar publicar. Si no esta conectado, loguear una advertencia y descartar el mensaje (o encolarlo si quieres un nivel extra).
  7. En el callback de recepcion ESP-NOW (donde ya se procesan los datos de sensores), llamar a `mqtt_bridge_publish_sensor_data()` con los datos parseados.
  8. Probar con `mosquitto_sub -h localhost -t "piscifactoria/#" -v` para ver los mensajes publicados.
- **Archivos a crear/modificar**:
  - Modificar: `components/mqtt_bridge/mqtt_bridge.c` (agregar funcion de publicacion)
  - Modificar: `components/mqtt_bridge/mqtt_bridge.h` (declarar la funcion publica)
  - Modificar: `main/espnow_handler.c` (o equivalente, llamar a la funcion de publicacion al recibir datos)
- **Criterio de aceptacion**:
  - Cada lectura de sensor de cada nodo genera un mensaje MQTT visible con `mosquitto_sub`.
  - El payload JSON es valido y contiene los 4 campos requeridos (value, unit, timestamp, node_id).
  - El topic sigue la jerarquia definida (verificable con `mosquitto_sub -t "piscifactoria/+/nodo/+/+" -v`).
  - Se usa QoS 1 (verificable en logs de Mosquitto con log_type all).
- **Dependencias**: T4.1.1, Fase 2 (recepcion ESP-NOW), Fase 3 (sensores leyendo datos)
- **Pistas**:
  - Para el gateway_id, puedes usar los ultimos 4 caracteres de la MAC del gateway o un nombre configurable desde NVS.
  - `cJSON_CreateObject()`, `cJSON_AddNumberToObject()`, `cJSON_AddStringToObject()`, `cJSON_PrintUnformatted()` son las funciones clave de cJSON.
  - No olvides llamar a `cJSON_Delete()` y `free()` para liberar la memoria del JSON generado.
  - El timestamp se obtiene con `time(NULL)` si el reloj SNTP esta sincronizado.
- **Errores comunes**:
  - Memory leak: no liberar el string generado por `cJSON_PrintUnformatted()`. Esta funcion devuelve un puntero a memoria dinamica que DEBES liberar con `free()` despues de publicar.
  - Publicar la MAC con formato incorrecto (con o sin los `:` de separacion). Manten un formato consistente en todo el proyecto.
- **Tiempo estimado**: 3-4 horas

---

### Tarea T4.1.3: Publicar estado del gateway y alertas

- **Dificultad**: Basico
- **Descripcion**: Publicar periodicamente el estado del gateway y enviar alertas cuando se disparen. Paso a paso:
  1. Crear la funcion `mqtt_bridge_publish_status()` que publique en el topic `piscifactoria/{gateway_id}/status`.
  2. El payload JSON de status debe contener: `uptime` (segundos desde el arranque, usando `esp_timer_get_time() / 1000000`), `nodos_conectados` (numero de nodos activos), `wifi_rssi` (intensidad de senal WiFi), `free_heap` (memoria libre con `esp_get_free_heap_size()`), `timestamp`.
  3. Crear un timer periodico con `esp_timer_create()` que llame a `mqtt_bridge_publish_status()` cada 60 segundos.
  4. Crear la funcion `mqtt_bridge_publish_alert()` que publique en el topic `piscifactoria/{gateway_id}/alertas`.
  5. El payload JSON de alerta debe contener: `tipo` (temperatura_alta, ph_bajo, etc.), `node_id`, `value` (valor que disparo la alerta), `threshold` (umbral configurado), `timestamp`, `severity` (warning/critical).
  6. Llamar a `mqtt_bridge_publish_alert()` desde el modulo de alertas (Fase 3) cuando se detecte una condicion fuera de rango.
  7. Usar QoS 1 para status y QoS 1 para alertas.
  8. Publicar el status con retain = 1 para que nuevos suscriptores reciban el ultimo estado conocido.
- **Archivos a crear/modificar**:
  - Modificar: `components/mqtt_bridge/mqtt_bridge.c` (agregar funciones de status y alertas)
  - Modificar: `components/mqtt_bridge/mqtt_bridge.h` (declarar funciones publicas)
  - Modificar: `main/alert_handler.c` (o equivalente, llamar a publicacion de alerta)
- **Criterio de aceptacion**:
  - Cada 60 segundos aparece un mensaje en el topic `piscifactoria/{gateway_id}/status` visible con `mosquitto_sub`.
  - El JSON de status contiene al menos 4 de los 5 campos definidos.
  - Cuando se fuerza una alerta (ej. sensor de temperatura fuera de rango), aparece un mensaje en el topic de alertas.
  - El mensaje de status tiene retain=1 (verificable suscribiendose despues de que se publique: el ultimo mensaje se recibe inmediatamente).
- **Dependencias**: T4.1.1, Fase 3 (sistema de alertas)
- **Pistas**:
  - `esp_wifi_sta_get_ap_info()` te da el RSSI actual de la conexion WiFi.
  - Para el conteo de nodos conectados, consulta la tabla/lista de nodos que ya mantienes desde Fase 2.
  - El flag `retain = 1` en `esp_mqtt_client_publish()` es el cuarto parametro.
- **Errores comunes**:
  - No arrancar el timer de status hasta que MQTT este conectado. Si publicas sin conexion, los mensajes se pierden silenciosamente (o se encolan hasta llenar la memoria).
  - Olvidar que `esp_timer_get_time()` devuelve microsegundos, no segundos. Dividir por 1.000.000.
- **Tiempo estimado**: 2-3 horas

---

### Tarea T4.1.4: Suscribirse a topics de control remoto

- **Dificultad**: Avanzado
- **Descripcion**: Permitir que el gateway reciba comandos remotos via MQTT para controlar actuadores y configurar nodos. Paso a paso:
  1. En el handler de `MQTT_EVENT_CONNECTED`, suscribirse al topic `piscifactoria/{gateway_id}/control/#` usando `esp_mqtt_client_subscribe()` con QoS 2.
  2. En el handler de `MQTT_EVENT_DATA`, parsear el topic recibido para determinar la accion.
  3. Si el topic coincide con `piscifactoria/{gateway_id}/control/actuador/{id}`, parsear el payload JSON que contendra: `action` ("on" o "off"), `duration` (opcional, en segundos). Llamar a la funcion correspondiente para activar/desactivar el actuador.
  4. Si el topic coincide con `piscifactoria/{gateway_id}/control/config/nodo/{mac}`, parsear el payload JSON que contendra parametros de configuracion (ej. `intervalo_lectura`, `umbrales`). Reenviar esta configuracion al nodo via ESP-NOW.
  5. Crear la funcion `mqtt_bridge_handle_command()` que haga el dispatch segun el topic.
  6. Implementar validacion basica: verificar que el JSON es valido, que los campos requeridos existen, que los valores estan en rangos razonables.
  7. Publicar una respuesta/ACK en `piscifactoria/{gateway_id}/control/response` confirmando la ejecucion o reportando un error.
  8. Probar con: `mosquitto_pub -h localhost -t "piscifactoria/GW01/control/actuador/1" -m '{"action":"on","duration":30}'`.
- **Archivos a crear/modificar**:
  - Modificar: `components/mqtt_bridge/mqtt_bridge.c` (agregar handler de comandos)
  - Modificar: `components/mqtt_bridge/mqtt_bridge.h` (declarar funciones)
  - Modificar: `main/actuator_handler.c` (o equivalente, exponer funcion para activar/desactivar)
  - Modificar: `main/espnow_handler.c` (o equivalente, exponer funcion para enviar config a nodo)
- **Criterio de aceptacion**:
  - Enviar un mensaje MQTT de control de actuador desde `mosquitto_pub` activa/desactiva el actuador en el gateway.
  - Enviar un mensaje de configuracion de nodo resulta en un mensaje ESP-NOW enviado al nodo correspondiente.
  - Se recibe un ACK en el topic de respuesta confirmando la accion.
  - Un payload JSON invalido genera un mensaje de error en el topic de respuesta (no un crash del gateway).
- **Dependencias**: T4.1.1, Fase 2 (comunicacion ESP-NOW), Fase 3 (actuadores configurados)
- **Pistas**:
  - Para parsear el topic, puedes usar `strncmp()` y `strtok()` o una funcion personalizada que extraiga los segmentos del topic.
  - QoS 2 (exactly once) es importante para comandos de actuadores: no quieres que se ejecute un comando dos veces.
  - `cJSON_Parse()` devuelve NULL si el JSON es invalido; siempre verificar antes de acceder a los campos.
  - El topic recibido en `MQTT_EVENT_DATA` NO esta null-terminated. Usar `event->topic_len` para copiar el topic a un buffer local con `\0`.
- **Errores comunes**:
  - No null-terminar el topic y el payload recibidos en `MQTT_EVENT_DATA`. Los campos `event->topic` y `event->data` NO son strings C validos. Copiarlos a un buffer local y agregar `\0` manualmente usando `event->topic_len` y `event->data_len`.
  - No validar el payload antes de actuar. Un JSON malformado o con campos faltantes puede causar un crash si se accede directamente con `cJSON_GetObjectItem()` sin verificar NULL.
- **Tiempo estimado**: 5-6 horas

---

### Checkpoint 4.1: Verificacion del MQTT Bridge

Antes de continuar con la Sub-tarea 4.2, verifica lo siguiente:

- [ ] El gateway se conecta automaticamente al broker Mosquitto al arrancar.
- [ ] Los datos de sensores aparecen en los topics MQTT correctos (verificar con `mosquitto_sub -v -t "piscifactoria/#"`).
- [ ] El status del gateway se publica cada 60 segundos.
- [ ] Las alertas se publican cuando un sensor supera un umbral.
- [ ] Los comandos de control enviados por MQTT ejecutan la accion correspondiente en el gateway.
- [ ] El gateway reconecta automaticamente si el broker se reinicia.
- [ ] No hay memory leaks (monitorear `esp_get_free_heap_size()` durante 10 minutos de operacion).

**Prueba integradora**: Abrir dos terminales. En la primera, ejecutar `mosquitto_sub -v -t "piscifactoria/#"`. En la segunda, enviar un comando con `mosquitto_pub`. Verificar que fluyen datos en ambas direcciones.

---

## Sub-tarea 4.2: Instalacion y configuracion del broker (2 tareas)

### Tarea T4.2.1: Instalar y configurar Mosquitto en el servidor

- **Dificultad**: Basico
- **Descripcion**: Instalar el broker MQTT Mosquitto en la Raspberry Pi o PC que hara de servidor. Paso a paso:
  1. Instalar Mosquitto: `sudo apt update && sudo apt install mosquitto mosquitto-clients`.
  2. Verificar que el servicio esta corriendo: `sudo systemctl status mosquitto`.
  3. Editar la configuracion en `/etc/mosquitto/conf.d/piscifactoria.conf`:
     ```
     listener 1883
     allow_anonymous false
     password_file /etc/mosquitto/passwd
     log_type all
     ```
  4. Crear usuario y password: `sudo mosquitto_passwd -c /etc/mosquitto/passwd piscifactoria`. Introducir password cuando lo pida (ej. `piscifactoria2024`).
  5. Reiniciar Mosquitto: `sudo systemctl restart mosquitto`.
  6. Probar la autenticacion en una terminal: `mosquitto_sub -h localhost -t "test/#" -u piscifactoria -P piscifactoria2024 -v`.
  7. En otra terminal: `mosquitto_pub -h localhost -t "test/hola" -m "funciona" -u piscifactoria -P piscifactoria2024`.
  8. Verificar que el mensaje aparece en la primera terminal.
  9. Verificar que sin credenciales la conexion es rechazada: `mosquitto_sub -h localhost -t "test/#"` debe fallar con "Connection Refused: not authorised".
  10. Anotar la IP del servidor (usar `hostname -I`) para configurar el gateway.
- **Archivos a crear/modificar**:
  - Crear: `/etc/mosquitto/conf.d/piscifactoria.conf` (en el servidor)
  - Crear: `/etc/mosquitto/passwd` (generado por mosquitto_passwd)
- **Criterio de aceptacion**:
  - Mosquitto esta corriendo como servicio (`systemctl status mosquitto` muestra `active (running)`).
  - Se puede publicar y suscribir con usuario y password.
  - Sin credenciales, la conexion es rechazada.
  - El puerto 1883 esta escuchando (`sudo ss -tlnp | grep 1883`).
- **Dependencias**: Ninguna (puede hacerse en paralelo con T4.1.x)
- **Pistas**:
  - En Raspberry Pi OS (basado en Debian), Mosquitto se instala directamente desde los repositorios oficiales.
  - Si usas firewall (`ufw`), abre el puerto: `sudo ufw allow 1883`.
  - Los logs de Mosquitto se ven con: `sudo journalctl -u mosquitto -f`.
  - El flag `-c` en `mosquitto_passwd` CREA el archivo (borra existente). Para agregar usuarios adicionales, usar `-b` sin `-c`.
- **Errores comunes**:
  - Usar `mosquitto_passwd -c` para agregar un segundo usuario, lo que borra el primer usuario. Para usuarios adicionales, omitir el flag `-c`.
  - Dejar `allow_anonymous true` en la configuracion por defecto. Mosquitto 2.0+ requiere configuracion explicita del listener; sin ella, solo escucha en localhost.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T4.2.2: Verificar comunicacion gateway-broker

- **Dificultad**: Basico
- **Descripcion**: Conectar el gateway ESP32-S3 al broker Mosquitto y verificar que los datos fluyen correctamente. Paso a paso:
  1. Configurar en el gateway (via NVS o interfaz web de Fase 1) los parametros del broker: IP del servidor, puerto 1883, usuario `piscifactoria`, password `piscifactoria2024`.
  2. Flashear y reiniciar el gateway.
  3. En el monitor serial del gateway, verificar que aparece el log de conexion exitosa al broker.
  4. En el servidor, abrir una terminal con `mosquitto_sub -h localhost -t "piscifactoria/#" -u piscifactoria -P piscifactoria2024 -v`.
  5. Verificar que aparecen los datos de sensores de los nodos en formato JSON.
  6. Verificar que aparece el status del gateway cada 60 segundos.
  7. Probar enviar un comando de control desde el servidor y verificar que el gateway lo recibe y ejecuta.
  8. Desconectar el broker (parar Mosquitto) y verificar que el gateway detecta la desconexion y reconecta cuando el broker vuelve.
  9. Documentar los topics observados y un ejemplo del payload JSON de cada tipo.
- **Archivos a crear/modificar**:
  - Modificar: NVS del gateway (via interfaz web o `nvs_set` en codigo) con los parametros del broker.
  - Crear (opcional): `docs/mqtt_topics.md` con la documentacion de topics y payloads.
- **Criterio de aceptacion**:
  - El gateway se conecta al broker remoto (no localhost) exitosamente.
  - Los mensajes de sensores aparecen en `mosquitto_sub` con el formato JSON esperado.
  - El status del gateway aparece periodicamente.
  - Un comando de control enviado desde `mosquitto_pub` se ejecuta en el gateway.
  - La reconexion funciona tras reiniciar Mosquitto.
- **Dependencias**: T4.1.1, T4.1.2, T4.1.3, T4.2.1
- **Pistas**:
  - Si el gateway no conecta, verificar: 1) Que el servidor es accesible (`ping` desde el PC de desarrollo), 2) Que el puerto 1883 esta abierto, 3) Que las credenciales son correctas.
  - Usar `mosquitto_sub` con el flag `-v` (verbose) para ver el topic junto con el mensaje.
  - Si hay problemas de red, habilitar logs detallados de MQTT en el gateway con `esp_log_level_set("MQTT_CLIENT", ESP_LOG_DEBUG)`.
- **Errores comunes**:
  - La IP del broker configurada en el gateway es incorrecta o ha cambiado (DHCP). Considerar usar una IP fija para el servidor o mDNS.
  - El firewall del servidor bloquea las conexiones entrantes en el puerto 1883. Verificar con `sudo ufw status` o `iptables -L`.
- **Tiempo estimado**: 1-2 horas

---

### Checkpoint 4.2: Verificacion del broker MQTT

Antes de continuar con la Sub-tarea 4.3, verifica lo siguiente:

- [ ] Mosquitto esta corriendo como servicio en el servidor con autenticacion habilitada.
- [ ] El gateway se conecta al broker remoto y publica datos de sensores.
- [ ] Se puede ver el flujo completo: Nodo sensor -> ESP-NOW -> Gateway -> MQTT -> Broker.
- [ ] Los comandos de control fluyen en la direccion inversa: Broker -> MQTT -> Gateway.
- [ ] La reconexion automatica funciona correctamente.

**Prueba integradora**: Con al menos un nodo sensor activo, ejecutar `mosquitto_sub -v -t "piscifactoria/#"` durante 5 minutos y verificar que llegan datos de sensores y status del gateway sin interrupciones.

---

## Sub-tarea 4.3: Backend Bun/TypeScript (4 tareas)

### Tarea T4.3.1: Scaffold del proyecto backend

- **Dificultad**: Basico
- **Descripcion**: Crear la estructura del proyecto backend usando Bun y TypeScript. Paso a paso:
  1. Crear el directorio del servidor: `mkdir -p server/src`.
  2. Dentro de `server/`, inicializar el proyecto: `bun init` (o `npm init -y` si usas Node.js).
  3. Instalar dependencias: `bun add mqtt` (cliente MQTT para JavaScript), `bun add better-sqlite3` (base de datos SQLite) y `bun add @types/better-sqlite3 -d` (tipos TypeScript).
  4. Nota: Si usas Bun, puedes usar `bun:sqlite` integrado en lugar de `better-sqlite3`.
  5. Crear el archivo `server/src/index.ts` como punto de entrada. Por ahora, solo un `console.log("Servidor piscifactoria iniciado")`.
  6. Crear el archivo `server/src/database.ts` con la inicializacion de SQLite:
     - Crear la base de datos en `server/data/piscifactoria.db`.
     - Crear la tabla `sensor_readings` con columnas: `id` (INTEGER PRIMARY KEY AUTOINCREMENT), `node_id` (TEXT NOT NULL), `sensor_type` (TEXT NOT NULL), `value` (REAL NOT NULL), `unit` (TEXT NOT NULL), `timestamp` (INTEGER NOT NULL).
     - Crear la tabla `alerts` con columnas: `id`, `node_id`, `alert_type`, `value`, `threshold`, `severity`, `timestamp`.
     - Crear indices en `node_id` y `timestamp` para ambas tablas.
  7. Verificar que el proyecto arranca sin errores: `bun run src/index.ts`.
  8. Agregar script en `package.json`: `"dev": "bun --watch src/index.ts"` para desarrollo con recarga automatica.
- **Archivos a crear/modificar**:
  - Crear: `server/package.json` (generado por `bun init`)
  - Crear: `server/tsconfig.json` (generado por `bun init`)
  - Crear: `server/src/index.ts`
  - Crear: `server/src/database.ts`
  - Crear: `server/data/` (directorio para la base de datos)
- **Criterio de aceptacion**:
  - `bun run src/index.ts` ejecuta sin errores.
  - La base de datos SQLite se crea automaticamente en `server/data/piscifactoria.db`.
  - Las tablas `sensor_readings` y `alerts` existen con las columnas correctas (verificar con `sqlite3 data/piscifactoria.db ".schema"`).
  - Los indices estan creados (verificar con `.indices`).
- **Dependencias**: T4.2.1 (broker instalado para las tareas siguientes)
- **Pistas**:
  - Con Bun, puedes usar `import { Database } from "bun:sqlite"` sin instalar paquetes adicionales.
  - Con Node.js, usa `import Database from "better-sqlite3"`.
  - Usa `CREATE TABLE IF NOT EXISTS` para que el script sea idempotente (se pueda ejecutar multiples veces sin error).
  - Crea un archivo `.gitignore` que excluya `node_modules/`, `data/*.db`.
- **Errores comunes**:
  - No crear el directorio `data/` antes de intentar crear la base de datos. SQLite no crea directorios intermedios automaticamente.
  - Usar `TEXT` para el timestamp en lugar de `INTEGER`. Usar epoch (segundos desde 1970) como INTEGER facilita las consultas de rango.
- **Tiempo estimado**: 1-2 horas

---

### Tarea T4.3.2: Servicio MQTT subscriber en el backend

- **Dificultad**: Intermedio
- **Descripcion**: Crear el modulo que se conecta al broker MQTT, se suscribe a todos los topics de la piscifactoria, y guarda los datos en SQLite. Paso a paso:
  1. Crear `server/src/mqtt.ts`.
  2. Importar `mqtt` de la libreria `mqtt` (mqtt.js) y la instancia de la base de datos.
  3. Crear la funcion `connectMqtt()` que:
     a. Conecte al broker con `mqtt.connect("mqtt://IP_SERVIDOR:1883", { username: "piscifactoria", password: "piscifactoria2024" })`.
     b. En el evento `connect`, suscribirse a `piscifactoria/#`.
     c. En el evento `message`, recibir `(topic: string, payload: Buffer)`.
  4. Parsear el topic para extraer: gateway_id, nodo_mac, tipo_sensor.
     - Usar `topic.split("/")` para descomponer el topic.
     - Si el topic tiene formato `.../nodo/{mac}/{tipo}`, es un dato de sensor.
     - Si el topic termina en `/status`, es un status de gateway.
     - Si el topic termina en `/alertas`, es una alerta.
  5. Para datos de sensor: parsear el payload JSON con `JSON.parse(payload.toString())` y ejecutar un INSERT en `sensor_readings`.
  6. Para alertas: parsear el JSON e insertar en la tabla `alerts`.
  7. Usar prepared statements de SQLite para eficiencia: `db.prepare("INSERT INTO sensor_readings (...) VALUES (?, ?, ?, ?, ?)").run(...)`.
  8. Loguear cada insercion con `console.log()` mostrando nodo, sensor, valor.
  9. Manejar errores de parseo JSON con try/catch (un mensaje malformado no debe crashear el servidor).
  10. Llamar a `connectMqtt()` desde `index.ts`.
- **Archivos a crear/modificar**:
  - Crear: `server/src/mqtt.ts`
  - Modificar: `server/src/index.ts` (importar e inicializar MQTT)
- **Criterio de aceptacion**:
  - El servidor se conecta al broker y se suscribe a `piscifactoria/#`.
  - Los datos de sensores recibidos via MQTT se insertan en la tabla `sensor_readings`.
  - Las alertas se insertan en la tabla `alerts`.
  - Despues de 2 minutos de operacion con nodos activos, la tabla `sensor_readings` tiene registros (verificar con `sqlite3 data/piscifactoria.db "SELECT COUNT(*) FROM sensor_readings"`).
  - Un payload JSON invalido genera un log de error pero no crashea el servidor.
- **Dependencias**: T4.3.1, T4.2.1
- **Pistas**:
  - La libreria `mqtt` de npm es mqtt.js: `import mqtt from "mqtt"`.
  - `payload` es un `Buffer`, convertir a string con `payload.toString()` antes de `JSON.parse()`.
  - Para extraer la MAC del topic `piscifactoria/GW01/nodo/AA:BB:CC:DD:EE:FF/temperatura`, hacer `const parts = topic.split("/")` y acceder a `parts[3]` para la MAC y `parts[4]` para el tipo.
  - Considera exportar un EventEmitter o callback para que otros modulos (WebSocket) puedan reaccionar a datos nuevos.
- **Errores comunes**:
  - No manejar el caso donde `JSON.parse()` falla. Envolver siempre en try/catch, ya que mensajes corruptos o de otros clientes MQTT pueden tener formatos inesperados.
  - Insertar el `timestamp` del mensaje MQTT tal cual sin validar que es un numero razonable. Verificar que es un epoch valido (mayor que 1700000000 aproximadamente).
- **Tiempo estimado**: 3-4 horas

---

### Tarea T4.3.3: API REST para datos historicos

- **Dificultad**: Intermedio
- **Descripcion**: Crear endpoints HTTP para que el frontend pueda consultar datos historicos de sensores, lista de nodos y alertas. Paso a paso:
  1. Crear `server/src/api.ts`.
  2. Usar el servidor HTTP nativo de Bun (`Bun.serve()`) o, si usas Node.js, instalar Express (`npm install express @types/express`).
  3. Implementar los siguientes endpoints:

     **GET /api/nodes**
     - Consulta: `SELECT DISTINCT node_id FROM sensor_readings ORDER BY node_id`.
     - Para cada nodo, obtener la ultima lectura: `SELECT * FROM sensor_readings WHERE node_id = ? ORDER BY timestamp DESC LIMIT 1`.
     - Respuesta: `[{ "node_id": "AA:BB:...", "last_seen": 1700000000, "last_value": 25.3, "last_sensor": "temperatura" }]`.

     **GET /api/nodes/:id/readings**
     - Parametros query: `from` (timestamp inicio), `to` (timestamp fin), `type` (tipo sensor, opcional).
     - Consulta: `SELECT * FROM sensor_readings WHERE node_id = ? AND timestamp BETWEEN ? AND ? [AND sensor_type = ?] ORDER BY timestamp ASC`.
     - Limitar a 1000 registros maximo con `LIMIT 1000`.
     - Respuesta: `[{ "id": 1, "node_id": "...", "sensor_type": "temperatura", "value": 25.3, "unit": "C", "timestamp": 1700000000 }]`.

     **GET /api/alerts**
     - Parametros query: `limit` (default 50), `severity` (opcional).
     - Consulta: `SELECT * FROM alerts ORDER BY timestamp DESC LIMIT ?`.
     - Respuesta: `[{ "id": 1, "node_id": "...", "alert_type": "temp_alta", "value": 35.0, "threshold": 30.0, "severity": "critical", "timestamp": 1700000000 }]`.

  4. Agregar cabeceras CORS en todas las respuestas: `Access-Control-Allow-Origin: *`, `Content-Type: application/json`.
  5. Manejar errores con respuestas HTTP apropiadas: 400 para parametros invalidos, 404 para nodo no encontrado, 500 para errores internos.
  6. Iniciar el servidor HTTP en el puerto 3000.
  7. Probar cada endpoint con `curl` o un navegador.
- **Archivos a crear/modificar**:
  - Crear: `server/src/api.ts`
  - Modificar: `server/src/index.ts` (iniciar servidor HTTP)
- **Criterio de aceptacion**:
  - `curl http://localhost:3000/api/nodes` devuelve un JSON con la lista de nodos.
  - `curl http://localhost:3000/api/nodes/AA:BB:CC:DD:EE:FF/readings?from=0&to=9999999999` devuelve lecturas de ese nodo.
  - `curl http://localhost:3000/api/alerts` devuelve las alertas recientes.
  - Las cabeceras CORS estan presentes en las respuestas.
  - Parametros invalidos devuelven HTTP 400 con mensaje de error.
- **Dependencias**: T4.3.1, T4.3.2 (para tener datos en la BD)
- **Pistas**:
  - Con Bun, puedes usar `Bun.serve({ fetch(req) { ... } })` que recibe un `Request` y retorna un `Response` (API estandar Web).
  - Para parsear la URL: `const url = new URL(req.url); const params = url.searchParams;`.
  - Para extraer el `:id` de la ruta, puedes usar `url.pathname.split("/")` o una libreria de routing.
  - Los query params `from` y `to` son strings; convertir a numero con `parseInt()` y validar que son numeros validos.
- **Errores comunes**:
  - Olvidar las cabeceras CORS. Sin ellas, el navegador bloqueara las peticiones del frontend (que corre en otro puerto). Agregar `Access-Control-Allow-Origin: *` en TODAS las respuestas, incluyendo las de error.
  - No manejar el preflight OPTIONS request de CORS. Si el frontend envia headers custom o POST con JSON, el navegador primero envia un OPTIONS. Responder con 204 y las cabeceras CORS correspondientes.
- **Tiempo estimado**: 4-5 horas

---

### Tarea T4.3.4: WebSocket server para datos en tiempo real

- **Dificultad**: Intermedio
- **Descripcion**: Crear un servidor WebSocket que reenvie a los clientes conectados (el dashboard) cada dato de sensor recibido via MQTT en tiempo real. Paso a paso:
  1. Crear `server/src/websocket.ts`.
  2. Con Bun, el servidor WebSocket se integra en `Bun.serve()` usando la opcion `websocket`. Con Node.js, instalar `ws` (`npm install ws @types/ws`).
  3. Mantener un `Set<WebSocket>` con todos los clientes conectados.
  4. En el evento `open` del WebSocket, agregar el cliente al Set. Enviarle un mensaje de bienvenida con la lista de nodos activos.
  5. En el evento `close`, eliminar el cliente del Set.
  6. En el evento `message`, parsear el mensaje del cliente. Implementar un sistema basico de suscripcion: el cliente puede enviar `{"action": "subscribe", "node_id": "AA:BB:..."}` para recibir solo datos de un nodo especifico, o `{"action": "subscribe", "node_id": "*"}` para recibir todos.
  7. Exportar una funcion `broadcastSensorData(data)` que sera llamada desde `mqtt.ts` cada vez que llega un nuevo dato de sensor.
  8. En `broadcastSensorData()`, iterar sobre todos los clientes conectados y enviarles el dato si coincide con su suscripcion. Formato: `{ "type": "sensor_data", "node_id": "...", "sensor_type": "...", "value": 25.3, "unit": "C", "timestamp": 1700000000 }`.
  9. Tambien crear `broadcastAlert(data)` para alertas en tiempo real.
  10. Modificar `mqtt.ts` para llamar a `broadcastSensorData()` y `broadcastAlert()` al recibir datos.
  11. El WebSocket debe escuchar en el mismo puerto que la API HTTP (3000) o en un puerto separado (ej. 3001).
  12. Probar con una herramienta como `websocat` o un script JavaScript simple en el navegador.
- **Archivos a crear/modificar**:
  - Crear: `server/src/websocket.ts`
  - Modificar: `server/src/mqtt.ts` (llamar a broadcast al recibir datos)
  - Modificar: `server/src/index.ts` (integrar WebSocket en el servidor)
- **Criterio de aceptacion**:
  - Un cliente WebSocket puede conectarse a `ws://localhost:3000` (o el puerto elegido).
  - Al conectarse, recibe un mensaje de bienvenida.
  - Datos de sensores recibidos por MQTT se reenvian al cliente WebSocket en menos de 1 segundo.
  - Las alertas tambien se reenvian en tiempo real.
  - Al desconectar un cliente, no se producen errores al intentar enviarle datos.
  - Multiples clientes pueden estar conectados simultaneamente.
- **Dependencias**: T4.3.1, T4.3.2
- **Pistas**:
  - Con Bun: `Bun.serve({ fetch(req, server) { if (server.upgrade(req)) return; /* HTTP normal */ }, websocket: { open(ws) {...}, message(ws, msg) {...}, close(ws) {...} } })`.
  - Antes de enviar a un cliente, verificar que el WebSocket esta en estado `OPEN` (readyState === 1).
  - Usar `JSON.stringify()` para enviar objetos como string al cliente.
  - Para pruebas rapidas, instalar `websocat`: `sudo apt install websocat` y conectar con `websocat ws://localhost:3000`.
- **Errores comunes**:
  - Intentar enviar datos a un WebSocket que ya se cerro. Siempre verificar el estado antes de enviar y manejar la excepcion.
  - No eliminar el cliente del Set cuando se desconecta, causando un memory leak progresivo y errores de envio.
- **Tiempo estimado**: 3-4 horas

---

### Checkpoint 4.3: Verificacion del backend

Antes de continuar con la Sub-tarea 4.4, verifica lo siguiente:

- [ ] El servidor backend arranca sin errores con `bun run src/index.ts`.
- [ ] Se conecta al broker MQTT y recibe datos de sensores.
- [ ] Los datos se almacenan en SQLite (verificar con `sqlite3`).
- [ ] La API REST responde correctamente a `curl` en los 3 endpoints.
- [ ] El WebSocket reenvía datos en tiempo real (verificar con `websocat`).
- [ ] Los headers CORS estan presentes en las respuestas HTTP.
- [ ] El servidor no crashea con datos malformados.

**Prueba integradora**: Con nodos sensores activos, abrir en el navegador `http://localhost:3000/api/nodes` y verificar que devuelve nodos. Simultaneamente, conectar con `websocat ws://localhost:3000` y verificar que llegan datos en tiempo real.

---

## Sub-tarea 4.4: Frontend Dashboard React (4 tareas)

### Tarea T4.4.1: Scaffold del proyecto frontend con Vite + React

- **Dificultad**: Basico
- **Descripcion**: Crear la estructura del proyecto frontend usando Vite, React y TypeScript. Paso a paso:
  1. Desde el directorio raiz del proyecto, crear la app: `bunx create-vite dashboard --template react-ts` (o `npx create-vite dashboard --template react-ts`).
  2. Entrar al directorio e instalar dependencias: `cd dashboard && bun install`.
  3. Instalar dependencias adicionales:
     - `bun add react-router-dom` (navegacion entre paginas).
     - `bun add recharts` (graficos).
     - `bun add tailwindcss @tailwindcss/vite` (estilos).
  4. Configurar Tailwind CSS:
     - Agregar el plugin de Tailwind en `vite.config.ts`.
     - Agregar `@import "tailwindcss"` al principio de `src/index.css`.
  5. Crear la estructura de carpetas:
     ```
     dashboard/src/
     ├── components/     (componentes reutilizables)
     ├── pages/          (paginas principales)
     ├── hooks/          (custom hooks)
     ├── services/       (llamadas API y WebSocket)
     ├── types/          (interfaces TypeScript)
     └── App.tsx         (layout principal y rutas)
     ```
  6. Crear el layout basico en `App.tsx`:
     - Sidebar con links de navegacion: Dashboard, Historicos, Configuracion.
     - Header con el titulo "Piscifactoria - Panel de Control".
     - Area de contenido principal donde se renderizan las paginas.
  7. Configurar React Router en `App.tsx` con rutas: `/` (Dashboard), `/history` (Historicos), `/config` (Configuracion).
  8. Crear paginas placeholder: `Dashboard.tsx`, `History.tsx`, `Config.tsx` con un `<h1>` del nombre de la pagina.
  9. Verificar que todo funciona: `bun run dev` y abrir `http://localhost:5173`.
  10. Verificar que la navegacion entre paginas funciona sin recargar.
- **Archivos a crear/modificar**:
  - Crear: `dashboard/` (proyecto completo generado por create-vite)
  - Crear: `dashboard/src/pages/Dashboard.tsx`
  - Crear: `dashboard/src/pages/History.tsx`
  - Crear: `dashboard/src/pages/Config.tsx`
  - Crear: `dashboard/src/types/index.ts`
  - Crear: `dashboard/src/services/api.ts` (placeholder)
  - Crear: `dashboard/src/services/websocket.ts` (placeholder)
  - Modificar: `dashboard/src/App.tsx` (layout + rutas)
  - Modificar: `dashboard/src/index.css` (Tailwind)
  - Modificar: `dashboard/vite.config.ts` (plugin Tailwind)
- **Criterio de aceptacion**:
  - `bun run dev` arranca el servidor de desarrollo sin errores ni warnings.
  - La pagina carga en `http://localhost:5173` con el layout (sidebar + header + contenido).
  - Se puede navegar entre las 3 paginas usando la sidebar.
  - Tailwind CSS funciona (verificar que clases como `bg-blue-500` aplican estilos).
  - No hay errores en la consola del navegador.
- **Dependencias**: Ninguna (puede hacerse en paralelo con T4.3.x)
- **Pistas**:
  - Tailwind v4 se configura diferente a v3. Con v4 y Vite, solo necesitas el plugin `@tailwindcss/vite` y el import en CSS; no hay archivo `tailwind.config.js`.
  - Para React Router v6+: usar `<BrowserRouter>`, `<Routes>`, `<Route>`, y `<Link>`.
  - Para el sidebar, usar `<nav>` con `<Link to="/">Dashboard</Link>`, etc.
  - La estructura de layout con sidebar fija + contenido scroll se logra con Tailwind: `flex h-screen` en el contenedor, `w-64 bg-gray-800` en el sidebar, `flex-1 overflow-auto` en el contenido.
- **Errores comunes**:
  - Instalar Tailwind v3 en lugar de v4. Las instrucciones de configuracion son diferentes. Verifica la version instalada con `bun pm ls tailwindcss`.
  - No envolver la app en `<BrowserRouter>`. Sin el provider de Router, los componentes `<Link>` y `<Route>` no funcionan y dan error.
- **Tiempo estimado**: 2-3 horas

---

### Tarea T4.4.2: Pagina Dashboard con estado en tiempo real

- **Dificultad**: Intermedio
- **Descripcion**: Crear la pagina principal del dashboard que muestra el estado actual de todos los nodos con datos en tiempo real via WebSocket. Paso a paso:
  1. Crear `dashboard/src/types/index.ts` con las interfaces TypeScript:
     ```typescript
     interface SensorReading { node_id: string; sensor_type: string; value: number; unit: string; timestamp: number; }
     interface NodeStatus { node_id: string; last_seen: number; online: boolean; sensors: SensorReading[]; }
     interface Alert { id: number; node_id: string; alert_type: string; value: number; threshold: number; severity: string; timestamp: number; }
     ```
  2. Crear `dashboard/src/services/websocket.ts`:
     - Crear una funcion `connectWebSocket()` que se conecte a `ws://IP_SERVIDOR:3000`.
     - Manejar reconexion automatica: si se cierra la conexion, reintentar en 3 segundos.
     - Exportar un custom hook `useWebSocket()` que devuelva los ultimos datos recibidos.
  3. Crear `dashboard/src/services/api.ts`:
     - Definir la URL base del backend: `const API_BASE = "http://IP_SERVIDOR:3000/api"`.
     - Crear funciones: `fetchNodes()`, `fetchAlerts()`.
  4. En `Dashboard.tsx`:
     - Al montar el componente, conectar al WebSocket y cargar la lista de nodos via API REST.
     - Mostrar una tarjeta (card) por cada nodo con:
       - MAC del nodo (o nombre si se ha configurado).
       - Ultima lectura de cada sensor (temperatura, pH, oxigeno disuelto).
       - Indicador de estado: circulo verde si `last_seen` fue hace menos de 2 minutos, rojo si mas.
       - Timestamp de la ultima actualizacion.
     - Actualizar las tarjetas en tiempo real cuando llegan datos por WebSocket.
     - Seccion de alertas activas: mostrar las ultimas 5 alertas con icono de severidad (amarillo=warning, rojo=critical).
  5. Usar `useState` para el estado de nodos y `useEffect` para la conexion WebSocket.
  6. Agregar un indicador de conexion WebSocket en el header: "Conectado" (verde) o "Desconectado" (rojo).
- **Archivos a crear/modificar**:
  - Modificar: `dashboard/src/types/index.ts` (agregar interfaces)
  - Crear: `dashboard/src/services/websocket.ts`
  - Crear: `dashboard/src/services/api.ts`
  - Modificar: `dashboard/src/pages/Dashboard.tsx`
  - Crear: `dashboard/src/components/NodeCard.tsx` (tarjeta de nodo reutilizable)
  - Crear: `dashboard/src/components/AlertBanner.tsx` (banner de alertas)
  - Crear: `dashboard/src/components/ConnectionStatus.tsx` (indicador de conexion)
- **Criterio de aceptacion**:
  - La pagina Dashboard muestra una tarjeta por cada nodo activo con sus ultimas lecturas.
  - Los valores se actualizan en tiempo real sin recargar la pagina (visible cuando cambia un valor de sensor).
  - El indicador de estado del nodo muestra verde/rojo segun la ultima vez que se recibieron datos.
  - Las alertas recientes aparecen en la parte superior o lateral.
  - El indicador de conexion WebSocket refleja el estado real de la conexion.
  - Si se desconecta el WebSocket, se reconecta automaticamente y vuelve a mostrar datos.
- **Dependencias**: T4.4.1, T4.3.3, T4.3.4
- **Pistas**:
  - Para el custom hook de WebSocket, usar `useRef` para mantener la referencia al WebSocket y `useEffect` para la conexion/desconexion.
  - `new WebSocket("ws://IP:3000")` es la API nativa del navegador, no necesitas libreria.
  - Para determinar si un nodo esta "online", comparar `Date.now() / 1000 - last_seen < 120` (menos de 2 minutos).
  - Las tarjetas se pueden hacer con Tailwind: `bg-white rounded-lg shadow p-4`.
- **Errores comunes**:
  - No limpiar la conexion WebSocket al desmontar el componente. Agregar `return () => ws.close()` en el `useEffect` para evitar conexiones duplicadas al navegar entre paginas.
  - Mutar el estado directamente en lugar de crear un nuevo objeto. Con React, siempre usar `setNodes(prev => ({...prev, [nodeId]: newData}))` para que React detecte el cambio y re-renderice.
- **Tiempo estimado**: 5-6 horas

---

### Tarea T4.4.3: Pagina de historicos con graficos

- **Dificultad**: Intermedio
- **Descripcion**: Crear la pagina de datos historicos con graficos de linea interactivos usando Recharts. Paso a paso:
  1. Agregar a `dashboard/src/services/api.ts` la funcion `fetchReadings(nodeId, from, to, sensorType)` que llama a `GET /api/nodes/:id/readings?from=&to=&type=`.
  2. En `History.tsx`, crear los controles de filtro:
     - **Selector de nodo**: dropdown con la lista de nodos disponibles (cargar con `fetchNodes()`).
     - **Selector de tipo de sensor**: dropdown con opciones: Temperatura, pH, Oxigeno Disuelto.
     - **Selector de rango de fechas**: dos inputs `<input type="datetime-local">` para fecha inicio y fecha fin. Por defecto, ultimas 24 horas.
     - **Boton "Consultar"**: al pulsarlo, llama a la API con los filtros seleccionados.
  3. Mostrar los resultados en un grafico de linea con Recharts:
     - Componente `<LineChart>` con `<Line>`, `<XAxis>`, `<YAxis>`, `<Tooltip>`, `<CartesianGrid>`.
     - Eje X: timestamp formateado como hora:minuto (o dia/mes si el rango es largo).
     - Eje Y: valor del sensor con unidad.
     - Tooltip: al pasar el raton, mostrar el valor exacto y el timestamp completo.
  4. Debajo del grafico, mostrar una tabla con los datos numericos: timestamp, valor, unidad.
  5. Agregar indicadores de estadisticas basicas: valor minimo, maximo, promedio del rango consultado.
  6. Mostrar un spinner o indicador de carga mientras se espera la respuesta de la API.
  7. Manejar el caso de "sin datos" con un mensaje amigable.
- **Archivos a crear/modificar**:
  - Modificar: `dashboard/src/services/api.ts` (agregar fetchReadings)
  - Modificar: `dashboard/src/pages/History.tsx`
  - Crear: `dashboard/src/components/SensorChart.tsx` (componente de grafico reutilizable)
  - Crear: `dashboard/src/components/DataTable.tsx` (tabla de datos)
  - Crear: `dashboard/src/components/StatsCard.tsx` (tarjeta de estadisticas)
- **Criterio de aceptacion**:
  - Se puede seleccionar un nodo, tipo de sensor y rango de fechas.
  - Al pulsar "Consultar", aparece un grafico de linea con los datos historicos.
  - El tooltip del grafico muestra el valor y timestamp al pasar el raton.
  - Se muestran las estadisticas: min, max, promedio.
  - La tabla debajo del grafico muestra los datos numericos.
  - El spinner aparece durante la carga y desaparece al recibir los datos.
  - Si no hay datos para los filtros seleccionados, se muestra un mensaje apropiado.
- **Dependencias**: T4.4.1, T4.3.3
- **Pistas**:
  - Recharts basico: `<LineChart data={data}><Line dataKey="value" /><XAxis dataKey="timestamp" /><YAxis /></LineChart>`.
  - Para formatear el timestamp en el eje X, usar el prop `tickFormatter` de `<XAxis>`: `tickFormatter={(ts) => new Date(ts * 1000).toLocaleTimeString()}`.
  - Para calcular min/max/promedio: `Math.min(...values)`, `Math.max(...values)`, `values.reduce((a,b) => a+b, 0) / values.length`.
  - Convertir `datetime-local` a epoch: `new Date(inputValue).getTime() / 1000`.
- **Errores comunes**:
  - No convertir los timestamps correctamente. El input `datetime-local` devuelve un string local, y la API espera epoch en segundos. Usar `Math.floor(new Date(value).getTime() / 1000)`.
  - Renderizar un `<LineChart>` con un array vacio causa que Recharts muestre un grafico con ejes pero sin linea y sin mensaje de "sin datos". Verificar `data.length > 0` antes de renderizar el grafico.
- **Tiempo estimado**: 5-6 horas

---

### Tarea T4.4.4: Pagina de configuracion y control de actuadores

- **Dificultad**: Avanzado
- **Descripcion**: Crear la pagina de configuracion que permite ajustar umbrales de alertas, intervalos de lectura de los nodos, y controlar actuadores remotamente. Los cambios se envian al backend via API REST, que a su vez los publica en MQTT hacia el gateway. Paso a paso:
  1. Agregar al backend (`server/src/api.ts`) los endpoints:
     - **POST /api/control/actuador/:id** - Body: `{ "action": "on"|"off", "duration": number }`. Publica en topic MQTT `piscifactoria/{gw}/control/actuador/{id}`.
     - **POST /api/config/nodo/:mac** - Body: `{ "intervalo_lectura": number, "umbrales": {...} }`. Publica en topic MQTT `piscifactoria/{gw}/control/config/nodo/{mac}`.
     - **GET /api/config** - Devuelve la configuracion actual (almacenada en SQLite o en memoria).
  2. Crear tabla `config` en SQLite: `config(key TEXT PRIMARY KEY, value TEXT)` para persistir ajustes.
  3. En `Config.tsx`, crear tres secciones:

     **Seccion 1: Umbrales de alertas**
     - Formulario con inputs numericos para: temperatura maxima, temperatura minima, pH maximo, pH minimo, oxigeno disuelto minimo.
     - Boton "Guardar umbrales" que envia POST al backend.
     - Mostrar los valores actuales cargados desde GET /api/config.

     **Seccion 2: Configuracion de nodos**
     - Selector de nodo.
     - Input para intervalo de lectura en segundos (ej. 10, 30, 60).
     - Boton "Aplicar" que envia POST al backend.
     - Indicador de confirmacion: "Configuracion enviada al nodo" cuando se recibe el ACK.

     **Seccion 3: Control de actuadores**
     - Lista de actuadores disponibles (ej. bomba de agua, aireador, alimentador).
     - Por cada actuador: nombre, estado actual (on/off), boton toggle (on/off), input de duracion (minutos).
     - Al pulsar el boton, enviar POST al backend.
     - Actualizar el estado del boton segun la respuesta.

  4. Manejar feedback visual: deshabilitar botones mientras se envia la peticion, mostrar confirmacion de exito o error con un toast/mensaje.
  5. Validar inputs en el frontend antes de enviar: rangos razonables para umbrales, intervalo minimo de 5 segundos, etc.
- **Archivos a crear/modificar**:
  - Modificar: `server/src/api.ts` (agregar endpoints POST)
  - Modificar: `server/src/database.ts` (agregar tabla config)
  - Modificar: `server/src/mqtt.ts` (funcion para publicar comandos de control)
  - Modificar: `dashboard/src/pages/Config.tsx`
  - Modificar: `dashboard/src/services/api.ts` (agregar funciones POST)
  - Crear: `dashboard/src/components/ThresholdForm.tsx`
  - Crear: `dashboard/src/components/NodeConfigForm.tsx`
  - Crear: `dashboard/src/components/ActuatorControl.tsx`
- **Criterio de aceptacion**:
  - Se pueden modificar los umbrales de alertas desde el formulario web y se guardan en la base de datos.
  - Al cambiar la configuracion de un nodo, el comando llega al gateway via MQTT (verificar con `mosquitto_sub`).
  - Al pulsar el boton de un actuador, el comando se ejecuta en el gateway y el estado del boton se actualiza.
  - Los inputs validan rangos antes de enviar (ej. temperatura entre -10 y 50, pH entre 0 y 14).
  - Se muestra feedback visual claro: exito, error, cargando.
  - El flujo completo funciona: Frontend -> API REST -> MQTT -> Gateway -> Actuador.
- **Dependencias**: T4.4.1, T4.3.3, T4.3.4, T4.1.4
- **Pistas**:
  - Para los POST desde React, usar `fetch(url, { method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(data) })`.
  - El boton toggle se puede implementar con un `<button>` que cambia entre "Encender" y "Apagar" segun el estado actual.
  - Para feedback visual, un simple `useState<"idle"|"loading"|"success"|"error">` es suficiente; no necesitas libreria de toasts.
  - Validar con HTML5: `<input type="number" min="0" max="50" step="0.1">`.
- **Errores comunes**:
  - No incluir `Content-Type: application/json` en el header del POST. Sin este header, el backend no parsea el body correctamente y recibe `undefined`.
  - Olvidar manejar el caso donde el gateway no esta conectado al broker. El POST al backend puede tener exito (HTTP 200) pero el comando no llega al gateway. Implementar un mecanismo de ACK/timeout si es posible.
- **Tiempo estimado**: 6-8 horas

---

### Checkpoint 4.4: Verificacion del frontend Dashboard

Antes de continuar con la Sub-tarea 4.5, verifica lo siguiente:

- [ ] El dashboard React arranca sin errores con `bun run dev`.
- [ ] La pagina Dashboard muestra los nodos con datos en tiempo real via WebSocket.
- [ ] Los indicadores de estado (online/offline) funcionan correctamente.
- [ ] La pagina Historicos muestra graficos con datos reales de la API.
- [ ] Los filtros de nodo, tipo de sensor y rango de fechas funcionan.
- [ ] La pagina Configuracion permite controlar actuadores y los comandos llegan al gateway.
- [ ] Los formularios validan los inputs antes de enviar.
- [ ] No hay errores en la consola del navegador.

**Prueba integradora**: Con todo el sistema en marcha (nodos + gateway + broker + backend + frontend), verificar el flujo completo:
1. Un nodo sensor envia datos.
2. El dato aparece en el Dashboard en tiempo real (menos de 2 segundos).
3. El dato se puede consultar en Historicos con un grafico.
4. Un comando de control enviado desde Config activa un actuador en el gateway.

---

## Sub-tarea 4.5: Dashboard embebido mejorado (2 tareas - opcionales pero recomendadas)

### Tarea T4.5.1: Añadir datos live al panel web embebido del gateway

- **Dificultad**: Intermedio
- **Descripcion**: Mejorar la interfaz web embebida del ESP32-S3 gateway (creada en Fase 1) para mostrar datos de sensores en tiempo real y estado de los nodos. Paso a paso:
  1. Modificar el handler HTTP del servidor web embebido del gateway para agregar un nuevo endpoint: `GET /api/nodes`.
  2. Este endpoint debe devolver un JSON con la informacion de cada nodo conectado: MAC, ultimo valor de cada sensor, timestamp de ultima lectura, estado (online/offline).
  3. Modificar la pagina HTML embebida para agregar una seccion "Nodos conectados" con una tabla o tarjetas.
  4. Agregar JavaScript que haga un `fetch("/api/nodes")` cada 5 segundos y actualice el contenido de la pagina.
  5. Mostrar por cada nodo:
     - Identificador (MAC o nombre).
     - Ultima lectura de temperatura, pH, oxigeno disuelto.
     - Indicador visual de estado: fondo verde si la ultima lectura fue hace menos de 2 minutos, rojo si mas.
     - Timestamp de la ultima comunicacion.
  6. Agregar un indicador del estado MQTT: "Conectado al broker" o "Desconectado del broker".
  7. Minimizar el uso de memoria: el HTML/CSS/JS embebido debe ser lo mas compacto posible (el ESP32 tiene recursos limitados).
- **Archivos a crear/modificar**:
  - Modificar: `components/web_server/web_server.c` (o equivalente, agregar endpoint /api/nodes)
  - Modificar: `components/web_server/index_html.h` (o equivalente, agregar seccion de nodos y JavaScript)
- **Criterio de aceptacion**:
  - Al acceder a `http://IP_GATEWAY/` en un navegador, se ve la seccion de nodos conectados.
  - Los datos se actualizan automaticamente cada 5 segundos sin recargar la pagina.
  - El indicador de estado del nodo muestra verde/rojo correctamente.
  - El indicador de estado MQTT refleja la conexion real al broker.
  - El endpoint `GET /api/nodes` devuelve JSON valido con la informacion de los nodos.
  - El consumo de memoria del gateway se mantiene estable (no aumenta con cada peticion).
- **Dependencias**: Fase 1 (servidor web embebido), T4.1.1 (MQTT conectado)
- **Pistas**:
  - Para el JSON de respuesta del endpoint, reutilizar la estructura de datos de nodos que ya mantienes en memoria.
  - Usar `cJSON` para generar el JSON de respuesta en el ESP32.
  - El JavaScript de la pagina embebida debe ser inline (dentro de `<script>`) para evitar peticiones adicionales.
  - Usar `document.getElementById("nodos").innerHTML = ...` para actualizar el contenido dinamicamente.
  - `setInterval(() => fetch("/api/nodes").then(r => r.json()).then(actualizarUI), 5000)` para las actualizaciones periodicas.
- **Errores comunes**:
  - Olvidar que el HTML/JS embebido esta como string C en un `.h`. Los caracteres especiales como `"` deben escaparse como `\"`, y los `%` como `%%` si usas `printf`.
  - Generar el JSON de respuesta con `sprintf` en lugar de `cJSON`. Esto es propenso a buffer overflows y formatos invalidos. Siempre usar `cJSON`.
- **Tiempo estimado**: 4-5 horas

---

### Tarea T4.5.2: Configuracion MQTT desde la web embebida del gateway

- **Dificultad**: Basico
- **Descripcion**: Agregar un formulario en la interfaz web embebida del gateway para configurar los parametros de conexion MQTT sin necesidad de reflashear. Paso a paso:
  1. Agregar al HTML embebido un formulario con los campos:
     - Broker Host (IP o hostname): `<input type="text" name="mqtt_host" placeholder="192.168.1.100">`.
     - Puerto: `<input type="number" name="mqtt_port" value="1883">`.
     - Usuario: `<input type="text" name="mqtt_user">`.
     - Password: `<input type="password" name="mqtt_pass">`.
     - Boton "Guardar y reconectar".
  2. Crear un endpoint POST en el servidor web embebido: `POST /api/mqtt/config`.
  3. En el handler del POST, parsear el body (URL-encoded o JSON) y extraer los 4 campos.
  4. Guardar cada campo en NVS con las claves: `mqtt_host`, `mqtt_port`, `mqtt_user`, `mqtt_pass`.
  5. Llamar a `esp_mqtt_client_stop()` para desconectar el cliente actual.
  6. Reinicializar el cliente MQTT con los nuevos parametros y llamar a `esp_mqtt_client_start()`.
  7. Devolver una respuesta indicando si la reconexion fue exitosa o fallo.
  8. Al cargar la pagina, rellenar los campos del formulario con los valores actuales (excepto el password, por seguridad). Crear un endpoint `GET /api/mqtt/config` que devuelva los valores actuales (sin el password).
- **Archivos a crear/modificar**:
  - Modificar: `components/web_server/web_server.c` (agregar endpoints GET y POST para config MQTT)
  - Modificar: `components/web_server/index_html.h` (agregar formulario HTML)
  - Modificar: `components/mqtt_bridge/mqtt_bridge.c` (exponer funcion para reinicializar con nuevos parametros)
  - Modificar: `components/mqtt_bridge/mqtt_bridge.h` (declarar funcion de reinicializacion)
- **Criterio de aceptacion**:
  - El formulario de configuracion MQTT aparece en la pagina web del gateway.
  - Los campos se pre-rellenan con los valores actuales (excepto password).
  - Al guardar, los nuevos valores se almacenan en NVS (verificable con `nvs_get_str` o reiniciando el gateway).
  - El cliente MQTT se reconecta al nuevo broker sin reiniciar el gateway.
  - Si el nuevo broker no es alcanzable, se muestra un mensaje de error.
- **Dependencias**: Fase 1 (servidor web embebido), T4.1.1
- **Pistas**:
  - Para parsear un body URL-encoded (`mqtt_host=192.168.1.100&mqtt_port=1883&...`), puedes usar `httpd_query_key_value()` de ESP-IDF.
  - Para guardar en NVS: `nvs_set_str(handle, "mqtt_host", valor)` seguido de `nvs_commit(handle)`.
  - La funcion de reinicializacion MQTT puede ser tan simple como: `esp_mqtt_client_destroy()` + crear nuevo config + `esp_mqtt_client_init()` + `esp_mqtt_client_start()`.
  - No expongas el password en el GET. Devuelve `"password": "****"` o simplemente omitelo.
- **Errores comunes**:
  - No llamar a `nvs_commit()` despues de `nvs_set_str()`. Sin commit, los datos no se persisten en flash y se pierden al reiniciar.
  - Intentar reusar el handle del cliente MQTT antiguo despues de destruirlo. Siempre crear un handle nuevo con `esp_mqtt_client_init()`.
- **Tiempo estimado**: 3-4 horas

---

### Checkpoint 4.5: Verificacion del dashboard embebido

Verifica lo siguiente:

- [ ] La pagina web del gateway muestra datos de nodos en tiempo real.
- [ ] Los datos se actualizan cada 5 segundos sin recargar.
- [ ] El formulario de configuracion MQTT permite cambiar el broker.
- [ ] Los cambios de configuracion MQTT se persisten en NVS y sobreviven un reinicio.
- [ ] La reconexion MQTT funciona correctamente tras cambiar la configuracion.

**Prueba integradora**: Cambiar la IP del broker desde la interfaz web del gateway. Verificar que se reconecta al nuevo broker y que los datos siguen fluyendo al backend y al dashboard React.

---

## Preguntas de autoevaluacion

Responde estas preguntas para verificar tu comprension de los conceptos de esta fase. Si no puedes responder alguna, revisa la documentacion recomendada.

1. **Que es QoS en MQTT y cuales son los tres niveles? Cuando usarias cada uno en el contexto de una piscifactoria?**
   > Pista: Piensa en que pasa si se pierde un dato de temperatura vs. un comando para apagar una bomba.

2. **Que diferencia hay entre un mensaje MQTT con retain=1 y uno con retain=0? Cuando es util el retain en este proyecto?**
   > Pista: Que pasa cuando un nuevo suscriptor (ej. el backend despues de reiniciar) se conecta al broker?

3. **Por que es necesario agregar cabeceras CORS en la API REST del backend? Que pasaria si no las agregas?**
   > Pista: El frontend corre en `localhost:5173` y la API en `localhost:3000`. Son el mismo "origen"?

4. **Cual es la diferencia entre usar HTTP polling (fetch cada N segundos) y WebSocket para datos en tiempo real? Cuales son las ventajas y desventajas de cada enfoque?**
   > Pista: Piensa en latencia, consumo de recursos del servidor, y complejidad de implementacion.

5. **Que es un prepared statement en SQLite y por que es importante usarlo en lugar de concatenar strings para las queries?**
   > Pista: Piensa en rendimiento (la query se compila una sola vez) y en seguridad (SQL injection).

6. **En el ESP32, por que es critico liberar la memoria devuelta por `cJSON_PrintUnformatted()`? Que pasaria si no lo haces en un dispositivo que corre 24/7?**
   > Pista: El ESP32 tiene ~320KB de RAM. Un JSON de 200 bytes generado cada 10 segundos sin liberar...

7. **Que ventajas tiene usar una estructura de topics MQTT jerarquica (`piscifactoria/gw01/nodo/mac/temp`) frente a topics planos (`sensor_temp_nodo1`)? Como se aprovechan los wildcards `+` y `#`?**
   > Pista: Suscribirse a todos los datos de un nodo especifico, o a toda la temperatura de todos los nodos.

8. **Si el broker MQTT se cae durante 5 minutos y luego se recupera, que pasa con los datos de sensores que el gateway intento publicar durante ese tiempo? Como podrias mitigar la perdida de datos?**
   > Pista: Investiga el concepto de "offline buffering" en el cliente MQTT del ESP32 y las limitaciones de memoria.

---

## Lectura recomendada

### MQTT
- [Protocolo MQTT - Documentacion oficial](https://mqtt.org/) - Entender los conceptos basicos: topics, QoS, retain, last will testament.
- [ESP-MQTT - Documentacion ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/mqtt.html) - API del cliente MQTT en ESP-IDF.
- [Mosquitto - Documentacion oficial](https://mosquitto.org/documentation/) - Configuracion del broker.
- [MQTT.js - GitHub](https://github.com/mqttjs/MQTT.js) - Cliente MQTT para JavaScript/TypeScript (usado en el backend).

### Backend
- [Bun - Documentacion oficial](https://bun.sh/docs) - Runtime, servidor HTTP, WebSocket, SQLite integrado.
- [better-sqlite3 - GitHub](https://github.com/WiseLibs/better-sqlite3) - API sincrona de SQLite para Node.js (alternativa a bun:sqlite).
- [WebSocket API - MDN](https://developer.mozilla.org/es/docs/Web/API/WebSockets_API) - Especificacion del protocolo WebSocket.

### Frontend
- [React - Documentacion oficial](https://react.dev/) - Hooks, componentes, estado.
- [Recharts - Documentacion oficial](https://recharts.org/) - Graficos de linea, area, barras para React.
- [Tailwind CSS - Documentacion](https://tailwindcss.com/docs) - Framework de estilos utility-first.
- [React Router - Documentacion](https://reactrouter.com/) - Enrutamiento del lado del cliente.
- [Vite - Documentacion](https://vite.dev/guide/) - Bundler y servidor de desarrollo.

### Conceptos generales
- [CORS - MDN](https://developer.mozilla.org/es/docs/Web/HTTP/CORS) - Entender el mecanismo de seguridad del navegador.
- [SQLite - Documentacion](https://www.sqlite.org/docs.html) - Referencia SQL y mejores practicas.
- [cJSON - GitHub](https://github.com/DaveGamble/cJSON) - Libreria de JSON para C usada en ESP-IDF.

---

## Errores frecuentes de la Fase 4

### 1. Memory leak en el ESP32 con cJSON
**Sintoma**: El gateway funciona bien las primeras horas pero luego se reinicia aleatoriamente o deja de responder.
**Causa**: No liberar la memoria asignada por `cJSON_CreateObject()` y `cJSON_PrintUnformatted()`.
**Solucion**: Siempre llamar a `cJSON_Delete(root)` para el objeto JSON y `free(string)` para el string generado por `cJSON_PrintUnformatted()`. Monitorear `esp_get_free_heap_size()` periodicamente.

### 2. MQTT publica antes de que WiFi este listo
**Sintoma**: Los primeros mensajes MQTT se pierden o el cliente MQTT reporta errores de conexion.
**Causa**: Llamar a `mqtt_bridge_init()` antes de obtener una IP via DHCP.
**Solucion**: Registrar `mqtt_bridge_init()` como callback del evento `IP_EVENT_STA_GOT_IP` o usar un semaforo/event group para esperar la conexion WiFi.

### 3. CORS bloquea las peticiones del frontend
**Sintoma**: La pagina web carga pero las llamadas a la API fallan con error en la consola: "has been blocked by CORS policy".
**Causa**: El backend no incluye las cabeceras CORS o no maneja las peticiones OPTIONS (preflight).
**Solucion**: Agregar `Access-Control-Allow-Origin: *`, `Access-Control-Allow-Methods: GET, POST, OPTIONS`, y `Access-Control-Allow-Headers: Content-Type` en TODAS las respuestas HTTP del backend. Manejar las peticiones OPTIONS con un 204 No Content.

### 4. WebSocket no reconecta tras desconexion
**Sintoma**: Si el backend se reinicia, el dashboard deja de recibir datos en tiempo real y no se recupera.
**Causa**: La API nativa de WebSocket del navegador no tiene reconexion automatica.
**Solucion**: Implementar logica de reconexion en el hook `useWebSocket()`: en el evento `onclose`, hacer `setTimeout(() => connect(), 3000)`. Incrementar el intervalo exponencialmente en caso de fallos repetidos (backoff).

### 5. El topic MQTT del ESP32 no esta null-terminated
**Sintoma**: Los mensajes MQTT recibidos en el gateway contienen basura al final del topic o del payload.
**Causa**: Los campos `event->topic` y `event->data` del evento `MQTT_EVENT_DATA` no terminan en `\0`.
**Solucion**: Copiar a un buffer local con `strncpy()` o `memcpy()` usando `event->topic_len` y `event->data_len`, y agregar el `\0` manualmente.

### 6. SQLite bloqueada por escrituras concurrentes
**Sintoma**: Errores "database is locked" en el backend cuando llegan muchos datos de sensores simultaneamente.
**Causa**: SQLite no soporta escrituras concurrentes por defecto.
**Solucion**: Usar modo WAL (Write-Ahead Logging): ejecutar `PRAGMA journal_mode=WAL;` al inicializar la base de datos. Esto permite lecturas concurrentes con escrituras.

### 7. El grafico de Recharts no actualiza al cambiar los datos
**Sintoma**: El grafico se renderiza con los primeros datos pero no cambia al seleccionar otro nodo o rango.
**Causa**: React no detecta el cambio de estado porque se esta mutando el array en lugar de crear uno nuevo.
**Solucion**: Usar `setData([...newData])` o `setData(newData)` donde `newData` es un nuevo array (no una mutacion del anterior). Verificar que la key del componente cambia cuando cambian los datos.

### 8. Timestamps inconsistentes entre ESP32 y servidor
**Sintoma**: Los datos en el grafico aparecen desordenados o con timestamps en el futuro/pasado.
**Causa**: El ESP32 no tiene reloj de tiempo real (RTC) y `time()` devuelve 0 o un valor incorrecto si SNTP no esta configurado.
**Solucion**: Configurar SNTP en el gateway al arrancar: `esp_sntp_setoperatingmode(SNTP_OPMODE_POLL)`, `esp_sntp_setservername(0, "pool.ntp.org")`, `esp_sntp_init()`. Esperar la sincronizacion antes de empezar a publicar datos. Alternativamente, asignar el timestamp en el backend al recibir el dato.

---

## Resumen de la Fase 4

Al completar esta fase, tendras un sistema completo de monitoreo de piscifactoria con:

- **Gateway MQTT Bridge**: El ESP32-S3 publica datos de sensores y estado al broker MQTT, y recibe comandos de control.
- **Broker Mosquitto**: Centraliza la comunicacion MQTT con autenticacion.
- **Backend Bun/TypeScript**: Almacena datos historicos en SQLite, expone API REST para consultas y WebSocket para tiempo real.
- **Dashboard React**: Visualizacion en tiempo real, graficos historicos, y control remoto de actuadores.
- **Web embebida mejorada**: El gateway tiene su propia interfaz web con datos live y configuracion MQTT.

El flujo de datos completo es:
```
Sensor -> Nodo ESP32-C3 -> ESP-NOW -> Gateway ESP32-S3 -> MQTT -> Broker Mosquitto
    -> Backend Bun -> SQLite (historicos) + WebSocket (tiempo real) -> Dashboard React
```

**Tiempo total estimado**: 50-65 horas (4 semanas a ritmo de dedicacion parcial).
