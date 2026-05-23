# Fase 6: Mejoras Futuras

> **CORRECCIONES APLICADAS** (2026-05-23): Relay estatico ya NO se asume como feature existente en v1.0 — se implementa aqui como feature nueva. Se agrega cifrado ESP-NOW por software (AES-256 a nivel de payload) como alternativa al limite de 6 peers cifrados. Ver detalles en `docs/replanificacion/03-fases-corregidas.md`.

## Resumen

- **Objetivo**: Funcionalidades avanzadas post-v1.0 que expanden las capacidades del sistema. Cada mejora es un mini-proyecto independiente.
- **Duracion estimada**: Continuo, sin plazo fijo. Abordar segun prioridad y necesidad.
- **Prerequisitos**: Fase 5 completada (sistema v1.0 estable y desplegado)
- **Nota**: Estas mejoras son opcionales. El sistema v1.0 es completamente funcional sin ellas. Pueden abordarse en cualquier orden.

## Dependencias externas

- Todo lo de fases anteriores
- Modulo SIM7600 (para 4G)
- Modulo tarjeta SD + tarjeta microSD (para backup)
- TensorFlow Lite para entrenamiento de modelos (PC)
- Conocimientos adicionales de ML, networking avanzado, autenticacion web

---

## Sub-tarea 6.1: OTA via ESP-NOW para nodos

### Contexto

En v1.0, la OTA de nodos requiere que se conecten temporalmente al WiFi AP del gateway. Esto funciona pero tiene limitaciones: el nodo debe estar al alcance WiFi del gateway y el proceso consume mucha bateria. OTA via ESP-NOW permitiria actualizar nodos usando el mismo canal de comunicacion que ya usan para datos, sin necesidad de WiFi.

### Tarea T6.1.1: Diseñar protocolo de fragmentacion

- **Dificultad**: [Avanzado]
- **Descripcion**: El firmware de un nodo pesa entre 800KB y 1.5MB. ESP-NOW permite maximo 250 bytes por paquete (230 bytes utiles tras el header del protocolo). Diseñar un protocolo de transferencia de ficheros sobre ESP-NOW que divida el firmware en fragmentos y garantice la entrega completa. Incluir: numero de fragmento, total de fragmentos, CRC-16 por fragmento, ACK por fragmento o por grupo (ventana deslizante), hash SHA-256 del firmware completo para verificacion final. Calcular: con ~4000-6500 fragmentos, ¿cuanto tarda la transferencia? Diseñar mecanismo de retransmision selectiva.
- **Archivos a crear/modificar**: `firmware/common/protocol/ota_espnow_protocol.h` (diseño), `docs/ota-espnow-protocol.md` (documentacion)
- **Criterio de aceptacion**:
  - [ ] Documento de diseño del protocolo con diagramas de estado y secuencia
  - [ ] Calculo de tiempo de transferencia teorico (estimado: 5-15 minutos por nodo)
  - [ ] Mecanismo de recuperacion ante perdida de paquetes definido
  - [ ] Formato de cabecera OTA definido (compatible con el protocolo existente)
- **Dependencias**: Fase 2 y Fase 5 completadas
- **Pistas**: Estudiar protocolos de transferencia como TFTP o XMODEM para inspiracion. Ventana deslizante: enviar N fragmentos, esperar ACK de todos, retransmitir los que fallen. CRC-16 es suficiente por fragmento (rapido), SHA-256 para el firmware completo (seguro).
- **Errores comunes**: No considerar que el nodo esta en deep sleep la mayor parte del tiempo (debe permanecer despierto durante todo el OTA). No considerar colisiones con otros nodos que siguen enviando datos.
- **Tiempo estimado**: 8-12 horas (solo diseño)

### Tarea T6.1.2: Implementar OTA ESP-NOW

- **Dificultad**: [Avanzado]
- **Descripcion**: Implementar el protocolo diseñado en T6.1.1. Gateway: lee el binario del firmware del nodo (desde SPIFFS o descargado del servidor), lo fragmenta y envia secuencialmente por ESP-NOW. Nodo: al recibir comando "inicio OTA", permanece despierto, recibe fragmentos, escribe en particion OTA, envia ACK por cada grupo. Al completar, verifica hash SHA-256, si es correcto aplica OTA y reinicia.
- **Archivos a crear/modificar**: `firmware/gateway/main/ota_espnow_sender.c/.h`, `firmware/node/main/ota_espnow_receiver.c/.h`, `firmware/common/protocol/ota_espnow_protocol.c`
- **Criterio de aceptacion**:
  - [ ] Transferencia completa de firmware de ~1MB en menos de 15 minutos
  - [ ] Verificacion SHA-256 exitosa tras transferencia
  - [ ] Retransmision automatica de fragmentos perdidos
  - [ ] El nodo aplica OTA y reinicia con firmware nuevo
  - [ ] Si la transferencia se interrumpe, el nodo vuelve a modo normal sin quedar bloqueado
- **Dependencias**: T6.1.1
- **Pistas**: El nodo necesita un buffer circular para acumular fragmentos antes de escribir en flash (escribir cada 250 bytes es ineficiente, agrupar en bloques de 4KB). El gateway debe regular la velocidad de envio para no saturar al nodo.
- **Errores comunes**: Escribir en flash demasiado frecuentemente (reduce vida util de la flash). No gestionar timeouts (si el gateway desaparece a mitad del OTA, el nodo debe poder salir del modo OTA tras un timeout).
- **Tiempo estimado**: 20-30 horas

> **Checkpoint T6.1**: OTA via ESP-NOW funcional. Un nodo puede actualizarse sin necesidad de conectarse a WiFi.

---

## Sub-tarea 6.2: Mesh relay automatico

### Contexto

En v1.0, cada nodo debe poder alcanzar directamente al gateway por ESP-NOW (~200m linea de vista, ~50m con obstaculos). En instalaciones grandes, algunos nodos pueden quedar fuera de alcance. Un sistema mesh permitiria que los nodos fuera de alcance envien sus datos a traves de nodos intermedios.

### Tarea T6.2.1: Diseñar protocolo de descubrimiento de rutas

- **Dificultad**: [Avanzado]
- **Descripcion**: Diseñar un protocolo para que los nodos descubran como llegar al gateway a traves de otros nodos. Conceptos necesarios: mensaje ROUTE_REQUEST (broadcast), mensaje ROUTE_REPLY (unicast de vuelta), tabla de vecinos por nodo (MAC, RSSI, hops al gateway), TTL (Time-To-Live) en cada mensaje para evitar bucles infinitos, metrica de ruta (preferir rutas con menos hops y mejor RSSI). Considerar que los nodos estan en deep sleep la mayor parte del tiempo.
- **Archivos a crear/modificar**: `docs/mesh-protocol-design.md`, `firmware/common/protocol/mesh_protocol.h`
- **Criterio de aceptacion**:
  - [ ] Documento de diseño con topologias de ejemplo
  - [ ] Protocolo de descubrimiento definido con diagramas de secuencia
  - [ ] Analisis de overhead: cuanta bateria adicional consume el relay
  - [ ] Estrategia para nodos en deep sleep (no pueden relay mientras duermen)
- **Dependencias**: Fase 2 completada
- **Pistas**: Estudiar protocolos como AODV (Ad-hoc On-demand Distance Vector) para inspiracion, pero simplificar drasticamente. En v1.0 ya existe relay estatico (configurado manualmente). Esta tarea lo hace automatico. Limitacion clave: un nodo en deep sleep no puede hacer relay. Solo nodos "siempre encendidos" o con ciclos de wake muy frecuentes pueden ser relays.
- **Errores comunes**: Diseñar un protocolo demasiado complejo que consume mas bateria de la que ahorra. No considerar que un nodo relay gasta mucha mas bateria que un nodo normal (no puede hacer deep sleep largo).
- **Tiempo estimado**: 10-15 horas

### Tarea T6.2.2: Implementar relay y tabla de rutas

- **Dificultad**: [Avanzado]
- **Descripcion**: Implementar el protocolo de T6.2.1. Cada nodo relay mantiene una tabla de vecinos con RSSI y hops. Cuando un nodo no alcanza el gateway directamente, busca el vecino con mejor ruta y le reenvia el mensaje. El gateway mantiene la topologia completa del sistema. Implementar deteccion de bucles (TTL decrementa en cada salto, se descarta si llega a 0).
- **Archivos a crear/modificar**: `firmware/node/main/mesh_relay.c/.h`, `firmware/gateway/main/mesh_topology.c/.h`, `firmware/common/protocol/mesh_protocol.c`
- **Criterio de aceptacion**:
  - [ ] Un nodo fuera de alcance del gateway puede enviar datos a traves de un nodo intermedio
  - [ ] Las rutas se descubren automaticamente
  - [ ] Si un nodo relay desaparece, se encuentra ruta alternativa
  - [ ] No hay bucles de mensajes (TTL funciona correctamente)
  - [ ] El gateway muestra la topologia de red completa en la API
- **Dependencias**: T6.2.1
- **Pistas**: Empezar con un solo salto (relay directo) antes de implementar multi-hop. Limitar a 3 hops maximo en v1.
- **Errores comunes**: Tormentas de broadcast: si todos los nodos reenvian ROUTE_REQUEST, la red se satura. Limitar con TTL y cooldown entre broadcasts.
- **Tiempo estimado**: 20-30 horas

> **Checkpoint T6.2**: Sistema mesh basico funcionando. Nodos fuera de alcance directo del gateway pueden comunicarse a traves de relays.

---

## Sub-tarea 6.3: Machine Learning edge con TFLite Micro

### Contexto

Los umbrales fijos (alerta si temperatura > 28°C) son utiles pero limitados. Un modelo de ML puede aprender que es "normal" para tu piscifactoria y detectar anomalias sutiles: una caida gradual de oxigeno disuelto, una correlacion inusual entre pH y temperatura, o patrones que preceden a problemas antes de que los umbrales se activen.

### Tarea T6.3.1: Entrenar modelo de deteccion de anomalias

- **Dificultad**: [Avanzado]
- **Descripcion**: Recopilar datos historicos de sensores (al menos 2 semanas de datos normales desde la base de datos del servidor). En un PC con Python + TensorFlow: entrenar un autoencoder simple (red neuronal que aprende a reconstruir datos normales; si no puede reconstruir un dato, es anomalo). Convertir el modelo entrenado a formato TFLite (cuantizado a INT8 para reducir tamaño). Validar con datos de prueba que incluyan anomalias conocidas.
- **Archivos a crear/modificar**: `ml/train_anomaly_detector.py`, `ml/models/anomaly_detector.tflite`, `ml/README.md`
- **Criterio de aceptacion**:
  - [ ] Modelo TFLite de menos de 50KB
  - [ ] Precision > 90% en datos de test (detecta anomalias conocidas)
  - [ ] Menos de 5% de falsos positivos en datos normales
  - [ ] Documentacion del proceso de entrenamiento reproducible
- **Dependencias**: Fase 4 (base de datos con historico de lecturas)
- **Pistas**: Autoencoder con 2-3 capas ocultas. Input: vector de lecturas (temp, pH, DO, ORP, nivel). Si el error de reconstruccion supera un umbral, es anomalia. Cuantizacion: `converter.optimizations = [tf.lite.Optimize.DEFAULT]`, `converter.target_spec.supported_types = [tf.int8]`.
- **Errores comunes**: Entrenar con datos que incluyen anomalias (el modelo las aprendera como "normales"). No cuantizar (modelo float32 es 4x mas grande y lento).
- **Tiempo estimado**: 15-20 horas

### Tarea T6.3.2: Ejecutar modelo en ESP32-S3

- **Dificultad**: [Avanzado]
- **Descripcion**: Integrar TFLite Micro en el firmware del gateway (ESP32-S3). Cargar el modelo desde flash. Cuando llegan lecturas de sensores, ejecutar inferencia. Si el modelo detecta anomalia (error de reconstruccion alto), generar alerta especial "anomalia ML" diferenciada de las alertas por umbral. El S3 tiene instrucciones SIMD que aceleran operaciones de tensores.
- **Archivos a crear/modificar**: `firmware/gateway/main/ml_anomaly.c/.h`, `firmware/gateway/CMakeLists.txt` (añadir TFLite Micro como componente)
- **Criterio de aceptacion**:
  - [ ] El modelo se carga y ejecuta inferencia en el ESP32-S3
  - [ ] Tiempo de inferencia < 100ms por prediccion
  - [ ] Uso de RAM adicional < 50KB
  - [ ] Las alertas ML aparecen en el dashboard con etiqueta diferenciada
- **Dependencias**: T6.3.1
- **Pistas**: ESP-IDF tiene soporte para TFLite Micro como componente. Embeber el modelo `.tflite` con `EMBED_FILES`. Allocator: usar `MicroInterpreter` con buffer estatico de tamaño fijo.
- **Errores comunes**: Buffer del interprete demasiado pequeño (causa crash sin mensaje claro). No normalizar los datos de entrada igual que durante el entrenamiento.
- **Tiempo estimado**: 10-15 horas

> **Checkpoint T6.3**: Deteccion de anomalias por ML funcionando en el gateway. Las alertas ML complementan las alertas por umbral.

---

## Sub-tarea 6.4: Soporte 4G/LTE como backup

### Contexto

Algunas piscifactorias no tienen cobertura WiFi o la conexion es inestable. Un modulo 4G/LTE proporciona conectividad a internet via red movil, independiente de la infraestructura local. Se usa como backup: si WiFi falla, se activa 4G automaticamente.

### Tarea T6.4.1: Integrar modulo SIM7600 con gateway

- **Dificultad**: [Avanzado]
- **Descripcion**: Conectar modulo SIM7600 al ESP32-S3 via UART. Implementar driver basico con comandos AT: verificar SIM insertada (`AT+CPIN?`), registrarse en red movil (`AT+CREG?`), establecer conexion de datos PPP. Usar el componente `esp_modem` de ESP-IDF que simplifica la interaccion con modems. Verificar que tras establecer PPP, el ESP32 puede hacer ping a un servidor externo.
- **Archivos a crear/modificar**: `firmware/gateway/main/cellular_manager.c/.h`, `firmware/gateway/main/CMakeLists.txt` (añadir esp_modem)
- **Criterio de aceptacion**:
  - [ ] El modulo SIM7600 se inicializa correctamente
  - [ ] Se establece conexion de datos PPP
  - [ ] El ESP32 puede resolver DNS y hacer ping via 4G
  - [ ] La conexion es estable durante al menos 1 hora
- **Dependencias**: Fase 1 completada, modulo SIM7600 disponible
- **Pistas**: ESP-IDF `esp_modem` component. Conexion UART: TX del S3 → RX del SIM7600, RX del S3 → TX del SIM7600. Baud rate tipico: 115200. El SIM7600 necesita alimentacion separada (consume picos de 2A).
- **Errores comunes**: Alimentar el SIM7600 desde el pin 3.3V del ESP32 (no tiene corriente suficiente, necesita fuente separada). No esperar a que el modulo arranque completamente antes de enviar comandos AT (~10 segundos).
- **Tiempo estimado**: 8-12 horas

### Tarea T6.4.2: Failover automatico WiFi a 4G

- **Dificultad**: [Avanzado]
- **Descripcion**: Implementar logica de failover: si WiFi STA pierde conexion durante mas de 5 minutos (despues de agotar los reintentos de backoff), activar la conexion 4G automaticamente. MQTT debe reconectar transparentemente a traves de la nueva interfaz de red. Cuando WiFi vuelve, desactivar 4G para ahorrar datos moviles (y coste). Logear cada transicion.
- **Archivos a crear/modificar**: `firmware/gateway/main/connectivity_manager.c/.h` (orquestador de conexiones), `firmware/gateway/main/mqtt_bridge.c` (reconexion transparente)
- **Criterio de aceptacion**:
  - [ ] Si WiFi se pierde > 5 minutos, 4G se activa automaticamente
  - [ ] MQTT reconecta via 4G y los datos fluyen al servidor
  - [ ] Cuando WiFi vuelve, se desactiva 4G y MQTT reconecta via WiFi
  - [ ] Las transiciones son transparentes para los nodos (ESP-NOW no se ve afectado)
  - [ ] El log muestra claramente "WiFi → 4G" y "4G → WiFi"
- **Dependencias**: T6.4.1
- **Pistas**: El `esp_netif` de ESP-IDF permite tener multiples interfaces de red con prioridades. Configurar WiFi STA como prioridad alta y PPP (4G) como prioridad baja. El routing del sistema operativo seleccionara automaticamente la interfaz disponible.
- **Errores comunes**: No considerar que 4G tiene latencia mayor (~100-200ms) que WiFi (~5-10ms). Los timeouts de MQTT pueden necesitar ajuste. El coste de datos moviles puede ser significativo si se publican datos cada pocos segundos.
- **Tiempo estimado**: 8-12 horas

> **Checkpoint T6.4**: El gateway tiene conectividad redundante. Si WiFi falla, 4G toma el relevo automaticamente.

---

## Sub-tarea 6.5: Backup en tarjeta SD

### Contexto

Si MQTT no esta disponible (WiFi caido, broker offline, sin 4G), los datos de sensores se pierden. Un backup en tarjeta SD garantiza que ningun dato se pierde, aunque la conexion al servidor falle durante horas o dias.

### Tarea T6.5.1: Montar sistema de ficheros en SD

- **Dificultad**: [Intermedio]
- **Descripcion**: Conectar modulo de tarjeta SD al ESP32-S3 via SPI. Montar sistema de ficheros FAT en la tarjeta. Verificar que se pueden crear, escribir y leer ficheros. Mostrar espacio total y disponible en el dashboard.
- **Archivos a crear/modificar**: `firmware/gateway/main/sd_manager.c/.h`, `firmware/gateway/main/CMakeLists.txt`
- **Criterio de aceptacion**:
  - [ ] La tarjeta SD se monta correctamente al arrancar
  - [ ] Se pueden crear y leer ficheros
  - [ ] El espacio disponible se muestra en GET /api/status
  - [ ] Si la SD no esta insertada, el sistema funciona normalmente (sin backup)
- **Dependencias**: Fase 1 completada
- **Pistas**: `esp_vfs_fat_sdmmc_mount()` monta la tarjeta como parte del VFS (Virtual File System). Despues se usan `fopen`, `fprintf`, `fclose` como en C estandar. Pines SPI: MOSI, MISO, SCK, CS.
- **Errores comunes**: No formatear la tarjeta como FAT32 antes de usar. No desmontar correctamente (puede corromper la FAT). Los nombres de fichero en FAT32 tienen limite de 8.3 caracteres (o usar LFN).
- **Tiempo estimado**: 3-4 horas

### Tarea T6.5.2: Store-and-forward con SD

- **Dificultad**: [Avanzado]
- **Descripcion**: Implementar patron store-and-forward: cuando MQTT no esta disponible, guardar datos de sensores en ficheros CSV en la SD (un fichero por dia: `2026-02-18.csv`). Cuando MQTT reconecta, leer los ficheros pendientes y publicar los datos con sus timestamps originales. Marcar ficheros como enviados (renombrar o mover a subdirectorio). Implementar rotacion: eliminar ficheros de mas de 30 dias para no llenar la SD.
- **Archivos a crear/modificar**: `firmware/gateway/main/sd_manager.c` (ampliar), `firmware/gateway/main/mqtt_bridge.c` (integrar store-and-forward)
- **Criterio de aceptacion**:
  - [ ] Cuando MQTT esta offline, los datos se guardan en la SD
  - [ ] Cuando MQTT reconecta, los datos pendientes se envian con timestamp original
  - [ ] No se envian datos duplicados
  - [ ] Los ficheros de mas de 30 dias se eliminan automaticamente
  - [ ] El proceso de "vaciado" de la SD no bloquea la recepcion de datos nuevos
- **Dependencias**: T6.5.1, T4.1.2
- **Pistas**: Formato CSV simple: `timestamp,node_mac,sensor_type,value`. Al reconectar MQTT, enviar en lotes (no todo de golpe para no saturar). Usar un fichero `.sent` o directorio `sent/` para marcar ficheros ya reenviados.
- **Errores comunes**: Intentar enviar todo el backlog de golpe al reconectar (satura MQTT y la red). No manejar el caso de SD llena (verificar espacio antes de escribir).
- **Tiempo estimado**: 8-12 horas

> **Checkpoint T6.5**: El gateway no pierde datos aunque pierda conectividad. La SD actua como buffer persistente.

---

## Sub-tarea 6.6: Dashboard multi-usuario y API publica

### Contexto

El dashboard v1.0 no tiene autenticacion (o tiene una basica compartida). Para un despliegue real, se necesitan diferentes usuarios con diferentes permisos, y una API publica para integracion con otros sistemas (ERP de la piscifactoria, sistemas de alerta por SMS, etc.).

### Tarea T6.6.1: Sistema de autenticacion con roles

- **Dificultad**: [Avanzado]
- **Descripcion**: Implementar autenticacion JWT en el backend. Crear tabla de usuarios en SQLite con roles: admin (crear usuarios, configurar todo), operador (ver datos, controlar actuadores), visor (solo lectura). Login endpoint que devuelve JWT. Middleware que verifica JWT en cada peticion y comprueba permisos. Frontend: pagina de login, mostrar/ocultar controles segun rol.
- **Archivos a crear/modificar**: `server/src/auth.ts`, `server/src/middleware.ts`, `server/src/db.ts` (tabla usuarios), `dashboard/src/pages/Login.tsx`, `dashboard/src/context/AuthContext.tsx`
- **Criterio de aceptacion**:
  - [ ] Login funciona con usuario/password, devuelve JWT
  - [ ] Las peticiones sin JWT valido reciben 401
  - [ ] Un visor no puede activar actuadores (403)
  - [ ] Un admin puede crear y gestionar usuarios
  - [ ] El JWT expira tras 24 horas y se requiere re-login
- **Dependencias**: Fase 4 completada
- **Pistas**: JWT: `jsonwebtoken` package. Payload: `{ userId, role, exp }`. Middleware: decodificar header `Authorization: Bearer <token>`, verificar firma, extraer rol. Password hashing: `bcrypt`.
- **Errores comunes**: Almacenar JWT en localStorage del browser (vulnerable a XSS). Mejor: HttpOnly cookie. No hashear passwords (NUNCA almacenar en texto plano).
- **Tiempo estimado**: 10-15 horas

### Tarea T6.6.2: API publica documentada

- **Dificultad**: [Avanzado]
- **Descripcion**: Diseñar API REST publica con documentacion OpenAPI (Swagger). Autenticacion por API key (diferente de JWT de usuarios). Rate limiting (max 100 peticiones/minuto por API key). Endpoints publicos: GET /api/v1/nodes, GET /api/v1/readings, GET /api/v1/alerts. La documentacion se genera automaticamente y se sirve en /docs (Swagger UI).
- **Archivos a crear/modificar**: `server/src/api-v1.ts`, `server/src/rate-limiter.ts`, `server/openapi.yaml`, `server/src/swagger.ts`
- **Criterio de aceptacion**:
  - [ ] Documentacion Swagger accesible en /docs
  - [ ] Autenticacion por API key en header `X-API-Key`
  - [ ] Rate limiting funciona (429 tras exceder limite)
  - [ ] Los endpoints devuelven datos en formato consistente con paginacion
  - [ ] Un sistema externo puede consultar datos de sensores usando la API
- **Dependencias**: T6.6.1
- **Pistas**: OpenAPI/Swagger: usar `swagger-ui-express` o servir el YAML directamente. Rate limiting: `Map<apiKey, {count, resetTime}>` en memoria (o Redis si hay multiples instancias). Paginacion: `?page=1&limit=50`.
- **Errores comunes**: No versionar la API (usar `/api/v1/` para poder cambiar sin romper clientes existentes). No documentar los codigos de error.
- **Tiempo estimado**: 8-12 horas

> **Checkpoint T6.6**: Dashboard con multi-usuario y API publica documentada. El sistema esta listo para integraciones externas.

---

## Preguntas de autoevaluacion

1. ¿Cuantos paquetes ESP-NOW se necesitan para transferir un firmware de 1MB? ¿Cuanto tiempo tardaria asumiendo 10ms por paquete sin perdidas?
2. ¿Por que un nodo en deep sleep no puede actuar como relay mesh? ¿Que compromiso de bateria implica ser un nodo relay?
3. ¿Que ventaja tiene un autoencoder sobre umbrales fijos para detectar anomalias en datos de sensores?
4. ¿Por que se usa un modulo 4G externo en vez de un ESP32 con SIM integrado?
5. ¿Que pasaria con los datos almacenados en la SD si la tarjeta se corrompe? ¿Como mitigarias este riesgo?
6. ¿Por que es importante versionar la API publica (v1, v2)?

## Lectura recomendada

- TFLite Micro en ESP32: https://github.com/espressif/esp-tflite-micro
- ESP-MESH (referencia, diferente de nuestra implementacion): https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/esp-wifi-mesh.html
- ESP-IDF PPP (para modems): https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/network/esp_netif.html
- SIM7600 AT Commands Manual (buscar en el sitio de SIMCom)
- ESP-IDF SD Card: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/storage/sdmmc.html
- OpenAPI Specification: https://swagger.io/specification/
- JWT Introduction: https://jwt.io/introduction

## Notas finales

Si has llegado hasta aqui, ¡enhorabuena! Has construido un sistema IoT completo desde cero: firmware embebido en C, comunicaciones inalambricas, servidor backend, dashboard web, actualizaciones OTA, seguridad, y pruebas de fiabilidad.

Las mejoras de esta fase son opcionales pero representan la evolucion natural del sistema. Cada una resuelve una limitacion real que encontraras en despliegues de produccion. No intentes implementarlas todas a la vez — elige la que mas necesites segun tu instalacion y ve avanzando de forma incremental, igual que has hecho con las fases anteriores.

El sistema v1.0 ya es completamente funcional. Todo lo de esta fase es la diferencia entre un buen sistema y un sistema excelente.
