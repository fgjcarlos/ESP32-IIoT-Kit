# Ejemplo: Sistema de Monitorización de Piscifactoría

Este directorio contiene la implementación de referencia del tutorial **ESP32-IIoT-Kit** aplicada a un sistema de monitorización de piscifactoría (acuicultura intensiva). Es un **ejemplo de dominio específico** que se construye encima de la plataforma genérica enseñada en las Fases 0–6.

> **Nota importante**: Todo el firmware del gateway, los nodos y el protocolo ESP-NOW se aprenden en el tutorial principal. Este directorio solo documenta cómo extender esa base con los sensores y la jerarquía MQTT propios de la acuicultura.

---

## Caso de uso

Una piscifactoría intensiva requiere monitorizar continuamente la calidad del agua para garantizar la supervivencia y el crecimiento óptimo de los peces. Los parámetros críticos son:

- **pH**: la acidez/alcalinidad del agua afecta directamente la salud de los peces
- **Oxígeno Disuelto (OD)**: imprescindible para la supervivencia; niveles bajos provocan mortalidad masiva en horas
- **ORP** (potencial de oxidación-reducción): indicador global de la calidad y la actividad bacteriana del agua
- **Turbidez**: mide la cantidad de partículas en suspensión; alta turbidez indica sobrecarga orgánica
- **Temperatura**: parámetro universal; condiciona la solubilidad del oxígeno y el metabolismo de los peces
- **Nivel de agua**: controla el volumen de los estanques y detecta fugas o pérdidas

Un nodo sensor ESP32-C3 se instala en cada estanque, lee los sensores periódicamente y envía los datos al gateway ESP32-S3 mediante ESP-NOW. El gateway los publica por MQTT con el espacio de nombres `fish-farm`.

---

## Sensores cubiertos

| Sensor | Tipo | Interfaz | `sensor_type_t` |
|--------|------|----------|-----------------|
| Sonda de pH analógica | pH | ADC | `SENSOR_TYPE_PH` (0x02) |
| Sonda de OD galvánica u óptica | Oxígeno Disuelto | ADC | `SENSOR_TYPE_DO` (0x03) |
| Sonda ORP | Oxidación-Reducción | ADC | `SENSOR_TYPE_ORP` (0x04) |
| Sensor de nivel ultrasónico (HC-SR04) | Nivel de agua | GPIO (trigger/echo) | `SENSOR_TYPE_LEVEL` (0x05) |
| Sensor de turbidez (SEN0189) | Turbidez | ADC | `SENSOR_TYPE_TURBIDITY` (0x06) |
| DS18B20 (ya cubierto en Fase 3) | Temperatura | 1-Wire | `SENSOR_TYPE_TEMPERATURE` (0x01) |

---

## Prerrequisitos

Antes de usar este ejemplo debes haber completado las Fases 1–3 del tutorial principal:

- **Fase 1**: Gateway ESP32-S3 con WiFi AP/STA y servidor HTTP operativo
- **Fase 2**: Nodos ESP32-C3 con ESP-NOW y deep sleep funcionando
- **Fase 3**: Driver de sensor genérico con DS18B20 como ejemplo; comprendes `sensor_manager` y `protocol.h`

---

## Cómo aplicar este ejemplo

1. **Lee `sensors.md`** en este directorio: documenta el driver de cada sensor acuícola (conexión hardware, calibración, integración con `sensor_manager`).
2. **Lee `mqtt-topics.md`** en este directorio: define la jerarquía de tópicos MQTT con el espacio de nombres `fish-farm`.
3. En el firmware del nodo, añade los drivers de los sensores que necesites al módulo `sensor_manager` siguiendo el patrón del DS18B20.
4. Configura el NVS del gateway para usar `mqtt_namespace = "fish-farm"` (ver Fase 4 y `docs/replanificacion/02-protocolo-unificado.md`).

---

## Nota sobre calibración

Los sensores electroquímicos (pH, OD, ORP) **requieren calibración periódica** con soluciones tampón de referencia. Sin calibración, las lecturas son orientativas, no fiables. Cada sección en `sensors.md` incluye el procedimiento de calibración paso a paso.

La calibración se almacena en NVS bajo el espacio de nombres `"sensor"` y persiste entre reinicios.

---

## Archivos en este directorio

| Archivo | Descripción |
|---------|-------------|
| `README.md` | Este documento — descripción general del caso de uso |
| `sensors.md` | Drivers, calibración e integración de los sensores acuícolas |
| `mqtt-topics.md` | Jerarquía de tópicos MQTT para el espacio de nombres `fish-farm` |

---

## Referencias

- **Fase 3**: `Fases/fase-03-sensores-actuadores.md` — sensor genérico, DS18B20, actuadores
- **Fase 4**: `Fases/fase-04-mqtt-dashboard.md` — dashboard embebido y MQTT opcional
- **Protocolo unificado**: `docs/replanificacion/02-protocolo-unificado.md` — definición completa del protocolo y espacios de nombres MQTT
