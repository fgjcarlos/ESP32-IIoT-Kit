# MQTT

#concepto

## Que es
(por completar cuando trabajes con este concepto)

## Por que importa en este proyecto

En ESP32-IIoT-Kit, **MQTT es una integracion opcional**, no el canal primario de datos. El gateway sirve datos localmente a traves de su dashboard Preact embebido (REST API + WebSocket). MQTT se activa cuando se quiere conectar a un broker externo (Mosquitto en LAN, o un broker cloud como HiveMQ, AWS IoT, etc.).

El espacio de nombres MQTT es **configurable**: se almacena en NVS como `mqtt_namespace` (valor por defecto: `iiot-kit`). Formato de topics: `{mqtt_ns}/{gateway_id}/nodo/{node_id}/...`

Ver Fase 4 (Dashboard Embebido + API REST) para el diseno completo.

## Como funciona
(por completar)

## API clave
(por completar)

## Gotchas
(por completar — anotar cosas que te sorprendan)

## Enlaces
- [[`_MOC`]]
