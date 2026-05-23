# Fase 5: OTA + Optimizacion + Pruebas

> **CORRECCIONES APLICADAS** (2026-05-23): Se agrega T5.4.5 (NVS encryption), T5.5.1 (tests unitarios con Unity). Ventana de confirmacion OTA ampliada de 60s a 120s. Condiciones de rollback definidas explicitamente. Duracion ajustada a 5 semanas. Ver detalles en `docs/replanificacion/03-fases-corregidas.md`.

## Resumen

- **Objetivo**: Implementar actualizaciones OTA (Over-The-Air) para gateway y nodos, realizar pruebas de estres y fiabilidad, endurecer la seguridad del sistema, y preparar la documentacion para el release v1.0
- **Duracion estimada**: 4 semanas
- **Prerequisitos**: Fases 0-4 completadas y funcionando end-to-end
- **Hardware necesario**: Sistema completo (gateway ESP32-S3 + nodos ESP32-C3 con sensores + servidor con Mosquitto + dashboard), bateria 3400mAh para test de autonomia, certificados TLS

## Dependencias externas

- ESP-IDF v5.x
- OpenSSL (para generar certificados TLS)
- Servidor HTTPS para alojar binarios de firmware OTA (puede ser el propio backend Bun/Node)
- Mosquitto configurado con soporte TLS
- Herramientas de monitorizacion de red (opcional: Wireshark)

---

## Sub-tarea 5.1: OTA para el gateway

### Contexto

El gateway esta en una instalacion que puede ser remota. Cada vez que corriges un bug o añades una funcionalidad, no puedes ir fisicamente a flashear con USB. OTA permite actualizar el firmware a traves de WiFi, descargando el nuevo binario desde un servidor HTTPS.

### Tarea T5.1.1: Configurar tabla de particiones OTA

- **Dificultad**: [Basico]
- **Descripcion**: Verificar que el fichero `partitions.csv` del gateway tiene las particiones necesarias para OTA: `ota_0` (3MB), `ota_1` (3MB), `otadata` (8KB). La particion `otadata` indica al bootloader cual es la particion activa. Flashear el gateway con esta tabla de particiones si no se hizo previamente.
- **Archivos a crear/modificar**: `firmware/gateway/partitions.csv`
- **Criterio de aceptacion**:
  - [ ] El fichero `partitions.csv` contiene particiones ota_0, ota_1 y otadata con los tamaños correctos
  - [ ] `idf.py build` compila sin errores con la nueva tabla
  - [ ] `esp_ota_get_running_partition()` devuelve la particion desde la que arranco
- **Dependencias**: Fase 1 completada
- **Pistas**: `esp_ota_get_running_partition()`, `esp_ota_get_next_update_partition()`. En `menuconfig` > `Partition Table` seleccionar "Custom partition table CSV"
- **Errores comunes**: Olvidar cambiar el tipo de tabla de particiones en menuconfig de "Single factory" a "Custom CSV". Los tamaños deben estar en hex y alineados a 0x1000.
- **Tiempo estimado**: 1-2 horas

### Tarea T5.1.2: Implementar OTA via HTTPS

- **Dificultad**: [Intermedio]
- **Descripcion**: Crear o ampliar `firmware/gateway/main/ota_manager.c/.h`. Implementar funcion `ota_manager_start(const char *url)` que descargue un binario firmware desde una URL HTTPS y lo escriba en la particion OTA siguiente. Usar la API `esp_https_ota()` que simplifica todo el proceso. El certificado CA del servidor debe embeberse en el firmware. El trigger para iniciar OTA puede ser un boton en el dashboard embebido o un comando MQTT en el topic `piscifactoria/{gw_id}/control/ota`.
- **Archivos a crear/modificar**: `firmware/gateway/main/ota_manager.c`, `firmware/gateway/main/ota_manager.h`, `firmware/gateway/main/CMakeLists.txt` (embeber certificado)
- **Criterio de aceptacion**:
  - [ ] Al pulsar "Actualizar firmware" en el dashboard embebido, el gateway descarga el binario y se actualiza
  - [ ] Tras reinicio, el gateway arranca con el nuevo firmware
  - [ ] Si la URL es incorrecta o el servidor no responde, el OTA falla limpiamente con log de error
  - [ ] El certificado TLS del servidor se verifica correctamente
- **Dependencias**: T5.1.1
- **Pistas**: `esp_https_ota_config_t`, `esp_https_ota()`, `extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start")`. Configurar `EMBED_TXTFILES` en CMake para el certificado. URL del firmware: `https://tu-servidor/firmware/gateway.bin`
- **Errores comunes**: No embeber el certificado CA (la conexion HTTPS falla sin verificacion). No reservar suficiente stack para la tarea OTA (minimo 8KB). Olvidar que el binario debe ser del mismo target (esp32s3).
- **Tiempo estimado**: 4-6 horas

### Tarea T5.1.3: Rollback automatico

- **Dificultad**: [Avanzado]
- **Descripcion**: Implementar mecanismo de rollback: tras un OTA exitoso y reinicio, el nuevo firmware tiene 60 segundos para "confirmarse" llamando a `esp_ota_mark_app_valid_cancel_rollback()`. Si el firmware crashea antes de confirmarse, el bootloader revertira automaticamente a la particion anterior en el proximo arranque. La confirmacion debe hacerse despues de verificar que los sistemas criticos funcionan (WiFi conecta, ESP-NOW activo, al menos 1 nodo responde).
- **Archivos a crear/modificar**: `firmware/gateway/main/ota_manager.c`, `firmware/gateway/main/main.c`
- **Criterio de aceptacion**:
  - [ ] Tras OTA, si el nuevo firmware funciona correctamente, se confirma y queda activo
  - [ ] Si se flashea un firmware defectuoso que crashea al arrancar, tras reinicio se vuelve al firmware anterior automaticamente
  - [ ] El log muestra claramente si estamos en firmware nuevo pendiente de confirmacion o confirmado
- **Dependencias**: T5.1.2
- **Pistas**: `esp_ota_mark_app_valid_cancel_rollback()`, `esp_ota_check_rollback_is_possible()`, `esp_ota_get_state_partition()`. En menuconfig habilitar `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`.
- **Errores comunes**: Llamar a la confirmacion demasiado pronto (antes de verificar que todo funciona). No habilitar rollback en menuconfig (el bootloader no soporta rollback por defecto).
- **Tiempo estimado**: 3-4 horas

> **Checkpoint T5.1**: En este punto deberias poder:
> - [ ] Actualizar el firmware del gateway remotamente via HTTPS
> - [ ] Ver que tras OTA arranca el nuevo firmware
> - [ ] Verificar que si el nuevo firmware falla, se revierte al anterior automaticamente
> Si algo no funciona: verifica que la tabla de particiones es correcta, que el certificado esta embebido, y que el rollback esta habilitado en menuconfig.

---

## Sub-tarea 5.2: OTA para nodos

### Contexto

Los nodos ESP32-C3 no tienen conexion WiFi directa a internet (solo usan ESP-NOW). Para actualizarlos, el nodo debe conectarse temporalmente al WiFi AP del gateway y descargar el firmware via HTTP. Este proceso es mas complejo que el OTA del gateway porque requiere cambiar el modo de operacion del nodo.

### Tarea T5.2.1: Nodo se conecta temporalmente al AP del gateway

- **Dificultad**: [Avanzado]
- **Descripcion**: Modificar firmware del nodo para soportar un "modo OTA": cuando recibe un comando OTA via ESP-NOW del gateway, el nodo sale del modo ESP-NOW, se conecta al WiFi AP del gateway como STA, descarga el firmware via HTTP, y reinicia. Despues del reinicio, vuelve a su modo normal (ESP-NOW + deep sleep). El comando OTA incluye la URL del firmware.
- **Archivos a crear/modificar**: `firmware/node/main/ota_node.c`, `firmware/node/main/ota_node.h`, `firmware/node/main/espnow_node.c` (procesar comando OTA)
- **Criterio de aceptacion**:
  - [ ] Al enviar comando OTA desde el gateway, el nodo se conecta al AP del gateway
  - [ ] El nodo descarga y aplica el nuevo firmware
  - [ ] Tras reinicio, el nodo vuelve a operar normalmente con ESP-NOW
  - [ ] Si el OTA falla, el nodo vuelve a modo normal sin quedarse bloqueado
- **Dependencias**: T5.1.1, Fase 2 completada
- **Pistas**: Deshabilitar ESP-NOW antes de conectar WiFi STA al AP. Usar `esp_http_client` para descargar (HTTP, no HTTPS, ya que es red local). Timeout de 120 segundos para todo el proceso.
- **Errores comunes**: No deshabilitar ESP-NOW antes de cambiar modo WiFi (conflicto de canal). Olvidar que el nodo usa esp32c3 target y el binario debe ser del target correcto. El nodo puede tener la bateria baja durante OTA (el proceso WiFi consume mucho mas).
- **Tiempo estimado**: 6-8 horas

### Tarea T5.2.2: Servir firmware de nodo desde el gateway

- **Dificultad**: [Intermedio]
- **Descripcion**: El gateway debe servir el binario del firmware del nodo en un endpoint HTTP. Opciones: 1) Almacenar el binario en SPIFFS del gateway (si cabe, ~1.5MB), 2) El gateway actua como proxy, descargando del servidor y reenviando al nodo. Implementar endpoint `GET /ota/node.bin` en el HTTP server del gateway. El servidor backend puede subir el binario al gateway via otro endpoint `POST /ota/upload`.
- **Archivos a crear/modificar**: `firmware/gateway/main/http_server.c` (nuevo endpoint), `firmware/gateway/main/ota_manager.c` (gestion de binarios)
- **Criterio de aceptacion**:
  - [ ] El endpoint `GET /ota/node.bin` devuelve el binario del firmware del nodo
  - [ ] El Content-Length es correcto
  - [ ] Un nodo puede descargar el binario y aplicar OTA exitosamente
- **Dependencias**: T5.2.1, T1.3.1
- **Pistas**: `httpd_resp_send_chunk()` para enviar ficheros grandes sin cargar todo en RAM. Si usas SPIFFS: `esp_vfs_spiffs_register()`, luego abrir fichero normal con `fopen`.
- **Errores comunes**: Intentar cargar el binario completo (1.5MB) en RAM (no cabe). No configurar SPIFFS con suficiente espacio en la tabla de particiones.
- **Tiempo estimado**: 3-4 horas

> **Checkpoint T5.2**: En este punto deberias poder:
> - [ ] Actualizar el firmware de un nodo remotamente desde el gateway
> - [ ] El nodo se conecta al AP del gateway, descarga el firmware, y reinicia
> - [ ] Tras reinicio, el nodo vuelve a operar normalmente
> Si algo no funciona: verifica que el AP del gateway esta accesible, que el binario es del target correcto (esp32c3), y que el nodo tiene suficiente bateria para completar el OTA.

---

## Sub-tarea 5.3: Pruebas de estres y fiabilidad

### Contexto

Un sistema que funciona en el escritorio con 2 nodos puede fallar miserablemente con 10 nodos en un entorno real. Las pruebas de estres revelan problemas de concurrencia, memoria, alcance y fiabilidad que solo aparecen bajo carga.

### Tarea T5.3.1: Test con multiples nodos simultaneos

- **Dificultad**: [Intermedio]
- **Descripcion**: Configurar al menos 5-10 nodos (reales o simulados con scripts que envian datos ESP-NOW) enviando datos simultaneamente al gateway. Ejecutar durante 1 hora. Medir: tasa de paquetes perdidos, latencia media sensor-a-dashboard, uso de memoria del gateway (heap libre), estabilidad MQTT. Si no tienes suficientes nodos fisicos, puedes simular con un ESP32 que envia datos con diferentes MACs falsas.
- **Archivos a crear/modificar**: `docs/test-results/stress-test.md` (resultados)
- **Criterio de aceptacion**:
  - [ ] Tasa de paquetes perdidos < 1% con 10 nodos
  - [ ] Latencia media < 100ms (ESP-NOW hop)
  - [ ] Uso de RAM del gateway < 85% durante toda la prueba
  - [ ] No hay memory leaks (heap libre estable tras 1 hora)
  - [ ] MQTT sigue publicando sin errores
- **Dependencias**: Fases 0-4 completadas
- **Pistas**: `esp_get_free_heap_size()`, `esp_get_minimum_free_heap_size()` para detectar leaks. Logear heap cada 60 segundos. Para simular nodos: un ESP32 puede enviar con `esp_now_send()` hacia el gateway con datos ficticios.
- **Errores comunes**: No medir el heap minimo (un leak lento puede no verse en la hora pero causara crash en dias). No verificar que todos los datos llegan al dashboard (verificar en la base de datos).
- **Tiempo estimado**: 4-6 horas (setup + ejecucion + analisis)

### Tarea T5.3.2: Test de reconexion WiFi

- **Dificultad**: [Basico]
- **Descripcion**: Con el sistema funcionando (gateway + nodos + MQTT): 1) Desconectar el router WiFi. 2) Verificar que ESP-NOW sigue funcionando (los nodos siguen enviando datos al gateway). 3) Verificar que el gateway detecta la desconexion STA y empieza backoff. 4) Reconectar el router. 5) Verificar que el gateway reconecta automaticamente. 6) Verificar que MQTT reconecta y publica los datos que no pudo enviar.
- **Archivos a crear/modificar**: `docs/test-results/reconnection-test.md`
- **Criterio de aceptacion**:
  - [ ] ESP-NOW continua funcionando sin WiFi STA
  - [ ] El gateway detecta la desconexion en < 10 segundos
  - [ ] El backoff exponencial funciona (logs muestran intervalos crecientes)
  - [ ] La reconexion automatica ocurre en < 30 segundos tras volver el router
  - [ ] MQTT se reconecta y los datos fluyen al dashboard
- **Dependencias**: T1.2.2 (backoff exponencial)
- **Pistas**: Observar los logs del gateway con `idf.py monitor`. Verificar los timestamps en la base de datos para detectar gaps.
- **Errores comunes**: Esperar que MQTT reenvie datos del periodo sin conexion (por defecto no lo hace, necesitarias un buffer local). Confundir desconexion STA con desconexion AP (son independientes).
- **Tiempo estimado**: 2-3 horas

### Tarea T5.3.3: Test de ciclo de alimentacion del gateway

- **Dificultad**: [Basico]
- **Descripcion**: Simular un corte de corriente del gateway: desenchufar, esperar 30 segundos, volver a enchufar. Verificar la recuperacion completa: NVS intacta, WiFi reconecta, ESP-NOW se reinicia, nodos vuelven a enviar datos (se reconectan automaticamente), MQTT reconecta. Repetir 5 veces.
- **Archivos a crear/modificar**: `docs/test-results/power-cycle-test.md`
- **Criterio de aceptacion**:
  - [ ] El gateway arranca correctamente tras cada corte
  - [ ] Las credenciales WiFi se recuperan de NVS
  - [ ] Los nodos se vuelven a registrar automaticamente (auto-discovery)
  - [ ] El tiempo total de recuperacion es < 30 segundos
  - [ ] No hay corrupcion de NVS tras 5 ciclos
- **Dependencias**: Fase 1, Fase 2
- **Pistas**: Medir tiempo desde power-on hasta "sistema operativo" (primer dato de sensor publicado por MQTT).
- **Errores comunes**: Asumir que la recovery es instantanea (WiFi tarda unos segundos, SNTP necesita internet, los nodos pueden estar dormidos).
- **Tiempo estimado**: 2-3 horas

### Tarea T5.3.4: Test de autonomia real de bateria

- **Dificultad**: [Intermedio]
- **Descripcion**: Conectar un nodo ESP32-C3 a una bateria 3400mAh 18650 (con TP4056 + proteccion). Configurar wake cada 5 minutos. Medir voltaje de bateria al inicio. Dejar funcionando durante 1 semana minimo. Medir voltaje final. Calcular tasa de descarga y extrapolar autonomia total. Comparar con el calculo teorico de la Fase 0.
- **Archivos a crear/modificar**: `docs/test-results/battery-test.md`
- **Criterio de aceptacion**:
  - [ ] El nodo funciona continuamente durante 1 semana sin intervencion
  - [ ] La tasa de descarga medida permite extrapolar > 12 meses de autonomia
  - [ ] Los datos llegan correctamente al gateway durante toda la prueba
  - [ ] El informe incluye: voltaje inicial/final, consumo estimado, autonomia extrapolada
- **Dependencias**: Fase 0 (benchmarks), Fase 2 (firmware nodo)
- **Pistas**: Voltaje 18650: 4.2V cargada, 3.0V descargada. Si el nodo tiene ADC libre, puedes leer voltaje de bateria con un divisor resistivo y enviarlo como dato adicional.
- **Errores comunes**: No proteger la bateria contra sobredescarga (la celda se daña por debajo de 2.5V). Olvidar que el TP4056 tiene una corriente quiescente que suma al consumo.
- **Tiempo estimado**: 2 horas setup + 1 semana ejecucion + 1 hora analisis

> **Checkpoint T5.3**: En este punto deberias tener:
> - [ ] Resultados documentados de pruebas de estres (10+ nodos)
> - [ ] Resultados de pruebas de reconexion y ciclo de alimentacion
> - [ ] Prueba de bateria en curso o completada
> - [ ] Confianza en que el sistema es estable bajo condiciones adversas

---

## Sub-tarea 5.4: Seguridad

### Contexto

Un sistema IoT en produccion debe proteger los datos en transito y en reposo. Sin cifrado, cualquier persona con un receptor WiFi puede interceptar datos de sensores o enviar comandos falsos a los actuadores.

### Tarea T5.4.1: Cifrado ESP-NOW con PMK y LMK

- **Dificultad**: [Intermedio]
- **Descripcion**: Habilitar cifrado en las comunicaciones ESP-NOW. Configurar una Primary Master Key (PMK) compartida entre gateway y todos los nodos. Configurar Local Master Keys (LMK) por cada par gateway-nodo para cifrado individual. Almacenar las claves en NVS. Verificar que los datos en transito estan cifrados (un tercer ESP32 sin las claves no puede leer los datos).
- **Archivos a crear/modificar**: `firmware/gateway/main/espnow_manager.c`, `firmware/node/main/espnow_node.c`, `firmware/gateway/main/nvs_config.c` (almacenamiento de claves)
- **Criterio de aceptacion**:
  - [ ] ESP-NOW usa cifrado AES-128 (CCMP)
  - [ ] Un nodo sin la PMK/LMK correcta no puede comunicarse con el gateway
  - [ ] Las claves se almacenan en NVS, no hardcodeadas en el codigo
  - [ ] El rendimiento no se degrada significativamente (latencia < 20ms adicionales)
- **Dependencias**: Fase 2 completada
- **Pistas**: `esp_now_set_pmk(pmk)` para la clave primaria. En `esp_now_peer_info_t`, campo `encrypt = true` y campo `lmk` para la clave local del peer. PMK debe ser 16 bytes. LMK debe ser 16 bytes.
- **Errores comunes**: Olvidar que el numero maximo de peers cifrados es 6 (no 20, ese es el total). Si necesitas mas de 6 peers cifrados, algunos deben ser no cifrados. No usar las mismas claves en gateway y nodos.
- **Tiempo estimado**: 3-4 horas

### Tarea T5.4.2: MQTT con TLS

- **Dificultad**: [Avanzado]
- **Descripcion**: Configurar la conexion MQTT entre gateway y broker con TLS (puerto 8883 en lugar de 1883). Generar certificados con OpenSSL: CA propio, certificado del servidor (Mosquitto), opcionalmente certificado cliente (gateway). Configurar Mosquitto para requerir TLS. Embeber el certificado CA en el firmware del gateway.
- **Archivos a crear/modificar**: `firmware/gateway/main/mqtt_bridge.c`, configuracion de Mosquitto (`mosquitto.conf`), directorio de certificados
- **Criterio de aceptacion**:
  - [ ] La conexion MQTT usa TLS (puerto 8883)
  - [ ] Mosquitto rechaza conexiones sin TLS en puerto 8883
  - [ ] El gateway verifica el certificado del broker
  - [ ] Los datos MQTT estan cifrados en transito (verificable con Wireshark: el payload no es legible)
- **Dependencias**: T4.1.1 (MQTT basico funcionando)
- **Pistas**: Generar CA: `openssl req -x509 -newkey rsa:2048 -keyout ca.key -out ca.crt -days 365`. Generar cert servidor: `openssl req -newkey rsa:2048 -keyout server.key -out server.csr`, firmar con CA. En `esp_mqtt_client_config_t`, campo `broker.verification.certificate`. Mosquitto: `listener 8883`, `cafile`, `certfile`, `keyfile`.
- **Errores comunes**: Los certificados auto-firmados requieren que el CA este embebido en el firmware (no se pueden verificar con CAs publicas). Verificar que la fecha del ESP32 es correcta (SNTP) — los certificados tienen fecha de validez. mbedTLS consume ~40KB de RAM por conexion.
- **Tiempo estimado**: 4-6 horas

### Tarea T5.4.3: Autenticacion en web embebida

- **Dificultad**: [Intermedio]
- **Descripcion**: Proteger el panel web del gateway con autenticacion basica HTTP. Al acceder a cualquier pagina o endpoint API, el navegador pedira usuario y password. Las credenciales se almacenan en NVS (password hasheada, no en texto plano). Proteger especialmente los endpoints sensibles: `/api/config`, `/ota/`, `/api/actuator`.
- **Archivos a crear/modificar**: `firmware/gateway/main/http_server.c`, `firmware/gateway/main/nvs_config.c`
- **Criterio de aceptacion**:
  - [ ] Al acceder al panel web sin credenciales, se recibe codigo 401 y el navegador muestra dialogo de login
  - [ ] Con credenciales correctas, se accede normalmente
  - [ ] El password se almacena hasheado en NVS (no en texto plano)
  - [ ] Los endpoints publicos (ej. `/api/status` basico) pueden estar excluidos de la autenticacion si se desea
- **Dependencias**: T1.3.1 (servidor web)
- **Pistas**: HTTP Basic Auth: el cliente envia header `Authorization: Basic base64(user:pass)`. En el handler, decodificar y comparar. Para hash: `mbedtls_md5` o `mbedtls_sha256` (ya incluidos en ESP-IDF). Header de respuesta para pedir auth: `WWW-Authenticate: Basic realm="Gateway"`, status 401.
- **Errores comunes**: HTTP Basic Auth envia credenciales en base64 (NO es cifrado). Sin HTTPS es inseguro en redes abiertas. Para la red local AP del gateway es aceptable en v1.0.
- **Tiempo estimado**: 3-4 horas

> **Checkpoint T5.4**: En este punto deberias tener:
> - [ ] ESP-NOW cifrado entre gateway y nodos
> - [ ] MQTT protegido con TLS
> - [ ] Panel web del gateway protegido con autenticacion
> - [ ] Ninguna comunicacion viaja en texto plano

---

## Sub-tarea 5.5: Documentacion y release

### Contexto

Un proyecto sin documentacion de despliegue es un proyecto que solo puede instalar su creador. Para v1.0, necesitas documentar como poner el sistema en marcha desde cero.

### Tarea T5.5.1: Guia de despliegue

- **Dificultad**: [Basico]
- **Descripcion**: Crear una guia paso a paso para desplegar el sistema completo desde cero. Debe cubrir: 1) Hardware necesario y donde comprarlo, 2) Flashear gateway con firmware, 3) Flashear nodos, 4) Configurar WiFi del gateway via AP, 5) Instalar y configurar Mosquitto, 6) Desplegar backend y dashboard, 7) Calibrar sensores, 8) Verificar que todo funciona end-to-end.
- **Archivos a crear/modificar**: `docs/deployment.md`
- **Criterio de aceptacion**:
  - [ ] Una persona que no ha participado en el desarrollo puede seguir la guia y montar el sistema
  - [ ] Todos los comandos necesarios estan documentados
  - [ ] Incluye troubleshooting basico (problemas comunes y soluciones)
- **Dependencias**: Todas las fases anteriores
- **Pistas**: Pide a alguien que siga la guia y anota donde se atasca.
- **Errores comunes**: Asumir que el lector conoce tu entorno. Documentar paths absolutos en vez de relativos.
- **Tiempo estimado**: 4-6 horas

### Tarea T5.5.2: Scripts de flasheo y release v1.0

- **Dificultad**: [Intermedio]
- **Descripcion**: Crear scripts bash que simplifiquen el flasheo: `flash_gateway.sh` (compila + flashea gateway en un comando), `flash_node.sh` (compila + flashea nodo). Crear tag Git `v1.0` con binarios pre-compilados. Incluir fichero `CHANGELOG.md` con las funcionalidades de v1.0.
- **Archivos a crear/modificar**: `scripts/flash_gateway.sh`, `scripts/flash_node.sh`, `CHANGELOG.md`
- **Criterio de aceptacion**:
  - [ ] `./scripts/flash_gateway.sh /dev/ttyUSB0` compila y flashea el gateway
  - [ ] `./scripts/flash_node.sh /dev/ttyACM0` compila y flashea un nodo
  - [ ] Existe tag Git `v1.0` con los binarios adjuntos
  - [ ] `CHANGELOG.md` lista todas las funcionalidades implementadas
- **Dependencias**: Todas las fases
- **Pistas**: En el script: `cd firmware/gateway && idf.py set-target esp32s3 && idf.py build && idf.py -p $1 flash`. Para release: `git tag -a v1.0 -m "Release v1.0"`.
- **Errores comunes**: No verificar que `IDF_PATH` esta configurado en el script. No manejar errores (si `build` falla, no intentar `flash`).
- **Tiempo estimado**: 2-3 horas

> **Checkpoint T5.5**: En este punto deberias tener:
> - [ ] Guia de despliegue completa y probada
> - [ ] Scripts de flasheo funcionales
> - [ ] Release v1.0 etiquetado en Git
> - [ ] ¡Sistema listo para produccion!

---

## Preguntas de autoevaluacion

1. ¿Por que se necesitan dos particiones de aplicacion (ota_0, ota_1) para OTA? ¿Que pasaria si solo hubiera una?
2. ¿Que ocurre si el nuevo firmware tras OTA no llama a `esp_ota_mark_app_valid_cancel_rollback()` antes del timeout?
3. ¿Por que los nodos no pueden hacer OTA directamente por internet y necesitan usar el AP del gateway?
4. ¿Que diferencia hay entre PMK y LMK en el cifrado ESP-NOW? ¿Por que se usan dos niveles de clave?
5. ¿Por que HTTP Basic Auth no es seguro sin HTTPS? ¿Que podria hacer un atacante en la misma red?
6. ¿Que es un memory leak y como lo detectarias en un sistema embebido que funciona 24/7?
7. ¿Por que el backoff exponencial es mejor que reintentar cada segundo fijo cuando WiFi se pierde?
8. ¿Que ventaja tiene verificar el certificado del servidor MQTT (TLS) frente a no verificarlo?

## Lectura recomendada

- ESP-IDF OTA: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/ota.html
- ESP-NOW cifrado: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/network/esp_now.html#security
- mbedTLS en ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/protocols/mbedtls.html
- Mosquitto TLS: https://mosquitto.org/man/mosquitto-tls-7.html
- ESP-IDF HTTPS OTA example: https://github.com/espressif/esp-idf/tree/master/examples/system/ota/advanced_https_ota
- ESP-IDF Watchdog: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/wdts.html

## Errores frecuentes de esta fase

1. **OTA sin tabla de particiones correcta**: Si flasheas con la tabla "Single factory app", no hay particion ota_1 y el OTA falla. Verifica siempre con `idf.py partition-table` antes de intentar OTA.
2. **Certificado TLS expirado o con fecha futura**: Si SNTP no ha sincronizado la hora y el certificado tiene fecha de validez, la verificacion TLS falla. Asegurate de sincronizar hora antes de conectar MQTT con TLS.
3. **Limite de peers cifrados ESP-NOW**: Son 6, no 20. Si tienes mas de 6 nodos, necesitas una estrategia: peers no cifrados, rotacion de peers, o broadcast sin cifrado peer-to-peer.
4. **Memory leaks en pruebas largas**: Un leak de 100 bytes por hora no se nota en una prueba de 1 hora, pero en 30 dias son 72KB — suficiente para crashear el gateway. Logea `esp_get_minimum_free_heap_size()` periodicamente.
5. **OTA de nodo con binario equivocado**: Flashear un binario compilado para esp32s3 en un esp32c3 (o viceversa) causa un crash inmediato. Verifica siempre el target.
6. **Rollback en bucle infinito**: Si el firmware nuevo siempre crashea antes de confirmarse, y el firmware viejo tambien tiene un bug, puedes entrar en un loop de rollback. Solución: flashear por USB.
