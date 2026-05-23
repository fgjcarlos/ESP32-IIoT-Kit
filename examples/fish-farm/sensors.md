# Sensores Acuícolas: Drivers e Integración

Este documento contiene el detalle técnico de los sensores específicos del dominio acuícola. Se corresponde con las Subtareas 3.2, 3.3 y 3.4 del tutorial original, que han sido trasladadas aquí para mantener la Fase 3 genérica.

> **Prerequisito**: Antes de implementar cualquier sensor de este documento, asegúrate de que el driver del DS18B20 (Subtarea 3.1 de la Fase 3) funciona correctamente. El patrón de driver que usarás aquí es idéntico: `sensor_read()` devuelve `esp_err_t` y rellena una estructura de datos que `sensor_manager` empaqueta en el protocolo.

---

## Subtarea 3.2: Sensor de pH

### ¿Por qué monitorizar el pH?

El pH del agua determina su acidez o alcalinidad en una escala de 0 a 14. La mayoría de las especies de piscifactoría toleran un rango de **pH 6,5 – 8,5**. Fuera de ese rango se produce estrés, enfermedad y, en casos extremos, mortalidad. A diferencia de la temperatura, el pH no es directamente observable, por lo que un sensor continuo es imprescindible.

### Hardware

- **Sonda de pH analógica** (cualquier sonda industrial estándar, p. ej. E-201C)
- **Módulo amplificador de pH** (BNC breakout con salida de 0–3,3 V): convierte el potencial electroquímico de la sonda en una tensión legible por el ADC del ESP32
- Conexión al ESP32-C3: pin de salida analógica → `ADC1_CHANNEL_x` (evitar GPIO0 y GPIO1 en C3)

### Driver: lectura y conversión

```c
// sensor_ph.h
#define PH_ADC_CHANNEL   ADC1_CHANNEL_0
#define PH_CALIBRATION_K "sensor"   // namespace NVS

esp_err_t ph_sensor_init(void);
esp_err_t ph_sensor_read(float *ph_value);
```

```c
// sensor_ph.c — lectura básica
esp_err_t ph_sensor_read(float *ph_value) {
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                             ADC_WIDTH_BIT_12, 1100, &adc_chars);

    int raw = adc1_get_raw(PH_ADC_CHANNEL);
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    // Aplicar calibración lineal: pH = m * V + b
    float m, b;
    nvs_load_calibration(&m, &b);           // carga m y b desde NVS
    *ph_value = m * (voltage_mv / 1000.0f) + b;
    return ESP_OK;
}
```

**Por qué `esp_adc_cal`**: El ADC del ESP32 tiene una no-linealidad de hasta ±6 %. Usar `esp_adc_cal_raw_to_voltage` con la curva de calibración del fabricante (almacenada en eFuse) reduce ese error a <3 %. Sin calibración ADC, las lecturas de pH serían poco fiables.

### Procedimiento de calibración (2 puntos)

La calibración de 2 puntos establece la relación lineal entre el voltaje de la sonda y el pH real.

**Materiales**: soluciones tampón pH 4,0 y pH 7,0 (o pH 7,0 y pH 10,0 según el rango de trabajo).

1. Sumergir la sonda en la solución pH 4,0 durante 2 minutos (equilibrio térmico).
2. Ejecutar el modo de calibración del firmware: leer 10 muestras ADC, promediar, guardar el voltaje como `V_pH4`.
3. Enjuagar con agua destilada, sumergir en pH 7,0 durante 2 minutos.
4. Leer y guardar `V_pH7`.
5. Calcular la pendiente: `m = (7.0 - 4.0) / (V_pH7 - V_pH4)`.
6. Calcular el offset: `b = 7.0 - m * V_pH7`.
7. Guardar `m` y `b` en NVS (`nvs_set_blob` bajo espacio de nombres `"sensor"`, claves `"ph_m"` y `"ph_b"`). No olvidar `nvs_commit`.

**Frecuencia de calibración recomendada**: mensual, o antes de cada campaña productiva.

### Integración con el protocolo

```c
// En sensor_manager.c, al componer el frame de datos:
sensor_data.type   = SENSOR_TYPE_PH;    // 0x02
sensor_data.values[0] = ph_value * 100; // e.g. pH 7.23 → 723 (uint16_t, sin coma)
sensor_data.unit   = UNIT_PH;
```

---

## Subtarea 3.3: Sensor de Oxígeno Disuelto (OD)

### ¿Por qué es crítico el OD?

El oxígeno disuelto (OD) es el parámetro más crítico en piscicultura intensiva. Concentraciones por debajo de **5 mg/L** provocan estrés en la mayoría de las especies; por debajo de **3 mg/L** se producen mortalidades masivas en pocas horas. A diferencia del pH, el OD puede cambiar dramáticamente en minutos (algas fotosintéticas, consumo nocturno, sobrecarga de biomasa).

### Hardware

Existen dos tecnologías principales:

| Tipo | Principio | Ventaja | Inconveniente |
|------|-----------|---------|---------------|
| Galvánica (Clark) | Electroquímica — O₂ difunde a través de membrana y se reduce | Bajo coste | Requiere cambio de membrana cada 3–6 meses |
| Óptica (luminiscente) | El O₂ atenúa la luminiscencia de un fluoróforo | Sin membrana, más estable | Mayor coste inicial |

Ambas producen una salida de voltaje analógica conectada al ADC del ESP32-C3.

### Driver: lectura y compensación de temperatura

El OD se expresa en mg/L a una temperatura de referencia (normalmente 25 °C). La solubilidad del oxígeno **disminuye con la temperatura**, por lo que es imprescindible compensar.

```c
// sensor_do.c — lectura con compensación de temperatura
esp_err_t do_sensor_read(float temp_celsius, float *do_mg_l) {
    // 1. Leer voltaje del sensor OD
    uint32_t voltage_mv = adc_read_calibrated(DO_ADC_CHANNEL);

    // 2. Convertir voltaje a saturación en % (calibración 2 puntos: 0% y 100%)
    float saturation_pct = do_voltage_to_saturation(voltage_mv);

    // 3. Concentración de saturación a 100% en función de la temperatura
    // Fórmula de García & Gordon (1992), simplificada:
    // DO_sat(T) = 468 / (31.6 + T)  [mg/L, aproximación válida 0-40°C]
    float do_sat = 468.0f / (31.6f + temp_celsius);

    // 4. OD real = saturación * OD_sat
    *do_mg_l = (saturation_pct / 100.0f) * do_sat;
    return ESP_OK;
}
```

**Por qué la compensación de temperatura**: sin compensarla, una lectura del 80 % de saturación a 15 °C (10,6 mg/L) y a 30 °C (7,6 mg/L) son valores muy distintos biológicamente. El sensor sin compensación engañaría al operador.

### Calibración del sensor OD

1. **Calibración a cero (0 %)**: sumergir la sonda en solución saturada de Na₂SO₃ (elimina todo el O₂). Leer voltaje → `V_0`.
2. **Calibración a saturación (100 %)**: exponer la sonda al aire húmedo o agua bien oxigenada y agitada. Leer voltaje → `V_100`.
3. Guardar `V_0` y `V_100` en NVS (claves `"do_v0"` y `"do_v100"`).
4. En `do_voltage_to_saturation`: `pct = (V - V_0) / (V_100 - V_0) * 100`.

### Integración con el protocolo

```c
sensor_data.type      = SENSOR_TYPE_DO;    // 0x03
sensor_data.values[0] = do_mg_l * 100;    // e.g. 8.45 mg/L → 845
sensor_data.values[1] = temp_celsius * 10; // temperatura usada para la compensación
```

---

## Subtarea 3.4a: Sensor ORP

### ¿Qué mide el ORP?

El potencial de oxidación-reducción (ORP, *Oxidation-Reduction Potential*) mide la tendencia del agua a oxidar o reducir sustancias. Un ORP alto (>300 mV) indica agua rica en oxidantes (bien desinfectada). Un ORP bajo o negativo indica predominio de reductores (materia orgánica, falta de desinfección). Es un indicador global de la calidad microbiológica del agua.

**Rango típico en piscifactoría**: 200–400 mV. Por debajo de 100 mV se dispara una alerta.

### Hardware

- **Sonda ORP** (electrodo de platino con referencia Ag/AgCl)
- **Módulo amplificador ORP** (circuito similar al de pH, misma conexión BNC)
- Conexión: salida analógica → ADC del ESP32-C3

La sonda ORP y la sonda de pH son electroquímicamente similares. La diferencia está en el electrodo de trabajo (platino en lugar de vidrio sensible a H⁺) y en el rango de voltaje de salida (±1 V en lugar de 0–14 pH).

### Driver: lectura

```c
esp_err_t orp_sensor_read(float *orp_mv) {
    uint32_t voltage_mv = adc_read_calibrated(ORP_ADC_CHANNEL);

    // La sonda ORP genera una tensión centrada en ~2048 mV (referencia del módulo)
    // ORP (mV) = voltaje_sonda - voltaje_referencia
    float v_ref;
    nvs_load_orp_reference(&v_ref);      // calibrado una sola vez en producción
    *orp_mv = (float)voltage_mv - v_ref;
    return ESP_OK;
}
```

### Calibración del ORP

A diferencia del pH, la calibración del ORP es de **un único punto** con una solución tampón ORP de referencia certificada (habitualmente 225 mV o 475 mV a 25 °C).

1. Sumergir la sonda en la solución de referencia. Esperar 2 minutos.
2. Leer el voltaje bruto del ADC → `V_medido`.
3. Offset = `V_referencia_certificada - V_medido` (en mV).
4. Guardar el offset en NVS (clave `"orp_offset"`).

### Integración con el protocolo

```c
sensor_data.type      = SENSOR_TYPE_ORP;  // 0x04
sensor_data.values[0] = (int16_t)orp_mv; // puede ser negativo → usar int16_t
```

---

## Subtarea 3.4b: Sensor de Nivel (Ultrasónico HC-SR04)

### ¿Por qué monitorizar el nivel del agua?

En un estanque de recirculación, una pérdida de nivel indica una fuga o un fallo en la válvula de entrada. Detectar la pérdida en minutos evita que el nivel baje hasta descubrir los peces. El nivel también permite calcular el volumen de agua y, combinado con el OD, estimar la demanda de oxígeno total.

### Hardware: HC-SR04

El HC-SR04 es un sensor ultrasónico de distancia de bajo coste y muy bajo consumo, ideal para aplicaciones de bajo presupuesto:

| Propiedad | Valor |
|-----------|-------|
| Rango de medida | 2 cm – 400 cm |
| Precisión | ±3 mm |
| Tensión de alimentación | 5 V (requiere divisor resistivo si el GPIO del C3 es 3,3 V) |
| Interfaz | Trigger (GPIO salida) + Echo (GPIO entrada con captura de tiempo) |
| Corriente en medida | ~15 mA durante el pulso |

Conexión al ESP32-C3:
- `TRIG` → GPIO de salida (p. ej. GPIO4)
- `ECHO` → GPIO de entrada con tolerancia a 3,3 V, o usar divisor resistivo (10 kΩ / 20 kΩ)

### Driver: medida por tiempo de eco

El HC-SR04 funciona enviando un pulso ultrasónico de 40 kHz y midiendo el tiempo que tarda en reflejarse. La distancia es `d = t_echo * 343 / 2` (velocidad del sonido en aire a 20 °C).

```c
// sensor_level.c
#define TRIG_GPIO     GPIO_NUM_4
#define ECHO_GPIO     GPIO_NUM_5
#define SOUND_SPEED   343.0f  // m/s a 20°C

esp_err_t level_sensor_init(void) {
    gpio_set_direction(TRIG_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO_GPIO, GPIO_MODE_INPUT);
    return ESP_OK;
}

esp_err_t level_sensor_read_cm(float *distance_cm) {
    // 1. Pulso de trigger: 10 µs a nivel alto
    gpio_set_level(TRIG_GPIO, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIG_GPIO, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_GPIO, 0);

    // 2. Medir duración del echo (tiempo en µs)
    int64_t t_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 0) {
        if (esp_timer_get_time() - t_start > 30000) return ESP_ERR_TIMEOUT;
    }
    t_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 1) {
        if (esp_timer_get_time() - t_start > 30000) return ESP_ERR_TIMEOUT;
    }
    int64_t t_echo_us = esp_timer_get_time() - t_start;

    // 3. Distancia en cm
    *distance_cm = (t_echo_us * SOUND_SPEED * 100.0f) / (2.0f * 1e6f);
    return ESP_OK;
}
```

**Conversión a nivel de agua**: el sensor se monta en la parte superior del estanque mirando hacia abajo. `nivel = altura_estanque - distancia_medida`.

### Compensación de temperatura (avanzado)

La velocidad del sonido varía con la temperatura: `v = 331.3 + 0.606 * T_celsius`. Si el sistema dispone de un DS18B20 en el mismo nodo, se puede compensar la medida de nivel en tiempo real.

### Integración con el protocolo

```c
sensor_data.type      = SENSOR_TYPE_LEVEL;  // 0x05
sensor_data.values[0] = nivel_cm * 10;     // e.g. 45.3 cm → 453 (resolución 0,1 cm)
```

---

## Subtarea 3.4c: Sensor de Turbidez (SEN0189)

### ¿Qué es la turbidez?

La turbidez mide la cantidad de partículas en suspensión en el agua. Se expresa en NTU (*Nephelometric Turbidity Units*). Agua clarificada tiene <10 NTU; agua con sobrecarga orgánica puede superar 100 NTU. Un incremento rápido de turbidez indica resuspensión de sedimentos, exceso de alimentación o muerte de peces.

### Hardware: SEN0189

El SEN0189 (DFRobot) es un sensor óptico que emite luz infrarroja y mide la luz dispersada lateralmente (nefelometría). Su salida es una tensión analógica de 0–4,5 V proporcional a la turbidez.

| Rango | Voltaje de salida |
|-------|-------------------|
| 0 NTU (agua clara) | ~4,2 V |
| 3000 NTU (muy turbio) | ~2,5 V |

**Importante**: la relación voltaje-NTU NO es lineal. El fabricante proporciona una curva polinómica de tercer grado.

### Driver: lectura y conversión

```c
esp_err_t turbidity_sensor_read(float *ntu) {
    uint32_t v_mv = adc_read_calibrated(TURBIDITY_ADC_CHANNEL);
    float v = v_mv / 1000.0f; // convertir a voltios

    // Curva polinómica del fabricante (SEN0189, 5V alimentación, ajustada a 3.3V)
    // NTU = -1120.4 * v^2 + 5742.3 * v - 4352.9  [válido para 2.5V < v < 4.2V]
    if (v < 2.5f) {
        *ntu = 3000.0f;  // saturación máxima
    } else {
        *ntu = -1120.4f * v * v + 5742.3f * v - 4352.9f;
        if (*ntu < 0) *ntu = 0;
    }
    return ESP_OK;
}
```

### Calibración del sensor de turbidez

El SEN0189 no requiere calibración de usuario habitual, pero es recomendable verificar el punto de cero con agua destilada (<1 NTU) y ajustar el offset si hay desviación significativa.

### Integración con el protocolo

```c
sensor_data.type      = SENSOR_TYPE_TURBIDITY; // 0x06
sensor_data.values[0] = (uint16_t)ntu;         // NTU entero, máximo 3000
```

---

## Resumen: Mapeo de `sensor_type_t` para este ejemplo

| Sensor | `sensor_type_t` | Valor hex | `values[0]` | Unidad |
|--------|-----------------|-----------|-------------|--------|
| DS18B20 (Fase 3) | `SENSOR_TYPE_TEMPERATURE` | 0x01 | °C × 10 | 0,1 °C |
| pH | `SENSOR_TYPE_PH` | 0x02 | pH × 100 | 0,01 pH |
| Oxígeno Disuelto | `SENSOR_TYPE_DO` | 0x03 | mg/L × 100 | 0,01 mg/L |
| ORP | `SENSOR_TYPE_ORP` | 0x04 | mV (int16_t) | 1 mV |
| Nivel (ultrasónico) | `SENSOR_TYPE_LEVEL` | 0x05 | cm × 10 | 0,1 cm |
| Turbidez | `SENSOR_TYPE_TURBIDITY` | 0x06 | NTU | 1 NTU |

---

## Referencias

- Protocolo completo: `docs/replanificacion/02-protocolo-unificado.md`
- Fase 3 (sensor genérico y DS18B20): `Fases/fase-03-sensores-actuadores.md`
- README del ejemplo: `examples/fish-farm/README.md`
- Tópicos MQTT: `examples/fish-farm/mqtt-topics.md`
