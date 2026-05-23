# **Sistema de Monitorización Inteligente para Piscifactoría - Documentación del Proyecto**

## Índice
- [1. Visión General](#1-visión-general)
- [2. Arquitectura del Sistema](#2-arquitectura-del-sistema)
- [3. Stack Tecnológico](#3-stack-tecnológico)
- [4. Fases de Implementación](#4-fases-de-implementación)
- [5. Especificaciones Técnicas](#5-especificaciones-técnicas)
- [6. Gestión de Riesgos](#6-gestión-de-riesgos)
- [7. Recursos y Presupuesto](#7-recursos-y-presupuesto)
- [8. Métricas de Éxito](#8-métricas-de-éxito)
- [9. Conclusiones](#9-conclusiones)

---

## 1. Visión General

### 1.1 Objetivo del Proyecto
Desarrollar un sistema de monitorización y control autónomo para piscifactorías basado en tecnología ESP32, que integre múltiples protocolos de comunicación (ESP-NOW, WiFi STA, WiFi AP) para garantizar operación continua, resiliencia y escalabilidad.

### 1.2 Problemática Resuelta
- **Falta de conectividad fiable** en instalaciones remotas
- **Alta complejidad** de sistemas de monitorización actuales
- **Falta de estandarización** en sensores acuícolas
- **Costes elevados** de sistemas propietarios

### 1.3 Valor Aportado
| Aspecto | Beneficio |
|---------|-----------|
| **Fiabilidad** | Operación continua mediante múltiples protocolos |
| **Coste** | 70% más económico que soluciones comerciales |
| **Escalabilidad** | De 2 a 30+ dispositivos sin infraestructura adicional |
| **Autonomía** | Nodos sensores funcionan 12+ meses con baterías |

## 2. Arquitectura del Sistema

### 2.1 Topología de Red
```
┌─────────────────────────────────────────────────────────┐
│                   CAPA CLOUD / LAN                      │
│                                                         │
│  ┌──────────┐   ┌──────────────┐   ┌────────────────┐  │
│  │ Mosquitto│   │ Bun/Node API │   │ Vite + React   │  │
│  │ (Broker) │◄─►│ + SQLite/    │◄─►│ Dashboard      │  │
│  │  MQTT    │   │  InfluxDB    │   │ (Recharts)     │  │
│  └────┬─────┘   └──────────────┘   └────────────────┘  │
│       │                                                 │
└───────┼─────────────────────────────────────────────────┘
        │ MQTT (WiFi STA / 4G)
┌───────▼────────┐
│    GATEWAY     │◄──► WiFi AP (Dashboard embebido HTML/JS)
│    CENTRAL     │
│  (ESP32-S3)    │     Firmware: C (ESP-IDF v5.x)
│  8MB Flash     │     ESP-NOW + WiFi AP/STA + MQTT + HTTP
└───────┬────────┘
        │ ESP-NOW (2.4GHz, peer-to-peer)
        │
   ┌────┼──────────────┬──────────────┐
   │    │              │              │
┌──▼────▼──┐    ┌──────▼──┐    ┌─────▼─────┐
│  NODO A  │    │  NODO B │    │  NODO C   │
│  ESP32-C3│    │ ESP32-C3│    │  ESP32-C3 │
│  Temp/DO │    │ pH/ORP  │    │  Nivel    │
│  4MB     │    │ 4MB     │    │  4MB      │
└──────────┘    └─────────┘    └───────────┘
   Firmware: C (ESP-IDF v5.x)
   Sensores + Deep Sleep + ESP-NOW
```

### 2.2 Protocolos de Comunicación

**Prioridad 1: ESP-NOW (backbone del sistema)**
- **Función**: Comunicación directa nodo → gateway para datos de sensores
- **Ventajas**: Baja latencia (<10ms), bajo consumo, cifrado AES-128 (CCMP)
- **Limitación**: No es mesh nativo. Los nodos envían directamente al gateway. Si un nodo no alcanza el gateway, un nodo intermedio puede actuar como relay (implementación custom, no routing automático)
- **Payload máximo**: 250 bytes por frame
- **Peers máximos**: 6 peers cifrados / 20 no cifrados (limitación hardware). En v1.0 se usan 20 peers sin cifrar para maximizar la cantidad de nodos. Ver `docs/replanificacion/01-decisiones-corregidas.md` (DC-01)

**Prioridad 2: WiFi AP**
- **Función**: Configuración local, dashboard embebido de emergencia
- **Ventajas**: Sin dependencia externa, interfaz web directa desde móvil/portátil
- **Activación**: Siempre activo en el gateway (coexiste con ESP-NOW y WiFi STA)

**Prioridad 3: WiFi STA**
- **Función**: Conexión a Internet/red local para MQTT, OTA, sincronización horaria (SNTP)
- **Ventajas**: Integración con servidor cloud/LAN
- **Gestión**: Reconexión automática con backoff exponencial

**Nota sobre coexistencia**: ESP-NOW y WiFi comparten el radio 2.4GHz. Ambos pueden operar simultáneamente si ESP-NOW usa el mismo canal que WiFi. El gateway debe fijar el canal WiFi y configurar ESP-NOW en ese mismo canal.

## 3. Stack Tecnológico

### 3.1 Firmware (ESP32)
| Componente | Tecnología | Justificación |
|------------|------------|---------------|
| **Lenguaje** | C (C11) | Nativo ESP-IDF, máxima documentación, ejemplos oficiales |
| **Framework** | ESP-IDF v5.x | SDK oficial de Espressif, todas las features first-class |
| **Build system** | CMake + idf.py | Standard ESP-IDF |
| **Comunicación** | `esp_now`, `esp_mqtt`, `esp_http_server` | Componentes nativos ESP-IDF |
| **Almacenamiento** | `nvs_flash` | Key-value en flash con wear leveling |
| **OTA** | `esp_ota_ops` + `esp_https_ota` | A/B partitions con rollback |
| **Testing** | Unity (integrado en ESP-IDF), Wokwi | Tests unitarios + simulación |

### 3.2 Backend Servidor
| Componente | Tecnología | Justificación |
|------------|------------|---------------|
| **Runtime** | Bun (o Node.js) | Rápido, ecosistema JS/TS compartido con frontend |
| **Lenguaje** | TypeScript | Type safety, mismo lenguaje que frontend |
| **MQTT Client** | mqtt.js | Cliente MQTT para suscribirse al broker |
| **Broker MQTT** | Mosquitto | Ligero, probado, estándar de la industria |
| **Base de datos** | SQLite (inicio) → InfluxDB (si se necesitan series temporales) | Mínima complejidad inicial |
| **API** | REST + WebSocket | REST para config, WebSocket para datos en tiempo real |

### 3.3 Frontend Dashboard
| Componente | Tecnología | Justificación |
|------------|------------|---------------|
| **Bundler** | Vite | Rápido, ligero, sin overhead de SSR innecesario |
| **Framework** | React | Experiencia previa del equipo, ecosistema masivo |
| **Gráficos** | Recharts o Tremor | Componentes React nativos para series temporales |
| **MQTT directo** | mqtt.js (vía WebSocket) | Opción de suscripción directa desde el browser |
| **Estilos** | Tailwind CSS | Utility-first, rápido para dashboards |

### 3.4 Dashboard Embebido (ESP32 Gateway)
| Componente | Tecnología | Justificación |
|------------|------------|---------------|
| **Servidor** | `esp_http_server` | Nativo ESP-IDF, soporte WebSocket |
| **Frontend** | HTML + CSS + JS vanilla | Embebido en flash con `EMBED_FILES` de CMake |
| **Gráficos** | uPlot (~35KB) o sin gráficos | Mínimo footprint para no saturar flash |
| **Función** | Config WiFi, estado nodos, lectura actual | Panel de emergencia/configuración, no dashboard completo |

## 4. Fases de Implementación

### FASE 0: PREPARACIÓN Y APRENDIZAJE (3 semanas)
**Objetivo**: Validar hardware y establecer entorno de desarrollo

- **Setup ESP-IDF v5.x** con toolchain para ESP32-S3 (Xtensa) y ESP32-C3 (RISC-V)
- **Blink + UART** en ambas placas para validar toolchain
- **Lectura de sensor** por I2C/ADC (sensor de temperatura como prueba)
- **ESP-NOW básico**: envío/recepción entre 2 placas
- **WiFi AP + STA** simultáneo en el S3 (modo APSTA)
- **Benchmark de consumo** con multímetro: active, modem sleep, light sleep, deep sleep
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

**Entregables**: Gateway funcional con web de config, documentación API

### FASE 2: NODOS SENSORES + ESP-NOW FIABLE (4 semanas)
**Objetivo**: Nodos autónomos que reportan al gateway de forma fiable

- **Firmware de nodo genérico** parametrizable por tipo de sensor
- **Ciclo de vida del nodo**: deep sleep → wake → leer sensor → enviar ESP-NOW → deep sleep
- **Protocolo de mensajería** sobre ESP-NOW:
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

**Entregables**: Firmware nodo genérico, protocolo documentado, ejemplo con 3 nodos

### FASE 3: SENSORES Y ACTUADORES (3 semanas)
**Objetivo**: Drivers específicos y control de actuadores

- **Drivers para sensores acuícolas**:
  - Temperatura (DS18B20 / PT100 vía MAX31865): 1-Wire / SPI
  - pH (sonda + ADC): calibración 2 puntos (pH 4.0 y 7.0)
  - Oxígeno disuelto (DO): ADC con compensación de temperatura
  - ORP: ADC
  - Nivel de agua: ultrasonido (HC-SR04) o presión
- **Sistema de calibración** con coeficientes almacenados en NVS
- **Validación de lecturas**: rango válido por tipo de sensor, filtro de outliers
- **Control de actuadores** en el gateway o nodos dedicados:
  - Relés (GPIO) para bombas, aireadores, alimentadores
  - Activación por umbral (configurable) o por comando remoto
- **Alertas locales** en el gateway: si una lectura supera umbral → activar actuador + flag alerta

**Entregables**: Drivers probados, manual de calibración, validación de precisión

### FASE 4: INTEGRACIÓN MQTT + DASHBOARD WEB (4 semanas)
**Objetivo**: Sistema completo de visualización y control remoto

**Backend (Bun/Node + TypeScript):**
- Servicio MQTT subscriber que recibe datos del gateway
- Almacenamiento en SQLite (o InfluxDB para series temporales)
- API REST para consultas históricas y configuración
- WebSocket server para push de datos en tiempo real al frontend

**Frontend (Vite + React):**
- Dashboard con estado en tiempo real de todos los nodos
- Gráficos históricos por sensor (Recharts)
- Panel de configuración de alertas y umbrales
- Control remoto de actuadores
- Estado de conectividad de cada nodo (último contacto, batería)

**Gateway - Bridge MQTT:**
- Publicación de datos de sensores en topics MQTT jerárquicos:
  - `piscifactoria/{gateway_id}/nodo/{nodo_id}/temperatura`
  - `piscifactoria/{gateway_id}/nodo/{nodo_id}/status`
  - `piscifactoria/{gateway_id}/alertas`
- Suscripción a topics de control:
  - `piscifactoria/{gateway_id}/control/actuador/{id}`
  - `piscifactoria/{gateway_id}/config/nodo/{nodo_id}`
- QoS 1 para datos de sensores, QoS 2 para comandos de actuadores

**Dashboard embebido (ESP32):**
- Mejora del panel web con lectura actual de todos los nodos
- Estado de conectividad WiFi y MQTT
- Config de broker MQTT (host, puerto, usuario, contraseña)

**Entregables**: Sistema completo end-to-end funcionando, documentación de topics MQTT

### FASE 5: OTA + OPTIMIZACIÓN + PRUEBAS (4 semanas)
**Objetivo**: Estabilidad, seguridad y mantenimiento remoto

- **OTA para gateway** vía WiFi: descarga HTTPS de firmware → `esp_ota_ops` → reboot con rollback
- **OTA para nodos**: el nodo se conecta temporalmente a WiFi AP del gateway para recibir OTA (no vía ESP-NOW, demasiado complejo y frágil para v1.0)
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

- **OTA vía ESP-NOW** para nodos (fragmentación de firmware en paquetes de 250B con ACK)
- **Mesh relay automático** con descubrimiento de rutas
- **Machine Learning edge** para detección de anomalías (TFLite Micro en ESP32-S3)
- **Soporte 4G/LTE** mediante módulo SIM7600 conectado al gateway
- **Exportación de datos** CSV/JSON desde el dashboard
- **Sistema de backup** en tarjeta SD en el gateway
- **Sistema multi-usuario** con roles y permisos en el dashboard
- **API pública** para integración con sistemas de terceros
- **Certificaciones** CE/FCC si se plantea comercialización

## 5. Especificaciones Técnicas

### 5.1 Requisitos Hardware
| Componente | Especificación | Cantidad | Coste Aprox. |
|------------|----------------|----------|--------------|
| Gateway | ESP32-S3-WROOM-1 (8MB Flash, 2MB PSRAM) | 1 | 8€ |
| Nodos | ESP32-C3-MINI-1 (4MB Flash) | 3-10 | 4€/unidad |
| Sensores | Kit acuícola (5 tipos) | 1 set | 120€ |
| Baterías | Li-Ion 18650 3400mAh (ej. NCR18650B) | 3-10 | 5€/unidad |
| Cargador | TP4056 con protección | 3-10 | 1€/unidad |
| Carcasas | IP68 para exterior | según necesidad | 5-15€ |
| Servidor | Raspberry Pi 4 / mini PC (para Mosquitto + backend) | 1 | 50-80€ |
| **Total inicial (5 nodos)** | | | **~280€** |

### 5.2 Tabla de Particiones ESP32

**Gateway (ESP32-S3, 8MB Flash):**
```
# Name,   Type, SubType,  Offset,   Size
nvs,      data, nvs,      0x9000,   0x6000    (24KB)
phy_init, data, phy,      0xf000,   0x1000    (4KB)
ota_0,    app,  ota_0,    0x10000,  0x300000  (3MB)
ota_1,    app,  ota_1,    0x310000, 0x300000  (3MB)
storage,  data, spiffs,   0x610000, 0x1E0000  (1.9MB - web assets)
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
| Tiempo respuesta web embebida | < 500ms | < 2s |
| Uso memoria RAM gateway | < 70% | < 85% |
| Tamaño binario gateway | < 1.5MB | < 2MB |
| Tamaño binario nodo | < 800KB | < 1.2MB |

### 5.4 Estructura del Proyecto
```
piscifactoria/
├── firmware/
│   ├── gateway/              # Proyecto ESP-IDF para ESP32-S3
│   │   ├── main/
│   │   │   ├── main.c
│   │   │   ├── wifi_manager.c/.h
│   │   │   ├── espnow_manager.c/.h
│   │   │   ├── mqtt_bridge.c/.h
│   │   │   ├── http_server.c/.h
│   │   │   ├── ota_manager.c/.h
│   │   │   ├── nvs_config.c/.h
│   │   │   └── actuator_ctrl.c/.h
│   │   ├── components/
│   │   │   └── protocol/     # Protocolo de mensajería compartido
│   │   ├── web/              # HTML/CSS/JS embebido
│   │   ├── partitions.csv
│   │   └── CMakeLists.txt
│   │
│   ├── node/                 # Proyecto ESP-IDF para ESP32-C3
│   │   ├── main/
│   │   │   ├── main.c
│   │   │   ├── espnow_node.c/.h
│   │   │   ├── sensor_manager.c/.h
│   │   │   ├── power_manager.c/.h
│   │   │   └── nvs_config.c/.h
│   │   ├── components/
│   │   │   ├── protocol/     # Mismo componente compartido
│   │   │   └── drivers/      # Drivers de sensores
│   │   │       ├── ds18b20/
│   │   │       ├── ph_adc/
│   │   │       ├── do_sensor/
│   │   │       └── ultrasonic/
│   │   ├── partitions.csv
│   │   └── CMakeLists.txt
│   │
│   └── common/               # Código compartido (protocolo, tipos)
│       └── protocol/
│           ├── protocol.h
│           └── protocol.c
│
├── server/
│   ├── src/
│   │   ├── index.ts          # Entry point Bun/Node
│   │   ├── mqtt.ts           # MQTT subscriber
│   │   ├── api.ts            # REST API
│   │   ├── websocket.ts      # WebSocket server
│   │   └── db.ts             # SQLite/InfluxDB
│   ├── package.json
│   └── tsconfig.json
│
├── dashboard/
│   ├── src/
│   │   ├── App.tsx
│   │   ├── components/
│   │   │   ├── SensorChart.tsx
│   │   │   ├── NodeStatus.tsx
│   │   │   ├── AlertPanel.tsx
│   │   │   └── ActuatorControl.tsx
│   │   ├── hooks/
│   │   │   ├── useMqtt.ts
│   │   │   └── useWebSocket.ts
│   │   └── pages/
│   │       ├── Dashboard.tsx
│   │       ├── Config.tsx
│   │       └── History.tsx
│   ├── package.json
│   ├── vite.config.ts
│   └── tailwind.config.js
│
├── docs/
│   ├── api.md
│   ├── mqtt-topics.md
│   ├── deployment.md
│   └── calibration.md
│
└── README.md
```

## 6. Gestión de Riesgos

### 6.1 Riesgos Técnicos
| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|--------------|---------|------------|
| Interferencia WiFi/ESP-NOW en mismo canal | Media | Alta | ESP-NOW y WiFi DEBEN usar mismo canal. Elegir canal menos congestionado al arrancar |
| Corrupción NVS por cortes de alimentación | Baja | Media | NVS tiene journaling interno. Backup de config crítica en otra partición |
| Consumo mayor al estimado en nodos | Media | Media | Medir consumo real en Fase 0. Ajustar duty cycle por configuración |
| Compatibilidad/precisión sensores acuícolas | Alta | Media | Calibración software, drivers genéricos ADC, validación con instrumentos de referencia |
| Límite de 6 peers cifrados ESP-NOW | Media | Alta | En v1.0 usar 20 peers sin cifrar. Seguridad via validacion de protocolo + whitelist de MACs. Cifrado por software (AES-256 payload) planificado para v2.0 |
| Coexistencia ESP-NOW + WiFi AP + WiFi STA | Media | Alta | Probar en Fase 0. Todas deben operar en el mismo canal WiFi |

### 6.2 Riesgos de Proyecto
| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|--------------|---------|------------|
| Complejidad de 3 protocolos simultáneos | Alta | Alta | Implementación incremental: primero ESP-NOW solo, luego WiFi, luego MQTT |
| Scope creep (añadir features antes de tener lo básico) | Alta | Alta | Fases estrictas. No pasar a Fase N+1 sin Fase N funcionando |
| Debugging remoto en piscifactoría | Media | Media | Logging por MQTT, core dumps en flash, modo diagnóstico activable |
| Mantenimiento a largo plazo | Media | Media | Código documentado, estructura modular, OTA para correcciones |

## 7. Recursos y Presupuesto

### 7.1 Cronograma
| Fase | Duración | Acumulado |
|------|----------|-----------|
| Fase 0: Preparación | 3 semanas | 3 semanas |
| Fase 1: Gateway núcleo | 5 semanas | 8 semanas |
| Fase 2: Nodos + ESP-NOW | 5 semanas | 13 semanas |
| Fase 3: Sensores y actuadores | 3 semanas | 16 semanas |
| Fase 4: MQTT + Dashboard web | 4 semanas | 20 semanas |
| Fase 5: OTA + Seguridad + Pruebas | 5 semanas | 25 semanas |
| **Total hasta v1.0** | **25 semanas** | |

### 7.2 Presupuesto Estimado
| Concepto | Coste |
|----------|-------|
| Hardware prototipos (gateway + 5 nodos + sensores + servidor) | 500€ |
| Herramientas (multímetro, analizador lógico, fuente) | 200€ |
| Testing en entorno real (desplazamientos, materiales) | 500€ |
| **Total hardware** | **~1.200€** |

*Nota: coste de desarrollo no incluido al ser proyecto interno/aprendizaje*

## 8. Métricas de Éxito

### 8.1 KPIs Técnicos
- **Disponibilidad sistema**: > 99% (excluyendo mantenimiento programado)
- **Precisión mediciones**: dentro de ±5% de las especificaciones del sensor
- **Tiempo despliegue nuevo nodo**: < 30 minutos (flash + calibrar + registrar)
- **Consumo energía gateway**: < 1W promedio (alimentado por red eléctrica)
- **Consumo nodos**: < 50uA promedio (deep sleep con wake periódico)
- **Pérdida de paquetes ESP-NOW**: < 1% en condiciones normales

### 8.2 KPIs de Producto
- **Reducción costes vs solución comercial**: > 60%
- **Escalabilidad**: +10 nodos sin degradación de rendimiento
- **Latencia dato sensor → dashboard**: < 2 segundos end-to-end
- **Uptime sin intervención manual**: > 30 días

## 9. Conclusiones

### 9.1 Viabilidad
El proyecto es **técnicamente viable**. La combinación de C/ESP-IDF + ESP32 + triple conectividad + stack web moderno (React + Bun) ofrece:

1. **Máxima madurez** del firmware gracias a ESP-IDF nativo en C
2. **Documentación abundante** con miles de ejemplos oficiales y comunitarios
3. **Toolchain simple**: `idf.py build && idf.py flash && idf.py monitor`
4. **Separación clara**: firmware en C (donde importa el rendimiento), backend/frontend en TypeScript (donde importa la velocidad de desarrollo)
5. **Costes reducidos** mediante componentes estándar y software open source

### 9.2 Decisiones Técnicas Clave
| Decisión | Justificación |
|----------|---------------|
| **C en vez de Rust** para firmware | ESP-IDF es C nativo. Rust añade wrappers sobre el mismo código. Con experiencia en C, se elimina una capa de indirección y se accede directamente a toda la documentación oficial |
| **ESP-NOW punto a punto, no mesh** en v1.0 | ESP-NOW no tiene mesh nativo. Implementar mesh routing custom es complejo. Para v1.0, los nodos envían directo al gateway. Relay estático planificado como mejora futura (Fase 6) |
| **ESP-NOW sin cifrar** en v1.0 | El hardware limita a 6 peers cifrados, insuficiente para 10-20 nodos. Se usan 20 peers sin cifrar con validación de protocolo y whitelist de MACs. Cifrado AES-256 por software planificado para v2.0 |
| **OTA por WiFi, no ESP-NOW** en v1.0 | OTA por ESP-NOW requiere fragmentar firmware en paquetes de 250B con retransmisión. Demasiado frágil. Los nodos se conectan temporalmente al AP del gateway para OTA |
| **Baterías 3400mAh** en vez de 1200mAh | Para alcanzar 12+ meses de autonomía con wake cada 5 minutos |
| **Vite + React SPA** en vez de Next.js/Astro | Dashboard interno sin necesidad de SEO/SSR. SPA pura es más simple y suficiente |
| **SQLite inicial** en vez de InfluxDB | Menor complejidad para empezar. Migrar a InfluxDB cuando el volumen de datos lo justifique |

### 9.3 Próximos Pasos
1. **Adquirir hardware**: 1x ESP32-S3-DevKitC, 3x ESP32-C3-DevKitM, sensores básicos (1 semana)
2. **Setup ESP-IDF v5.x** y validar compilación para ambos targets (1 semana)
3. **Iniciar Fase 0** según cronograma
4. **Crear repositorio Git** con la estructura de proyecto definida

---

**Documento v2.0** - Stack: C (ESP-IDF) + Bun/TypeScript + React
*Fecha: 2026-02-09*
