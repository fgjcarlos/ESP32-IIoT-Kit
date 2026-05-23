# WebSocket

#concepto #fase/4

## Que es
WebSocket es un protocolo de comunicación bidireccional y persistente sobre TCP, estandarizado en RFC 6455. A diferencia de HTTP, donde el cliente siempre inicia la petición, WebSocket permite al **servidor enviar datos al cliente en cualquier momento** sin que el cliente los haya pedido.

En ESP32-IIoT-Kit, el gateway usa WebSocket para enviar lecturas de sensores en tiempo real al dashboard Preact embebido en el navegador.

## Por que importa en este proyecto

El dashboard necesita datos actualizados continuamente. Las alternativas son:

| Técnica | Descripción | Problema en ESP32 |
|---------|-------------|-------------------|
| **Polling** | El cliente pide datos cada N segundos con `fetch()` | Genera muchas conexiones HTTP; latencia innecesaria |
| **SSE** (Server-Sent Events) | Conexión HTTP persistente, server-push unidireccional | Solo del servidor al cliente; no se pueden enviar comandos |
| **WebSocket** | Conexión bidireccional persistente | Ligero, bidireccional, soporte nativo en `esp_http_server` |

WebSocket es la elección natural: una sola conexión persistente, baja latencia, bidireccional (útil para futuros comandos desde el dashboard).

## Como funciona

### Handshake inicial

WebSocket comienza con un handshake HTTP estándar. El cliente envía:

```
GET /ws HTTP/1.1
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
```

El servidor responde con `101 Switching Protocols` y a partir de ese momento la conexión es un canal de frames binarios o de texto.

### Ciclo de vida de la conexión

```
Cliente              Servidor (ESP32)
   |-- HTTP Upgrade -->|
   |<-- 101 OK --------|
   |                   |  (conexión establecida)
   |<-- datos JSON -----|  cada vez que llega una lectura ESP-NOW
   |<-- datos JSON -----|
   |-- comando ------->|  (futuro: comandos desde el dashboard)
   |                   |
   |-- close frame --->|
   |<-- close frame ---|
```

### Implementación en ESP-IDF

`esp_http_server` soporta WebSocket desde ESP-IDF v5.0 de forma nativa:

```c
// Handler WebSocket en el gateway
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // Handshake inicial — esp_http_server lo gestiona automáticamente
        ESP_LOGI(TAG, "WebSocket handshake completado");
        return ESP_OK;
    }

    // Leer frame entrante (si el cliente envía algo)
    httpd_ws_frame_t ws_pkt = { .type = HTTPD_WS_TYPE_TEXT };
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    // ... procesar ws_pkt.payload si es necesario

    return ESP_OK;
}

// Enviar datos al cliente desde cualquier tarea de FreeRTOS
void ws_send_sensor_reading(int fd, const sensor_reading_t *reading) {
    char json[128];
    snprintf(json, sizeof(json),
             "{\"nodo\":\"%s\",\"temp\":%d,\"ts\":%lu}",
             reading->node_mac, reading->temperature, reading->timestamp);

    httpd_ws_frame_t frame = {
        .payload = (uint8_t *)json,
        .len     = strlen(json),
        .type    = HTTPD_WS_TYPE_TEXT,
    };
    httpd_ws_send_frame_async(server_handle, fd, &frame);
}
```

### Reconexión desde el cliente (Preact)

El navegador cierra la conexión WebSocket si el ESP32 se reinicia o pierde WiFi. El cliente debe reconectar automáticamente:

```javascript
// Lógica de reconexión con backoff exponencial
function conectarWS() {
  const ws = new WebSocket(`ws://${location.host}/ws`);

  ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    actualizarDashboard(data);
  };

  ws.onclose = () => {
    console.warn('WebSocket cerrado. Reintentando en 3s...');
    setTimeout(conectarWS, 3000);
  };

  ws.onerror = (err) => console.error('WebSocket error:', err);
}

conectarWS();
```

## Gotchas

- `httpd_ws_send_frame_async` es necesaria cuando se envía desde una tarea FreeRTOS diferente al handler HTTP; usar `httpd_ws_send_frame` solo desde el propio handler
- El servidor HTTP del ESP32 mantiene el `fd` (file descriptor) de cada conexión WebSocket; hay que trackear los fds activos para saber a quién enviar
- Las conexiones WebSocket ocupan un socket del servidor HTTP; el límite por defecto es 7 sockets totales (REST + WS)
- El frame WebSocket tiene un máximo práctico de ~1400 bytes para evitar fragmentación; para payloads mayores, dividir en múltiples frames
- Si el cliente no envía `ping` periódicamente y el ESP32 tiene un firewall/NAT intermedio, la conexión puede cerrarse silenciosamente; implementar `ping/pong` cada 30 segundos

## WebSocket vs polling: el tradeoff real

| Métrica | Polling cada 1s | WebSocket |
|---------|-----------------|-----------|
| Peticiones HTTP por hora | 3.600 | 0 (una sola conexión) |
| Latencia de datos nuevos | 0–1 s | < 100 ms |
| Overhead de headers HTTP | Alto | Mínimo (frames binarios) |
| Complejidad del servidor | Baja | Media |
| Complejidad del cliente | Baja | Media (gestionar reconexión) |

Para datos de sensores que cambian frecuentemente, WebSocket es claramente superior.

## Referencias

- [[REST-API]] — Canal complementario para lecturas bajo demanda y configuración
- Fase 4: `Fases/fase-04-mqtt-dashboard.md` — Subtarea 4.1.3 (WebSocket handler)
- Tutorial 4: `Tutorial/tutorial-04-mqtt-dashboard.md`
- [esp_http_server WebSocket](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_http_server.html#websocket-server)
- [RFC 6455 — The WebSocket Protocol](https://datatracker.ietf.org/doc/html/rfc6455)
