# Tutorial: Sistema de Monitorizacion Inteligente para Piscifactoria

## Que es este proyecto

Este proyecto consiste en construir un sistema completo de monitorizacion y control para piscifactorias (granjas de peces) utilizando microcontroladores ESP32. El sistema mide parametros criticos del agua como temperatura, pH, oxigeno disuelto, ORP y nivel, enviando los datos de forma inalambrica a un gateway central que los procesa, almacena y visualiza en un dashboard web.

La arquitectura se basa en nodos sensores autonomos (ESP32-C3) que funcionan con bateria durante meses, un gateway central (ESP32-S3) que coordina todo el sistema, y un servidor con dashboard para visualizacion remota. La comunicacion entre nodos y gateway usa ESP-NOW, un protocolo ligero y eficiente de Espressif que no requiere router WiFi.

Este es un **proyecto educativo**. El objetivo principal es aprender desarrollo de sistemas embebidos con ESP-IDF, comunicaciones inalambricas, protocolos IoT y desarrollo web fullstack, construyendo algo real y funcional.

## Que vas a aprender

- **ESP-IDF y programacion en C** para microcontroladores ESP32
- **Comunicaciones inalambricas**: ESP-NOW, WiFi (AP, STA, APSTA)
- **Protocolos IoT**: MQTT, REST API, WebSocket
- **Gestion de energia**: deep sleep, presupuestos de consumo, optimizacion bateria
- **Sensores y actuadores**: I2C, SPI, ADC, 1-Wire, calibracion
- **Servidor web embebido**: HTTP server en un microcontrolador
- **Backend y frontend**: Bun/TypeScript, React, bases de datos de series temporales
- **OTA (Over-The-Air)**: actualizaciones remotas de firmware
- **Seguridad en IoT**: cifrado, TLS, autenticacion

## Requisitos previos

Antes de empezar necesitas:

- **Programacion en C**: variables, funciones, structs, punteros, memoria. No hace falta ser experto, pero debes sentirte comodo leyendo y escribiendo C basico.
- **Electronica basica**: saber que es voltaje, corriente, resistencia. Poder conectar componentes en una breadboard. Entender un pinout de un modulo.
- **Linea de comandos**: moverte por directorios, ejecutar comandos, usar Git basico.
- **Opcional pero recomendable**: nociones de redes (IP, puerto, cliente/servidor) y algo de JavaScript/TypeScript para las fases de dashboard.

## Arquitectura del sistema

```
┌─────────────────────────────────────────────────┐
│              SERVIDOR (LAN / Cloud)              │
│                                                  │
│  Mosquitto ←→ API Bun/TS + SQLite ←→ Dashboard  │
│  (Broker MQTT)     (Backend)       (React + Vite)│
└────────┬─────────────────────────────────────────┘
         │ MQTT por WiFi
         │
┌────────▼────────┐
│    GATEWAY       │ ←── WiFi AP (panel web embebido)
│    ESP32-S3      │
│                  │     Recibe datos de todos los nodos
│  WiFi AP + STA   │     Publica por MQTT al servidor
│  + ESP-NOW       │     Sirve pagina web de configuracion
└────────┬────────┘
         │ ESP-NOW (sin router, directo por radio)
         │
    ┌────┼────────────┐
    │    │            │
 Nodo A  Nodo B    Nodo C        ← ESP32-C3 con bateria
 Temp/DO  pH/ORP   Nivel
```

**Flujo de datos**: Un nodo sensor se despierta del deep sleep, lee su sensor, envia los datos por ESP-NOW al gateway, y vuelve a dormir. El gateway recibe los datos, los muestra en su panel web local, y los publica por MQTT al servidor. El dashboard web los muestra en tiempo real con graficos.

## Mapa de tutoriales

| Fase | Tutorial | Conceptos clave | Lectura est. |
| ---- | -------- | --------------- | ------------ |
| 0 | [Primeros pasos con ESP32 y ESP-IDF](tutorial-00-preparacion.md) | Variantes ESP32, ESP-IDF vs Arduino, build system, GPIO, I2C/SPI/ADC, modos WiFi, modos energia | 45 min |
| 1 | [Construyendo el cerebro del sistema](tutorial-01-gateway-nucleo.md) | NVS, WiFi APSTA, event-driven, HTTP server embebido, REST API, SNTP, watchdog | 40 min |
| 2 | [Nodos autonomos y comunicacion fiable](tutorial-02-nodos-espnow.md) | Deep sleep, RTC memory, protocolo binario, ACK/retry, auto-discovery, FreeRTOS basico | 40 min |
| 3 | [Midiendo el mundo real](tutorial-03-sensores-actuadores.md) | Sensores analogicos vs digitales, 1-Wire, calibracion, filtrado, reles, histeresis | 35 min |
| 4 | [Del microcontrolador a la pantalla](tutorial-04-mqtt-dashboard.md) | MQTT pub/sub, topics, QoS, Mosquitto, WebSocket, React, series temporales | 40 min |
| 5 | [Actualizaciones seguras y sistema robusto](tutorial-05-ota-optimizacion.md) | OTA A/B particiones, rollback, TLS, seguridad IoT, stress testing | 35 min |
| 6 | [Hacia donde seguir](tutorial-06-mejoras-futuras.md) | Mesh networking, Edge ML, IoT celular, escalabilidad | 20 min |

## Como seguir los tutoriales

1. **Lee el tutorial** de la fase correspondiente para entender los conceptos
2. **Consulta las tareas** en la carpeta `Fases/` para saber exactamente que implementar
3. **Vuelve al tutorial** como referencia cuando necesites repasar un concepto
4. **Responde las preguntas de reflexion** al final de cada tutorial para consolidar lo aprendido
5. **No saltes fases**: cada fase construye sobre la anterior

## Glosario

| Termino | Descripcion |
| ------- | ----------- |
| **ADC** | Analog-to-Digital Converter. Convierte señales analogicas (voltaje continuo) a valores digitales que el microcontrolador puede procesar. |
| **AP** | Access Point. Modo WiFi en el que el ESP32 crea su propia red, como un router. Otros dispositivos se conectan a el. |
| **APSTA** | Modo WiFi dual: el ESP32 actua como AP y STA simultaneamente. Crea su red Y se conecta a otra. |
| **Deep sleep** | Modo de bajo consumo extremo del ESP32. Apaga casi todo el chip excepto el controlador RTC. Consume microamperios. Al despertar, el programa arranca desde cero (como un reinicio). |
| **ESP-IDF** | Espressif IoT Development Framework. El SDK oficial de Espressif para programar ESP32 en C. Basado en FreeRTOS. |
| **ESP-NOW** | Protocolo de comunicacion propietario de Espressif. Permite enviar datos entre ESP32 sin necesidad de router WiFi. Rapido, bajo consumo, maximo 250 bytes por mensaje. |
| **FreeRTOS** | Sistema operativo en tiempo real que ejecuta ESP-IDF. Gestiona tareas (hilos), semaforos, colas y temporizadores. |
| **I2C** | Inter-Integrated Circuit. Bus de comunicacion serie con 2 cables (SDA, SCL). Permite conectar multiples sensores con solo 2 pines. |
| **LMK** | Local Master Key. Clave de cifrado ESP-NOW especifica para cada par de dispositivos. |
| **MQTT** | Message Queuing Telemetry Transport. Protocolo de mensajeria ligero para IoT. Basado en publicar/suscribirse a "topics" (canales). |
| **NVS** | Non-Volatile Storage. Almacenamiento clave-valor en la memoria flash del ESP32. Sobrevive a reinicios y cortes de alimentacion. |
| **OTA** | Over-The-Air. Actualizacion de firmware sin conectar cable USB. Se descarga el nuevo firmware por WiFi. |
| **PMK** | Primary Master Key. Clave de cifrado ESP-NOW compartida por todos los dispositivos del grupo. |
| **QoS** | Quality of Service. En MQTT, define el nivel de garantia de entrega: 0 (best effort), 1 (al menos una vez), 2 (exactamente una vez). |
| **RSSI** | Received Signal Strength Indicator. Medida de la potencia de señal recibida, en dBm. Mas cercano a 0 = mejor señal. |
| **RTC memory** | Memoria del controlador RTC del ESP32. Pequeña (8KB) pero sobrevive al deep sleep. Util para guardar estado entre ciclos de sueño. |
| **SNTP** | Simple Network Time Protocol. Permite sincronizar el reloj del ESP32 con un servidor de tiempo en Internet. |
| **SPI** | Serial Peripheral Interface. Bus de comunicacion rapido con 4 cables (MOSI, MISO, SCK, CS). Usado para sensores de alta velocidad y pantallas. |
| **STA** | Station. Modo WiFi en el que el ESP32 se conecta a una red existente (como tu telefono se conecta al router de casa). |
| **Watchdog** | Temporizador de vigilancia. Si el programa no lo "alimenta" periodicamente, reinicia el ESP32. Protege contra bloqueos del software. |
| **1-Wire** | Protocolo de comunicacion con un solo cable de datos. Usado por sensores como el DS18B20 (temperatura). |
