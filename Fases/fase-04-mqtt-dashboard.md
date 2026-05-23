# Fase 4: Dashboard Embebido + API REST

> **Arquitectura actualizada** (2026-05-23): Esta fase ha sido completamente rediseñada. El dashboard ya no requiere un servidor externo (Bun/Node/React). El gateway ESP32-S3 es completamente autónomo: sirve una SPA Preact desde SPIFFS, expone una API REST y un WebSocket para datos en tiempo real. MQTT es una integración opcional para cloud o LAN.

## Resumen general

- **Objetivo**: Convertir el gateway ESP32-S3 en un servidor web autónomo que sirva un dashboard interactivo desde su propia memoria flash, exponiendo una API REST para datos y configuración, y un WebSocket para actualizaciones en tiempo real.
- **Duración**: 4 semanas
- **Prerequisitos**: Fases 0, 1, 2 y 3 completadas (gateway con WiFi AP/STA, ESP-NOW funcionando, nodos enviando datos de sensores, alertas configuradas)
- **Hardware necesario**: ESP32-S3 gateway (8MB flash, 2MB PSRAM), ESP32-C3 nodos con sensores, cualquier dispositivo con navegador web en la misma red WiFi

## Arquitectura de la Fase 4

```
┌──────────────────────────────────────────────────────────────┐
│                    Gateway ESP32-S3                          │
│                                                              │
│  ┌──────────┐    ┌─────────────┐    ┌─────────────────────┐  │
│  │  SPIFFS  │    │ esp_http_   │    │  espnow_manager     │  │
│  │ (Preact  │    │ server      │    │  (recibe datos de   │  │
│  │  SPA)    │    │ REST + WS   │    │   nodos sensor)     │  │
│  └────┬─────┘    └──────┬──────┘    └──────────┬──────────┘  │
│       │                 │                       │             │
│       └─────────────────┴───────────────────────┘            │
│                         │                                    │
│                [WiFi AP: 192.168.4.1]                        │
│                         │                                    │
└─────────────────────────┼────────────────────────────────────┘
                          │
               ┌──────────┴──────────┐
               │                     │
          [Navegador]           [Opcional: MQTT]
          cualquier            broker cloud/LAN
          dispositivo          (configurable)
          en la red WiFi

Nodo ESP32-C3  ──[ ESP-NOW ]──►  Gateway ESP32-S3
```

El flujo de datos completo es:

```
Sensor → Nodo ESP32-C3 → [ESP-NOW] → Gateway ESP32-S3
                                           │
                          ┌────────────────┼────────────────┐
                          │                │                │
                       SPIFFS           REST API        WebSocket
                     (Preact SPA)     GET /api/nodes   ws://gw/ws
                          │                │                │
                          └────────────────┼────────────────┘
                                           │
                                    Navegador web
                                 (dashboard en tiempo real)
                                           │
                               [Opcional: MQTT → cloud]
```

---

## Sub-tarea 4.1: API REST en el Gateway ESP32-S3 (3 tareas)

La API REST permite que el dashboard (y cualquier cliente HTTP) consulte el estado del gateway y sus nodos, configure parámetros y dispare actualizaciones OTA. Se implementa con `esp_http_server`, que ya está disponible en ESP-IDF sin dependencias adicionales.

### Tarea T4.1.1: Diseñar e implementar los endpoints REST

- **Dificultad**: Intermedio
- **Descripción**: Crear el módulo `http_api` que registra los handlers REST en el servidor HTTP del gateway. El servidor ya existe desde la Fase 1; en esta tarea se añaden los endpoints de datos.

**Endpoints a implementar**:

#### GET /api/status

Estado general del gateway.

```
Petición: GET /api/status
Cabeceras: ninguna requerida

Respuesta 200 OK:
{
  "gateway_id": "GW-AABBCCDD",
  "uptime_s": 3600,
  "free_heap_bytes": 245760,
  "wifi_rssi_dbm": -62,
  "wifi_ap_clients": 2,
  "espnow_nodes_registered": 3,
  "espnow_nodes_active": 2,
  "mqtt_connected": false,
  "firmware_version": "1.0.0"
}
```

#### GET /api/nodes

Lista de nodos registrados con su último dato recibido.

```
Petición: GET /api/nodes

Respuesta 200 OK:
[
  {
    "node_id": "AA:BB:CC:DD:EE:01",
    "last_seen_epoch": 1716480000,
    "status": "active",
    "last_readings": [
      { "sensor_type": "temperatura", "value": 24.5, "unit": "C" }
    ]
  },
  {
    "node_id": "AA:BB:CC:DD:EE:02",
    "last_seen_epoch": 1716479950,
    "status": "active",
    "last_readings": [
      { "sensor_type": "temperatura", "value": 22.1, "unit": "C" }
    ]
  }
]
```

#### GET /api/nodes/{id}/readings

Últimas N lecturas de un nodo específico. `{id}` es la MAC del nodo con `:` reemplazados por `-` (ejemplo: `AA-BB-CC-DD-EE-01`).

```
Petición: GET /api/nodes/AA-BB-CC-DD-EE-01/readings?limit=20

Parámetros query:
  limit: número de lecturas a devolver (default: 10, max: 100)
  sensor_type: filtro opcional ("temperatura", "ph", etc.)

Respuesta 200 OK:
{
  "node_id": "AA:BB:CC:DD:EE:01",
  "readings": [
    { "epoch": 1716480000, "sensor_type": "temperatura", "value": 24.5, "unit": "C" },
    { "epoch": 1716479700, "sensor_type": "temperatura", "value": 24.3, "unit": "C" }
  ]
}

Respuesta 404 Not Found (nodo no registrado):
{ "error": "node_not_found", "node_id": "AA:BB:CC:DD:EE:01" }
```

> **Nota de implementación**: El gateway almacena las últimas 100 lecturas por nodo en un ring buffer en RAM. No hay base de datos en disco: la memoria flash del ESP32-S3 no está diseñada para escrituras frecuentes.

#### POST /api/config

Configura parámetros del gateway. Los valores se persisten en NVS.

```
Petición: POST /api/config
Content-Type: application/json

Body:
{
  "wifi_ssid": "MiRedWiFi",
  "wifi_password": "password123",
  "mqtt_enabled": true,
  "mqtt_broker": "192.168.1.100",
  "mqtt_port": 1883,
  "mqtt_namespace": "mi-instalacion",
  "alert_temp_max": 35.0,
  "alert_temp_min": 5.0
}

Respuesta 200 OK:
{ "status": "saved", "reboot_required": true }

Respuesta 400 Bad Request (JSON inválido o campo fuera de rango):
{ "error": "invalid_config", "detail": "alert_temp_max must be between -40 and 85" }
```

#### POST /api/ota

Dispara una actualización OTA. El firmware se descarga desde la URL proporcionada.

```
Petición: POST /api/ota
Content-Type: application/json

Body:
{
  "url": "http://192.168.1.50/firmware/gateway_v1.1.0.bin"
}

Respuesta 202 Accepted:
{ "status": "ota_started", "url": "http://192.168.1.50/firmware/gateway_v1.1.0.bin" }

Respuesta 400 Bad Request:
{ "error": "invalid_url" }
```

> **Pista**: El handler OTA devuelve 202 inmediatamente y ejecuta la descarga en una tarea FreeRTOS separada. El gateway se reinicia automáticamente al completar la actualización.

**Archivos a crear/modificar**:
- Crear: `firmware/gateway/main/http_api.c`
- Crear: `firmware/gateway/main/http_api.h`
- Modificar: `firmware/gateway/main/http_server.c` (registrar los nuevos handlers)

**Criterio de aceptación**:
- `curl http://192.168.4.1/api/status` devuelve JSON válido con los campos definidos
- `curl http://192.168.4.1/api/nodes` devuelve el array de nodos registrados
- `curl -X POST http://192.168.4.1/api/config -H "Content-Type: application/json" -d '{"alert_temp_max":30.0}'` devuelve 200 y el valor se persiste en NVS
- Los campos fuera de rango devuelven HTTP 400 con mensaje de error descriptivo

**Errores comunes**:
- Olvidar registrar el handler OPTIONS para CORS. Los navegadores envían un preflight OPTIONS antes de POST con `Content-Type: application/json`. Sin respuesta 204, las peticiones del dashboard fallarán con error CORS.
- No liberar la memoria del body parseado con `cJSON_Delete()`. Cada petición POST con body JSON debe liberar el objeto cJSON después de procesarlo.

**Tiempo estimado**: 4-5 horas

---

### Tarea T4.1.2: Serialización JSON con cJSON

- **Dificultad**: Básico
- **Descripción**: Implementar las funciones de serialización y deserialización JSON usando cJSON, que ya viene incluido en ESP-IDF. Este módulo centraliza toda la lógica de generación de respuestas JSON para los endpoints REST.

**Funciones a implementar en `http_api.c`**:

```c
// Serializa el estado del gateway a JSON
// Caller es responsable de llamar cJSON_Delete() sobre el resultado
cJSON *http_api_build_status_json(void);

// Serializa la lista de nodos a JSON array
cJSON *http_api_build_nodes_json(void);

// Serializa las lecturas de un nodo a JSON
// node_mac: dirección MAC en formato "AA:BB:CC:DD:EE:FF"
// limit: máximo número de lecturas a incluir
cJSON *http_api_build_readings_json(const uint8_t *node_mac, uint8_t limit);

// Parsea el body de una petición POST /api/config
// Devuelve ESP_OK si el JSON es válido y los valores están en rango
// Devuelve ESP_ERR_INVALID_ARG si hay campos inválidos
esp_err_t http_api_parse_config_body(const char *body, gateway_config_t *config_out);
```

**Patrón estándar de respuesta JSON**:

```c
// En cada handler HTTP:
cJSON *root = http_api_build_status_json();
char *json_str = cJSON_PrintUnformatted(root);
cJSON_Delete(root);  // liberar el árbol cJSON inmediatamente

httpd_resp_set_type(req, "application/json");
httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
httpd_resp_sendstr(req, json_str);
free(json_str);  // liberar el string generado por cJSON_PrintUnformatted
return ESP_OK;
```

> **Importante**: `cJSON_PrintUnformatted()` devuelve memoria dinámica que DEBES liberar con `free()`. `cJSON_Delete()` libera el árbol cJSON pero NO el string. Son dos punteros independientes.

**Criterio de aceptación**:
- No hay memory leaks: `esp_get_free_heap_size()` permanece estable tras 100 peticiones consecutivas a la API
- El JSON generado pasa validación con `python3 -m json.tool`
- Los handlers devuelven la cabecera `Content-Type: application/json`

**Tiempo estimado**: 2-3 horas

---

### Tarea T4.1.3: WebSocket para datos en tiempo real

- **Dificultad**: Avanzado
- **Descripción**: Implementar un handler WebSocket en `esp_http_server` que hace push de datos de sensores a los clientes conectados cada vez que el gateway recibe una lectura de un nodo via ESP-NOW. `esp_http_server` soporta WebSocket nativamente desde ESP-IDF 4.4.

**URL del endpoint WebSocket**:

```
ws://192.168.4.1/ws
```

**Formato de mensaje (JSON)**:

Cada vez que el gateway recibe datos de un nodo, envía a todos los clientes WebSocket conectados:

```json
{
  "type": "sensor_data",
  "node_id": "AA:BB:CC:DD:EE:01",
  "sensor_type": "temperatura",
  "value": 24.5,
  "unit": "C",
  "epoch": 1716480000,
  "sequence": 142
}
```

Para alertas:

```json
{
  "type": "alert",
  "node_id": "AA:BB:CC:DD:EE:01",
  "sensor_type": "temperatura",
  "value": 38.2,
  "threshold": 35.0,
  "severity": "warning",
  "epoch": 1716480120
}
```

Para cambios de estado del gateway:

```json
{
  "type": "gateway_status",
  "free_heap_bytes": 245760,
  "espnow_nodes_active": 2,
  "mqtt_connected": false,
  "epoch": 1716480300
}
```

**Implementación del handler WebSocket**:

```c
// En http_server.c, registrar el handler:
static const httpd_uri_t ws_handler = {
    .uri       = "/ws",
    .method    = HTTP_GET,
    .handler   = ws_handler_func,
    .user_ctx  = NULL,
    .is_websocket = true  // CRÍTICO: marcar como WebSocket
};

// El handler gestiona el handshake automáticamente
// La función ws_broadcast() envía a todos los clientes activos:
void ws_broadcast(const char *json_payload, size_t len);
```

**Lista de file descriptors de clientes conectados**:

```c
// Mantener un array de fds activos (máx. 4 clientes simultáneos)
#define WS_MAX_CLIENTS 4
static int ws_clients[WS_MAX_CLIENTS];
static int ws_client_count = 0;

// Proteger con mutex para acceso desde múltiples tareas FreeRTOS
static SemaphoreHandle_t ws_clients_mutex;
```

**Guía de reconexión para el cliente JavaScript**:

```javascript
// En el dashboard Preact (firmware/gateway/web/src/hooks/useWebSocket.js):
function useWebSocket(url) {
  const [data, setData] = useState(null);
  const ws = useRef(null);

  function connect() {
    ws.current = new WebSocket(url);

    ws.current.onmessage = (event) => {
      setData(JSON.parse(event.data));
    };

    ws.current.onclose = () => {
      // Reconectar tras 3 segundos (backoff simple)
      setTimeout(connect, 3000);
    };

    ws.current.onerror = () => {
      ws.current.close();
    };
  }

  useEffect(() => {
    connect();
    return () => ws.current?.close();
  }, [url]);

  return data;
}
```

> **Nota**: Si no hay clientes WebSocket conectados, los datos de sensores siguen procesándose y almacenándose en los ring buffers internos del gateway. El WebSocket es solo el canal de push; no es necesario para la recogida de datos.

**Criterio de aceptación**:
- Desde la consola del navegador: `new WebSocket('ws://192.168.4.1/ws')` se conecta sin errores
- Cada lectura de sensor enviada por un nodo llega al navegador en menos de 500ms
- Si se cierra el navegador, el gateway limpia el fd del cliente sin crashear
- Hasta 4 clientes pueden estar conectados simultáneamente
- El ciclo reconexión (WebSocket cerrado → reconectar tras 3s) funciona sin intervención manual

**Errores comunes**:
- No marcar el uri handler con `.is_websocket = true`. Sin esta propiedad, `esp_http_server` trata la petición como HTTP GET normal y el handshake WebSocket falla.
- Intentar enviar a un fd de cliente que ya se cerró. Siempre verificar el retorno de `httpd_ws_send_frame_async()` y eliminar el fd si devuelve error.
- No proteger el array `ws_clients[]` con un mutex. La recepción ESP-NOW ocurre en una tarea diferente a la del servidor HTTP; el acceso concurrente sin sincronización puede corromper el array.

**Tiempo estimado**: 5-6 horas

---

### Checkpoint 4.1: Verificación de la API REST y WebSocket

Antes de continuar con la Sub-tarea 4.2, verifica lo siguiente:

- [ ] `curl http://192.168.4.1/api/status` devuelve JSON válido
- [ ] `curl http://192.168.4.1/api/nodes` devuelve el array de nodos (vacío `[]` si no hay nodos aún)
- [ ] `curl -X POST http://192.168.4.1/api/config -H "Content-Type: application/json" -d '{}'` devuelve 200
- [ ] El endpoint WebSocket acepta conexiones: `new WebSocket('ws://192.168.4.1/ws')` en la consola del navegador
- [ ] Cuando un nodo envía datos, aparece el mensaje JSON en la consola del navegador sin recargar la página
- [ ] `esp_get_free_heap_size()` es estable tras 100 peticiones (sin memory leaks)

**Prueba integradora**: Abre la consola del navegador en `http://192.168.4.1`, conecta el WebSocket y activa un nodo sensor. Verifica que los datos llegan en tiempo real al navegador.

---

## Sub-tarea 4.2: SPA Preact Embebida en SPIFFS (3 tareas)

Preact es una alternativa a React de 3KB que tiene la misma API. Con Vite como bundler, el resultado es una SPA que cabe cómodamente en la partición SPIFFS de 1.9MB del gateway. El código fuente vive en `firmware/gateway/web/` y el proceso de build genera los assets gzipados que se flashean a SPIFFS.

### Presupuesto de tamaño SPIFFS

| Recurso | Objetivo | Límite duro |
|---------|----------|-------------|
| Preact SPA gzipada (total) | ≤200 KB | 500 KB |
| Partición SPIFFS total | 1.9 MB | 1.9 MB (fijo en partitions.csv) |
| Margen para futuros assets | ≥1.4 MB | — |

> Si tu build supera los 500 KB gzipados, consulta la sección de troubleshooting al final de T4.2.3.

---

### Tarea T4.2.1: Scaffold del proyecto Preact + Vite

- **Dificultad**: Básico
- **Descripción**: Crear el proyecto frontend en `firmware/gateway/web/` y configurar Vite para generar assets optimizados para SPIFFS.

**Pasos**:

1. Crear el proyecto Preact con Vite:
   ```bash
   cd firmware/gateway
   npm create vite@latest web -- --template preact
   cd web && npm install
   ```

2. Instalar dependencias mínimas:
   ```bash
   npm install preact
   npm install -D vite @preact/preset-vite
   # NO instalar React, React-DOM, React-Router ni otras librerías pesadas
   ```

3. Configurar `firmware/gateway/web/vite.config.js` para output optimizado:
   ```javascript
   import { defineConfig } from 'vite';
   import preact from '@preact/preset-vite';
   import { createGzip } from 'zlib';
   import { createReadStream, createWriteStream } from 'fs';
   import { readdir, stat } from 'fs/promises';
   import path from 'path';

   // Plugin para gzipar los assets tras el build
   function gzipPlugin() {
     return {
       name: 'gzip-assets',
       closeBundle: async () => {
         const distDir = 'dist';
         const files = await readdir(distDir, { recursive: true });
         for (const file of files) {
           const fullPath = path.join(distDir, file);
           const info = await stat(fullPath);
           if (info.isFile() && !file.endsWith('.gz')) {
             await new Promise((resolve, reject) => {
               createReadStream(fullPath)
                 .pipe(createGzip({ level: 9 }))
                 .pipe(createWriteStream(fullPath + '.gz'))
                 .on('finish', resolve)
                 .on('error', reject);
             });
             console.log(`Gzipped: ${file}`);
           }
         }
       }
     };
   }

   export default defineConfig({
     plugins: [preact(), gzipPlugin()],
     build: {
       outDir: 'dist',
       assetsInlineLimit: 4096,  // inline assets < 4KB
       rollupOptions: {
         output: {
           manualChunks: undefined  // un solo chunk para minimizar requests
         }
       }
     }
   });
   ```

4. Verificar el tamaño del bundle:
   ```bash
   npm run build
   du -sh dist/assets/*.gz  # debe ser < 200KB en total
   ```

5. Estructura de directorios resultante:
   ```
   firmware/gateway/web/
   ├── src/
   │   ├── app.jsx          # componente raíz
   │   ├── main.jsx         # punto de entrada
   │   ├── components/      # componentes reutilizables
   │   ├── pages/           # páginas de la SPA
   │   └── hooks/           # custom hooks (useWebSocket, useApi)
   ├── index.html
   ├── vite.config.js
   └── package.json
   ```

**Criterio de aceptación**:
- `npm run build` en `firmware/gateway/web/` completa sin errores
- `du -sh firmware/gateway/web/dist/*.gz` muestra < 200KB total
- `npm run dev` sirve la app en `http://localhost:5173` para desarrollo

**Tiempo estimado**: 2-3 horas

---

### Tarea T4.2.2: Páginas de la SPA

- **Dificultad**: Intermedio
- **Descripción**: Implementar las tres páginas del dashboard: vista en tiempo real de nodos, configuración del gateway, y estado del sistema. Toda la lógica de red usa los hooks `useWebSocket` y `useApi` que conectan con los endpoints de T4.1.

**Página 1: Dashboard (ruta `/`)**

Muestra tarjetas en tiempo real, una por nodo registrado:

```jsx
// src/pages/Dashboard.jsx
import { useState, useEffect } from 'preact/hooks';
import { useWebSocket } from '../hooks/useWebSocket';
import { useApi } from '../hooks/useApi';
import { NodeCard } from '../components/NodeCard';

export function Dashboard() {
  const { data: nodes, loading } = useApi('/api/nodes');
  const wsData = useWebSocket('ws://' + window.location.host + '/ws');

  // Actualizar el nodo correspondiente al recibir datos por WebSocket
  const [nodeMap, setNodeMap] = useState({});

  useEffect(() => {
    if (nodes) {
      const map = {};
      nodes.forEach(n => { map[n.node_id] = n; });
      setNodeMap(map);
    }
  }, [nodes]);

  useEffect(() => {
    if (wsData?.type === 'sensor_data') {
      setNodeMap(prev => ({
        ...prev,
        [wsData.node_id]: {
          ...prev[wsData.node_id],
          last_readings: [{ sensor_type: wsData.sensor_type, value: wsData.value, unit: wsData.unit }],
          last_seen_epoch: wsData.epoch
        }
      }));
    }
  }, [wsData]);

  if (loading) return <div>Cargando nodos...</div>;

  return (
    <div class="grid">
      {Object.values(nodeMap).map(node => (
        <NodeCard key={node.node_id} node={node} />
      ))}
    </div>
  );
}
```

Cada `NodeCard` muestra: ID del nodo (últimos 8 chars de la MAC), tipo de sensor, valor, unidad, y un indicador de estado (verde si `last_seen` hace < 2 minutos, rojo si más).

**Página 2: Configuración (ruta `/config`)**

Formulario para configurar parámetros del gateway:
- Campo: SSID WiFi y contraseña (STA mode)
- Campo: URL del broker MQTT y namespace (opcionales)
- Campo: umbrales de alerta (temperatura máxima/mínima)
- Botón "Guardar" → `POST /api/config`
- Botón "Aplicar y reiniciar" → guarda + POST /api/config con flag `reboot: true`

**Página 3: Estado (ruta `/status`)**

Muestra datos del endpoint `GET /api/status` actualizados cada 10 segundos:
- Uptime del gateway
- Memoria libre (heap)
- RSSI WiFi
- Número de nodos activos / registrados
- Estado de la conexión MQTT (si está configurado)
- Versión del firmware

**Navegación entre páginas**:

Usar Preact Router (muy pequeño, ~2KB) o una navegación manual con `window.location.hash` para evitar dependencias extra:

```jsx
// Routing mínimo sin librería externa
function Router() {
  const [page, setPage] = useState(window.location.hash || '#/');

  useEffect(() => {
    window.addEventListener('hashchange', () => setPage(window.location.hash));
  }, []);

  return page === '#/config' ? <Config /> :
         page === '#/status' ? <Status /> :
         <Dashboard />;
}
```

**Criterio de aceptación**:
- La página Dashboard muestra tarjetas de nodos y se actualiza en tiempo real via WebSocket
- Los valores de los nodos cambian en pantalla cuando llegan datos nuevos por WebSocket
- La página Configuración envía POST a `/api/config` y muestra confirmación
- La página Estado muestra los valores de `/api/status`
- La navegación entre páginas funciona sin recargar la página completa

**Tiempo estimado**: 5-6 horas

---

### Tarea T4.2.3: Integración con SPIFFS — CMake build hook

- **Dificultad**: Avanzado
- **Descripción**: Conectar el proceso de build de Vite con el sistema de build CMake de ESP-IDF. El objetivo es que `idf.py build` compile automáticamente la SPA Preact y la incluya en la imagen SPIFFS que se flashea al gateway.

**Partición SPIFFS en `firmware/gateway/partitions.csv`**:

```csv
# Name,   Type, SubType, Offset,   Size,  Flags
nvs,      data, nvs,     0x9000,   0x6000,
otadata,  data, ota,     0xf000,   0x2000,
ota_0,    app,  ota_0,   0x10000,  0x300000,
ota_1,    app,  ota_1,   0x310000, 0x300000,
spiffs,   data, spiffs,  0x610000, 0x1E0000,
```

> La partición `spiffs` tiene 1.9MB (0x1E0000 bytes). Es donde viven los assets del dashboard.

**Hook CMake en `firmware/gateway/CMakeLists.txt`**:

```cmake
# Al final del CMakeLists.txt principal del proyecto:

# 1. Construir la SPA Preact antes de generar la imagen SPIFFS
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html.gz
    COMMAND npm install
    COMMAND npm run build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web
    COMMENT "Building Preact SPA..."
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/src/app.jsx
)

add_custom_target(preact_build
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html.gz
)

# 2. Generar la imagen SPIFFS a partir de dist/
set(SPIFFS_IMAGE_PATH ${CMAKE_BINARY_DIR}/spiffs_image.bin)

add_custom_command(
    OUTPUT ${SPIFFS_IMAGE_PATH}
    COMMAND python3 ${IDF_PATH}/components/spiffs/spiffsgen.py
            0x1E0000
            ${CMAKE_CURRENT_SOURCE_DIR}/web/dist
            ${SPIFFS_IMAGE_PATH}
    DEPENDS preact_build
    COMMENT "Generating SPIFFS image..."
)

add_custom_target(spiffs_image ALL DEPENDS ${SPIFFS_IMAGE_PATH})

# 3. Flashear la imagen SPIFFS con idf.py flash
# El target esptool_py_flash_project se encarga de esto automáticamente
# si se configura la dirección de la partición spiffs (0x610000)
```

**Flashear la partición SPIFFS por separado** (útil durante desarrollo):

```bash
# Solo flashear el dashboard (sin reflashear el firmware):
python3 $IDF_PATH/components/spiffs/spiffsgen.py \
    0x1E0000 \
    firmware/gateway/web/dist \
    spiffs.bin

esptool.py --port /dev/ttyUSB0 write_flash 0x610000 spiffs.bin
```

**Servir assets gzipados desde el gateway**:

El handler HTTP para assets estáticos DEBE detectar si el cliente acepta gzip (cabecera `Accept-Encoding: gzip`) y servir el archivo `.gz` directamente:

```c
// En el handler de archivos estáticos:
void serve_static_file(httpd_req_t *req, const char *path) {
    // Intentar servir la versión gzipada
    char gz_path[256];
    snprintf(gz_path, sizeof(gz_path), "%s.gz", path);

    FILE *f = fopen(gz_path, "rb");
    if (f) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        // Servir el archivo...
    } else {
        f = fopen(path, "rb");
        // Servir sin compresión...
    }
}
```

**Troubleshooting: bundle size overruns**

Si el bundle supera los 500KB gzipados:

1. **Auditar dependencias**: `npm run build -- --analyze` (con el plugin `rollup-plugin-visualizer`) muestra qué módulos ocupan más espacio.

2. **Verificar que no se importó React**: `grep -r "react" firmware/gateway/web/src/` — si aparece algún resultado, reemplazar por la equivalente de Preact.

3. **Eliminar fuentes externas**: Si el CSS importa Google Fonts u otras fuentes externas, reemplazar por fuentes del sistema (`font-family: system-ui, sans-serif`).

4. **Reducir el número de iconos**: Las librerías de iconos (heroicons, lucide, etc.) son pesadas. Usar solo SVG inline para los 3-5 iconos necesarios.

5. **Comprimir imágenes**: Si hay imágenes PNG/JPEG, convertirlas a WebP y reducir su resolución.

6. **Medir progreso**:
   ```bash
   npm run build && du -sh firmware/gateway/web/dist/assets/*.gz
   ```

**Criterio de aceptación**:
- `idf.py build` completa sin errores e incluye la imagen SPIFFS
- `curl -H "Accept-Encoding: gzip" http://192.168.4.1/` devuelve la SPA con la cabecera `Content-Encoding: gzip`
- El dashboard carga en el navegador sin errores en la consola
- `du -sh firmware/gateway/web/dist/assets/*.gz` muestra ≤ 200KB total

**Tiempo estimado**: 3-4 horas

---

### Checkpoint 4.2: Verificación de la SPA Preact

Antes de continuar con la Sub-tarea 4.3, verifica lo siguiente:

- [ ] `npm run build` en `firmware/gateway/web/` completa sin errores
- [ ] El bundle total gzipado ocupa ≤ 200KB
- [ ] `idf.py build && idf.py flash` incluye la imagen SPIFFS con la SPA
- [ ] Al abrir `http://192.168.4.1/` en el navegador, carga el dashboard
- [ ] Las tarjetas de nodos se actualizan en tiempo real cuando llegan datos por ESP-NOW
- [ ] La navegación entre Dashboard / Config / Status funciona sin recargar la página
- [ ] No hay errores en la consola del navegador

**Prueba integradora**: Con al menos un nodo ESP32-C3 activo enviando datos por ESP-NOW, abrir el dashboard en un teléfono móvil conectado al WiFi AP del gateway (`192.168.4.1`). Verificar que los datos del nodo aparecen y se actualizan automáticamente.

---

## Sub-tarea 4.3: MQTT como Integración Opcional (2 tareas)

> **Importante**: MQTT no es un requisito del sistema. El gateway funciona completamente sin MQTT configurado: el dashboard embebido, la API REST y el WebSocket son independientes de MQTT. Esta sub-tarea añade MQTT como canal opcional para integración con sistemas cloud o LAN existentes.

### Tarea T4.3.1: Módulo mqtt_bridge como componente opcional

- **Dificultad**: Intermedio
- **Descripción**: Refactorizar la documentación del módulo `mqtt_bridge` para que quede claro que es opcional y documentar el namespace configurable. El módulo solo se inicializa si hay una URL de broker configurada en NVS.

**Lógica de inicialización condicional**:

```c
// En app_main.c, DESPUÉS de inicializar WiFi y el servidor HTTP:
esp_err_t err = nvs_get_str(nvs_handle, "mqtt_broker", broker_url, &len);
if (err == ESP_OK && strlen(broker_url) > 0) {
    ESP_LOGI(TAG, "MQTT configurado: %s — iniciando mqtt_bridge", broker_url);
    mqtt_bridge_init(broker_url);
} else {
    ESP_LOGI(TAG, "MQTT no configurado — el gateway opera en modo standalone");
}
```

**Namespace MQTT configurable**:

El prefijo de todos los topics MQTT es configurable via NVS:

| Parámetro NVS | Clave | Valor por defecto |
|---------------|-------|-------------------|
| Namespace MQTT | `mqtt_namespace` | `iiot-kit` |
| URL del broker | `mqtt_broker` | `` (vacío = deshabilitado) |
| Puerto del broker | `mqtt_port` | `1883` |

El patrón de topic resultante:

```
{mqtt_namespace}/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}
```

Ejemplos con distintas configuraciones:

```
# Con mqtt_namespace = "iiot-kit" (default):
iiot-kit/GW-AABBCCDD/nodo/AA:BB:CC:DD:EE:01/temperatura

# Con mqtt_namespace = "invernadero":
invernadero/GW-AABBCCDD/nodo/AA:BB:CC:DD:EE:01/temperatura

# Con mqtt_namespace = "fabrica-norte":
fabrica-norte/GW-AABBCCDD/nodo/AA:BB:CC:DD:EE:01/temperatura
```

**Configurar desde la interfaz web**:

En la página de Configuración de la SPA (T4.2.2), el formulario incluye los campos MQTT. Al guardar, se hace `POST /api/config` con:

```json
{
  "mqtt_enabled": true,
  "mqtt_broker": "mqtt://192.168.1.100",
  "mqtt_port": 1883,
  "mqtt_namespace": "mi-instalacion"
}
```

El gateway almacena estos valores en NVS y, en el próximo reinicio, inicializa `mqtt_bridge` con la configuración guardada.

**Criterio de aceptación**:
- El gateway arranca y sirve el dashboard correctamente SIN ningún broker MQTT configurado
- Si se configura un broker, los datos de sensores aparecen en el topic `{namespace}/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}`
- Cambiar `mqtt_namespace` via la web y reiniciar el gateway usa el nuevo namespace en todos los topics
- El log de arranque indica claramente si MQTT está activo o en modo standalone

**Tiempo estimado**: 2-3 horas

---

### Tarea T4.3.2: Documentar topics MQTT genéricos

- **Dificultad**: Básico
- **Descripción**: Documentar la jerarquía completa de topics MQTT con el namespace genérico `{mqtt_ns}`. Los topics específicos de dominio (por ejemplo, para una instalación de monitorización de agua) se documentan en `examples/fish-farm/mqtt-topics.md`.

**Jerarquía de topics con namespace configurable**:

```
{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/temperatura
{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/humedad
{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/presion
{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/custom   ← SENSOR_TYPE_CUSTOM (0xFF)
{mqtt_ns}/{gateway_id}/status
{mqtt_ns}/{gateway_id}/alertas
{mqtt_ns}/{gateway_id}/control/actuador/{id}
{mqtt_ns}/{gateway_id}/config/nodo/{nodo_id}
```

**Payload JSON para datos de sensor**:

```json
{
  "value": 24.5,
  "unit": "C",
  "timestamp": 1716480000,
  "node_id": "AA:BB:CC:DD:EE:01",
  "sequence": 142
}
```

**QoS recomendado por tipo de topic**:

| Topic | QoS | Retain | Motivo |
|-------|-----|--------|--------|
| Datos de sensor | 1 | 0 | Al menos una vez; los duplicados son tolerables |
| Status del gateway | 1 | 1 | Con retain: los nuevos suscriptores ven el último estado |
| Alertas | 1 | 0 | No retener: las alertas son eventos puntuales |
| Comandos de actuador | 2 | 0 | Exactamente una vez: un duplicado podría activar dos veces |

> Para topics y configuración específicos de una instalación de tipo fish-farm, consulta `examples/fish-farm/mqtt-topics.md`.

**Criterio de aceptación**:
- `mosquitto_sub -t "{mqtt_ns}/#" -v` muestra los mensajes publicados con el namespace correcto
- El payload JSON de datos de sensor contiene los 5 campos definidos (value, unit, timestamp, node_id, sequence)
- Los topics de status usan retain=1 (verificable al suscribirse después de que se publique)

**Tiempo estimado**: 1-2 horas

---

### Checkpoint 4.3: Verificación de la integración MQTT opcional

- [ ] El gateway opera correctamente SIN broker MQTT configurado
- [ ] Al configurar un broker, los datos aparecen con `mosquitto_sub -t "{mqtt_ns}/#" -v`
- [ ] El namespace configurable funciona: cambiar `mqtt_namespace` cambia el prefijo de todos los topics
- [ ] El status del gateway se publica con retain=1
- [ ] Los comandos de actuador usan QoS 2

---

## Sub-tarea 4.4: Integración y Verificación (2 tareas)

### Tarea T4.4.1: Flujo end-to-end completo

- **Dificultad**: Básico (verificación)
- **Descripción**: Verificar el flujo completo de datos desde un nodo sensor hasta el navegador web, pasando por todos los componentes de la Fase 4.

**Flujo a verificar**:

```
1. Nodo ESP32-C3 → (ESP-NOW) → Gateway ESP32-S3
2. Gateway → almacena lectura en ring buffer
3. Gateway → hace push via WebSocket a clientes conectados
4. Navegador → recibe el mensaje WebSocket y actualiza la tarjeta del nodo
5. [Opcional] Gateway → publica en MQTT si está configurado
```

**Checklist de verificación end-to-end**:

```bash
# Terminal 1: Monitor del gateway
idf.py -p /dev/ttyUSB0 monitor

# Terminal 2: Cliente WebSocket desde la línea de comandos
# (requiere: npm install -g wscat)
wscat -c ws://192.168.4.1/ws

# Deberías ver mensajes JSON cada vez que el nodo envía datos:
# { "type": "sensor_data", "node_id": "...", "value": 24.5, ... }

# Terminal 3: Verificar la API REST
curl http://192.168.4.1/api/nodes
curl http://192.168.4.1/api/status

# [Opcional] Terminal 4: Verificar MQTT si está configurado
mosquitto_sub -h 192.168.1.100 -t "iiot-kit/#" -v
```

**Escenario de prueba con timing**:

1. Con el monitor del gateway abierto, activa el nodo sensor.
2. En el log del gateway verás: `ESPNOW: recibido de AA:BB:... temperatura=24.5°C`
3. En el cliente WebSocket verás el mensaje JSON en < 500ms.
4. En el navegador con el dashboard abierto, la tarjeta del nodo se actualizará en tiempo real.

**Criterio de aceptación**:
- El dato aparece en el navegador en menos de 1 segundo desde que el nodo lo envía
- El dashboard muestra el estado correcto de todos los nodos (activo/inactivo)
- No hay errores en el monitor serial del gateway durante 10 minutos de operación continua
- La memoria libre del gateway (`esp_get_free_heap_size()`) es estable (no decrece continuamente)

**Tiempo estimado**: 2-3 horas

---

### Tarea T4.4.2: Verificación del presupuesto de tamaño del bundle Preact

- **Dificultad**: Básico
- **Descripción**: Verificar que el bundle Preact cumple con el presupuesto de tamaño definido y documentar cómo medirlo en cada iteración de desarrollo.

**Cómo medir el tamaño del bundle**:

```bash
# Desde firmware/gateway/web/:
npm run build

# Tamaño de archivos individuales gzipados:
ls -lh dist/assets/*.gz

# Tamaño total de la partición SPIFFS que se va a usar:
du -sh dist/

# Verificar que el total gzipado está dentro del presupuesto:
find dist -name "*.gz" -exec du -sb {} + | awk '{total+=$1} END {print total/1024 "KB gzipped"}'
```

**Criterios de aceptación del presupuesto**:

| Métrica | Objetivo | Límite duro |
|---------|----------|-------------|
| JS principal gzipado | ≤150KB | 400KB |
| CSS gzipado | ≤20KB | 50KB |
| Total assets gzipados | ≤200KB | 500KB |
| Partición SPIFFS usada | ≤600KB | 1.9MB |

**Script de verificación automática**:

```bash
#!/bin/bash
# firmware/gateway/web/scripts/check-size.sh
npm run build --silent

TOTAL=$(find dist -name "*.gz" -exec stat -c%s {} + | awk '{sum+=$1} END {print sum}')
LIMIT=$((500 * 1024))  # 500KB en bytes

echo "Bundle gzipado total: $((TOTAL / 1024))KB"

if [ "$TOTAL" -gt "$LIMIT" ]; then
  echo "ERROR: El bundle supera el límite de 500KB gzipados"
  echo "Consulta la sección de troubleshooting en T4.2.3"
  exit 1
else
  echo "OK: El bundle cumple el presupuesto de tamaño"
fi
```

**Criterio de aceptación**:
- El script `check-size.sh` termina con código 0
- El total de assets gzipados es ≤ 200KB (objetivo) o ≤ 500KB (límite duro)
- La partición SPIFFS tiene al menos 1.4MB libres para futuros assets

**Tiempo estimado**: 1 hora

---

### Checkpoint 4.4: Verificación final de integración

- [ ] Flujo end-to-end verificado: nodo → ESP-NOW → gateway → WebSocket → navegador
- [ ] El presupuesto de tamaño del bundle Preact se cumple (≤ 200KB gzipados)
- [ ] El sistema funciona durante 30 minutos sin reinicios ni memory leaks
- [ ] El dashboard es accesible desde un dispositivo móvil conectado al WiFi AP del gateway
- [ ] Si MQTT está configurado, los datos aparecen en el broker correctamente

**Prueba integradora final**: Con el sistema completo funcionando (gateway + al menos un nodo), abre el dashboard en un teléfono móvil conectado al WiFi AP. Desconecta y vuelve a conectar el teléfono. Verifica que el WebSocket se reconecta automáticamente y los datos vuelven a fluir.

---

## Preguntas de autoevaluación

Responde estas preguntas para verificar tu comprensión de los conceptos de esta fase. Si no puedes responder alguna, revisa la documentación recomendada.

1. **¿Por qué Preact en lugar de React para el dashboard embebido? ¿Cuál es la diferencia de tamaño y qué implicaciones tiene para la partición SPIFFS?**
   > Pista: Compara el tamaño gzipado de Preact (~3KB) con React (~40KB). ¿Cuántos KBs quedan disponibles en la partición SPIFFS de 1.9MB para el resto de assets?

2. **¿Por qué `esp_http_server` soporta WebSocket de forma nativa? ¿Qué ventaja tiene esto frente a implementar un servidor WebSocket separado?**
   > Pista: El ESP32-S3 tiene un solo socket de servidor HTTP. ¿Qué pasa con la memoria si tienes dos servidores HTTP en el mismo dispositivo?

3. **El gateway mantiene un ring buffer de las últimas 100 lecturas por nodo en RAM, en lugar de guardarlas en flash. ¿Por qué? ¿Cuáles son los trade-offs de esta decisión?**
   > Pista: La memoria flash NOR del ESP32 soporta aproximadamente 100.000 ciclos de escritura por sector. Si escribes una lectura cada 30 segundos...

4. **¿Qué ocurre con los datos de sensores que llegan por ESP-NOW si no hay ningún cliente WebSocket conectado? ¿Se pierden?**
   > Pista: Revisa la nota al final de T4.1.3.

5. **¿Por qué es importante que la función `ws_broadcast()` esté protegida con un mutex (SemaphoreHandle_t)?**
   > Pista: La recepción ESP-NOW ocurre en el contexto de tarea del protocolo ESP-NOW. La tarea del servidor HTTP corre en otro contexto. ¿Qué pasa si ambas intentan modificar el array `ws_clients[]` simultáneamente?

6. **¿Cómo afecta el tamaño del bundle a la experiencia del usuario? Compara el tiempo de carga de un asset de 50KB gzipado vs. uno de 500KB gzipado a través del WiFi AP del gateway (ancho de banda típico: ~5 Mbps).**
   > Pista: Calcula el tiempo de descarga en ambos casos y piensa en el contexto: un operador técnico accediendo al panel de emergencia.

7. **¿Por qué el endpoint `POST /api/ota` devuelve HTTP 202 Accepted en lugar de 200 OK?**
   > Pista: ¿Cuánto tarda la descarga del firmware? ¿Puede el handler HTTP esperar bloqueado durante ese tiempo?

8. **Si el namespace MQTT está configurado como `"invernadero"` y el gateway_id es `"GW-001"`, ¿cuál sería el topic MQTT completo para la lectura de temperatura del nodo `AA:BB:CC:DD:EE:01`?**
   > Respuesta esperada: `invernadero/GW-001/nodo/AA:BB:CC:DD:EE:01/temperatura`

---

## Lectura recomendada

### ESP-IDF y ESP32
- [esp_http_server — Documentación ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/esp_http_server.html) — Incluye la sección sobre soporte WebSocket nativo.
- [SPIFFS — Documentación ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/spiffs.html) — Cómo montar la partición, leer archivos, y las limitaciones de escritura.
- [cJSON — GitHub](https://github.com/DaveGamble/cJSON) — Referencia de la API: `cJSON_CreateObject`, `cJSON_AddNumberToObject`, `cJSON_PrintUnformatted`, `cJSON_Delete`.
- [Ejemplo SPIFFS + HTTP Server — esp-idf](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server) — Código de referencia para servir archivos estáticos.
- [Ejemplo WebSocket — esp-idf](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/ws_echo_server) — Punto de partida para el handler WebSocket.

### Preact y Vite
- [Documentación oficial de Preact](https://preactjs.com/guide/v10/getting-started/) — La guía de inicio cubre todo lo necesario para este proyecto.
- [Vite — Guía de configuración](https://vite.dev/guide/) — Cómo configurar el output de build, plugins y optimización.
- [Preact Signals](https://preactjs.com/guide/v10/signals/) — Estado reactivo de Preact sin useState, muy ligero.

### WebSocket
- [WebSocket API — MDN](https://developer.mozilla.org/es/docs/Web/API/WebSockets_API) — Referencia de la API del navegador: `new WebSocket()`, `onmessage`, `onclose`, `send()`.
- [RFC 6455 — WebSocket Protocol](https://datatracker.ietf.org/doc/html/rfc6455) — La especificación del protocolo (lectura opcional para comprender el handshake).

### MQTT (opcional)
- [ESP-MQTT — Documentación ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/mqtt.html) — Para la integración opcional en T4.3.
- [MQTT.org](https://mqtt.org/) — Conceptos: publish/subscribe, QoS, retained messages.

---

## Errores frecuentes de la Fase 4

### 1. Memory leak en cJSON en el ESP32
**Síntoma**: El gateway funciona bien las primeras horas pero luego se reinicia o deja de responder.
**Causa**: No liberar la memoria de `cJSON_PrintUnformatted()` (con `free()`) o del árbol cJSON (con `cJSON_Delete()`). Son dos operaciones distintas y ambas son necesarias.
**Solución**: Siempre seguir el patrón: `cJSON *root = ...; char *str = cJSON_PrintUnformatted(root); cJSON_Delete(root); /* usar str */ free(str);`. Monitorear `esp_get_free_heap_size()` periódicamente.

### 2. WebSocket handler sin `.is_websocket = true`
**Síntoma**: El navegador lanza `WebSocket connection to 'ws://...' failed`.
**Causa**: El URI handler no está marcado como WebSocket handler, por lo que `esp_http_server` lo trata como HTTP GET normal.
**Solución**: Añadir `.is_websocket = true` a la estructura `httpd_uri_t` del handler WebSocket.

### 3. CORS bloqueando peticiones POST del dashboard
**Síntoma**: El dashboard puede hacer GET a la API pero los POST fallan con error CORS en la consola del navegador.
**Causa**: El servidor no responde a las peticiones preflight OPTIONS con las cabeceras CORS correctas.
**Solución**: Registrar un handler para `HTTP_OPTIONS` en todos los endpoints que acepten POST, devolviendo 204 con las cabeceras: `Access-Control-Allow-Origin: *`, `Access-Control-Allow-Methods: GET, POST, OPTIONS`, `Access-Control-Allow-Headers: Content-Type`.

### 4. Bundle Preact demasiado grande
**Síntoma**: `du -sh firmware/gateway/web/dist/` muestra más de 500KB.
**Causa**: Se importó React en lugar de Preact, o se añadió una librería de iconos/gráficos pesada.
**Solución**: Ver sección de troubleshooting en T4.2.3. Ejecutar `npm run build -- --analyze` para identificar qué módulos contribuyen más al tamaño.

### 5. SPIFFS no montada antes de servir archivos
**Síntoma**: El servidor HTTP devuelve 404 para todos los archivos estáticos.
**Causa**: `esp_vfs_spiffs_register()` no se llamó antes de iniciar el servidor HTTP, o la imagen SPIFFS no se flasheó a la dirección correcta.
**Solución**: Verificar el orden de inicialización en `app_main()`: NVS → SPIFFS → WiFi → HTTP Server. Verificar que la imagen se flasheó a la dirección de la partición `spiffs` (0x610000 en la configuración por defecto).

### 6. Acceso concurrente al array `ws_clients[]` sin mutex
**Síntoma**: El gateway se reinicia aleatoriamente con `LoadProhibited` o `StoreProhibited` (acceso a dirección inválida).
**Causa**: La tarea de ESP-NOW y la tarea del servidor HTTP modifican `ws_clients[]` sin sincronización.
**Solución**: Proteger todas las lecturas y escrituras de `ws_clients[]` con `xSemaphoreTake(ws_clients_mutex, portMAX_DELAY)` / `xSemaphoreGive(ws_clients_mutex)`.

### 7. WebSocket no reconecta tras reinicio del gateway
**Síntoma**: El dashboard deja de recibir datos si el gateway se reinicia y no vuelve a actualizarse.
**Causa**: El código JavaScript del dashboard no implementa lógica de reconexión en el evento `onclose`.
**Solución**: Implementar el hook `useWebSocket` con reconexión automática (ver código de ejemplo en T4.1.3).

### 8. Datos históricos no disponibles tras reinicio del gateway
**Síntoma**: Al reiniciar el gateway, `GET /api/nodes/{id}/readings` devuelve un array vacío.
**Causa**: El ring buffer de lecturas vive en RAM (no en NVS ni SPIFFS). Un reinicio borra todos los datos históricos.
**Nota**: Este es el comportamiento esperado y documentado. El gateway no es una base de datos. Para histórico persistente, usa la integración MQTT opcional y guarda los datos en un sistema externo.

---

## Resumen de la Fase 4

Al completar esta fase, dispondrás de un gateway ESP32-S3 completamente autónomo que:

- **Sirve un dashboard interactivo** desde su propia memoria flash (SPIFFS), accesible desde cualquier dispositivo con navegador en la red WiFi del gateway.
- **Expone una API REST** (`/api/status`, `/api/nodes`, `/api/nodes/{id}/readings`, `/api/config`, `/api/ota`) para consulta de datos y configuración remota.
- **Empuja datos en tiempo real** via WebSocket a todos los navegadores conectados, sin polling.
- **Publica opcionalmente en MQTT** con namespace configurable, para integración con sistemas cloud o LAN.
- **Opera sin dependencias externas**: no requiere Bun, Node.js, SQLite, Mosquitto, ni ningún otro servicio externo para funcionar.

El sistema es completamente standalone: si se va la red local o el cloud, el gateway sigue funcionando, el dashboard sigue siendo accesible y los datos de sensores siguen llegando.

```
Flujo de datos completo:
Sensor → Nodo ESP32-C3 → [ESP-NOW] → Gateway ESP32-S3 (autónomo)
                                           ↓
                          Dashboard embebido (Preact en SPIFFS)
                          API REST (esp_http_server)
                          WebSocket (push en tiempo real)
                               ↓ [opcional]
                          MQTT → broker cloud/LAN
```

**Tiempo total estimado**: 28-38 horas (4 semanas a ritmo de dedicación parcial).
