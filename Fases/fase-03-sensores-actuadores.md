# Fase 3: Sensores y Actuadores

> **CORRECCIONES APLICADAS** (2026-05-23): sensor_manager.c ya existe desde T2.1.4 (Fase 2). Filtrado de lecturas corregido: multiples lecturas en UN wake cycle (no entre wakes). Nombres de enum consistentes con protocolo unificado. Ver detalles en `docs/replanificacion/03-fases-corregidas.md`.
>
> **REESTRUCTURACION IIoT** (2026-05-23): Esta fase ahora usa el DS18B20 como ejemplo de sensor universal. Los sensores especificos de dominio (pH, oxigeno disuelto, ORP, nivel ultrasonico) se han movido a `examples/fish-farm/sensors.md`. Ver nota de referencia al final de la seccion de sub-tareas.

## Resumen general

- **Objetivo**: Implementar el driver para el sensor de temperatura DS18B20 (ejemplo universal de sensor digital), sistema de validacion y filtrado de lecturas, y control de actuadores con alertas. El DS18B20 demuestra todos los patrones que se aplican a cualquier tipo de sensor en una plataforma IIoT.
- **Duracion estimada**: 2 semanas
- **Prerequisitos**: Fase 0 (Entorno), Fase 1 (Firmware base) y Fase 2 (Comunicaciones) completadas
- **Hardware necesario**:
  - Sensor de temperatura DS18B20 (con resistencia pull-up de 4.7K ohm)
  - Modulos de rele (1 o 2 canales)
  - ESP32-C3 (nodos sensores)
  - ESP32-S3 (gateway)
  - Protoboard, cables dupont, fuente de alimentacion

## Dependencias externas

- ESP-IDF v5.x
- Componente `onewire_bus` del ESP-IDF Component Registry
- Datasheet DS18B20

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
    main/
      sensor_manager.c   (modificar)
      sensor_manager.h   (modificar)
      sensor_filter.c    (crear)
      sensor_filter.h    (crear)
  gateway/
    main/
      actuator_ctrl.c
      actuator_ctrl.h
      alert_system.c
      alert_system.h
```

---

## Sub-tarea 3.1: Driver de temperatura DS18B20

El DS18B20 es un sensor de temperatura digital que se comunica por el protocolo 1-Wire. Un solo pin de datos puede conectar multiples sensores en el mismo bus. Es el sensor de referencia de esta fase: preciso (+/- 0.5 grados C), resistente al agua cuando viene encapsulado en tubo de acero inoxidable, y muy extendido en aplicaciones IIoT industriales y de proceso.

> **Por que DS18B20 como ejemplo universal**: Este sensor ilustra todos los patrones que se repiten en cualquier sensor industrial — inicializacion del driver, lectura de datos, integracion en el ciclo del nodo, y manejo de errores. Una vez que domines este patron con el DS18B20, podras aplicarlo a cualquier sensor de tu dominio especifico.

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
  3. En la funcion de inicializacion del sensor manager, verificar el tipo de sensor configurado. Si `sensor_type == SENSOR_TYPE_TEMPERATURE`, llamar a `ds18b20_init(pin_configurado)`.
  4. En la funcion de lectura del sensor manager, si el tipo es temperatura, llamar a `ds18b20_read_temp(&valor)`.
  5. Empaquetar el valor de temperatura en el payload ESP-NOW existente (estructura de datos definida en Fase 2). Asegurar que el campo `sensor_type` indique `SENSOR_TYPE_TEMPERATURE` y el campo `values[0]` contenga el float de temperatura.
  6. En el gateway, verificar que al recibir el paquete ESP-NOW se decodifica correctamente y se muestra la temperatura en el log.

- **Archivos a crear/modificar**:
  - Modificar: `firmware/node/main/sensor_manager.c`
  - Modificar: `firmware/node/main/sensor_manager.h` (agregar SENSOR_TYPE_TEMPERATURE si no existe)

- **Criterio de aceptacion**:
  - El nodo envia paquetes ESP-NOW que contienen la temperatura real leida del DS18B20.
  - El gateway recibe el paquete, decodifica el valor y lo muestra en el monitor serie: `"Nodo XX: Temperatura = 24.5 C"`.
  - La temperatura que muestra el gateway coincide con la lectura directa del sensor (verificar con termometro si se dispone de uno).

- **Dependencias**: T3.1.1 completada, Fase 2 completada (comunicacion ESP-NOW funcionando).

- **Pistas**:
  - Reusar la estructura de payload ESP-NOW de la Fase 2. Asignar el valor de temperatura a `values[0]` y establecer `value_count = 1`.
  - Si se usa deep sleep, recordar que `ds18b20_init()` debe llamarse en cada ciclo de wake, ya que el periferico se pierde al dormir.

- **Errores comunes**:
  1. **No reinicializar el bus 1-Wire tras deep sleep**: El ESP32 pierde el estado de los GPIOs al entrar en deep sleep. Hay que volver a llamar `ds18b20_init()` cada vez que se despierta.
  2. **Enviar el float con endianness diferente entre nodo y gateway**: Ambos ESP32 son little-endian, pero si se serializa manualmente, asegurarse de que el orden de bytes es consistente. Recomendacion: usar `memcpy` para copiar el float al buffer.

- **Tiempo estimado**: 2-3 horas

---

**Checkpoint 3.1**: Al completar estas 2 tareas, debes poder ver en el monitor serie del gateway la temperatura real del entorno monitorizado leida por un nodo remoto via ESP-NOW. Toca el sensor con la mano y confirma que el valor sube. Desconecta el sensor y confirma que el nodo maneja el error sin bloquearse.

---

> **Nota sobre sensores especificos de dominio**: Para implementar sensores especificos del dominio como pH, oxigeno disuelto o nivel ultrasonico, consulta los ejemplos en `examples/fish-farm/sensors.md`. Esos ejemplos siguen exactamente el mismo patron de driver demostrado en la sub-tarea 3.1 con el DS18B20.

---

## Sub-tarea 3.5: Validacion y filtrado de lecturas

Los sensores del mundo real producen lecturas ruidosas y a veces erroneas (picos, valores fuera de rango, lecturas NaN). Antes de enviar los datos al gateway, es necesario validar y filtrar para asegurar la calidad de los datos. Este pipeline de calidad es independiente del tipo de sensor y se aplica igual a temperatura, pH, DO, o cualquier otro sensor de tu instalacion.

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
         [SENSOR_TYPE_TEMPERATURE] = { .min_value = -40.0,   .max_value = 85.0   },  // grados C (rango DS18B20)
         [SENSOR_TYPE_PH]          = { .min_value = 0.0,     .max_value = 14.0   },  // unidades pH
         [SENSOR_TYPE_DO]          = { .min_value = 0.0,     .max_value = 20.0   },  // mg/L
         [SENSOR_TYPE_ORP]         = { .min_value = -2000.0, .max_value = 2000.0 },  // mV
         [SENSOR_TYPE_LEVEL]       = { .min_value = 2.0,     .max_value = 400.0  },  // cm
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

- **Dependencias**: Al menos uno de los drivers de sensor completado (T3.1.1 o similar).

- **Pistas**:
  - Usar `isnan()` o `isinf()` de `<math.h>` para detectar valores NaN o infinito que pueden resultar de divisiones por cero en los drivers.
  - Los rangos pueden hacerse configurables por NVS en el futuro, pero por ahora constantes esta bien.

- **Errores comunes**:
  1. **No verificar NaN/Inf**: Un sensor que falla puede producir un valor `NaN` o `Inf` al hacer calculos (ej. division por cero en la compensacion de DO). Estos valores pasan cualquier comparacion numerica de forma impredecible. Siempre verificar con `isnan()` e `isinf()` antes de comparar con el rango.
  2. **Rechazar lecturas validas por rangos demasiado estrechos**: En entornos industriales, los valores pueden acercarse a los limites del sensor. Usar rangos lo suficientemente amplios para cubrir todos los escenarios reales del sistema monitorizado.

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

El monitoreo sin accion no es suficiente. Cuando los parametros del entorno monitorizado estan fuera de limites seguros, el sistema debe poder activar actuadores (bombas, calentadores, ventiladores, aireadores) automaticamente. Este modulo es completamente generico: la misma logica de rele y de alertas con histeresis aplica independientemente del dominio de la instalacion.

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
         char name[16];       // nombre descriptivo (ej. "actuador1")
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
  3. Agregar endpoints REST en el gateway:
     - `GET /api/actuators`: Lista todos los actuadores y su estado.
     - `POST /api/actuators/{index}`: Cuerpo `{"on": true/false}`. Enciende o apaga el actuador indicado.
  4. Probar: conectar un LED o un rele a un GPIO, llamar al endpoint REST y verificar que se enciende/apaga.

- **Archivos a crear/modificar**:
  - Crear: `firmware/gateway/main/actuator_ctrl.h`
  - Crear: `firmware/gateway/main/actuator_ctrl.c`
  - Modificar: `firmware/gateway/main/http_server.c` (agregar endpoints REST para actuadores)

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
  1. **No inicializar el GPIO en estado apagado al arrancar**: Si el GPIO queda en un estado indeterminado, el rele podria activarse brevemente durante el arranque. Esto puede ser peligroso si controla una bomba o un calentador. Siempre poner el estado "apagado" inmediatamente al inicializar.
  2. **Confundir logica activo-bajo/activo-alto**: Muchos modulos de rele economicos se activan con nivel bajo (0V). Si se asume activo-alto, al poner GPIO HIGH se apaga el rele (lo opuesto de lo esperado). Siempre verificar la logica del modulo de rele usado.

- **Tiempo estimado**: 3-4 horas

---

### Tarea T3.6.2: Sistema de alertas por umbral con histeresis

- **Dificultad**: Avanzado
- **Descripcion**:
  1. Entender la histeresis: Si se activa un actuador cuando la temperatura sube a 28 grados C y se desactiva cuando baja de 28 grados C, el actuador oscilara constantemente (encendido-apagado-encendido cada pocos segundos) cuando la temperatura este alrededor de 28 grados C. Con histeresis, se activa a 28 grados C pero no se desactiva hasta que baje a 26 grados C. Esto crea una "banda muerta" que evita la oscilacion.
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
     - `alert_process_reading()`: Logica central con histeresis. Para cada regla que aplique al sensor/nodo:
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
  5. Ejemplo de configuracion de reglas:
     - Temperatura alta: activar ventilador/sistema de enfriamiento si >28 grados C, desactivar si <26 grados C.
     - Temperatura baja: activar calentador si <15 grados C, desactivar si >17 grados C.
  6. Agregar endpoint REST para configurar reglas:
     - `GET /api/alerts`: Lista todas las reglas.
     - `POST /api/alerts`: Crea una nueva regla.
     - `DELETE /api/alerts/{index}`: Elimina una regla.

- **Archivos a crear/modificar**:
  - Crear: `firmware/gateway/main/alert_system.h`
  - Crear: `firmware/gateway/main/alert_system.c`
  - Modificar: `firmware/gateway/main/main.c` (llamar a `alert_process_reading` al recibir datos)
  - Modificar: `firmware/gateway/main/http_server.c` (agregar endpoints REST para alertas)

- **Criterio de aceptacion**:
  - Con una regla de temperatura (activar a 28, desactivar a 26), al enviar lecturas simuladas:
    - Lectura 27 grados C: nada sucede.
    - Lectura 28.5 grados C: actuador se enciende, log muestra "ALERTA: Temperatura alta (28.5 C > 28.0 C). Activando actuador1".
    - Lectura 27 grados C: nada sucede (histeresis, el actuador sigue encendido).
    - Lectura 25.5 grados C: actuador se apaga, log muestra "Temperatura normalizada (25.5 C < 26.0 C). Desactivando actuador1".
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

1. **Driver DS18B20**: Sensor de temperatura digital funcionando via 1-Wire, integrado en el ciclo ESP-NOW del nodo.
2. **Calidad de datos**: Validacion por rango y filtro de media recortada. Las lecturas enviadas al gateway son limpias y fiables.
3. **Actuadores**: Control de reles por GPIO, accesible via API REST.
4. **Alertas**: Sistema de alertas por umbral con histeresis. Configuracion persistida en NVS.

**Prueba integral**: Un nodo con sensor DS18B20 envia lecturas al gateway. El gateway tiene configurada una regla de alerta: "si temperatura > 30 grados C, activar rele del actuador; desactivar cuando baje de 28 grados C". Calienta el sensor (con la mano o agua tibia) hasta superar 30 grados C y verifica que el rele se activa. Deja enfriar y verifica que se desactiva al bajar de 28 grados C.

---

## Resumen de tiempos estimados

| Sub-tarea | Tareas | Tiempo estimado |
|-----------|--------|-----------------|
| 3.1 Driver DS18B20 | T3.1.1 + T3.1.2 | 6-8 horas |
| 3.5 Validacion y filtrado | T3.5.1 + T3.5.2 | 5-6 horas |
| 3.6 Actuadores y alertas | T3.6.1 + T3.6.2 | 8-10 horas |
| **Total** | **6 tareas** | **19-24 horas (~2 semanas a medio tiempo)** |

---

## Preguntas de autoevaluacion

Antes de dar la fase por completada, asegurate de poder responder estas preguntas:

1. **¿Por que es necesaria la resistencia pull-up de 4.7K ohm en el bus 1-Wire del DS18B20?** El protocolo 1-Wire usa un bus de drenaje abierto (open drain). Sin la resistencia pull-up, la linea de datos no puede volver al nivel alto, y la comunicacion falla.

2. **¿Cual es la diferencia entre media movil simple y media recortada (trimmed mean)?** La media movil simple promedia todas las lecturas y es sensible a outliers. La media recortada descarta los valores extremos antes de promediar, haciendola mucho mas robusta ante picos o errores esporadicos del sensor.

3. **¿Por que es importante la histeresis en el control de actuadores?** Sin histeresis, cuando la lectura oscila naturalmente alrededor del umbral (ej. 27.9 - 28.1 grados C), el actuador se encendera y apagara continuamente (chattering). Esto desgasta el rele, genera picos de corriente y no logra el efecto deseado.

4. **¿Por que se deben inicializar los reles en estado "apagado" al arrancar el sistema?** Al reiniciar, el estado de los GPIOs es indeterminado. Si un rele se activa accidentalmente durante el arranque, podria encender una bomba o un calentador sin supervision. Inicializar en apagado es una medida de seguridad fundamental.

5. **¿Que pasa si el ESP32-C3 intenta usar ADC2?** El ESP32-C3 solo tiene ADC1 (canales ADC1_CH0 a ADC1_CH4). ADC2 no existe en este chip. Intentar configurar un canal de ADC2 retornara un error de inicializacion.

---

## Lectura recomendada

- **Datasheet DS18B20**: [Maxim DS18B20](https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf) - Especificaciones del sensor de temperatura, protocolo 1-Wire, comandos ROM y funcion.
- **ESP-IDF ADC Oneshot Mode**: [Documentacion oficial ESP-IDF - ADC Oneshot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/adc_oneshot.html) - Referencia completa de la API ADC para ESP-IDF v5.x.
- **ESP-IDF ADC Calibration**: [Documentacion oficial ESP-IDF - ADC Calibration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/adc_calibration.html) - Como usar la calibracion del ADC para obtener lecturas de voltaje precisas.
- **ESP-IDF NVS (Non-Volatile Storage)**: [Documentacion oficial ESP-IDF - NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/storage/nvs_flash.html) - API para almacenar datos persistentes en flash.
- **Principios de control on-off con histeresis**: Cualquier libro de introduccion a sistemas de control explica el concepto de banda muerta y por que es necesaria para evitar oscilaciones.

---

## Errores frecuentes de toda la fase (resumen)

1. **Olvidar la resistencia pull-up de 4.7K ohm en el DS18B20**: Es el error mas comun. Sin ella, el sensor no responde. Es un problema de hardware que no tiene solucion por software.

2. **No reinicializar el bus 1-Wire tras deep sleep**: El ESP32 pierde el estado de los perifericos al entrar en deep sleep. Hay que llamar `ds18b20_init()` en cada ciclo de wake.

3. **Bouncing de reles (rebote)**: Los reles mecanicos pueden "rebotar" al conmutar, creando pulsos rapidos. Para equipos sensibles, agregar un delay minimo entre conmutaciones (ej. 500 ms) y nunca conmutar mas de una vez por segundo.

4. **No implementar histeresis en las alertas**: Sin histeresis, el actuador oscila cuando la lectura esta cerca del umbral. Esto desgasta el rele, genera ruido electrico y no logra controlar efectivamente el parametro.

5. **No manejar errores de lectura de sensores**: Un sensor desconectado, un cable roto o una sonda sucia pueden producir lecturas erroneas. Sin validacion de rango ni filtrado, estos valores erroneos llegan al gateway y pueden activar alertas falsas o, peor, no activar alertas cuando deberian.
