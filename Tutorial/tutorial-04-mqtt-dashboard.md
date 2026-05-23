# Tutorial 4: Dashboard Embebido, API REST y WebSocket en el ESP32

## Qué vas a aprender

En las fases anteriores construimos la columna vertebral del sistema: los nodos leen sensores, los empaquetan en un protocolo binario eficiente y los envían por ESP-NOW al gateway. Los datos llegan al gateway, pero ahí se quedan: atrapados en el mundo de los microcontroladores, sin forma de que un ser humano los vea desde un navegador.

Este tutorial abre esa "caja negra". Aprenderás a convertir el gateway ESP32-S3 en un servidor web completamente autónomo: sin ordenador externo, sin servidor de aplicaciones externo, sin base de datos externa. El propio microcontrolador sirve el dashboard, expone una API REST y empuja datos en tiempo real. La idea es radical y liberadora: un solo dispositivo de bajo consumo que hace todo.

Los conceptos que aprenderás:

1. **¿Por qué dashboards embebidos?** — Autonomía, operación offline, despliegue de dispositivo único.
2. **API REST en dispositivos limitados** — Nomenclatura de recursos, códigos HTTP, cJSON, formatos de petición y respuesta.
3. **WebSocket: comunicación bidireccional en tiempo real** — Contraste con polling y SSE; ciclo de vida de la conexión, reconexión.
4. **Preact: desarrollo tipo React a 3KB** — Por qué no React (demasiado grande para SPIFFS), por qué no vanilla JS (mantenibilidad).
5. **SPIFFS: sirviendo assets estáticos desde flash** — Distribución de particiones, presupuestos de tamaño, workflow de gzip.
6. **MQTT como integración cloud opcional** — Pub/sub revisitado, namespace configurable, cuándo usarlo.

---

## 1. ¿Por qué dashboards embebidos?

### La analogía del edificio inteligente

Imagina que diseñas un edificio inteligente con sensores de temperatura en cada habitación. Tienes dos opciones arquitectónicas:

**Opción A — Arquitectura centralizada**: Cada sensor envía sus datos a un servidor central en otro edificio. Ese servidor los procesa y sirve el dashboard al operador. El panel de control del edificio depende de que el servidor externo esté en marcha, que la conexión de internet funcione y que nadie haya actualizado mal el software del servidor.

**Opción B — Arquitectura distribuida**: Cada edificio tiene su propio panel de control integrado. Los datos se gestionan localmente. Si falla la conexión a internet, el edificio sigue funcionando y el panel sigue siendo accesible. El servidor externo es opcional, no imprescindible.

Para un sistema IIoT industrial, la Opción B es casi siempre la correcta. La razón: **disponibilidad**. Un sistema de monitorización que deja de funcionar cuando se cae internet no es un sistema de monitorización; es una ilusión de sistema.

### Las tres ventajas del dashboard embebido

**Autonomía total**: El gateway opera sin ninguna dependencia externa. Si falla la red LAN, si el servidor cloud está caído, si el router falla — el gateway sigue recogiendo datos, el dashboard sigue siendo accesible desde cualquier dispositivo conectado al WiFi AP del propio gateway.

```
Dashboard embebido:
  Dispositivo móvil → WiFi AP del gateway → Dashboard

Sin ninguna dependencia de:
  - Servidor externo
  - Internet
  - Base de datos externa
  - Servidor de aplicaciones
```

**Operación offline**: En entornos industriales (almacenes, granjas, instalaciones remotas), la conectividad a internet es frecuentemente intermitente. Un gateway autónomo almacena los últimos datos en memoria y los sirve localmente siempre, independientemente de la conectividad exterior.

**Despliegue de dispositivo único**: Para instalar el sistema basta con flashear el firmware en el ESP32-S3, conectarlo a la red eléctrica y acceder a `http://192.168.4.1` desde cualquier navegador. No hay "stack" de servicios a mantener, no hay Docker, no hay actualizaciones de Node.js, no hay dependencias que queden desactualizadas.

### El trade-off que aceptamos

Esta arquitectura tiene un coste real: **no hay histórico persistente**. El gateway almacena las últimas 100 lecturas por nodo en RAM. Cuando se reinicia, esos datos se pierden. Para histórico a largo plazo, se usa la integración MQTT opcional con un sistema externo.

Es un compromiso consciente: priorizamos autonomía y simplicidad sobre persistencia completa. Para la mayoría de los casos de uso (visualización en tiempo real, alertas, configuración remota), es exactamente el trade-off correcto.

---

## 2. Diseño de APIs REST en dispositivos limitados

### ¿Qué es una API REST?

REST (Representational State Transfer) no es un protocolo sino un estilo arquitectónico. Define cómo organizar los recursos de un sistema como URLs y cómo manipularlos con los verbos HTTP estándar.

La idea central es simple: **los recursos son sustantivos, los verbos HTTP son las acciones**.

```
Mal diseño (verbo en la URL):
  GET /obtener-nodos         ← antipatrón
  GET /getNodes              ← antipatrón
  POST /actualizarConfig     ← antipatrón

Buen diseño REST (sustantivos en la URL):
  GET /api/nodes             ← obtener la lista de nodos
  GET /api/nodes/AA-BB/readings ← obtener lecturas del nodo AA-BB
  POST /api/config           ← crear/actualizar la configuración
  POST /api/ota              ← disparar actualización OTA
```

### Nomenclatura de recursos en ESP-IDF

En un dispositivo embebido, cada endpoint es un handler C registrado en `esp_http_server`. La clave es ser preciso y predecible:

```
/api/status              → estado del gateway
/api/nodes               → lista de nodos con último dato
/api/nodes/{id}/readings → lecturas históricas de un nodo
/api/config              → configuración del gateway
/api/ota                 → trigger de actualización OTA
```

Las reglas prácticas:

- **Todo en minúsculas y con guiones** (`/api/gateway-status`, no `/api/gatewayStatus`).
- **Plural para colecciones** (`/api/nodes`, no `/api/node`).
- **Singular para un elemento específico** (`/api/nodes/{id}`, no `/api/nodes/{ids}`).
- **Sustantivos, no verbos** (nunca `/api/get-nodes`).
- **Rutas jerárquicas para recursos anidados** (`/api/nodes/{id}/readings` para lecturas del nodo `{id}`).

### Códigos de estado HTTP: el lenguaje del servidor

Los códigos HTTP no son decoración: comunican el resultado de cada operación. En un dispositivo embebido con recursos limitados, es tentador devolver siempre 200. Es un error:

| Código | Significado | Cuándo usarlo |
|--------|-------------|---------------|
| 200 OK | Éxito | Petición GET o POST que completó correctamente |
| 202 Accepted | Aceptado (procesamiento diferido) | POST /api/ota — la descarga tarda; responde inmediatamente |
| 400 Bad Request | Petición malformada | JSON inválido, campo fuera de rango, parámetro faltante |
| 404 Not Found | Recurso no encontrado | GET /api/nodes/{id} cuando el nodo no existe |
| 500 Internal Server Error | Error interno | Fallo inesperado en el handler |

Un cliente que recibe 400 sabe que el error es suyo (petición malformada). Un cliente que recibe 500 sabe que el error es del servidor. Esta distinción es fundamental para depurar un sistema.

### cJSON: serialización JSON en C

cJSON viene incluido en ESP-IDF — no es una dependencia externa. Es una librería ligera (un solo `.c` y un `.h`) perfecta para microcontroladores.

**El patrón estándar** para generar una respuesta JSON en un handler HTTP:

```c
// 1. Construir el árbol JSON
cJSON *root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "gateway_id", "GW-AABBCCDD");
cJSON_AddNumberToObject(root, "uptime_s", 3600);
cJSON_AddNumberToObject(root, "free_heap_bytes", 245760);
cJSON_AddBoolToObject(root, "mqtt_connected", false);

// 2. Serializar a string
char *json_str = cJSON_PrintUnformatted(root);

// 3. LIBERAR el árbol cJSON (no libera json_str)
cJSON_Delete(root);

// 4. Enviar la respuesta
httpd_resp_set_type(req, "application/json");
httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
httpd_resp_sendstr(req, json_str);

// 5. LIBERAR el string (cJSON_Delete no lo hace)
free(json_str);
```

El error más común es olvidar alguno de los dos `free`. En un dispositivo que corre 24/7, un leak de 200 bytes por petición acabará agotando los 320KB de RAM del ESP32-S3 en cuestión de horas.

### Formato de petición y respuesta

Una API bien documentada especifica exactamente qué envía y qué recibe. Para el gateway, el formato estándar de respuesta de error es:

```json
{
  "error": "invalid_config",
  "detail": "alert_temp_max must be between -40 and 85"
}
```

Y el de éxito siempre incluye al menos un campo `status`:

```json
{
  "status": "saved",
  "reboot_required": true
}
```

Esta consistencia permite al cliente JavaScript del dashboard manejar errores de forma genérica sin conocer cada posible respuesta.

---

## 3. WebSocket: comunicación bidireccional en tiempo real

### El problema del polling

HTTP fue diseñado para un modelo petición-respuesta: el cliente pregunta, el servidor responde, la conexión se cierra. Para un dashboard que muestra datos en tiempo real, esto es un problema.

La solución ingenua es el **polling**: el cliente pregunta cada N segundos si hay datos nuevos.

```
Polling cada 2 segundos:
  Cliente → "¿Hay datos nuevos?" → Servidor: "No"   [500ms]
  (espera 2 segundos)
  Cliente → "¿Hay datos nuevos?" → Servidor: "No"   [500ms]
  (espera 2 segundos)
  Cliente → "¿Hay datos nuevos?" → Servidor: "Sí"   [500ms]
  (espera 2 segundos)
  ...
```

Los problemas del polling son múltiples:
- **Latencia**: el dato puede tardar hasta N segundos en aparecer en pantalla.
- **Recursos**: el servidor responde a peticiones vacías constantemente.
- **Overhead**: cada petición HTTP lleva cabeceras de cientos de bytes, aunque el payload sea 0 bytes.

### Server-Sent Events (SSE): un paso intermedio

SSE permite que el servidor empuje datos al cliente sin que el cliente pregunte. Es unidireccional (servidor → cliente). Es más eficiente que el polling, pero no permite al cliente enviar datos al servidor por el mismo canal.

Para un dashboard que solo lee datos, SSE es perfectamente válido. Para un dashboard que también envía comandos (configuración, OTA), necesitamos bidireccionalidad.

### WebSocket: la conexión permanente

WebSocket establece una conexión persistente y bidireccional sobre TCP. Después del handshake inicial (que usa HTTP), la conexión se mantiene abierta indefinidamente. Cualquiera de los dos lados puede enviar mensajes en cualquier momento.

```
HTTP (polling):
  Cliente: "Hay datos?"  →  Servidor: "No"
  Cliente: "Hay datos?"  →  Servidor: "No"
  Cliente: "Hay datos?"  →  Servidor: "Sí, aquí tienes: 24.5°C"

WebSocket:
  Cliente: "Quiero conectarme al WebSocket"
  Servidor: "Conexión abierta"
  (... silencio hasta que hay datos ...)
  Servidor: "Dato nuevo: 24.5°C"
  (... silencio ...)
  Servidor: "Dato nuevo: 24.7°C"
  Cliente: "Cambia el umbral de temperatura a 30°C"
  Servidor: "Configuración guardada"
```

Comparación de los tres enfoques:

| Característica | Polling | SSE | WebSocket |
|----------------|---------|-----|-----------|
| Dirección | Cliente → Servidor | Servidor → Cliente | Bidireccional |
| Overhead | Alto (cabeceras HTTP en cada petición) | Bajo (conexión persistente) | Muy bajo (frames de 2-14 bytes de cabecera) |
| Latencia | Hasta N segundos | Inmediata | Inmediata |
| Complejidad de implementación | Mínima | Baja | Media |
| Soporte en esp_http_server | Sí | No nativo | Sí (desde ESP-IDF 4.4) |

Para el gateway ESP32-S3, WebSocket es la elección correcta porque:
1. `esp_http_server` lo soporta nativamente.
2. El dashboard necesita comunicación bidireccional (leer datos + enviar configuración).
3. El overhead mínimo importa en un dispositivo que puede tener hasta 8 clientes simultáneos.

### Ciclo de vida de la conexión WebSocket

```
1. APERTURA:
   Navegador → HTTP GET /ws + cabecera "Upgrade: websocket"
   Gateway   → HTTP 101 Switching Protocols
   [Conexión WebSocket abierta]

2. COMUNICACIÓN:
   Gateway   → { "type": "sensor_data", "value": 24.5, ... }
   Navegador → { "type": "set_config", "alert_temp_max": 30.0 }
   Gateway   → { "type": "config_saved" }

3. CIERRE (normal):
   Navegador → Frame de cierre WebSocket
   Gateway   → Frame de cierre de confirmación
   [Conexión cerrada, fd liberado]

4. CIERRE (abrupto — pérdida de red):
   [El gateway detecta que el fd ya no responde]
   [Gateway elimina el fd de la lista de clientes activos]
```

### Reconexión: el detalle que marca la diferencia

El estándar WebSocket no define reconexión automática. Cuando la conexión se cierra (reinicio del gateway, cambio de red WiFi), el navegador no intenta reconectarse por sí solo. Debes implementarlo:

```javascript
// El patrón de reconexión con backoff exponencial:
function conectarWebSocket() {
  const ws = new WebSocket('ws://' + window.location.host + '/ws');

  ws.onmessage = (e) => {
    const dato = JSON.parse(e.data);
    // Procesar el dato...
  };

  ws.onclose = () => {
    // Esperar 3 segundos y reconectar
    // (en producción, usar backoff exponencial: 1s, 2s, 4s, 8s...)
    setTimeout(conectarWebSocket, 3000);
  };

  return ws;
}
```

Sin lógica de reconexión, el operador tiene que recargar la página manualmente cada vez que el gateway se reinicia. En un entorno industrial, eso es inaceptable.

---

## 4. Preact: desarrollo tipo React a 3KB

### El problema de tamaño de React

React es una excelente librería para construir interfaces de usuario. En la web tradicional, un paquete de build de React de 40KB gzipados es perfectamente aceptable: se descarga en décimas de segundo con una conexión de 10Mbps y el navegador lo cachea.

En un sistema embebido con 1.9MB de SPIFFS y un WiFi AP que comparte ancho de banda con los clientes conectados, la ecuación cambia:

```
Partición SPIFFS total:       1,900 KB
React gzipado:                  ~40 KB
React-DOM gzipado:              ~40 KB
React Router gzipado:           ~15 KB
Librería de gráficos (recharts): ~70 KB
──────────────────────────────────────
Solo en dependencias:          ~165 KB

Queda para lógica de la app:  ~1,735 KB

Pero el paquete de build total (incluyendo la app):
  ≈ 200-400 KB gzipado con React
```

Comparado con Preact:

```
Preact gzipado:                  ~3 KB
Preact/hooks gzipado:            ~1 KB
Router mínimo (hash-based):      ~2 KB
──────────────────────────────────────
Solo en dependencias:             ~6 KB

Paquete total con la app:
  ≈ 20-80 KB gzipado con Preact
```

La diferencia es de 10-20x en el tamaño del paquete de build. En un dispositivo con 1.9MB de SPIFFS, eso pasa de "el dashboard ocupa el 20% de la partición" a "el dashboard ocupa el 1.5% de la partición".

### ¿Por qué no vanilla JavaScript?

La otra opción sería no usar ningún framework: HTML + CSS + JavaScript puro. Es tentador porque "menos código es mejor código".

El problema emerge cuando la aplicación crece:

```javascript
// Con vanilla JS, actualizar el DOM manualmente:
function actualizarTarjetaNodo(nodo) {
  const div = document.getElementById('nodo-' + nodo.id);
  div.querySelector('.valor').textContent = nodo.valor;
  div.querySelector('.unidad').textContent = nodo.unidad;
  div.querySelector('.status').className = 'status ' +
    (Date.now()/1000 - nodo.last_seen < 120 ? 'activo' : 'inactivo');
  // ... 10 líneas más para cada campo
}

// Y si el nodo no existe en el DOM todavía:
function crearTarjetaNodo(nodo) {
  const div = document.createElement('div');
  div.id = 'nodo-' + nodo.id;
  div.innerHTML = `<div class="valor">${nodo.valor}</div>...`;
  document.getElementById('grid').appendChild(div);
}

// Y si hay que eliminar un nodo que ya no existe:
function eliminarTarjetaNodo(id) {
  const div = document.getElementById('nodo-' + id);
  if (div) div.parentNode.removeChild(div);
}
```

Con 10 nodos, cada uno con 5 tipos de sensor, y 3 estados posibles, el código vanilla se vuelve imposible de mantener. El DOM manual es frágil: un bug en `actualizarTarjetaNodo` que no actualiza el DOM correctamente puede dejar la interfaz en un estado inconsistente sin que sea fácil detectarlo.

Preact resuelve esto con el mismo modelo declarativo de React:

```jsx
// Con Preact: declaras CÓMO debe verse el DOM dado el estado
function TarjetaNodo({ nodo }) {
  const activo = Date.now()/1000 - nodo.last_seen_epoch < 120;
  return (
    <div class={`tarjeta ${activo ? 'activo' : 'inactivo'}`}>
      <span class="id">{nodo.node_id.slice(-8)}</span>
      {nodo.last_readings.map(r => (
        <span class="lectura">{r.value} {r.unit}</span>
      ))}
    </div>
  );
}
```

Preact calcula automáticamente qué partes del DOM necesitan actualizarse. El desarrollador declara el estado deseado; Preact se encarga de las mutaciones DOM.

### Preact es compatible con React

Preact implementa la misma API que React. Los hooks `useState`, `useEffect`, `useRef`, `useContext` funcionan igual. Los componentes JSX son idénticos. La diferencia es únicamente el tamaño del runtime.

Si un proyecto crece y necesita más características de React (Suspense avanzado, Server Components), migrar de Preact a React es un cambio de import, no una reescritura.

Para el dashboard embebido del gateway — una aplicación de unas pocas páginas y componentes simples — Preact es la elección técnicamente correcta y suficiente.

---

## 5. SPIFFS: sirviendo assets estáticos desde flash

### Qué es SPIFFS

SPIFFS (SPI Flash File System) es un sistema de archivos diseñado para memorias flash NOR — el tipo de memoria que usa el ESP32. Permite montar una partición de la flash del dispositivo como un sistema de archivos convencional y acceder a los archivos con las funciones estándar de C (`fopen`, `fread`, `fwrite`).

En el gateway, la partición SPIFFS almacena los assets del dashboard Preact:

```
Contenido de la partición SPIFFS (montada en /spiffs):
  /spiffs/index.html.gz       ← página principal
  /spiffs/assets/main.js.gz   ← lógica de la app
  /spiffs/assets/main.css.gz  ← estilos
  /spiffs/favicon.ico.gz      ← icono del navegador
```

El servidor HTTP lee estos archivos y los sirve al navegador cuando este accede a `http://192.168.4.1/`.

### Distribución de particiones en el ESP32-S3

El ESP32-S3 del gateway tiene 8MB de flash. La distribución en `partitions.csv`:

```
# Name,   Type, SubType, Offset,   Size
nvs,      data, nvs,     0x9000,   24 KB    ← configuración NVS
otadata,  data, ota,     0xf000,    8 KB    ← metadatos OTA
ota_0,    app,  ota_0,   0x10000, 3072 KB   ← firmware activo
ota_1,    app,  ota_1,   0x310000, 3072 KB  ← firmware OTA
spiffs,   data, spiffs,  0x610000, 1920 KB  ← assets del dashboard
```

Visualizado:

```
┌─────────────────────────────────────────────────────┐
│ Flash 8MB                                           │
│                                                     │
│ [NVS 24K][OTA meta 8K][Firmware A 3MB][Firmware B 3MB][SPIFFS 1.9MB] │
│                                                     │
└─────────────────────────────────────────────────────┘
```

El espacio de SPIFFS (1.9MB) es generoso para un dashboard embebido. Un paquete de build Preact bien optimizado ocupa 20-80KB gzipado, dejando más del 95% del espacio libre.

### Presupuesto de tamaño y por qué importa

Aunque 1.9MB parece mucho para un dashboard sencillo, los presupuestos de tamaño son útiles porque:

1. **Disciplina de diseño**: tener un presupuesto explícito evita que "solo esta librería de gráficos" acabe costando 200KB extra.
2. **Tiempo de carga**: los navegadores que se conectan al WiFi AP del gateway pueden tener ancho de banda limitado (otros usuarios, distancia al AP). Un paquete de build más pequeño carga más rápido.
3. **Mantenibilidad**: un presupuesto de tamaño fuerza a elegir dependencias ligeras desde el principio.

El presupuesto para este proyecto:

| Asset | Objetivo | Límite duro |
|-------|----------|-------------|
| JS principal gzipado | ≤150KB | 400KB |
| CSS gzipado | ≤20KB | 50KB |
| Total gzipado | ≤200KB | 500KB |

### El workflow de gzip

Los navegadores modernos aceptan assets comprimidos con gzip si el servidor incluye la cabecera `Content-Encoding: gzip`. Esto es estándar y transparente para el usuario.

El proceso en el gateway:

```
Desarrollo:
  firmware/gateway/web/src/ (fuente Preact JSX)
       ↓ npm run build (Vite)
  firmware/gateway/web/dist/assets/main.js    (JS sin comprimir)
       ↓ gzip -9
  firmware/gateway/web/dist/assets/main.js.gz (JS gzipado)
       ↓ spiffsgen.py
  spiffs.bin (imagen de la partición)
       ↓ esptool.py write_flash 0x610000
  ESP32-S3 (SPIFFS montada en /spiffs)

En tiempo de ejecución:
  Navegador → GET /assets/main.js + "Accept-Encoding: gzip"
  ESP32-S3  → abre /spiffs/assets/main.js.gz
           → envía con Content-Encoding: gzip
  Navegador → descomprime y ejecuta el JS
```

La ventaja: el ESP32-S3 nunca comprime en tiempo real (operación costosa). Los archivos ya están comprimidos en flash; solo hay que servirlos directamente.

### Limitación importante de SPIFFS en ESP32

SPIFFS está diseñado para sistemas con escrituras infrecuentes. La memoria flash NOR soporta aproximadamente 100.000 ciclos de escritura por sector. Si escribes un archivo cada segundo, agotarás la flash en aproximadamente 28 horas.

Por eso, los assets del dashboard se escriben solo en el momento del flash (una vez) y no durante la operación. Para datos dinámicos (configuración, lecturas de sensores), se usa NVS (NVS está optimizado para escrituras frecuentes) o la RAM del sistema.

---

## 6. MQTT como integración cloud opcional

### Pub/sub revisitado

MQTT usa el modelo publicar/suscribir. Un publicador envía un mensaje a un topic; un broker lo recibe y lo distribuye a todos los suscriptores de ese topic. Los publicadores y suscriptores no se conocen entre sí.

```
Gateway (publicador)  →  Broker MQTT  →  Sistema de históricos (suscriptor)
                                      →  Sistema de alertas (suscriptor)
                                      →  Dashboard en la nube (suscriptor)
```

En la arquitectura de este kit, el gateway es el único publicador de datos de sensor. El broker actúa como relay. Los suscriptores pueden ser cualquier sistema que consuma datos (una base de datos de series temporales, un sistema de alertas empresarial, un dashboard en la nube para múltiples instalaciones).

### Namespace configurable: la clave para sistemas multi-dominio

El problema con los prefijos MQTT hardcodeados es que no escalan. Si el prefijo está fijo como `"sensor-data/"`, todos los gateways del mismo broker comparten el mismo espacio de topics, y es imposible distinguir qué instalación es cuál.

La solución es el namespace configurable via NVS:

```
Clave NVS: mqtt_namespace
Valor por defecto: "iiot-kit"

Topic resultante:
  {mqtt_namespace}/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}

Ejemplos:
  iiot-kit/GW-001/nodo/AA:BB/temperatura    ← instalación por defecto
  invernadero/GW-002/nodo/CC:DD/humedad     ← instalación de invernadero
  fabrica-norte/GW-003/nodo/EE:FF/presion   ← instalación industrial
```

Cada instalación tiene su propio espacio de nombres. Si un solo broker MQTT gestiona múltiples instalaciones (varios gateways de distintos clientes o ubicaciones), los datos de cada una están completamente separados. Los suscriptores de `invernadero/#` no ven los datos de `fabrica-norte/#`.

Configurar el namespace no requiere recompilar el firmware. El operador entra al panel de configuración del gateway (`http://192.168.4.1/config`) y cambia el campo `mqtt_namespace`. El gateway guarda el nuevo valor en NVS y usa el namespace actualizado en todos los topics en el próximo reinicio.

### Cuándo activar MQTT

MQTT no siempre es necesario. El dashboard embebido + API REST + WebSocket cubren el 100% de los casos de uso locales. MQTT añade valor cuando:

- **Necesitas histórico a largo plazo**: el gateway no almacena histórico en disco. Un broker MQTT + InfluxDB o TimescaleDB proporciona histórico ilimitado.
- **Tienes múltiples instalaciones que centralizar**: un solo broker puede recibir datos de 50 gateways en distintas ubicaciones. Sin MQTT, hay que consultar cada gateway individualmente.
- **Necesitas integración con sistemas existentes**: muchos sistemas SCADA, plataformas IoT y soluciones de monitorización empresarial hablan MQTT de forma nativa.
- **Quieres alertas externas**: enviar una notificación a un sistema de guardia (PagerDuty, Telegram bot, email) cuando un sensor supera un umbral es más sencillo suscribiéndose al topic de alertas del broker que haciendo polling a la API REST del gateway.

Si ninguno de estos casos aplica, MQTT añade complejidad operacional (mantener un broker) sin beneficio real. El dashboard embebido es suficiente.

### El sistema funciona sin MQTT: esta es la garantía de autonomía

La frase más importante sobre la integración MQTT es ésta: si no configuras el broker, el gateway funciona exactamente igual. Los datos de sensores llegan, el dashboard los muestra, las alertas se activan. MQTT es una extensión, no un requisito.

```c
// En app_main.c — solo se inicializa MQTT si hay broker configurado:
char broker_url[256] = {0};
if (nvs_get_str(handle, "mqtt_broker", broker_url, &len) == ESP_OK
    && strlen(broker_url) > 0) {
    mqtt_bridge_init(broker_url);
    ESP_LOGI(TAG, "MQTT activo: %s", broker_url);
} else {
    ESP_LOGI(TAG, "MQTT no configurado — modo standalone");
}
```

Esta decisión de diseño — hacer MQTT condicional — es lo que permite que el sistema sea autónomo por defecto y extensible por configuración.

---

## Cómo encajan las piezas

Al terminar la Fase 4, habrás construido un sistema donde el ESP32-S3 gateway es genuinamente autónomo:

```
┌────────────────────────────────────────────────────┐
│             Gateway ESP32-S3 autónomo              │
│                                                    │
│  Flash (8MB):                                      │
│  ├── Firmware (6MB OTA): toda la lógica C          │
│  └── SPIFFS (1.9MB): assets del dashboard Preact   │
│                                                    │
│  RAM (320KB):                                      │
│  ├── Tareas FreeRTOS: ESP-NOW, HTTP, MQTT          │
│  └── Ring buffers: últimas 100 lecturas por nodo   │
│                                                    │
│  Interfaces de red:                                │
│  ├── WiFi AP: sirve el dashboard localmente        │
│  └── WiFi STA: conecta a internet para MQTT (opt.) │
└────────────────────────────────────────────────────┘
```

El viaje completo de un dato:

```
1. Sensor mide temperatura → Nodo ESP32-C3 empaqueta en protocolo binario
2. Nodo envía por ESP-NOW → Gateway lo recibe
3. Gateway almacena en ring buffer en RAM
4. Gateway hace push via WebSocket → Navegador actualiza la tarjeta del nodo
5. [Opcional] Gateway publica en MQTT → broker → sistemas externos
```

El operador no necesita ir físicamente al nodo para saber qué está midiendo. No necesita un PC con software especializado. Solo necesita un navegador web y estar en la misma red WiFi que el gateway.

---

## Experimentos

> **Experimento 1: Medir el tamaño del paquete de build en cada iteración**
>
> Después de cada cambio significativo en `firmware/gateway/web/src/`, ejecuta `npm run build` y comprueba el tamaño total gzipado con `du -sh dist/assets/*.gz`. Observa cómo cada dependencia que añades (una librería de iconos, un componente extra, una fuente) afecta al tamaño. Intenta mantener el paquete de build por debajo de 100KB gzipado. ¿Qué pasa cuando añades una librería de gráficos completa? ¿Podrías usar SVG dibujado a mano para un gráfico simple en lugar de Recharts?

> **Experimento 2: Comparar latencia WebSocket vs. polling**
>
> Implementa primero el dashboard con polling (fetch cada 2 segundos a `/api/nodes`). Mide cuánto tiempo tarda un dato en aparecer en pantalla desde que el nodo lo envía. Luego migra a WebSocket. Mide la latencia de nuevo. ¿Cuál es la diferencia? ¿A qué se debe? (Pista: con polling, el peor caso es 2 segundos de espera más el tiempo de la petición HTTP.)

> **Experimento 3: Simular un corte de red y observar la reconexión WebSocket**
>
> Con el dashboard abierto en el navegador, reinicia el gateway (`idf.py monitor` + `Ctrl+T Ctrl+R`). Observa qué pasa en el dashboard: ¿se congela? ¿muestra un error? ¿vuelve a conectarse automáticamente? Implementa el hook `useWebSocket` con reconexión automática y repite el experimento. ¿Cuánto tarda en reconectarse? ¿Qué datos se perdieron durante la reconexión?

> **Experimento 4: El trade-off del ring buffer**
>
> El gateway almacena las últimas 100 lecturas por nodo en RAM. ¿Cuánta RAM ocupa eso si tienes 20 nodos, cada uno enviando temperatura y humedad (2 sensores)? Calcula: 20 nodos × 2 sensores × 100 lecturas × (tamaño de cada lectura en bytes). ¿Cabe en los 320KB de RAM disponibles para la aplicación? ¿Qué pasa si aumentas el número de lecturas almacenadas a 1000? ¿Cuántos nodos podrías soportar con el presupuesto de RAM actual?

> **Experimento 5: Configurar el namespace MQTT**
>
> Con un broker MQTT funcionando en tu red local, conecta al panel de configuración del gateway, establece el broker URL y pon un namespace personalizado (por ejemplo, `"mi-instalacion"`). Suscríbete con `mosquitto_sub -t "mi-instalacion/#" -v` y observa cómo los datos llegan con el prefijo correcto. Luego cambia el namespace a `"prueba"`, reinicia el gateway y verifica que los nuevos datos usan el nuevo namespace.

---

## Preguntas de reflexión

**1.** Hemos dicho que el gateway no almacena histórico en flash porque la memoria NOR tiene ~100.000 ciclos de escritura por sector. Si un sector es de 4KB y una lectura de sensor ocupa 20 bytes, ¿cuántas lecturas caben en un sector? Si se escribe en el mismo sector con cada lectura (sin distribución de desgaste), ¿cuánto tarda en agotarse con una lectura cada 30 segundos? ¿Cómo cambia el cálculo si usas un sistema con distribución de desgaste como NVS?

**2.** Preact ocupa ~3KB gzipado vs. React ~40KB. Sin embargo, ambas librerías implementan el mismo concepto (componentes reactivos, virtual DOM). ¿Qué tiene que sacrificar Preact para ser 13x más pequeño? Investiga qué características de React no están en Preact (o están solo como add-on). ¿Alguna de las características faltantes es necesaria para el dashboard de este proyecto?

**3.** Un WebSocket consume recursos en el gateway mientras está abierto: memoria para el fd, CPU para el heartbeat, etc. El gateway soporta hasta 4 clientes simultáneos. ¿Qué pasaría si un operador deja 10 pestañas abiertas en el navegador? ¿Qué mecanismo implementarías para gestionar este caso? ¿Debería el gateway rechazar la conexión número 5 o desconectar al cliente más antiguo?

**4.** La API REST del gateway devuelve JSON construido con cJSON en el ESP32. ¿Qué ventajas tendría usar MessagePack (formato binario) en lugar de JSON? ¿Cuáles serían los inconvenientes para un developer que necesita depurar la API con curl? ¿Tiene sentido usar formato binario en la API REST del gateway?

**5.** El dashboard embebido es accesible desde cualquier dispositivo conectado al WiFi AP del gateway. ¿Qué implicaciones de seguridad tiene esto? ¿Debería protegerse el dashboard con usuario y contraseña? ¿Cómo implementarías autenticación básica en `esp_http_server`? ¿Cuáles son los trade-offs de añadir autenticación en un sistema de emergencia/configuración?

**6.** La integración MQTT es opcional. Imagina que el sistema está desplegado en una instalación remota sin internet durante 6 meses. ¿Qué datos se perdieron al no tener MQTT configurado? ¿Podría el gateway compensar esto almacenando las lecturas en SPIFFS durante los periodos sin conectividad y enviándolas por MQTT en lotes cuando recupera la conexión? ¿Qué implicaciones tiene para el ciclo de vida de la flash?

---

## Para profundizar

- **[esp_http_server — Documentación ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/esp_http_server.html)** — La sección sobre WebSocket es especialmente relevante. Estudia los ejemplos `ws_echo_server` incluidos en los ejemplos de ESP-IDF.

- **[SPIFFS — Documentación ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/spiffs.html)** — Explica las limitaciones de escritura, cómo montar la partición y cómo generar imágenes SPIFFS con `spiffsgen.py`.

- **[Documentación oficial de Preact](https://preactjs.com/guide/v10/getting-started/)** — La guía de inicio, el tutorial interactivo y la sección de Signals. Si ya conoces React, la migración es inmediata.

- **[Vite — Guía de configuración del build](https://vite.dev/config/build-options.html)** — Opciones para optimizar el output: `assetsInlineLimit`, `rollupOptions`, análisis de paquete de build con plugins.

- **[WebSocket API — MDN](https://developer.mozilla.org/es/docs/Web/API/WebSockets_API)** — La referencia completa del navegador: `WebSocket()`, `onopen`, `onmessage`, `onclose`, `onerror`, `send()`, y el protocolo de cierre.

- **[RFC 6455 — The WebSocket Protocol](https://datatracker.ietf.org/doc/html/rfc6455)** — La especificación formal. La sección sobre el handshake (sección 4) explica exactamente qué cabeceras HTTP se intercambian al abrir una conexión WebSocket.

- **[cJSON — GitHub](https://github.com/DaveGamble/cJSON)** — README con todos los ejemplos de uso. Especialmente útil la sección sobre gestión de memoria y el patrón `cJSON_Delete` + `free`.

- **[MQTT.org](https://mqtt.org/)** — Conceptos fundamentales: publish/subscribe, QoS, retained messages, Last Will and Testament. La especificación MQTT 5.0 añade aliases de topic y otros mecanismos de eficiencia.

- **[HiveMQ MQTT Essentials](https://www.hivemq.com/mqtt-essentials/)** — Serie de artículos cortos y muy claros. Artículo 6 (retained messages) y artículo 4 (QoS) son los más relevantes para este proyecto.
