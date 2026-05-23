# REST API

#concepto #fase/4

## Que es
REST (*Representational State Transfer*) es un estilo arquitectónico para diseñar APIs sobre HTTP. No es un protocolo ni un estándar: es un conjunto de restricciones que, cuando se siguen, producen APIs predecibles, sin estado y fáciles de cachear.

En ESP32-IIoT-Kit, el gateway expone una REST API sobre `esp_http_server` que permite al dashboard (Preact SPA) y a clientes externos consultar el estado del sistema y enviar comandos.

## Por que importa en este proyecto

El dashboard embebido en SPIFFS necesita un canal para pedir datos al firmware del gateway. Las opciones son:

| Opción | Ventaja | Inconveniente |
|--------|---------|---------------|
| **REST API** | Estándar, sin estado, cacheble, compatible con cualquier cliente HTTP | Una petición por recurso; no es push |
| WebSocket | Push bidireccional en tiempo real | Más complejo de mantener; no adecuado para config/OTA |
| MQTT interno | Ya existe en el firmware | Overhead de broker; no directamente accesible desde el navegador |

La solución elegida combina ambos: REST para lecturas y configuración, WebSocket para datos en tiempo real (ver [[WebSocket]]).

## Principios REST aplicados a dispositivos restringidos

### 1. Recursos identificados por URL

Cada entidad del sistema es un recurso con su propia URL:

```
GET  /api/status                    → estado del gateway
GET  /api/nodes                     → lista de nodos registrados
GET  /api/nodes/{id}/readings       → últimas lecturas de un nodo
POST /api/config                    → actualizar configuración
POST /api/actuators/{id}/command    → enviar comando a actuador
POST /api/ota                       → iniciar actualización OTA
```

### 2. Sin estado (*stateless*)

Cada petición contiene toda la información necesaria. El servidor no guarda sesión entre peticiones. Esto es especialmente importante en un microcontrolador donde la RAM es limitada.

### 3. Métodos HTTP con semántica clara

| Método | Semántica | Idempotente |
|--------|-----------|-------------|
| `GET` | Leer recurso, sin efectos secundarios | Sí |
| `POST` | Crear o enviar acción | No |
| `PUT` | Reemplazar recurso completo | Sí |
| `PATCH` | Actualizar campos parcialmente | No necesariamente |
| `DELETE` | Eliminar recurso | Sí |

En el ESP32, `GET` y `POST` cubren el 90 % de los casos de uso.

### 4. Códigos de estado HTTP

```
200 OK              → éxito
201 Created         → recurso creado
400 Bad Request     → JSON malformado o campo faltante
404 Not Found       → nodo no registrado
500 Internal Error  → error de firmware
```

## API clave: esp_http_server + cJSON

```c
// Registrar un endpoint REST en el gateway
static esp_err_t api_status_handler(httpd_req_t *req) {
    // Construir respuesta JSON con cJSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "gateway_id", gateway_get_mac_str());
    cJSON_AddNumberToObject(root, "uptime_s", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(root, "nodes_registered", espnow_get_peer_count());

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

static const httpd_uri_t uri_status = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = api_status_handler,
};
```

**Por qué cJSON**: es la biblioteca de serialización JSON recomendada por Espressif para ESP-IDF. Está incluida como componente de ESP-IDF, maneja la asignación de memoria internamente y tiene una API clara. Siempre liberar el objeto con `cJSON_Delete` y el string con `free` para evitar fugas de memoria.

## Gotchas

- `httpd_resp_sendstr` solo puede llamarse UNA vez por petición; para respuestas grandes usar `httpd_resp_send_chunk`
- `cJSON_PrintUnformatted` es más eficiente en memoria que `cJSON_Print` (sin indentación)
- El servidor HTTP del ESP32 tiene un límite de conexiones simultáneas (por defecto 7); ajustar con `config.max_open_sockets`
- Los handlers se ejecutan en tareas del servidor HTTP — no bloquear con operaciones largas; usar colas de FreeRTOS si hace falta
- Manejar el caso `NULL` de `cJSON_ParseWithLength`: un JSON malformado no debe crashear el servidor

## Cuándo usar REST vs WebSocket

| Caso de uso | REST | WebSocket |
|-------------|------|-----------|
| Leer estado del gateway | ✓ | — |
| Obtener lista de nodos | ✓ | — |
| Actualizar configuración | ✓ | — |
| Iniciar OTA | ✓ | — |
| Datos de sensor en tiempo real | — | ✓ |
| Alertas inmediatas | — | ✓ |

## Referencias

- [[WebSocket]] — Canal de push complementario a REST
- Fase 4: `Fases/fase-04-mqtt-dashboard.md` — Subtarea 4.1 (REST API) y 4.3 (MQTT opcional)
- Tutorial 4: `Tutorial/tutorial-04-mqtt-dashboard.md`
- [cJSON en ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_http_server.html)
