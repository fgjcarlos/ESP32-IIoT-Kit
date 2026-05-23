# ESP-NOW

#concepto #fase/0 #fase/1 #fase/2

## Que es
Protocolo de comunicacion propietario de Espressif que permite enviar datos directamente entre dispositivos ESP32 sin necesidad de un router WiFi. Opera sobre la capa MAC de WiFi (802.11) pero sin la overhead de TCP/IP.

## Por que importa en este proyecto
Es el **backbone de comunicacion** entre los nodos sensores y el gateway. Los nodos despiertan del [[Deep-Sleep]], envian datos por ESP-NOW, y vuelven a dormir. No necesitan conectarse a un AP, lo que ahorra energia y tiempo.

## Como funciona
1. Ambos dispositivos inicializan WiFi (sin conectarse a un AP)
2. Se registran como "peers" usando sus direcciones MAC
3. Un dispositivo envia hasta 250 bytes al otro
4. El receptor ejecuta un callback al recibir el paquete
5. En modo unicast, hay ACK automatico a nivel MAC

## Caracteristicas clave

| Propiedad | Valor |
|-----------|-------|
| Payload maximo | 250 bytes por frame |
| Latencia | < 10ms tipico |
| Peers sin cifrar | 20 maximo |
| Peers cifrados (PMK/LMK) | 6 maximo |
| Canal | Debe coincidir con WiFi si WiFi esta activo |
| Rango | ~200m linea de vista, ~50m con obstaculos |

## API clave
```c
esp_now_init()                    // Inicializar ESP-NOW
esp_now_register_recv_cb(cb)      // Registrar callback de recepcion
esp_now_register_send_cb(cb)      // Registrar callback de envio
esp_now_add_peer(&peer_info)      // Agregar un peer
esp_now_send(mac, data, len)      // Enviar datos a un peer
esp_now_is_peer_exist(mac)        // Verificar si un peer existe
esp_now_deinit()                  // Limpiar ESP-NOW
```

## Gotchas
- WiFi DEBE estar inicializado y arrancado antes de ESP-NOW, aunque no te conectes a ningun AP
- ESP-NOW y WiFi comparten la misma radio → mismo canal obligatorio
- En broadcast (`FF:FF:FF:FF:FF:FF`), SEND_SUCCESS solo significa "transmitido", no "recibido"
- En unicast, SEND_SUCCESS significa que el receptor envio ACK a nivel MAC
- El callback de recepcion se ejecuta en la tarea WiFi — no hacer operaciones pesadas ahi
- Si el gateway cambia de canal (por reconexion STA), los nodos deben ajustarse → ver [[002-esp-now-sin-cifrado]]

## Decision de proyecto
Ver [[002-esp-now-sin-cifrado]]: usamos 20 peers sin cifrar en v1.0

## Enlaces
- [[WiFi-Modos]] - ESP-NOW requiere WiFi activo
- [[Deep-Sleep]] - Los nodos duermen entre envios
- [[003-protocolo-unificado]] - Protocolo que viaja sobre ESP-NOW
- [Doc oficial](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/network/esp_now.html)
