# Fase 3: Sensores y Actuadores

> **CORRECCIONES APLICADAS** (2026-05-23): sensor_manager.c ya existe desde T2.1.4 (Fase 2). Filtrado de lecturas corregido: multiples lecturas en UN wake cycle (no entre wakes). Nombres de enum consistentes con protocolo unificado. Ver detalles en `docs/replanificacion/03-fases-corregidas.md`.

## Resumen general

- **Objetivo**: Implementar drivers para sensores acuicolas (temperatura, pH, oxigeno disuelto, ORP, nivel de agua), sistema de calibracion, validacion de lecturas, y control de actuadores con alertas.
- **Duracion estimada**: 3 semanas
- **Prerequisitos**: Fase 0 (Entorno), Fase 1 (Firmware base) y Fase 2 (Comunicaciones) completadas
- **Hardware necesario**:
  - Sensor de temperatura DS18B20 (con resistencia pull-up de 4.7K ohm)
  - Sonda de pH con modulo amplificador/ADC
  - Sonda de oxigeno disuelto (DO)
  - Sonda ORP (potencial de oxidacion-reduccion)
  - Sensor ultrasonico HC-SR04
  - Modulos de rele (1 o 2 canales)
  - ESP32-C3 (nodos sensores)
  - ESP32-S3 (gateway)
  - Soluciones buffer pH 4.0 y pH 7.0 para calibracion
  - Protoboard, cables dupont, fuente de alimentacion

## Dependencias externas

- ESP-IDF v5.x
- Componente `onewire_bus` del ESP-IDF Component Registry
- Datasheets de cada sensor (DS18B20, HC-SR04, modulo pH, modulo DO, modulo ORP)
- Soluciones buffer pH 4.0 y 7.0

## Estructura de archivos de esta fase

```
firmware/
  node/
    components/
      drivers/
        ds18b20/
          ds18b20.c
          ds18b20.h
          CMakeLists.txt
        ph_adc/
          ph_adc.c
          ph_adc.h
          CMakeLists.txt
        do_sensor/
          do_sensor.c
          do_sensor.h
          CMakeLists.txt
        orp/
          orp.c
          orp.h
          CMakeLists.txt
        ultrasonic/
          ultrasonic.c
          ultrasonic.h
          CMakeLists.txt
    main/
      sensor_manager.c   (modificar)
      sensor_manager.h   (modificar)
  gateway/
    main/
      actuator_ctrl.c
      actuator_ctrl.h
      alert_system.c
      alert_system.h
```

---

## Sub-tarea 3.1: Driver de temperatura DS18B20

El DS18B20 es un sensor de temperatura digital que se comunica por el protocolo 1-Wire. Un solo pin de datos puede conectar multiples sensores. Es el sensor mas comun en acuicultura por su precision (+/- 0.5 grados C) y su resistencia al agua cuando viene encapsulado en tubo de acero inoxidable.

---

### Tarea T3.1.1: Implementar driver DS18B20 via 1-Wire

- **Dificultad**: Intermedio
- **Descripcion**:
  1. Instalar el componente `onewire_bus` desde el ESP-IDF Component Registry. Ejecutar en la carpeta del proyecto del nodo:
     ```bash
     idf.py add-dependency "espressif/onewire_bus"
     ```
  2. Crear la carpeta `firmware/node/components/drivers/ds18b20/`.
  3. Crear `ds18b20.h` con las siguientes declaraciones:
     ```c
     typedef struct {
         gpio_num_t data_pin;
         onewire_bus_handle_t bus_handle;
     } ds18b20_config_t;

     esp_err_t ds18b20_init(gpio_num_t data_pin);
     esp_err_t ds18b20_read_temp(float *temp_celsius);
     void ds18b20_deinit(void);
     ```
  4. Crear `ds18b20.c` implementando:
     - `ds18b20_init()`: Configura el bus 1-Wire en el GPIO indicado. Busca dispositivos en el bus y verifica que al menos uno sea un DS18B20 (family code 0x28).
     - `ds18b20_read_temp()`: Envia comando de conversion (0x44), espera 750 ms (resolucion 12-bit), lee el scratchpad (0xBE), convierte los bytes crudos a grados Celsius.
     - `ds18b20_deinit()`: Libera recursos del bus.
  5. Crear `CMakeLists.txt` del componente con la dependencia de `onewire_bus`.
  6. Conectar fisicamente el DS18B20: pin de datos al GPIO elegido, resistencia pull-up de 4.7K ohm entre datos y VCC (3.3V), GND a GND.
  7. Escribir un programa de prueba en `main.c` que lea la temperatura cada 2 segundos y la imprima por `ESP_LOGI`.

- **Archivos a crear/modificar**:
  - Crear: `firmware/node/components/drivers/ds18b20/ds18b20.h`
  - Crear: `firmware/node/components/drivers/ds18b20/ds18b20.c`
  - Crear: `firmware/node/components/drivers/ds18b20/CMakeLists.txt`

- **Criterio de aceptacion**:
  - Al ejecutar, el monitor serie muestra la temperatura ambiente real (entre 15 y 35 grados C aproximadamente).
  - La lectura cambia al tocar el sensor con los dedos (sube) o al soplar aire frio (baja).
  - Si se desconecta el sensor, la funcion retorna `ESP_ERR_NOT_FOUND` o `ESP_ERR_INVALID_RESPONSE`, no se cuelga.

- **Dependencias**: Fase 1 completada (proyecto base compilando), hardware DS18B20 conectado.

- **Pistas**:
  - Componente `onewire_bus`: `onewire_bus_install()`, `onewire_bus_search_rom()`, `onewire_bus_reset()`, `onewire_bus_write_bytes()`, `onewire_bus_read_bytes()`.
  - El DS18B20 usa comandos ROM (0xCC = Skip ROM si solo hay un sensor) y comandos de funcion (0x44 = Convert T, 0xBE = Read Scratchpad).
  - La conversion de los 2 bytes del scratchpad a temperatura: `temp = ((msb << 8) | lsb) / 16.0`.

- **Errores comunes**:
  1. **Olvidar la resistencia pull-up de 4.7K ohm**: Sin ella, el bus 1-Wire no funciona. El sensor no responde y se obtienen solo errores de timeout. Es un error de hardware, no de software.
  2. **No esperar suficiente tiempo tras el comando 0x44**: La conversion a 12-bit necesita hasta 750 ms. Si se lee antes, se obtiene el valor anterior o 85.0 grados C (valor por defecto del power-on reset).

- **Tiempo estimado**: 4-5 horas

---

### Tarea T3.1.2: Integrar DS18B20 en el ciclo del nodo

- **Dificultad**: Basico
- **Descripcion**:
  1. Abrir `firmware/node/main/sensor_manager.c` (creado en fases anteriores).
  2. Incluir `ds18b20.h` en las cabeceras.
  3. En la funcion de inicializacion del sensor manager, verificar el tipo de sensor configurado. Si `sensor_type == SENSOR_TYPE_TEMP`, llamar a `ds18b20_init(pin_configurado)`.
  4. En la funcion de lectura del sensor manager, si el tipo es temperatura, llamar a `ds18b20_read_temp(&valor)`.
  5. Empaquetar el valor de temperatura en el payload ESP-NOW existente (estructura de datos definida en Fase 2). Asegurar que el campo `sensor_type` indique `TEMP` y el campo `value` contenga el float de temperatura.
  6. En el gateway, verificar que al recibir el paquete ESP-NOW se decodifica correctamente y se muestra la temperatura en el log.

- **Archivos a crear/modificar**:
  - Modificar: `firmware/node/main/sensor_manager.c`
  - Modificar: `firmware/node/main/sensor_manager.h` (agregar SENSOR_TYPE_TEMP si no existe)

- **Criterio de aceptacion**:
  - El nodo envia paquetes ESP-NOW que contienen la temperatura real leida del DS18B20.
  - El gateway recibe el paquete, decodifica el valor y lo muestra en el monitor serie: `"Nodo XX: Temperatura = 24.5 C"`.
  - La temperatura que muestra el gateway coincide con la lectura directa del sensor (verificar con termometro si se dispone de uno).

- **Dependencias**: T3.1.1 completada, Fase 2 completada (comunicacion ESP-NOW funcionando).

- **Pistas**:
  - Reusar la estructura de payload ESP-NOW de la Fase 2. Si el payload tiene un campo generico `float value`, simplemente asignar el valor de temperatura ahi.
  - Si se usa deep sleep, recordar que `ds18b20_init()` debe llamarse en cada ciclo de wake, ya que el periferico se pierde al dormir.

- **Errores comunes**:
  1. **No reinicializar el bus 1-Wire tras deep sleep**: El ESP32 pierde el estado de los GPIOs al entrar en deep sleep. Hay que volver a llamar `ds18b20_init()` cada vez que se despierta.
  2. **Enviar el float con endianness diferente entre nodo y gateway**: Ambos ESP32 son little-endian, pero si se serializa manualmente, asegurarse de que el orden de bytes es consistente. Recomendacion: usar `memcpy` para copiar el float al buffer.

- **Tiempo estimado**: 2-3 horas

---

**Checkpoint 3.1**: Al completar estas 2 tareas, debes poder ver en el monitor serie del gateway la temperatura real del agua/ambiente leida por un nodo remoto via ESP-NOW. Toca el sensor con la mano y confirma que el valor sube. Desconecta el sensor y confirma que el nodo maneja el error sin bloquearse.

---

## Sub-tarea 3.2: Driver de pH por ADC

El pH mide la acidez o alcalinidad del agua en una escala de 0 a 14. Las sondas de pH generan un voltaje analogico proporcional al pH. Un modulo amplificador (como el SEN0161 de DFRobot o similares) acondiciona la senal para que el ESP32 la pueda leer por su conversor analogico-digital (ADC). La calibracion con soluciones buffer conocidas es imprescindible para obtener lecturas fiables.

---

### Tarea T3.2.1: Leer sonda pH via ADC

- **Dificultad**: Intermedio
- **Descripcion**:
  1. Crear la carpeta `firmware/node/components/drivers/ph_adc/`.
  2. Crear `ph_adc.h` con:
     ```c
     typedef struct {
         adc_unit_t adc_unit;       // ADC_UNIT_1 o ADC_UNIT_2
         adc_channel_t adc_channel; // Canal ADC
         adc_atten_t attenuation;   // Atenuacion (tipicamente ADC_ATTEN_DB_11)
     } ph_adc_config_t;

     esp_err_t ph_adc_init(const ph_adc_config_t *config);
     esp_err_t ph_adc_read_raw(int *raw_value);
     esp_err_t ph_adc_read_voltage(float *voltage_mv);
     void ph_adc_deinit(void);
     ```
  3. Crear `ph_adc.c` implementando:
     - `ph_adc_init()`: Configura el ADC con la atenuacion indicada. Usar la API de calibracion `esp_adc_cal` (ESP-IDF v5.x: `adc_cali_create_scheme_curve_fitting()` o `adc_cali_create_scheme_line_fitting()`). Almacenar el handle de calibracion para uso posterior.
     - `ph_adc_read_raw()`: Lee el valor crudo del ADC (0-4095 para 12 bits).
     - `ph_adc_read_voltage()`: Lee el ADC y convierte a mV usando el handle de calibracion. Esto da una lectura mas precisa que simplemente escalar el valor crudo.
     - `ph_adc_deinit()`: Libera recursos.
  4. Crear `CMakeLists.txt` del componente.
  5. Programa de prueba: leer el voltaje cada segundo y mostrarlo por `ESP_LOGI`. Sumergir la sonda en agua corriente y verificar que se obtiene un voltaje estable (aprox. 2.5V para pH 7 con la mayoria de modulos).

- **Archivos a crear/modificar**:
  - Crear: `firmware/node/components/drivers/ph_adc/ph_adc.h`
  - Crear: `firmware/node/components/drivers/ph_adc/ph_adc.c`
  - Crear: `firmware/node/components/drivers/ph_adc/CMakeLists.txt`

- **Criterio de aceptacion**:
  - El monitor serie muestra lecturas de voltaje estables (variacion menor a 10 mV entre lecturas consecutivas).
  - Al cambiar la solucion (agua vs buffer), el voltaje cambia de forma notable.
  - La funcion `ph_adc_read_raw()` retorna valores entre 0 y 4095.
  - La funcion `ph_adc_read_voltage()` retorna valores coherentes en mV (entre 0 y ~3300 mV con atenuacion 11 dB).

- **Dependencias**: Fase 1 completada, sonda de pH y modulo amplificador conectados al ESP32.

- **Pistas**:
  - En ESP-IDF v5.x, la API de ADC es `adc_oneshot_new_unit()`, `adc_oneshot_config_channel()`, `adc_oneshot_read()`.
  - Para calibracion de voltaje: `adc_cali_create_scheme_line_fitting()` o `adc_cali_create_scheme_curve_fitting()` segun el chip. Usar `adc_cali_raw_to_voltage()` para convertir.
  - El ESP32-C3 solo tiene ADC1 (no ADC2). Verificar que el canal elegido pertenece a ADC1.

- **Errores comunes**:
  1. **No usar la calibracion de voltaje del ADC**: El ADC del ESP32 tiene no-linealidad significativa. Sin calibracion, la lectura en mV puede tener un error de hasta 100 mV, lo que se traduce en ~0.5 unidades de pH de error.
  2. **Usar ADC2 en el ESP32-C3**: El ESP32-C3 solo tiene ADC1. Intentar usar ADC2 dara error de inicializacion.

- **Tiempo estimado**: 3-4 horas

---

### Tarea T3.2.2: Calibracion 2 puntos para pH

- **Dificultad**: Avanzado
- **Descripcion**:
  1. Entender la relacion lineal entre voltaje y pH: `pH = pendiente * voltaje_mV + offset`. Con dos puntos conocidos (pH 4.0 y pH 7.0), se pueden calcular pendiente y offset.
  2. En `ph_adc.c`, agregar las siguientes funciones:
     ```c
     // Estructura para almacenar calibracion
     typedef struct {
         float slope;     // pendiente (pH/mV)
         float offset;    // offset (pH)
         bool calibrated; // true si se ha calibrado
     } ph_calibration_t;

     esp_err_t ph_adc_calibrate_point(float ph_reference, int raw_value);
     esp_err_t ph_adc_calculate_calibration(void);
     float ph_adc_convert(int raw_value);
     esp_err_t ph_adc_save_calibration(void);
     esp_err_t ph_adc_load_calibration(void);
     ```
  3. Implementar la logica de calibracion:
     - `ph_adc_calibrate_point()`: Almacena un par (ph_referencia, valor_raw) en memoria. Se llama 2 veces: una con pH 4.0 y otra con pH 7.0.
     - `ph_adc_calculate_calibration()`: Con los 2 puntos almacenados, calcula la pendiente y el offset usando la formula de la recta que pasa por 2 puntos:
       ```
       pendiente = (pH2 - pH1) / (voltaje2 - voltaje1)
       offset = pH1 - pendiente * voltaje1
       ```
     - `ph_adc_convert()`: Aplica `pH = pendiente * voltaje + offset`.
     - `ph_adc_save_calibration()`: Guarda pendiente y offset en NVS con claves como `"ph_slope"` y `"ph_offset"`.
     - `ph_adc_load_calibration()`: Carga coeficientes desde NVS al arrancar. Si no existen, usar valores por defecto.
  4. En `ph_adc_init()`, llamar a `ph_adc_load_calibration()` para cargar la calibracion guardada.

- **Archivos a crear/modificar**:
  - Modificar: `firmware/node/components/drivers/ph_adc/ph_adc.c`
  - Modificar: `firmware/node/components/drivers/ph_adc/ph_adc.h`

- **Criterio de aceptacion**:
  - Tras calibrar con buffers pH 4.0 y pH 7.0, la lectura de la sonda en buffer pH 4.0 muestra un valor entre 3.8 y 4.2.
  - La lectura en buffer pH 7.0 muestra un valor entre 6.8 y 7.2.
  - Los coeficientes se persisten en NVS: al reiniciar el ESP32, la calibracion sigue activa sin necesidad de recalibrar.
  - Sin calibracion (NVS vacia), el driver usa valores por defecto razonables y logea un warning: `"pH no calibrado, usando valores por defecto"`.

- **Dependencias**: T3.2.1 completada.

- **Pistas**:
  - Para NVS: `nvs_open()`, `nvs_set_blob()` / `nvs_get_blob()` (para guardar el struct completo) o `nvs_set_i32()` con valores multiplicados por 1000 para evitar problemas de precision con floats.
  - Es mejor guardar los voltajes de calibracion en lugar de (o ademas de) los coeficientes, para poder recalcular si se cambia la formula.
  - Valores por defecto tipicos: pendiente ~ -5.7 mV/pH, offset ~ 7.0 (varian segun el modulo).

- **Errores comunes**:
  1. **Confundir el orden de los puntos de calibracion**: Si se pasan pH 4.0 y pH 7.0 pero con los voltajes intercambiados, la pendiente tendra signo incorrecto. Siempre asociar cada lectura con su pH de referencia correcto.
  2. **No convertir raw a voltaje antes de calcular la calibracion**: La relacion lineal es entre pH y voltaje (en mV), no entre pH y valor crudo del ADC. Si se usa el valor crudo directamente, la calibracion sera menos precisa por la no-linealidad del ADC.

- **Tiempo estimado**: 4-5 horas

---

### Tarea T3.2.3: Procedimiento de calibracion interactivo

- **Dificultad**: Intermedio
- **Descripcion**:
  1. Crear un modo de calibracion que se pueda activar de dos formas:
     - **Via comando serie (UART)**: Al enviar el string `"CAL_PH"` por la consola serie, el nodo entra en modo calibracion.
     - **Via mensaje del gateway**: El gateway envia un comando ESP-NOW al nodo indicandole que entre en modo calibracion.
  2. El flujo de calibracion interactiva es:
     ```
     [NODO] -> "=== CALIBRACION pH ==="
     [NODO] -> "Paso 1: Introduzca la sonda en solucion buffer pH 4.0"
     [NODO] -> "Espere 30 segundos para estabilizar"
     [NODO] -> "Envie 'OK' cuando este listo"
     [USUARIO] -> "OK"
     [NODO] -> Toma 10 lecturas, calcula promedio
     [NODO] -> "Lectura pH 4.0: voltaje promedio = 1850 mV"
     [NODO] -> "Paso 2: Enjuague la sonda y sumerjala en buffer pH 7.0"
     [NODO] -> "Espere 30 segundos para estabilizar"
     [NODO] -> "Envie 'OK' cuando este listo"
     [USUARIO] -> "OK"
     [NODO] -> Toma 10 lecturas, calcula promedio
     [NODO] -> "Lectura pH 7.0: voltaje promedio = 2500 mV"
     [NODO] -> "Calibracion completada. Pendiente = -0.00461, Offset = 12.53"
     [NODO] -> "Coeficientes guardados en NVS"
     ```
  3. Para la entrada por UART, usar `uart_read_bytes()` o el componente `esp_console`.
  4. Durante la calibracion, el nodo no debe enviar datos normales al gateway. Usar un flag o un estado en el sensor manager.

- **Archivos a crear/modificar**:
  - Crear: `firmware/node/main/calibration_mode.c`
  - Crear: `firmware/node/main/calibration_mode.h`
  - Modificar: `firmware/node/main/sensor_manager.c` (agregar estado de calibracion)

- **Criterio de aceptacion**:
  - Al enviar `"CAL_PH"` por la consola serie, el nodo entra en modo calibracion y muestra los mensajes guia.
  - Al completar el procedimiento con los 2 buffers, se muestran los coeficientes calculados.
  - Los coeficientes se guardan en NVS (verificable reiniciando y leyendo los valores de NVS).
  - Si el usuario cancela (enviar `"CANCEL"`), el nodo vuelve al modo normal sin modificar los coeficientes anteriores.
  - Durante la calibracion, el nodo no envia paquetes ESP-NOW de datos.

- **Dependencias**: T3.2.1 y T3.2.2 completadas.

- **Pistas**:
  - Para leer UART: `uart_driver_install()`, `uart_read_bytes(UART_NUM_0, buf, len, timeout_ticks)`.
  - Alternativa mas sencilla: usar `esp_console` con comandos registrados tipo `cal_ph_start`, `cal_ph_point1`, `cal_ph_point2`.
  - Para evitar que deep sleep interrumpa la calibracion, deshabilitar deep sleep temporalmente con un flag.

- **Errores comunes**:
  1. **No promediar suficientes lecturas**: Una sola lectura del ADC puede tener ruido. Tomar al menos 10 lecturas y promediar para obtener un valor estable en cada punto de calibracion.
  2. **No dar tiempo suficiente de estabilizacion a la sonda**: Las sondas de pH necesitan entre 15 y 60 segundos para estabilizarse en una nueva solucion. Si se lee demasiado pronto, la calibracion sera imprecisa.

- **Tiempo estimado**: 3-4 horas

---

**Checkpoint 3.2**: Al completar estas 3 tareas, debes poder ejecutar una calibracion interactiva de la sonda de pH por la consola serie. Tras calibrar con buffers 4.0 y 7.0, el nodo debe mostrar lecturas de pH con precision de +/- 0.2 unidades. Reinicia el ESP32 y verifica que la calibracion persiste (los coeficientes se cargan desde NVS).

---

## Sub-tarea 3.3: Driver de oxigeno disuelto (DO)

El oxigeno disuelto es uno de los parametros mas criticos en acuicultura. Los peces necesitan un minimo de O2 para sobrevivir (tipicamente >5 mg/L). Las sondas de DO generan un voltaje proporcional a la concentracion de oxigeno, pero este voltaje varia significativamente con la temperatura del agua, por lo que se necesita compensacion.

---

### Tarea T3.3.1: Leer sonda DO via ADC

- **Dificultad**: Intermedio
- **Descripcion**:
  1. Crear la carpeta `firmware/node/components/drivers/do_sensor/`.
  2. Crear `do_sensor.h` con:
     ```c
     typedef struct {
         adc_unit_t adc_unit;
         adc_channel_t adc_channel;
         adc_atten_t attenuation;
     } do_sensor_config_t;

     esp_err_t do_sensor_init(const do_sensor_config_t *config);
     esp_err_t do_sensor_read_raw(int *raw_value);
     esp_err_t do_sensor_read_voltage(float *voltage_mv);
     float do_sensor_convert(float voltage_mv, float temp_celsius);
     esp_err_t do_sensor_calibrate(float voltage_at_saturation, float temp_at_calibration);
     esp_err_t do_sensor_save_calibration(void);
     esp_err_t do_sensor_load_calibration(void);
     void do_sensor_deinit(void);
     ```
  3. Crear `do_sensor.c` implementando:
     - `do_sensor_init()`: Similar a `ph_adc_init()`. Configura el ADC con calibracion de voltaje. Carga coeficientes de calibracion desde NVS.
     - `do_sensor_read_raw()` y `do_sensor_read_voltage()`: Igual que en el driver pH.
     - `do_sensor_calibrate()`: Calibracion de un punto. Se expone la sonda al aire (saturacion 100%), se lee el voltaje y se registra junto con la temperatura ambiente. El voltaje de saturacion al aire se toma como referencia.
     - La conversion a mg/L se hara en la tarea T3.3.2.
  4. Crear `CMakeLists.txt` del componente.
  5. Probar leyendo el voltaje con la sonda al aire (deberia dar el voltaje maximo) y en agua (deberia dar un voltaje menor dependiendo del nivel de oxigenacion).

- **Archivos a crear/modificar**:
  - Crear: `firmware/node/components/drivers/do_sensor/do_sensor.h`
  - Crear: `firmware/node/components/drivers/do_sensor/do_sensor.c`
  - Crear: `firmware/node/components/drivers/do_sensor/do_sensor.h`
  - Crear: `firmware/node/components/drivers/do_sensor/CMakeLists.txt`

- **Criterio de aceptacion**:
  - El monitor serie muestra lecturas de voltaje estables para la sonda DO.
  - Con la sonda al aire, el voltaje es alto y estable.
  - Con la sonda en agua, el voltaje es menor.
  - La calibracion de un punto se puede ejecutar y los datos se guardan en NVS.

- **Dependencias**: Fase 1 completada, sonda de oxigeno disuelto conectada.

- **Pistas**:
  - La estructura del ADC es identica a la del driver de pH. Puedes copiar la base y adaptarla.
  - La calibracion de DO es mas sencilla que la de pH: solo necesitas un punto (saturacion al aire).
  - `esp_adc_cal` / `adc_cali_raw_to_voltage()` para conversion precisa.

- **Errores comunes**:
  1. **Confundir la sonda de DO con la de pH**: Ambas generan voltaje analogico, pero tienen rangos y comportamientos diferentes. Etiquetar bien los cables y los canales ADC.
  2. **No limpiar/secar la sonda antes de calibrar al aire**: Gotas de agua en la membrana de la sonda afectan la lectura de saturacion. Secar suavemente con papel antes de calibrar.

- **Tiempo estimado**: 3-4 horas

---

### Tarea T3.3.2: Compensacion de temperatura para DO

- **Dificultad**: Avanzado
- **Descripcion**:
  1. Entender por que es necesaria la compensacion: La cantidad de oxigeno que puede disolverse en agua depende fuertemente de la temperatura. A 0 grados C, la saturacion es ~14.6 mg/L. A 30 grados C, es ~7.5 mg/L. Si no se compensa, una lectura a 30 grados C parecera que hay mucho mas O2 del que realmente hay.
  2. Implementar la tabla o formula de saturacion de O2 segun la temperatura. Tabla simplificada (mg/L a saturacion 100%, presion atmosferica estandar):
     ```c
     // Tabla de saturacion O2 (mg/L) por temperatura (grados C)
     // Indices: 0=0C, 1=1C, ... 40=40C
     static const float do_saturation_table[] = {
         14.62, 14.22, 13.83, 13.46, 13.11,  // 0-4
         12.77, 12.45, 12.14, 11.84, 11.56,  // 5-9
         11.29, 11.03, 10.78, 10.54, 10.31,  // 10-14
         10.08,  9.87,  9.67,  9.47,  9.28,  // 15-19
          9.09,  8.92,  8.74,  8.58,  8.42,  // 20-24
          8.26,  8.11,  7.97,  7.83,  7.69,  // 25-29
          7.56,  7.43,  7.31,  7.18,  7.07,  // 30-34
          6.95,  6.84,  6.73,  6.62,  6.52,  // 35-39
          6.41                                 // 40
     };
     ```
  3. Implementar `do_sensor_convert()`:
     ```c
     float do_sensor_convert(float voltage_mv, float temp_celsius) {
         // 1. Obtener el voltaje de saturacion almacenado en calibracion
         // 2. Obtener la saturacion a la temperatura de calibracion (tabla)
         // 3. Obtener la saturacion a la temperatura actual (tabla)
         // 4. Calcular:
         //    DO_mg_L = (voltage_mv / voltage_saturacion) * saturacion_a_temp_actual
         // 5. Retornar el valor en mg/L
     }
     ```
  4. La temperatura debe venir del DS18B20 leido en el mismo nodo (si tiene ambos sensores) o pasada como parametro.
  5. Guardar los coeficientes de calibracion (voltaje de saturacion y temperatura de calibracion) en NVS.
  6. Probar: calibrar al aire a temperatura ambiente, luego leer en agua a la misma temperatura. El valor debe estar entre 0 y la saturacion maxima para esa temperatura.

- **Archivos a crear/modificar**:
  - Modificar: `firmware/node/components/drivers/do_sensor/do_sensor.c`
  - Modificar: `firmware/node/components/drivers/do_sensor/do_sensor.h`

- **Criterio de aceptacion**:
  - Con la sonda al aire a temperatura ambiente (ej. 25 grados C), `do_sensor_convert()` retorna un valor cercano a 8.26 mg/L (saturacion a 25 grados C).
  - Si se cambia la temperatura de compensacion artificialmente (ej. pasar 10 grados C en vez de 25 grados C), el valor calculado cambia de forma coherente.
  - La tabla de saturacion retorna valores correctos para temperaturas enteras.
  - Para temperaturas con decimales (ej. 22.5 grados C), se implementa interpolacion lineal entre los dos valores mas cercanos de la tabla.

- **Dependencias**: T3.3.1 y T3.1.1 completadas (necesita driver DO y driver de temperatura).

- **Pistas**:
  - Para interpolar linealmente: si temp = 22.5, tomar saturacion[22] y saturacion[23], interpolar: `resultado = sat[22] + 0.5 * (sat[23] - sat[22])`.
  - Usar `floorf()` y casting a `int` para obtener el indice inferior de la tabla.
  - Limitar la temperatura al rango de la tabla (0-40 grados C) para evitar accesos fuera del array.

- **Errores comunes**:
  1. **No compensar la temperatura y reportar valores incorrectos**: Sin compensacion, una lectura de DO a 30 grados C puede parecer un 30% mas alta de lo real. En acuicultura, esto puede significar no detectar una situacion de hipoxia peligrosa para los peces.
  2. **Acceder fuera de la tabla de saturacion**: Si la temperatura esta fuera del rango 0-40 grados C, el indice del array sera invalido. Siempre hacer `clamp` del valor de temperatura antes de indexar la tabla.

- **Tiempo estimado**: 4-5 horas

---

**Checkpoint 3.3**: Al completar estas 2 tareas, el nodo debe poder leer la concentracion de oxigeno disuelto en mg/L con compensacion de temperatura. Calibra la sonda al aire, sumergela en agua y verifica que el valor es coherente (tipicamente entre 5 y 9 mg/L en agua bien oxigenada a temperatura ambiente). Cambia la temperatura del agua (agua tibia vs fria) y observa como la compensacion ajusta la lectura.

---

## Sub-tarea 3.4: Driver ORP y nivel de agua

Estos dos sensores completan el conjunto de monitoreo acuicola. El ORP indica el estado de oxidacion-reduccion del agua (importante para la calidad del agua), y el sensor de nivel permite detectar el nivel del agua en el tanque.

---

### Tarea T3.4.1: Leer sonda ORP via ADC

- **Dificultad**: Basico
- **Descripcion**:
  1. Crear la carpeta `firmware/node/components/drivers/orp/`.
  2. Crear `orp.h` con:
     ```c
     typedef struct {
         adc_unit_t adc_unit;
         adc_channel_t adc_channel;
         adc_atten_t attenuation;
     } orp_config_t;

     esp_err_t orp_init(const orp_config_t *config);
     esp_err_t orp_read_mv(float *orp_mv);
     esp_err_t orp_set_offset(float offset_mv);
     esp_err_t orp_save_offset(void);
     esp_err_t orp_load_offset(void);
     void orp_deinit(void);
     ```
  3. Crear `orp.c` implementando:
     - `orp_init()`: Configura el ADC, carga offset desde NVS.
     - `orp_read_mv()`: Lee el voltaje del ADC y aplica el offset. El ORP se reporta directamente en mV. La mayoria de modulos ORP tienen un punto neutro alrededor de 1500 mV (que corresponde a 0 mV ORP), asi que: `ORP_mV = voltaje_leido - 1500 + offset`.
     - `orp_set_offset()`: Permite ajustar el offset si se dispone de una solucion de referencia ORP. Para uso basico, el offset puede ser 0.
     - `orp_save_offset()` y `orp_load_offset()`: Persistencia en NVS.
  4. Crear `CMakeLists.txt` del componente.
  5. Probar: con la sonda en agua corriente, el ORP deberia estar entre +200 y +400 mV (agua clorada del grifo suele ser positiva).

- **Archivos a crear/modificar**:
  - Crear: `firmware/node/components/drivers/orp/orp.h`
  - Crear: `firmware/node/components/drivers/orp/orp.c`
  - Crear: `firmware/node/components/drivers/orp/CMakeLists.txt`

- **Criterio de aceptacion**:
  - El monitor serie muestra valores de ORP en mV.
  - En agua corriente del grifo, los valores estan en un rango razonable (+100 a +500 mV).
  - El offset se puede ajustar y se persiste en NVS.
  - Las lecturas son estables (variacion menor a 20 mV entre lecturas consecutivas tras estabilizacion).

- **Dependencias**: Fase 1 completada, sonda ORP conectada.

- **Pistas**:
  - El driver ORP es el mas sencillo de todos los sensores analogicos. Es esencialmente una lectura de voltaje con un offset.
  - Reutilizar la logica de ADC ya implementada en los drivers de pH y DO.
  - El valor de 1500 mV como punto neutro depende del modulo usado. Consultar el datasheet del modulo especifico.

- **Errores comunes**:
  1. **Confundir ORP con pH**: Ambos usan sondas de aspecto similar y leen voltaje analogico, pero el ORP se reporta en mV (tipicamente -2000 a +2000 mV) y el pH en unidades de pH (0 a 14). No intercambiar las sondas.
  2. **Esperar valores de ORP muy precisos sin referencia**: Sin una solucion de referencia ORP para calibrar, los valores son aproximados. Para acuicultura basica, esto suele ser suficiente para detectar tendencias.

- **Tiempo estimado**: 2-3 horas

---

### Tarea T3.4.2: Sensor de nivel por ultrasonido HC-SR04

- **Dificultad**: Intermedio
- **Descripcion**:
  1. Crear la carpeta `firmware/node/components/drivers/ultrasonic/`.
  2. Crear `ultrasonic.h` con:
     ```c
     typedef struct {
         gpio_num_t trigger_pin;
         gpio_num_t echo_pin;
     } ultrasonic_config_t;

     esp_err_t ultrasonic_init(const ultrasonic_config_t *config);
     esp_err_t ultrasonic_measure_cm(float *distance_cm);
     void ultrasonic_deinit(void);
     ```
  3. Crear `ultrasonic.c` implementando:
     - `ultrasonic_init()`: Configura trigger_pin como salida (GPIO_MODE_OUTPUT), echo_pin como entrada (GPIO_MODE_INPUT). Poner trigger en bajo.
     - `ultrasonic_measure_cm()`:
       ```
       1. Poner trigger en HIGH
       2. Esperar 10 microsegundos (esp_rom_delay_us(10))
       3. Poner trigger en LOW
       4. Esperar a que echo suba a HIGH (inicio del pulso)
       5. Registrar timestamp de inicio: start = esp_timer_get_time()
       6. Esperar a que echo baje a LOW (fin del pulso)
       7. Registrar timestamp de fin: end = esp_timer_get_time()
       8. Calcular duracion en microsegundos: duration = end - start
       9. Calcular distancia: distance_cm = duration / 58.0
          (velocidad del sonido = 343 m/s, ida y vuelta, 1 cm = 58 us)
       10. Si la duracion excede 25000 us (~4.3 metros), retornar error (fuera de rango)
       ```
     - Agregar timeout en los pasos 4 y 6: si el echo no cambia en 30000 us, retornar error.
  4. Crear `CMakeLists.txt` del componente.
  5. Para calcular el nivel del agua: se monta el sensor apuntando hacia abajo desde el borde superior del tanque. La distancia medida es el espacio vacio. El nivel del agua = altura_total_tanque - distancia_medida. La altura total del tanque se configura como parametro.
  6. Probar: apuntar el sensor a una superficie plana (mesa, pared) y verificar que la distancia medida es correcta con una regla.

- **Archivos a crear/modificar**:
  - Crear: `firmware/node/components/drivers/ultrasonic/ultrasonic.h`
  - Crear: `firmware/node/components/drivers/ultrasonic/ultrasonic.c`
  - Crear: `firmware/node/components/drivers/ultrasonic/CMakeLists.txt`

- **Criterio de aceptacion**:
  - El sensor mide distancias entre 2 cm y 400 cm con una precision de +/- 3 mm (segun el datasheet del HC-SR04).
  - Al mover un objeto acercandolo y alejandolo del sensor, las lecturas cambian de forma coherente.
  - Si no hay objeto en el rango (>400 cm), la funcion retorna un error, no un valor basura.
  - El timeout funciona: si se desconecta el sensor, la funcion retorna error en menos de 50 ms, no se bloquea indefinidamente.

- **Dependencias**: Fase 1 completada, sensor HC-SR04 conectado.

- **Pistas**:
  - `gpio_set_direction()`, `gpio_set_level()`, `gpio_get_level()` para controlar los pines.
  - `esp_timer_get_time()` retorna microsegundos desde el arranque. Es la forma mas precisa de medir tiempos cortos.
  - `esp_rom_delay_us(10)` para el pulso de trigger de 10 microsegundos (no usar `vTaskDelay` que tiene resolucion de 1 ms como minimo).
  - **Importante**: El HC-SR04 necesita 5V de alimentacion. El ESP32 trabaja a 3.3V. El pin trigger del ESP32 puede activar el sensor (3.3V es suficiente como HIGH para el HC-SR04), pero el pin echo del HC-SR04 devuelve 5V, que puede dañar el ESP32. Usar un divisor de voltaje en el pin echo (2 resistencias) para bajar de 5V a 3.3V.

- **Errores comunes**:
  1. **Conectar el pin echo del HC-SR04 directamente al ESP32 sin divisor de voltaje**: El HC-SR04 trabaja a 5V y su pin echo devuelve 5V, pero el ESP32 es de 3.3V. Conectar 5V directamente puede dañar el GPIO del ESP32. Siempre usar un divisor de voltaje (ej. 1K y 2K) o un level shifter.
  2. **No implementar timeout en la espera del echo**: Si el sensor falla o no hay objeto en rango, el codigo se queda esperando indefinidamente en un bucle `while(gpio_get_level(echo) == 0)`. Siempre agregar un timeout.

- **Tiempo estimado**: 3-4 horas

---

**Checkpoint 3.4**: Al completar estas 2 tareas, tienes 5 tipos de sensores funcionando: temperatura (DS18B20), pH (ADC + calibracion), oxigeno disuelto (ADC + compensacion de temperatura), ORP (ADC + offset), y nivel de agua (HC-SR04). Cada uno debe poder leerse independientemente y mostrar valores coherentes en el monitor serie.

---

## Sub-tarea 3.5: Validacion y filtrado de lecturas

Los sensores del mundo real producen lecturas ruidosas y a veces erroneas (picos, valores fuera de rango, lecturas NaN). Antes de enviar los datos al gateway, es necesario validar y filtrar para asegurar la calidad de los datos.

---

### Tarea T3.5.1: Validacion por rango

- **Dificultad**: Basico
- **Descripcion**:
  1. Abrir `firmware/node/main/sensor_manager.c`.
  2. Definir los rangos validos por tipo de sensor como constantes o en una estructura:
     ```c
     typedef struct {
         float min_value;
         float max_value;
     } sensor_range_t;

     static const sensor_range_t valid_ranges[] = {
         [SENSOR_TYPE_TEMP]  = { .min_value = -5.0,    .max_value = 45.0   },  // grados C
         [SENSOR_TYPE_PH]    = { .min_value = 0.0,     .max_value = 14.0   },  // unidades pH
         [SENSOR_TYPE_DO]    = { .min_value = 0.0,     .max_value = 20.0   },  // mg/L
         [SENSOR_TYPE_ORP]   = { .min_value = -2000.0, .max_value = 2000.0 },  // mV
         [SENSOR_TYPE_LEVEL] = { .min_value = 2.0,     .max_value = 400.0  },  // cm
     };
     ```
  3. Crear una funcion de validacion:
     ```c
     bool sensor_validate_reading(sensor_type_t type, float value);
     ```
     Retorna `true` si el valor esta dentro del rango, `false` si esta fuera.
  4. En el flujo de lectura, despues de leer el sensor y antes de empaquetar el dato:
     - Si la lectura es valida, incluirla en el payload.
     - Si es invalida, logear un warning (`ESP_LOGW`) con el tipo de sensor y el valor rechazado, e intentar leer de nuevo o usar el ultimo valor valido conocido.
  5. Llevar un contador de lecturas invalidas por sensor. Si se acumulan mas de 5 lecturas invalidas consecutivas, marcar el sensor como en fallo y reportarlo al gateway como error.

- **Archivos a crear/modificar**:
  - Modificar: `firmware/node/main/sensor_manager.c`
  - Modificar: `firmware/node/main/sensor_manager.h` (agregar rangos y funcion de validacion)

- **Criterio de aceptacion**:
  - Una lectura de temperatura de 25.3 grados C pasa la validacion.
  - Una lectura de temperatura de 99.9 grados C es rechazada y se logea un warning.
  - Una lectura de pH de -1.5 es rechazada.
  - Despues de 5 lecturas invalidas consecutivas, el sensor se marca como en fallo.
  - El payload enviado al gateway nunca contiene valores fuera de rango.

- **Dependencias**: Al menos uno de los drivers de sensor completado (T3.1.1, T3.2.1, etc.).

- **Pistas**:
  - Usar `isnan()` o `isinf()` de `<math.h>` para detectar valores NaN o infinito que pueden resultar de divisiones por cero en los drivers.
  - Los rangos pueden hacerse configurables por NVS en el futuro, pero por ahora constantes esta bien.

- **Errores comunes**:
  1. **No verificar NaN/Inf**: Un sensor que falla puede producir un valor `NaN` o `Inf` al hacer calculos (ej. division por cero en la compensacion de DO). Estos valores pasan cualquier comparacion numerica de forma impredecible. Siempre verificar con `isnan()` e `isinf()` antes de comparar con el rango.
  2. **Rechazar lecturas validas por rangos demasiado estrechos**: En acuicultura tropical, el agua puede estar a 32-35 grados C. Si el rango maximo de temperatura fuera 30 grados C, se rechazarian lecturas validas. Usar rangos lo suficientemente amplios para cubrir todos los escenarios reales.

- **Tiempo estimado**: 2 horas

---

### Tarea T3.5.2: Filtro de outliers con media movil

- **Dificultad**: Intermedio
- **Descripcion**:
  1. Implementar un filtro de outliers robusto. El algoritmo:
     - Tomar N lecturas consecutivas del sensor (ej. N=5).
     - Ordenar las lecturas de menor a mayor.
     - Descartar la lectura mas alta y la mas baja (los posibles outliers).
     - Calcular el promedio de las N-2 lecturas restantes (en el caso de N=5, promediar 3).
     - Este metodo se llama "media recortada" o "trimmed mean".
  2. Crear las funciones en `sensor_manager.c` (o en un archivo aparte `sensor_filter.c`):
     ```c
     typedef struct {
         float readings[FILTER_WINDOW_SIZE];  // Buffer de lecturas
         int count;                            // Lecturas validas en el buffer
     } sensor_filter_t;

     void sensor_filter_init(sensor_filter_t *filter);
     esp_err_t sensor_filter_add(sensor_filter_t *filter, float value);
     float sensor_filter_get_trimmed_mean(const sensor_filter_t *filter);
     ```
  3. Para el ordenamiento de N=5 valores, un simple bubble sort es mas que suficiente (son solo 5 elementos).
  4. Integrarlo en el ciclo de lectura:
     - En cada ciclo de wake, tomar las N lecturas rapidamente (ej. 5 lecturas con 100 ms de separacion = 500 ms total).
     - Aplicar el filtro.
     - Enviar el valor filtrado.
  5. Alternativa para deep sleep: si el nodo usa deep sleep y las N lecturas se toman en un solo ciclo de wake, no se necesita RTC memory. Si se quiere acumular lecturas entre ciclos de wake, usar RTC memory (atributo `RTC_DATA_ATTR`).

- **Archivos a crear/modificar**:
  - Crear: `firmware/node/main/sensor_filter.c`
  - Crear: `firmware/node/main/sensor_filter.h`
  - Modificar: `firmware/node/main/sensor_manager.c` (usar el filtro antes de empaquetar datos)

- **Criterio de aceptacion**:
  - Dadas las lecturas [24.1, 24.2, 99.9, 24.3, 24.0], el filtro devuelve ~24.2 (descartando 99.9 y 24.0, promediando 24.1, 24.2, 24.3).
  - Dadas las lecturas [7.0, 7.1, 7.0, 7.1, 7.0], el filtro devuelve ~7.03 (lecturas estables, el filtro no distorsiona).
  - El filtro funciona correctamente con lecturas todas iguales (ej. [10.0, 10.0, 10.0, 10.0, 10.0] retorna 10.0).
  - Se puede verificar con un test unitario o con lecturas simuladas impresas en el log.

- **Dependencias**: T3.5.1 completada (la validacion por rango se aplica antes del filtro para evitar que valores absurdos entren al buffer).

- **Pistas**:
  - Bubble sort para 5 elementos (pseudocodigo):
    ```c
    for (int i = 0; i < n-1; i++)
        for (int j = 0; j < n-i-1; j++)
            if (arr[j] > arr[j+1]) swap(&arr[j], &arr[j+1]);
    ```
  - Si se necesita persistir entre deep sleeps: `RTC_DATA_ATTR static float last_readings[5];`
  - El delay entre lecturas consecutivas (`vTaskDelay(pdMS_TO_TICKS(100))`) debe ser suficiente para que el sensor tenga una nueva lectura lista.

- **Errores comunes**:
  1. **Aplicar el filtro sobre datos ya invalidos**: Si una lectura de 99.9 grados C pasa al filtro sin validacion previa, podria contaminar el resultado. Siempre aplicar primero la validacion de rango (T3.5.1) y solo pasar lecturas validas al filtro.
  2. **No manejar el caso de menos de N lecturas validas**: Si de 5 intentos de lectura, 3 fallan y solo se obtienen 2 validas, no se puede aplicar la media recortada. En ese caso, retornar el promedio simple de las disponibles o marcar como error si hay menos de 3 lecturas validas.

- **Tiempo estimado**: 3-4 horas

---

**Checkpoint 3.5**: Al completar estas 2 tareas, todas las lecturas de sensores pasan por un pipeline de calidad: primero se valida el rango, luego se aplica el filtro de media recortada. Prueba desconectando un sensor brevemente: los outliers deben ser descartados, y si el sensor no se recupera, se marca como en fallo. Las lecturas enviadas al gateway deben ser limpias y estables.

---

## Sub-tarea 3.6: Actuadores y alertas

El monitoreo sin accion no es suficiente. Cuando los parametros del agua estan fuera de limites seguros, el sistema debe poder activar actuadores (aireadores, bombas, calentadores) automaticamente.

---

### Tarea T3.6.1: Control de reles por GPIO

- **Dificultad**: Basico
- **Descripcion**:
  1. En el gateway (o un nodo actuador dedicado), crear `actuator_ctrl.h`:
     ```c
     #define MAX_ACTUATORS 4

     typedef struct {
         gpio_num_t gpio_pin;
         bool active_low;     // true si el rele se activa con nivel bajo
         bool current_state;  // true = encendido
         char name[16];       // nombre descriptivo (ej. "aireador1")
     } actuator_t;

     esp_err_t actuator_init(int index, gpio_num_t pin, bool active_low, const char *name);
     esp_err_t actuator_set(int index, bool on);
     bool actuator_get_state(int index);
     const char* actuator_get_name(int index);
     esp_err_t actuator_init_all(void);
     ```
  2. Crear `actuator_ctrl.c` implementando:
     - `actuator_init()`: Configura el GPIO como salida. Poner el rele en estado apagado (respetando `active_low`). Si `active_low` es true, apagado = GPIO HIGH; si false, apagado = GPIO LOW.
     - `actuator_set()`: Enciende o apaga el rele. Guardar el estado actual. Si `active_low` y `on`, poner GPIO LOW; si `active_low` y no `on`, poner GPIO HIGH. Logear el cambio de estado.
     - `actuator_get_state()`: Retorna el estado actual.
     - `actuator_init_all()`: Inicializa todos los actuadores configurados.
  3. Si el gateway tiene API REST (de la Fase 2), agregar endpoints:
     - `GET /api/actuators`: Lista todos los actuadores y su estado.
     - `POST /api/actuators/{index}`: Cuerpo `{"on": true/false}`. Enciende o apaga el actuador indicado.
  4. Probar: conectar un LED o un rele a un GPIO, llamar al endpoint REST y verificar que se enciende/apaga.

- **Archivos a crear/modificar**:
  - Crear: `firmware/gateway/main/actuator_ctrl.h`
  - Crear: `firmware/gateway/main/actuator_ctrl.c`
  - Modificar: `firmware/gateway/main/http_server.c` (agregar endpoints REST para actuadores si aplica)

- **Criterio de aceptacion**:
  - Se puede encender y apagar un rele desde la API REST del gateway.
  - El estado del actuador se reporta correctamente (GET retorna el estado actual).
  - Los reles activo-bajo y activo-alto funcionan correctamente (probar ambas configuraciones).
  - Al reiniciar el ESP32, todos los actuadores arrancan apagados (estado seguro por defecto).

- **Dependencias**: Fase 2 completada (servidor HTTP en el gateway), modulo de rele conectado.

- **Pistas**:
  - `gpio_set_direction(pin, GPIO_MODE_OUTPUT)` para configurar como salida.
  - `gpio_set_level(pin, level)` para poner HIGH o LOW.
  - Los modulos de rele baratos suelen ser activo-bajo (se activan con 0V). Verificar con un multimetro o LED antes de conectar equipos reales.

- **Errores comunes**:
  1. **No inicializar el GPIO en estado apagado al arrancar**: Si el GPIO queda en un estado indeterminado, el rele podria activarse brevemente durante el arranque. Esto puede ser peligroso si controla una bomba o calentador. Siempre poner el estado "apagado" inmediatamente al inicializar.
  2. **Confundir logica activo-bajo/activo-alto**: Muchos modulos de rele economicos se activan con nivel bajo (0V). Si se asume activo-alto, al poner GPIO HIGH se apaga el rele (lo opuesto de lo esperado). Siempre verificar la logica del modulo de rele usado.

- **Tiempo estimado**: 3-4 horas

---

### Tarea T3.6.2: Sistema de alertas por umbral con histeresis

- **Dificultad**: Avanzado
- **Descripcion**:
  1. Entender la histeresis: Si se activa un aireador cuando la temperatura sube a 28 grados C y se desactiva cuando baja de 28 grados C, el aireador oscilara constantemente (encendido-apagado-encendido cada pocos segundos) cuando la temperatura este alrededor de 28 grados C. Con histeresis, se activa a 28 grados C pero no se desactiva hasta que baje a 26 grados C. Esto crea una "banda muerta" que evita la oscilacion.
  2. Crear `alert_system.h`:
     ```c
     #define MAX_ALERTS 8

     typedef struct {
         sensor_type_t sensor_type;
         int node_id;              // ID del nodo a monitorear (-1 = todos)
         float threshold_high;     // Umbral superior (activar si lectura > este valor)
         float threshold_low;      // Umbral inferior (desactivar si lectura < este valor)
         int actuator_index;       // Indice del actuador a controlar
         bool actuator_on_high;    // true: activar cuando se supera threshold_high
                                   // false: activar cuando se baja de threshold_low
         bool alert_active;        // Estado actual de la alerta
         bool enabled;             // Alerta habilitada/deshabilitada
     } alert_rule_t;

     esp_err_t alert_system_init(void);
     esp_err_t alert_add_rule(const alert_rule_t *rule);
     void alert_process_reading(int node_id, sensor_type_t type, float value);
     esp_err_t alert_save_rules(void);
     esp_err_t alert_load_rules(void);
     ```
  3. Crear `alert_system.c` implementando:
     - `alert_system_init()`: Carga reglas desde NVS.
     - `alert_add_rule()`: Agrega una regla de alerta. Validar que los umbrales sean coherentes (threshold_high > threshold_low).
     - `alert_process_reading()`: Logica central. Para cada regla que aplique al sensor/nodo:
       ```
       Si actuator_on_high es true:
           Si lectura >= threshold_high Y alerta NO activa:
               Activar actuador, marcar alerta como activa, logear
           Si lectura <= threshold_low Y alerta activa:
               Desactivar actuador, marcar alerta como inactiva, logear
       Si actuator_on_high es false:
           Si lectura <= threshold_low Y alerta NO activa:
               Activar actuador, marcar alerta como activa, logear
           Si lectura >= threshold_high Y alerta activa:
               Desactivar actuador, marcar alerta como inactiva, logear
       ```
     - `alert_save_rules()` y `alert_load_rules()`: Persistir reglas en NVS usando `nvs_set_blob`.
  4. En el gateway, llamar a `alert_process_reading()` cada vez que se recibe una lectura de un nodo.
  5. Ejemplo de configuracion de reglas tipicas:
     - Temperatura alta: activar aireador si >28 grados C, desactivar si <26 grados C.
     - Oxigeno bajo: activar aireador si DO <4 mg/L, desactivar si DO >6 mg/L.
     - pH bajo: activar bomba de cal si pH <6.5, desactivar si pH >7.0.
  6. Agregar endpoint REST para configurar reglas:
     - `GET /api/alerts`: Lista todas las reglas.
     - `POST /api/alerts`: Crea una nueva regla.
     - `DELETE /api/alerts/{index}`: Elimina una regla.

- **Archivos a crear/modificar**:
  - Crear: `firmware/gateway/main/alert_system.h`
  - Crear: `firmware/gateway/main/alert_system.c`
  - Modificar: `firmware/gateway/main/main.c` (llamar a `alert_process_reading` al recibir datos)
  - Modificar: `firmware/gateway/main/http_server.c` (agregar endpoints REST para alertas si aplica)

- **Criterio de aceptacion**:
  - Con una regla de temperatura (activar a 28, desactivar a 26), al enviar lecturas simuladas:
    - Lectura 27 grados C: nada sucede.
    - Lectura 28.5 grados C: actuador se enciende, log muestra "ALERTA: Temperatura alta (28.5 C > 28.0 C). Activando aireador1".
    - Lectura 27 grados C: nada sucede (histeresis, el actuador sigue encendido).
    - Lectura 25.5 grados C: actuador se apaga, log muestra "Temperatura normalizada (25.5 C < 26.0 C). Desactivando aireador1".
  - Las reglas persisten en NVS tras reinicio.
  - Se pueden tener multiples reglas activas simultaneamente sin conflicto.
  - Si el actuador ya esta controlado por otra regla, logear un warning (conflicto de actuadores).

- **Dependencias**: T3.6.1 completada (control de actuadores), al menos un sensor integrado (T3.1.2 o similar).

- **Pistas**:
  - Para NVS con structs: `nvs_set_blob(handle, "alert_rules", rules, sizeof(alert_rule_t) * count)`.
  - La histeresis se implementa simplemente con el par `threshold_high`/`threshold_low` y el flag `alert_active`. Sin el flag de estado, no se sabe si hay que activar o desactivar.
  - Para pruebas sin hardware, se pueden enviar lecturas simuladas via UART o API REST.

- **Errores comunes**:
  1. **No implementar histeresis (usar un solo umbral)**: Sin histeresis, el actuador oscilara rapidamente entre encendido y apagado cuando la lectura este cerca del umbral. Esto desgasta el rele, genera ruido electrico y no es efectivo. La banda muerta entre threshold_high y threshold_low es esencial.
  2. **No persistir el estado de la alerta tras reinicio**: Si el ESP32 se reinicia mientras una alerta esta activa, al arrancar no sabe si el actuador deberia estar encendido o apagado. Solucion: al arrancar, todos los actuadores en apagado (estado seguro), y recalcular el estado de las alertas con la primera lectura que llegue.

- **Tiempo estimado**: 5-6 horas

---

**Checkpoint 3.6**: Al completar estas 2 tareas, tienes un sistema completo de alerta y actuacion. Configura una regla de temperatura con histeresis. Simula lecturas crecientes y decrecientes y observa como el actuador se enciende y apaga respetando los umbrales y la banda muerta. Verifica que las reglas persisten tras reinicio y que el endpoint REST permite consultarlas y modificarlas.

---

## Checkpoint final de la Fase 3

Al completar todas las tareas de esta fase, debes tener:

1. **5 tipos de sensores funcionando**: temperatura (DS18B20), pH (ADC calibrado), oxigeno disuelto (ADC con compensacion de temperatura), ORP (ADC), nivel de agua (ultrasonico).
2. **Calibracion**: pH calibrable con 2 puntos (soluciones buffer), DO calibrable al aire, ORP con offset ajustable. Coeficientes persistidos en NVS.
3. **Calidad de datos**: Validacion por rango y filtro de media recortada. Las lecturas enviadas al gateway son limpias y fiables.
4. **Actuadores**: Control de reles por GPIO, accesible via API REST.
5. **Alertas**: Sistema de alertas por umbral con histeresis. Configuracion persistida en NVS.

**Prueba integral**: Un nodo con sensor de temperatura envia lecturas al gateway. El gateway tiene configurada una regla de alerta: "si temperatura > 30 grados C, activar rele del aireador; desactivar cuando baje de 28 grados C". Calienta el sensor (con la mano o agua tibia) hasta superar 30 grados C y verifica que el rele se activa. Deja enfriar y verifica que se desactiva al bajar de 28 grados C.

---

## Resumen de tiempos estimados

| Sub-tarea | Tareas | Tiempo estimado |
|-----------|--------|-----------------|
| 3.1 Driver DS18B20 | T3.1.1 + T3.1.2 | 6-8 horas |
| 3.2 Driver pH ADC | T3.2.1 + T3.2.2 + T3.2.3 | 10-13 horas |
| 3.3 Driver DO | T3.3.1 + T3.3.2 | 7-9 horas |
| 3.4 Driver ORP + Nivel | T3.4.1 + T3.4.2 | 5-7 horas |
| 3.5 Validacion y filtrado | T3.5.1 + T3.5.2 | 5-6 horas |
| 3.6 Actuadores y alertas | T3.6.1 + T3.6.2 | 8-10 horas |
| **Total** | **13 tareas** | **41-53 horas (~3 semanas a medio tiempo)** |

---

## Preguntas de autoevaluacion

Antes de dar la fase por completada, asegurate de poder responder estas preguntas:

1. **¿Por que es necesaria la resistencia pull-up de 4.7K ohm en el bus 1-Wire del DS18B20?** El protocolo 1-Wire usa un bus de drenaje abierto (open drain). Sin la resistencia pull-up, la linea de datos no puede volver al nivel alto, y la comunicacion falla.

2. **¿Que ventaja tiene usar la calibracion del ADC (`esp_adc_cal`) en lugar de escalar linealmente el valor crudo?** El ADC del ESP32 tiene no-linealidad inherente, especialmente en los extremos del rango. La calibracion usa las constantes de referencia almacenadas en eFuse de fabrica para corregir esta no-linealidad, mejorando la precision de ~100 mV a ~10 mV.

3. **¿Por que se necesitan 2 puntos de calibracion para el sensor de pH?** La relacion entre voltaje y pH es lineal (pH = a*V + b), pero tiene 2 incognitas (pendiente y offset). Con un solo punto no se pueden determinar ambas. Dos puntos definen univocamente la recta.

4. **¿Que sucede si no se compensa la temperatura al medir oxigeno disuelto?** Se sobreestima el oxigeno en agua caliente y se subestima en agua fria. A 30 grados C, la saturacion es ~7.5 mg/L, pero sin compensacion el calculo asume la saturacion de la temperatura de calibracion. Esto puede llevar a no detectar niveles peligrosamente bajos de oxigeno.

5. **¿Cual es la diferencia entre media movil simple y media recortada (trimmed mean)?** La media movil simple promedia todas las lecturas y es sensible a outliers. La media recortada descarta los valores extremos antes de promediar, haciendola mucho mas robusta ante picos o errores esporadicos del sensor.

6. **¿Por que es importante la histeresis en el control de actuadores?** Sin histeresis, cuando la lectura oscila naturalmente alrededor del umbral (ej. 27.9 - 28.1 grados C), el actuador se encendera y apagara continuamente (chattering). Esto desgasta el rele, genera picos de corriente y no logra el efecto deseado.

7. **¿Que pasa si el ESP32-C3 intenta usar ADC2?** El ESP32-C3 solo tiene ADC1 (canales ADC1_CH0 a ADC1_CH4). ADC2 no existe en este chip. Intentar configurar un canal de ADC2 retornara un error de inicializacion.

8. **¿Por que se deben inicializar los reles en estado "apagado" al arrancar el sistema?** Al reiniciar, el estado de los GPIOs es indeterminado. Si un rele se activa accidentalmente durante el arranque, podria encender una bomba o un calentador sin supervision. Inicializar en apagado es una medida de seguridad fundamental.

---

## Lectura recomendada

- **ESP-IDF ADC Oneshot Mode**: [Documentacion oficial ESP-IDF - ADC Oneshot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/adc_oneshot.html) - Referencia completa de la API ADC para ESP-IDF v5.x.
- **ESP-IDF ADC Calibration**: [Documentacion oficial ESP-IDF - ADC Calibration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/adc_calibration.html) - Como usar la calibracion del ADC para obtener lecturas de voltaje precisas.
- **Datasheet DS18B20**: [Maxim DS18B20](https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf) - Especificaciones del sensor de temperatura, protocolo 1-Wire, comandos ROM y funcion.
- **Teoria de medicion de pH**: Principio de la celda galvanica, electrodo de referencia, relacion de Nernst. Cualquier libro de quimica analitica instrumental cubre los fundamentos.
- **Datasheet HC-SR04**: Sensor ultrasonico de distancia. Tiempos de trigger, rango de medicion (2-400 cm), velocidad del sonido.
- **ESP-IDF NVS (Non-Volatile Storage)**: [Documentacion oficial ESP-IDF - NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/storage/nvs_flash.html) - API para almacenar datos persistentes en flash.
- **Principios de control on-off con histeresis**: Cualquier libro de introduccion a sistemas de control explica el concepto de banda muerta y por que es necesaria para evitar oscilaciones.

---

## Errores frecuentes de toda la fase (resumen)

1. **Olvidar la resistencia pull-up de 4.7K ohm en el DS18B20**: Es el error mas comun. Sin ella, el sensor no responde. Es un problema de hardware que no tiene solucion por software.

2. **No calibrar el ADC del ESP32**: El ADC del ESP32 tiene una no-linealidad significativa. Sin calibracion, las lecturas de voltaje pueden tener errores de hasta 100 mV, lo que se traduce en errores importantes en pH y DO.

3. **No compensar la temperatura al medir oxigeno disuelto**: La concentracion de saturacion de O2 varia un ~3% por cada grado Celsius. Sin compensacion, las lecturas pueden ser peligrosamente inexactas.

4. **Bouncing de reles (rebote)**: Los reles mecanicos pueden "rebotar" al conmutar, creando pulsos rapidos. Para equipos sensibles, agregar un delay minimo entre conmutaciones (ej. 500 ms) y nunca conmutar mas de una vez por segundo.

5. **No implementar histeresis en las alertas**: Sin histeresis, el actuador oscila cuando la lectura esta cerca del umbral. Esto desgasta el rele, genera ruido electrico y no logra controlar efectivamente el parametro.

6. **Conectar el pin echo del HC-SR04 directamente al ESP32**: El HC-SR04 trabaja a 5V. Su pin echo devuelve 5V, que puede dañar el GPIO de 3.3V del ESP32. Siempre usar un divisor de voltaje.

7. **No manejar errores de lectura de sensores**: Un sensor desconectado, un cable roto o una sonda sucia pueden producir lecturas erroneas. Sin validacion de rango ni filtrado, estos valores erroneos llegan al gateway y pueden activar alertas falsas o, peor, no activar alertas cuando deberian.

8. **No persistir la calibracion en NVS**: Si los coeficientes de calibracion se pierden al reiniciar, hay que recalibrar cada vez que el sistema arranca. En una instalacion de acuicultura, esto es impractico. Siempre guardar los coeficientes en NVS.
