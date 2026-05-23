# ESP32-IIoT-Kit — Plataforma IIoT Genérica sobre ESP32

## Índice

- [1. Visión General](#1-visión-general)
- [2. Arquitectura del Sistema](#2-arquitectura-del-sistema)
- [3. Stack Tecnológico](#3-stack-tecnológico)
- [4. Fases de Implementación](#4-fases-de-implementación)
- [5. Especificaciones Técnicas](#5-especificaciones-técnicas)
- [6. Gestión de Riesgos](#6-gestión-de-riesgos)
- [7. Recursos y Cronograma](#7-recursos-y-cronograma)
- [8. Métricas de Éxito](#8-métricas-de-éxito)
- [9. Conclusiones](#9-conclusiones)

---

## 1. Visión General

### 1.1 Objetivo del Proyecto

**ESP32-IIoT-Kit** es una plataforma de monitorización y control IIoT (Industrial Internet of Things) autónoma construida sobre microcontroladores ESP32. El objetivo es proporcionar una base de aprendizaje completa para cualquier dominio de aplicación: sistemas agrícolas, instalaciones industriales, edificios inteligentes, entornos de laboratorio y cualquier entorno que requiera monitorización distribuida de sensores.

El proyecto tiene una vocación **didáctica**: enseñar ESP-IDF, comunicaciones embebidas y arquitecturas IoT mediante un sistema real y funcional, no un ejemplo de juguete.

### 1.2 Problemática Resuelta

- **Dependencia de infraestructura externa**: los sistemas de monitorización convencionales requieren servidores, bases de datos y dashboards externos para funcionar. ESP32-IIoT-Kit es completamente autónomo: el gateway ESP32-S3 sirve su propia interfaz web desde flash.
- **Alta complejidad de instalación**: sin servidores externos, el despliegue se reduce a flashear el gateway y los nodos.
- **Falta de estandarización en sensores**: el protocolo de mensajería sobre ESP-NOW es genérico y admite cualquier tipo de sensor mediante un tipo personalizable (`SENSOR_TYPE_CUSTOM`).
- **Costes elevados** de sistemas propietarios equivalentes.

### 1.3 Valor Aportado

| Aspecto | Beneficio |
|---------|-----------|
| **Autonomía** | Gateway completamente autónomo: Preact SPA + REST API + WebSocket embebidos en SPIFFS |
| **Escalabilidad** | De 2 a 20+ nodos sin infraestructura adicional |
| **Fiabilidad** | Operación continua sin dependencia de conectividad a Internet |
| **Autonomía de nodos** | 12+ meses con baterías 18650 (wake cada 5 minutos) |
| **Extensibilidad** | Arquitectura de ejemplos (`examples/`) para cualquier dominio |

---

## 2. Arquitectura del Sistema

### 2.1 Topología de Red

El gateway ESP32-S3 es el corazón del sistema. Es un dispositivo completamente autónomo que no requiere servidores externos para operar.

```
                   ┌──────────────────────────────────┐
                   │        ESP32-S3 GATEWAY           │
                   │        (Totalmente Autónomo)       │
                   │                                    │
                   │  ┌──────────────────────────────┐  │
  Browser ◄───────►│  │  Preact SPA (SPIFFS)         │  │
  (cualquier       │  │  ~200 KB gzipped             │  │
  dispositivo)     │  └──────────────────────────────┘  │
                   │  ┌──────────────────────────────┐  │
  REST API ◄──────►│  │  esp_http_server              │  │
                   │  │  Config + Datos + OTA          │  │
                   │  └──────────────────────────────┘  │
                   │  ┌──────────────────────────────┐  │
  WebSocket ◄─────►│  │  Push de datos en tiempo real │  │
                   │  │  Stream de eventos de sensores │  │
                   │  └──────────────────────────────┘  │
                   │  ┌──────────────────────────────┐  │
  MQTT (opcional) ◄►  │  mqtt_bridge                  │  │
                   │  │  Integración cloud/LAN         │  │
                   │  └──────────────────────────────┘  │
                   │  ┌──────────────────────────────┐  │
  ESP-NOW ◄───────►│  │  espnow_manager               │  │
  (nodos sensores) │  │  Protocolo backbone            │  │
                   │  └──────────────────────────────┘  │
                   └──────────────────────────────────┘
                              │ ESP-NOW (2.4GHz)
                   ┌──────────┼──────────────┐
                   │          │              │
                Nodo A     Nodo B         Nodo C
                ESP32-C3   ESP32-C3       ESP32-C3
                (cualquier sensor + deep sleep)
```

**Principio de diseño clave**: el gateway es completamente funcional sin conectividad a Internet. Un navegador se conecta directamente al WiFi AP del gateway y carga la Preact SPA desde SPIFFS. Los datos fluyen por WebSocket en tiempo real y por REST para configuración e historial. MQTT es una integración opcional para conectividad cloud o LAN.

### 2.2 Protocolos de Comunicación

**Prioridad 1: ESP-NOW (backbone del sistema)**

- **Función**: Comunicación directa nodo → gateway para datos de sensores
- **Ventajas**: Baja latencia (<10ms), bajo consumo, cifrado AES-128 (CCMP)
- **Limitación**: No es mesh nativo. Los nodos envían directamente al gateway. Si un nodo no alcanza al gateway, un nodo intermedio puede actuar como relay (implementación personalizada, no enrutamiento automático)
- **Payload máximo**: 250 bytes por frame
- **Peers máximos**: 20 no cifrados (limitación hardware). En v1.0 se usan 20 peers sin cifrar para maximizar la cantidad de nodos. Ver `docs/replanificacion/01-decisiones-corregidas.md` (DC-01)

**Prioridad 2: WiFi AP**

- **Función**: Configuración local, acceso al dashboard embebido (Preact SPA en SPIFFS)
- **Ventajas**: Sin dependencia externa; la interfaz web se carga directamente desde el gateway
- **Activación**: Siempre activo en el gateway (coexiste con ESP-NOW y WiFi STA)

**Prioridad 3: WiFi STA**

- **Función**: Conexión a Internet o red local para MQTT opcional, OTA, sincronización horaria (SNTP)
- **Ventajas**: Integración con sistemas cloud o LAN cuando se necesita
- **Gestión**: Reconexión automática con backoff exponencial

**Nota sobre coexistencia**: ESP-NOW y WiFi comparten el radio 2.4GHz. Ambos pueden operar simultáneamente si ESP-NOW usa el mismo canal que WiFi. El gateway debe fijar el canal WiFi y configurar ESP-NOW en ese mismo canal.

---

## 3. Stack Tecnológico

### 3.1 Firmware (ESP32)

| Componente | Tecnología | Justificación |
|------------|------------|---------------|
| **Lenguaje** | C (C11) | Nativo ESP-IDF, máxima documentación, ejemplos oficiales |
| **Framework** | ESP-IDF v5.x | SDK oficial de Espressif, todas las features first-class |
| **Build system** | CMake + `idf.py` | Standard ESP-IDF |
| **Comunicación** | `esp_now`, `esp_mqtt`, `esp_http_server` | Componentes nativos ESP-IDF |
| **Almacenamiento** | `nvs_flash` + SPIFFS | NVS para configuración; SPIFFS para Preact SPA |
| **OTA** | `esp_ota_ops` + `esp_https_ota` | Particiones A/B con rollback |
| **Testing** | Unity (integrado en ESP-IDF), Wokwi | Tests unitarios + simulación |

### 3.2 Dashboard Embebido (Gateway ESP32-S3)

| Componente | Tecnología | Justificación |
|------------|------------|---------------|
| **Servidor** | `esp_http_server` | Nativo ESP-IDF, soporte WebSocket nativo |
| **Frontend** | Preact + Vite | ~3KB runtime, compatible React; builds de ~200KB gzipped en SPIFFS |
| **Datos en tiempo real** | WebSocket (esp_http_server) | Push de eventos de sensores al navegador |
| **API de datos y config** | REST sobre HTTP (`esp_http_server`) | Endpoints `/api/status`, `/api/nodes`, `/api/config`, `/api/actuators` |
| **MQTT (opcional)** | `esp_mqtt_client` | Integración cloud/LAN cuando se necesita; no requerida para operación básica |

### 3.3 Dominio de Ejemplo: Piscifactoría

El tutorial enseña la plataforma genérica. Para aplicaciones específicas de dominio (sensores de pH, oxígeno disuelto, ORP, nivel de agua), consulta `examples/fish-farm/`. Esos ejemplos usan exactamente la misma plataforma con sensores y configuraciones específicas del dominio de aplicación.

---

## 4. Fases de Implementación

### FASE 0: PREPARACIÓN Y APRENDIZAJE (3 semanas)

**Objetivo**: Validar hardware y establecer el entorno de desarrollo

- **Setup ESP-IDF v5.x** con toolchain para ESP32-S3 (Xtensa) y ESP32-C3 (RISC-V)
- **Blink + UART** en ambas placas para validar el toolchain
- **Lectura de sensor** por I2C/1-Wire (DS18B20 como sensor de temperatura de referencia)
- **ESP-NOW básico**: envío/recepción entre 2 placas
- **WiFi AP + STA** simultáneo en el S3 (modo APSTA)
- **Benchmark de consumo** con multímetro: active, modem sleep, light sleep, deep sleep
- **Setup Preact + Vite**: instalar Node.js, crear proyecto de prueba, verificar build
- **Definir estructura del proyecto** (componentes ESP-IDF, convenciones de código)

**Entregables**: Repositorio Git estructurado, informe de benchmarks, toolchain documentado

### FASE 1: GATEWAY - NÚCLEO (4 semanas)

**Objetivo**: Gateway funcional con conectividad triple

- **Inicialización del sistema**: NVS → WiFi APSTA → ESP-NOW → SNTP
- **Gestor de conexiones** con reconexión automática WiFi STA (backoff exponencial)
- **Servidor web embebido** (`esp_http_server`) con página de configuración WiFi
- **API REST mínima**: GET /api/status, GET /api/nodes, POST /api/config
- **Recepción ESP-NOW** con registro de nodos (MAC, tipo, último contacto)
- **Almacenamiento NVS** para credenciales WiFi, configuración de nodos
- **Sistema de logging** con `esp_log` (niveles: error, warn, info, debug, verbose)
- **Watchdog timer** configurado para reinicio automático ante bloqueos

**Entregables**: Gateway funcional con web de configuración, documentación API

### FASE 2: NODOS SENSORES + ESP-NOW FIABLE (4 semanas)

**Objetivo**: Nodos autónomos que reportan al gateway de forma fiable

- **Firmware de nodo genérico** parametrizable por tipo de sensor
- **Ciclo de vida del nodo**: deep sleep → wake → leer sensor → enviar ESP-NOW → deep sleep
- **Protocolo de mensajería** sobre ESP-NOW (ver `docs/replanificacion/02-protocolo-unificado.md`):
  - Header: tipo mensaje (1B) + ID nodo (6B MAC) + secuencia (2B) + timestamp (4B)
  - Payload: datos sensor (variable, max 230B)
  - ACK desde gateway (el nodo reintenta 3 veces si no recibe ACK)
- **Registro de nodos** en el gateway (auto-discovery por primer mensaje)
- **Relay básico** (opcional): un nodo puede reenviar mensajes de otro nodo si el gateway lo configura. No es mesh automático, es relay estático configurado
- **Gestión de energía** optimizada:
  - Wake cada N minutos (configurable por NVS)
  - Si falla el envío 5 veces seguidas → entrar en modo ahorro (wake cada 30 min)
  - Si recibe comando del gateway → ajustar intervalo
- **Sincronización de tiempo** básica: el gateway incluye timestamp en los ACK

**Entregables**: Firmware de nodo genérico, protocolo documentado, ejemplo con 3 nodos

### FASE 3: SENSORES Y ACTUADORES (3 semanas)

**Objetivo**: Driver universal de sensor y control de actuadores

- **DS18B20 como sensor de referencia**: driver 1-Wire completo, integración con el ciclo de nodo
- **sensor_manager genérico**: patrón de abstracción para cualquier tipo de sensor
- **SENSOR_TYPE_CUSTOM = 0xFF**: extensión del protocolo para sensores definidos por el usuario
- **Validación de lecturas**: rango válido por tipo de sensor, filtro de outliers
- **Control de actuadores** en el gateway o nodos dedicados:
  - Relés (GPIO) para cualquier carga (bombas, válvulas, avisadores, etc.)
  - Activación por umbral (configurable) o por comando remoto vía REST
- **Alertas locales** en el gateway: si una lectura supera un umbral → activar actuador + flag de alerta

Para sensores específicos de dominio (pH, oxígeno disuelto, ORP, nivel de agua), consulta `examples/fish-farm/sensors.md`.

**Entregables**: Driver DS18B20, sensor_manager genérico, sistema de alertas, manual de extensión

### FASE 4: DASHBOARD EMBEBIDO + API REST (4 semanas)

**Objetivo**: Sistema completo de visualización y control embebido en el gateway

**REST API (esp_http_server):**
- `GET /api/status` — Estado del gateway (WiFi, memoria, uptime)
- `GET /api/nodes` — Lista de nodos registrados con último dato
- `GET /api/nodes/{id}/readings` — Histórico de lecturas de un nodo
- `POST /api/config` — Actualización de configuración (umbrales, intervalos)
- `POST /api/actuators/{id}` — Control de actuadores
- Serialización JSON con cJSON (nativo ESP-IDF)

**WebSocket (esp_http_server):**
- Push de eventos de sensores en tiempo real al navegador
- Reconexión automática gestionada en el cliente Preact
- Formato: `{ "node_id": "aa:bb:...", "type": "temperature", "value": 23.5, "ts": 1748000000 }`

**Preact SPA (SPIFFS):**
- Proyecto Preact + Vite en `firmware/gateway/web/`
- Páginas: dashboard (tarjetas de nodos en tiempo real), configuración (WiFi, umbrales), estado (salud del gateway)
- Build con Vite → gzip → flash a partición SPIFFS (1.9MB)
- Presupuesto de tamaño: objetivo ≤ 200KB gzipped, límite duro 1.9MB SPIFFS
- CMake hook para construir Preact antes del flash

**MQTT como integración opcional:**
- `mqtt_bridge` puede activarse con credenciales en NVS
- Namespace configurable: clave NVS `mqtt_namespace`, valor por defecto `iiot-kit`
- Patrón de topic: `{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}`
- Para temas MQTT específicos de dominio, ver `examples/fish-farm/mqtt-topics.md`

**Entregables**: Preact SPA funcional, REST API completa, WebSocket operativo, documentación de topics MQTT

### FASE 5: OTA + OPTIMIZACIÓN + PRUEBAS (4 semanas)

**Objetivo**: Estabilidad, seguridad y mantenimiento remoto

- **OTA para gateway** vía WiFi: descarga HTTPS → `esp_ota_ops` → reboot con rollback automático
- **OTA para nodos**: el nodo se conecta temporalmente al WiFi AP del gateway para recibir OTA (no vía ESP-NOW, demasiado frágil para v1.0)
- **Pruebas de estrés** con 10+ nodos enviando datos simultáneamente
- **Pruebas de reconexión**: cortar WiFi, cortar alimentación gateway, interferencia
- **Pruebas de autonomía real**: nodo con batería midiendo consumo durante 1 semana
- **Seguridad**:
  - ESP-NOW: cifrado AES con PMK/LMK
  - MQTT: TLS con certificados (mbedtls integrado en ESP-IDF)
  - Web embebida: autenticación básica (usuario/contraseña en NVS)
  - WiFi AP: WPA2 con contraseña fuerte
- **Documentación completa** de instalación, configuración y mantenimiento
- **Kit de despliegue**: scripts de flasheo, particiones preconfiguradas, guía paso a paso

**Entregables**: Release v1.0, informe de pruebas, manual de despliegue

### FASE 6: MEJORAS FUTURAS (continuo)

**Objetivo**: Funcionalidades avanzadas post-v1.0

- **MQTT Sparkplug B**: migración desde MQTT plano a Sparkplug B v1.0 para interoperabilidad industrial. Gateway como EoN Node, nodos sensores como Devices. Protobuf/nanopb en ESP32
- **OTA vía ESP-NOW** para nodos (fragmentación de firmware en paquetes de 250B con ACK)
- **Mesh relay automático** con descubrimiento de rutas
- **Machine Learning edge** para detección de anomalías (TFLite Micro en ESP32-S3)
- **Soporte 4G/LTE** mediante módulo SIM7600 conectado al gateway
- **Exportación de datos** CSV/JSON desde el dashboard
- **Sistema multi-usuario** con roles y permisos en el dashboard
- **API pública** para integración con sistemas de terceros
- **Certificaciones** CE/FCC si se plantea comercialización

---

## 5. Especificaciones Técnicas

### 5.1 Requisitos Hardware

| Componente | Especificación | Cantidad | Coste Aprox. |
|------------|----------------|----------|--------------|
| Gateway | ESP32-S3-WROOM-1 (8MB Flash, 2MB PSRAM) | 1 | 8 € |
| Nodos | ESP32-C3-MINI-1 (4MB Flash) | 3-10 | 4 €/unidad |
| Sensor de referencia | DS18B20 (temperatura 1-Wire) | 1+ | 2 €/unidad |
| Baterías | Li-Ion 18650 3400mAh (ej. NCR18650B) | 3-10 | 5 €/unidad |
| Cargador | TP4056 con protección | 3-10 | 1 €/unidad |
| Carcasas | IP67/IP68 para exterior | según necesidad | 5-15 € |
| **Total inicial (3 nodos)** | | | **~100-150 €** |

*Para sensores adicionales específicos de dominio (pH, ORP, nivel, etc.), consulta `examples/fish-farm/` u otros ejemplos de dominio.*

### 5.2 Tabla de Particiones ESP32

**Gateway (ESP32-S3, 8MB Flash):**

```
# Name,   Type, SubType,  Offset,   Size
nvs,      data, nvs,      0x9000,   0x6000    (24KB)
phy_init, data, phy,      0xf000,   0x1000    (4KB)
ota_0,    app,  ota_0,    0x10000,  0x300000  (3MB)
ota_1,    app,  ota_1,    0x310000, 0x300000  (3MB)
storage,  data, spiffs,   0x610000, 0x1E0000  (1.9MB - Preact SPA + assets)
otadata,  data, ota,      0x7F0000, 0x2000    (8KB)
```

**Nodo (ESP32-C3, 4MB Flash):**

```
# Name,   Type, SubType,  Offset,   Size
nvs,      data, nvs,      0x9000,   0x6000    (24KB)
phy_init, data, phy,      0xf000,   0x1000    (4KB)
ota_0,    app,  ota_0,    0x10000,  0x180000  (1.5MB)
ota_1,    app,  ota_1,    0x190000, 0x180000  (1.5MB)
otadata,  data, ota,      0x310000, 0x2000    (8KB)
```

### 5.3 Especificaciones de Rendimiento

| Métrica | Objetivo | Límite |
|---------|----------|--------|
| Latencia ESP-NOW (1 salto) | < 50ms | < 100ms |
| Autonomía nodos (wake cada 5 min, 3400mAh) | 15 meses | 12 meses mínimo |
| Autonomía nodos (wake cada 1 min, 3400mAh) | 5 meses | 3 meses mínimo |
| Nodos por gateway | 15 activos | 20 máximo (limitación ESP-NOW peers sin cifrar) |
| Rango ESP-NOW | 200m (línea de visión) | 50m (con obstáculos) |
| Tiempo respuesta dashboard embebido | < 500ms | < 2s |
| Uso memoria RAM gateway | < 70% | < 85% |
| Tamaño binario gateway | < 1.5MB | < 2MB |
| Tamaño binario nodo | < 800KB | < 1.2MB |
| **Preact SPA gzipped** | ≤ 200KB | ≤ 500KB (presupuesto SPIFFS) |

### 5.4 Estructura del Proyecto

```
ESP32-IIoT-Kit/
├── firmware/
│   ├── gateway/                  # Proyecto ESP-IDF para ESP32-S3
│   │   ├── main/
│   │   │   ├── main.c
│   │   │   ├── wifi_manager.c/.h
│   │   │   ├── espnow_manager.c/.h
│   │   │   ├── mqtt_bridge.c/.h  # MQTT opcional
│   │   │   ├── http_server.c/.h  # REST API + WebSocket
│   │   │   ├── ota_manager.c/.h
│   │   │   ├── nvs_config.c/.h
│   │   │   └── actuator_ctrl.c/.h
│   │   ├── components/
│   │   │   └── protocol/         # Protocolo de mensajería compartido
│   │   ├── web/                  # Fuente Preact SPA (build → SPIFFS)
│   │   │   ├── src/
│   │   │   │   ├── app.jsx
│   │   │   │   ├── components/
│   │   │   │   └── hooks/
│   │   │   ├── package.json
│   │   │   └── vite.config.js    # Preact + build con gzip
│   │   ├── partitions.csv
│   │   └── CMakeLists.txt
│   │
│   ├── node/                     # Proyecto ESP-IDF para ESP32-C3
│   │   ├── main/
│   │   │   ├── main.c
│   │   │   ├── espnow_node.c/.h
│   │   │   ├── sensor_manager.c/.h
│   │   │   ├── power_manager.c/.h
│   │   │   └── nvs_config.c/.h
│   │   ├── components/
│   │   │   ├── protocol/         # Mismo componente compartido
│   │   │   └── drivers/          # Drivers de sensores
│   │   │       └── ds18b20/      # Driver DS18B20 (sensor de referencia)
│   │   ├── partitions.csv
│   │   └── CMakeLists.txt
│   │
│   └── common/                   # Código compartido (protocolo, tipos)
│       └── protocol/
│           ├── protocol.h        # + SENSOR_TYPE_CUSTOM = 0xFF
│           └── protocol.c
│
├── examples/                     # Ejemplos de dominio específico
│   └── fish-farm/
│       ├── README.md             # Visión general del ejemplo de dominio
│       ├── sensors.md            # Sensores específicos: pH, DO, ORP, nivel
│       └── mqtt-topics.md        # Jerarquía MQTT para piscifactoría
│
├── Tutorial/                     # Tutoriales didácticos (Fases 0-6)
├── Fases/                        # Documentación técnica detallada
├── knowledge/                    # Vault de conocimiento (Obsidian)
│   ├── Conceptos/                # Notas de conceptos técnicos
│   ├── Decisiones/               # Registros de decisiones (ADR)
│   ├── Proyecto/                 # Estado y arquitectura del proyecto
│   └── Referencia/               # Material de referencia rápida
├── docs/                         # Documentación interna y replanificación
├── docs_site/                    # Fuente MkDocs (sitio web publicado)
├── project.md                    # Este documento (fuente de verdad)
├── CLAUDE.md                     # Contexto para asistentes de IA
├── README.md                     # Presentación del repositorio
└── mkdocs.yml                    # Configuración del sitio MkDocs
```

---

## 6. Gestión de Riesgos

### 6.1 Riesgos Técnicos

| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|--------------|---------|------------|
| Interferencia WiFi/ESP-NOW en mismo canal | Media | Alta | ESP-NOW y WiFi DEBEN usar el mismo canal. Elegir canal menos congestionado al arrancar |
| Corrupción NVS por cortes de alimentación | Baja | Media | NVS tiene journaling interno. Backup de configuración crítica en otra partición |
| Consumo mayor al estimado en nodos | Media | Media | Medir consumo real en Fase 0. Ajustar duty cycle por configuración |
| Tamaño del bundle Preact supera presupuesto SPIFFS | Media | Media | Preact+Vite con gzip debería dar ~50-150KB. Documentar cómo medir y reducir |
| Límite de 20 peers sin cifrar ESP-NOW | Media | Alta | En v1.0 usar 20 peers sin cifrar. Cifrado por software (AES payload) planificado para v2.0 |
| Coexistencia ESP-NOW + WiFi AP + WiFi STA | Media | Alta | Probar en Fase 0. Las tres deben operar en el mismo canal WiFi |

### 6.2 Riesgos de Proyecto

| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|--------------|---------|------------|
| Complejidad de 3 protocolos simultáneos | Alta | Alta | Implementación incremental: primero ESP-NOW solo, luego WiFi, luego REST/WebSocket |
| Scope creep (añadir features antes de tener lo básico) | Alta | Alta | Fases estrictas. No pasar a Fase N+1 sin Fase N funcionando |
| Depuración remota en instalaciones sin conectividad | Media | Media | Logging por MQTT (opcional), core dumps en flash, modo diagnóstico activable |
| Mantenimiento a largo plazo | Media | Media | Código documentado, estructura modular, OTA para correcciones |

---

## 7. Recursos y Cronograma

### 7.1 Cronograma

| Fase | Duración | Acumulado |
|------|----------|-----------|
| Fase 0: Preparación | 3 semanas | 3 semanas |
| Fase 1: Gateway núcleo | 4 semanas | 7 semanas |
| Fase 2: Nodos + ESP-NOW | 4 semanas | 11 semanas |
| Fase 3: Sensores y actuadores | 3 semanas | 14 semanas |
| Fase 4: Dashboard embebido + API REST | 4 semanas | 18 semanas |
| Fase 5: OTA + Seguridad + Pruebas | 4 semanas | 22 semanas |
| **Total hasta v1.0** | **22 semanas** | |

### 7.2 Presupuesto Estimado (Hardware)

| Concepto | Coste |
|----------|-------|
| Hardware prototipos (gateway + 3 nodos + sensores básicos) | 150-200 € |
| Herramientas (multímetro, analizador lógico, fuente de laboratorio) | 150-200 € |
| **Total hardware mínimo** | **~300-400 €** |

*Nota: coste de desarrollo no incluido al ser proyecto de aprendizaje.*

---

## 8. Métricas de Éxito

### 8.1 KPIs Técnicos

- **Autonomía del gateway**: operación continua sin servidor externo (sin broker MQTT externo, sin servidor Node, sin base de datos)
- **Precisión mediciones**: dentro de ±5% de las especificaciones del sensor
- **Tiempo de despliegue de un nuevo nodo**: < 30 minutos (flash + registrar)
- **Consumo nodos**: < 50µA promedio (deep sleep con wake periódico)
- **Pérdida de paquetes ESP-NOW**: < 1% en condiciones normales
- **Tamaño Preact SPA gzipped**: ≤ 200KB (objetivo), ≤ 500KB (límite)

### 8.2 KPIs de Aprendizaje

- El desarrollador puede implementar un driver de sensor nuevo sin consultar ejemplos
- El desarrollador comprende el ciclo de vida completo de un nodo ESP-NOW
- El desarrollador puede añadir un endpoint REST sin modificar la arquitectura base
- El desarrollador puede desplegar el sistema completo en una instalación sin conectividad a Internet

---

## 9. Conclusiones

### 9.1 Viabilidad

El proyecto es **técnicamente viable**. La combinación de C/ESP-IDF + ESP32 + Preact/Vite embebido ofrece:

1. **Máxima madurez** del firmware gracias a ESP-IDF nativo en C
2. **Documentación abundante**: miles de ejemplos oficiales y comunitarios de Espressif
3. **Toolchain simple**: `idf.py build && idf.py flash && idf.py monitor`
4. **Separación clara**: firmware en C (donde importa el rendimiento), SPA en Preact (donde importa la mantenibilidad de la UI)
5. **Despliegue autónomo**: sin dependencias de infraestructura, funciona en cualquier entorno

### 9.2 Decisiones Técnicas Clave

| Decisión | Justificación |
|----------|---------------|
| **Preact en vez de React** para la SPA | React (~40KB+) es demasiado grande para SPIFFS. Preact (~3KB) es una alternativa compatible que genera builds de ~200KB con Vite |
| **Sin servidor externo** | El gateway ESP32-S3 es suficientemente potente para servir la SPA y la API REST. Un servidor externo añade complejidad de despliegue sin beneficio para un kit IIoT |
| **WebSocket sobre MQTT** para el dashboard | El dashboard embebido se conecta directamente por WebSocket al gateway. MQTT sigue disponible como integración opcional para sistemas cloud |
| **C en vez de Rust** para firmware | ESP-IDF es C nativo. Con experiencia en C, se accede directamente a toda la documentación oficial sin capas de indirección |
| **ESP-NOW punto a punto, no mesh** en v1.0 | Implementar mesh routing custom es complejo. Los nodos envían directo al gateway. Relay estático planificado como mejora futura |
| **ESP-NOW sin cifrar** en v1.0 | El hardware limita a 6 peers cifrados, insuficiente para 10-20 nodos. Cifrado AES-256 por software planificado para v2.0 |
| **DS18B20 como sensor de referencia** | Sensor de temperatura universal, fácil de obtener, driver 1-Wire bien documentado. Todos los estudiantes lo implementan como primer sensor |
| **SENSOR_TYPE_CUSTOM = 0xFF** | Extensión del protocolo para cualquier sensor definido por el usuario, sin modificar el protocolo base |
| **Baterías 3400mAh** | Para alcanzar 12+ meses de autonomía con wake cada 5 minutos |

### 9.3 Próximos Pasos

1. **Adquirir hardware**: 1x ESP32-S3-DevKitC, 3x ESP32-C3-DevKitM, sensor DS18B20 (1 semana)
2. **Setup ESP-IDF v5.x** y validar compilación para ambos targets (1 semana)
3. **Setup Preact + Vite**: validar que un build de prueba produce un bundle < 200KB gzipped (1 semana)
4. **Iniciar Fase 0** según cronograma

---

**Documento v3.0** — Stack: C (ESP-IDF) + Preact/Vite embebido
*Fecha: 2026-05-23 — Transformación a ESP32-IIoT-Kit (iiot-kit-transformation)*
