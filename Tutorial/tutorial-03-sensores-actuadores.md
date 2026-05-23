# Tutorial 3: Midiendo el Mundo Real

## Que vas a aprender

- Diferencia entre sensores analogicos y digitales, y cuando usar cada tipo.
- Como funciona el protocolo 1-Wire que usa el DS18B20 para medir temperatura.
- Por que el ADC del ESP32 no es preciso "de fabrica" y como calibrarlo para obtener lecturas fiables.
- Tecnicas de filtrado de senal (media movil, mediana, media recortada) para limpiar lecturas ruidosas.
- Como controlar actuadores de potencia (reles, bombas, ventiladores) de forma segura desde un GPIO de 3.3V.
- Que es la histeresis y por que los umbrales de activacion y desactivacion deben ser diferentes.

---

## Conceptos clave

### Sensores analogicos vs digitales

En el mundo de los sensores existen dos grandes familias, y la diferencia fundamental esta en como le comunican la informacion al microcontrolador.

Un **sensor analogico** produce un voltaje continuo que es proporcional a lo que mide. Una sonda de pH, por ejemplo, genera un voltaje que sube o baja segun la acidez del liquido. Un sensor de oxigeno disuelto produce otro voltaje proporcional a la concentracion de oxigeno. En todos estos casos, la salida es una senal electrica que tu tienes que interpretar.

Un **sensor digital** hace el trabajo de interpretacion por ti. Dentro del propio sensor hay un pequeno circuito que mide, convierte y empaqueta el dato en un formato estructurado. El DS18B20 (temperatura) usa el protocolo 1-Wire. El BMP280 (presion atmosferica) usa I2C. Cuando le preguntas al sensor "que temperatura hay?", te responde con un numero ya listo para usar.

    Sensor analogico                    Sensor digital
    ================                    ===============

    Fenomeno fisico                     Fenomeno fisico
         |                                   |
         v                                   v
    Voltaje continuo                    Circuito interno
    (0.3V ... 2.8V)                    (mide, convierte, empaqueta)
         |                                   |
         v                                   v
    ADC del ESP32                       Protocolo (1-Wire, I2C...)
    (convierte a numero)                     |
         |                                   v
         v                              Dato formateado
    Numero crudo                        (ej: "24.5 grados")
    (necesita calibracion)

Las ventajas y desventajas de cada tipo son claras:

Los sensores analogicos son susceptibles al ruido electrico (un cable largo actuando como antena puede corromper la senal), necesitan un ADC para convertir el voltaje a un numero, y ese ADC introduce sus propios errores. Ademas requieren calibracion para mapear el voltaje a unidades reales. Pero tienen la ventaja de la sencillez: son baratos, hay enorme variedad disponible y puedes elegir tu propia velocidad de muestreo.

Los sensores digitales producen una senal mucho mas limpia porque la conversion se hace dentro del propio sensor, cerca de la fuente de la medicion. Muchos vienen calibrados de fabrica. Pero estas atado a su protocolo y sus tiempos: si el DS18B20 necesita 750 milisegundos para hacer una conversion a 12 bits, no puedes apurarlo.

**Analogia**: Un sensor analogico es como un termometro de mercurio. Tu ves la columna de mercurio y tienes que interpretar donde cae entre las marcas. Si miras desde un angulo raro, lees mal (error de paralaje = ruido). Un sensor digital es como un termometro digital de farmacia: pulsas un boton y te muestra "37.2" en la pantalla. El numero ya esta listo, pero no puedes cambiar la velocidad a la que mide.

En esta fase, el DS18B20 es el ejemplo universal. Es el sensor mas ensenado en proyectos IIoT porque ilustra perfectamente todos los patrones que luego se repiten en cualquier sensor digital: inicializacion del bus, descubrimiento de dispositivos, lectura, conversion de datos, y manejo de errores. Una vez que dominas el patron con el DS18B20, puedes aplicarlo a cualquier sensor de tu dominio.

> **Sensores especificos de dominio**: Si tu instalacion requiere sensores analogicos avanzados como pH, oxigeno disuelto, ORP, o nivel por ultrasonidos, consulta los ejemplos en `examples/fish-farm/` donde se muestra como aplicar estos patrones a sensores de mayor complejidad.

---

### El protocolo 1-Wire

El DS18B20 es probablemente el sensor de temperatura mas popular del mundo para proyectos con microcontroladores, y con razon: es resistente al agua cuando viene encapsulado, preciso (0.5 grados de exactitud de fabrica), barato y usa un protocolo que solo necesita un cable de datos.

El protocolo se llama 1-Wire ("un cable") porque toda la comunicacion ocurre a traves de un unico hilo. En la practica necesitas dos conexiones: el cable de datos y tierra (GND). Existe incluso un modo llamado "alimentacion parasita" donde el sensor toma la energia del propio cable de datos, eliminando la necesidad de un tercer cable de alimentacion. Aunque para instalaciones fijas, es mas fiable usar los tres cables (datos, VCC y GND).

    ESP32                         DS18B20 #1        DS18B20 #2
    +-------+                     +--------+        +--------+
    |       |                     |        |        |        |
    | GPIO4 |-----+-------+------| DQ     |--------| DQ     |
    |       |     |       |      |        |        |        |
    | 3.3V  |-----+  [4.7K]     | VDD    |--------| VDD    |----> 3.3V
    |       |         |         |        |        |        |
    | GND   |---------+---------| GND    |--------| GND    |----> GND
    +-------+                    +--------+        +--------+
                  Resistencia
                  pull-up
                  (esencial)

La **resistencia pull-up de 4.7K ohmios** es absolutamente esencial. El bus 1-Wire funciona con logica de "drenador abierto": los dispositivos solo pueden tirar la linea hacia abajo (poner a 0V). La resistencia pull-up es la que tira la linea hacia arriba (3.3V) cuando nadie la esta forzando abajo. Sin ella, la linea nunca vuelve al estado alto y la comunicacion es imposible. Es como una puerta con muelle: alguien puede empujarla para abrirla, pero sin el muelle nunca volveria a cerrarse sola.

**Direccionamiento unico**: Cada DS18B20 sale de fabrica con un numero de serie unico de 64 bits grabado de forma permanente en su memoria. Esto significa que puedes conectar multiples sensores al mismo cable y dirigirte a cada uno individualmente. El ESP32 (el "maestro" del bus) puede enviar un comando de descubrimiento para encontrar todos los dispositivos conectados y luego hablar con cada uno por su direccion.

**El proceso de lectura paso a paso**:

    1. El ESP32 envia un pulso de "reset" (tira la linea abajo 480 microsegundos)
    2. Los DS18B20 responden con un pulso de "presencia" (confirman que estan ahi)
    3. El ESP32 envia el comando "iniciar conversion de temperatura"
    4. El DS18B20 necesita tiempo para convertir (hasta 750ms a 12 bits de resolucion)
    5. El ESP32 envia el comando "leer resultado"
    6. El DS18B20 transmite los bytes con la temperatura codificada

Esos 750 milisegundos de conversion son un detalle importante para el diseno del firmware. Si tienes una tarea que lee temperatura cada segundo, no puedes bloquearte 750ms esperando. La solucion es enviar el comando de conversion, ir a hacer otra cosa (leer otro sensor, procesar datos) y volver a recoger el resultado cuando ya este listo.

**Analogia**: Imagina un canal de radio compartido (como un walkie-talkie) donde cada persona tiene un indicativo unico. El ESP32 es la base que dice "Alfa-Bravo-Charlie-123, reporta tu temperatura". Solo el sensor con ese indicativo responde. Los demas permanecen en silencio. Todos comparten la misma frecuencia (el mismo cable), pero no hay confusion porque cada uno tiene su identidad unica.

---

### ADC: el conversor analogico-digital en profundidad

Ya conoces el concepto basico del ADC del Tutorial 0: convierte un voltaje en un numero. Ahora vamos a profundizar en por que las lecturas crudas del ADC del ESP32 no son de fiar y que hacer al respecto.

El ADC del ESP32 tiene una resolucion de 12 bits. Eso significa que puede representar 2^12 = 4096 niveles diferentes, del 0 al 4095. En teoria, si el rango de medida es 0 a 3.3V, cada incremento de 1 en la lectura corresponde a unos 0.8 milivoltios. En teoria.

En la practica, el ADC del ESP32 tiene problemas conocidos y bien documentados:

**No-linealidad**: La relacion entre voltaje real y valor digital no es perfectamente recta. Es como si las marcas de una regla estuvieran mas juntas en unos sitios y mas separadas en otros. Esta no-linealidad es peor en los extremos del rango (cerca de 0V y cerca de 3.3V) y relativamente mejor en la zona media.

    Valor ADC ideal (linea recta)        Valor ADC real (curva)
    4095|          /                      4095|          _..--
        |        /                            |      _.-'
        |      /                              |   .-'
        |    /                                | .'
        |  /                                  |.'
        |/                                    |
      0 +----------> Voltaje               0  +----------> Voltaje
        0V      3.3V                          0V      3.3V

        Cada marca esta               Las marcas no estan
        perfectamente espaciada        uniformemente espaciadas

**Variaciones entre chips**: Dos ESP32 del mismo lote de fabricacion pueden dar lecturas diferentes para el mismo voltaje de entrada. Es una consecuencia del proceso de fabricacion de semiconductores: hay pequenas variaciones inevitables.

**Atenuacion**: Nativamente, el ADC del ESP32 solo puede medir voltajes entre 0 y aproximadamente 1.1V. Si tu sensor produce 2.5V, necesitas "atenuar" esa senal para que quepa en el rango del ADC. ESP-IDF ofrece cuatro niveles de atenuacion:

    +----------------+--------------------+----------------------------+
    | Atenuacion     | Rango aproximado   | Cuando usarla              |
    +----------------+--------------------+----------------------------+
    | 0 dB (ninguna) | 0 - 1.1V          | Senales muy pequenas       |
    | 2.5 dB         | 0 - 1.5V          | Senales pequenas           |
    | 6 dB           | 0 - 2.2V          | Rango intermedio           |
    | 11 dB          | 0 - 3.3V          | El mas comun, rango maximo |
    +----------------+--------------------+----------------------------+

Pero aqui esta la trampa: con atenuacion de 11 dB, aunque el rango teorico llega a 3.3V, la precision se degrada significativamente por encima de 2.6V aproximadamente. La zona "fiable" es mas pequena de lo que parece.

**La solucion: calibracion por eFuse y esp_adc_cal**

Espressif es consciente de estos problemas. Por eso, durante la fabricacion, graban datos de calibracion en una zona especial del chip llamada eFuse (fusibles electronicos, datos que se escriben una sola vez y no se pueden borrar). Estos datos de calibracion capturan las particularidades de ese chip concreto.

La API de calibracion del ADC (disponible en ESP-IDF) usa esos datos de eFuse para corregir las lecturas. En vez de darte un valor crudo del 0 al 4095, te da una lectura en milivoltios ya compensada por las no-linealidades y variaciones de tu chip concreto.

    Sin calibracion:  GPIO lee 2048  -->  "Sera 1.65V? Puede que si, puede que no"
    Con calibracion:  GPIO lee 2048  -->  API calcula --> "1.612 mV"  (valor real corregido)

**Regla de oro**: Nunca uses valores crudos del ADC para tomar decisiones. Siempre pasa por la API de calibracion. En un sistema de monitoreo industrial donde un error de medicion puede desencadenar una alerta falsa o suprimir una alerta real, la calibracion del ADC no es opcional.

**Analogia**: El ADC es como una regla de madera artesanal. El fabricante sabe que las marcas no estan perfectas, asi que incluye una hoja con correcciones: "cuando la regla dice 15 cm, en realidad son 14.8 cm". La API de calibracion es esa hoja de correcciones, y los datos de eFuse son las mediciones especificas de tu regla concreta.

---

### Filtrado de senal: limpiando lecturas ruidosas

Acabas de tomar una lectura del sensor. Obtienes 24.1 grados. Otra: 24.2 grados. Otra: 24.0. Otra: 99.9 grados. Otra: 24.3 grados. Los valores son estables, pero de vez en cuando aparece un valor disparatado que claramente no es real.

Esto es normal. Las lecturas de sensores siempre tienen ruido: interferencias electricas, fluctuaciones en la fuente de alimentacion, imprecision inherente del sensor. El ruido es inevitable; lo que puedes hacer es filtrarlo.

**Media movil (Moving Average)**

La tecnica mas sencilla. Mantienes un buffer con las ultimas N lecturas y calculas su promedio. Cada vez que llega una lectura nueva, entra en el buffer y la mas antigua sale.

    Lecturas entrantes:  24.1  24.2  99.9  24.3  24.0  24.2  24.1  24.3
                         ----  ----  ----  ----
                         Buffer de 4 lecturas

    Media movil (N=4):
    Paso 1: (24.1 + 24.2 + 99.9 + 24.3) / 4 = 43.1  <-- el outlier contamina
    Paso 2: (24.2 + 99.9 + 24.3 + 24.0) / 4 = 43.1  <-- sigue afectado

El problema es evidente: un solo valor anomalo (99.9) arrastra el promedio durante N pasos. La media movil es buena para suavizar fluctuaciones pequenas pero mala para rechazar valores extremos.

**Filtro de mediana (Median Filter)**

En vez de promediar, ordenas las ultimas N lecturas de menor a mayor y tomas la del centro.

    Lecturas en el buffer:  24.1  24.2  99.9  24.3  24.0
    Ordenadas:              24.0  24.1  24.2  24.3  99.9
                                         ^
                                      Mediana = 24.2

El outlier de 99.9 queda relegado a un extremo y no afecta al valor central. La mediana es excelente para rechazar picos aislados.

**Media recortada (Trimmed Mean) -- la que usamos en este proyecto**

Combina lo mejor de ambas tecnicas. Tomas N lecturas, descartas las mas altas y las mas bajas, y promedias el resto.

    Lecturas: 24.1  24.2  99.9  24.3  24.0
    Ordenadas: 24.0  24.1  24.2  24.3  99.9
               ^^^^                    ^^^^
               descarta               descarta
               (minimo)               (maximo)

    Media recortada = (24.1 + 24.2 + 24.3) / 3 = 24.2

El valor anomalo 99.9 fue descartado automaticamente. Y el suavizado del promedio se aplica sobre los valores restantes.

**Analogia**: Imagina que cinco personas estiman la temperatura de una sala. Cuatro dicen valores entre 22 y 24 grados. Una persona dice 100 grados (claramente esta bromeando). Si haces la media de los cinco, obtienes 31.2 grados, un disparate. Si descartas el valor mas alto y el mas bajo y promedias los tres centrales, obtienes algo alrededor de 23 grados, que es la realidad. Eso es una media recortada.

**Cual elegir depende de la situacion**: Para un sensor que necesita respuesta rapida (detector de fuga), la mediana es mejor. Para un sensor donde importa la precision a largo plazo (temperatura de proceso que cambia lentamente), la media recortada es ideal. En este proyecto, los sensores de proceso cambian lentamente, asi que la media recortada nos da estabilidad sin sacrificar precision.

---

### Reles y control de actuadores

Hasta ahora hemos hablado de medir. Pero un sistema de monitoreo que solo mide y no actua es como un detector de humo que suena pero no activa los aspersores. Cuando la temperatura sube, necesitas activar un sistema de enfriamiento. Cuando un nivel baja, necesitas arrancar una bomba. Eso requiere actuadores: dispositivos que hacen algo fisico en el mundo real.

El problema es que un GPIO del ESP32 produce 3.3V y puede suministrar unos 12 miliamperios como maximo. Un actuador industrial consume mucha mas corriente y a menudo mas voltaje. No puedes conectar el actuador directamente al ESP32. Necesitas un intermediario: el rele.

**Que es un rele**

Un rele es un interruptor controlado electricamente. Dentro tiene una bobina de cobre. Cuando pasa corriente por la bobina, genera un campo magnetico que mueve mecanicamente un contacto metalico, cerrando (o abriendo) un circuito externo.

    Circuito de control               Circuito de potencia
    (bajo voltaje, ESP32)             (alto voltaje, actuador)

    GPIO --> [Transistor] --> [Bobina del rele]
                                    |
                              [Contacto movil]
                                    |
                              /     |     \
                          NC   COM   NO
                                    |
                          220V ---- Actuador ---- 220V

    NC  = Normalmente Cerrado (conectado cuando el rele esta apagado)
    COM = Comun
    NO  = Normalmente Abierto (conectado cuando el rele esta encendido)

En la mayoria de aplicaciones, conectas el actuador entre COM y NO: asi el actuador esta apagado por defecto y se enciende cuando activas el rele. Es mas seguro: si el ESP32 se cuelga o pierde alimentacion, el rele se desactiva y el actuador se apaga.

**Por que necesitas un transistor u optoacoplador**

El GPIO del ESP32 no tiene suficiente corriente para activar la bobina del rele directamente (la bobina tipica necesita 70-100 mA). Necesitas un transistor que actue como amplificador de corriente: la pequena corriente del GPIO controla una corriente mayor que fluye hacia la bobina.

Los modulos de reles comerciales ya incluyen este transistor (y a menudo un optoacoplador). Un **optoacoplador** aisle electricamente el circuito del ESP32 del circuito del rele mediante luz: un LED interno ilumina un fototransistor. Asi, aunque el lado de potencia tenga un problema electrico, el ESP32 esta protegido porque no hay conexion electrica directa.

**El diodo flyback: proteccion esencial**

Una bobina de rele almacena energia en su campo magnetico mientras esta encendida. Cuando cortas la corriente (apagas el rele), esa energia tiene que ir a algun sitio. Se manifiesta como un pico de voltaje transitorio (llamado back-EMF o "fuerza contra-electromotriz") que puede alcanzar decenas o cientos de voltios durante microsegundos.

Ese pico puede destruir el transistor que controla la bobina, y por extension daniar el GPIO del ESP32. La solucion es un **diodo flyback** conectado en paralelo con la bobina, en sentido inverso. Cuando aparece el pico de back-EMF, el diodo conduce y disipa la energia de forma segura.

Los modulos de reles de buena calidad ya incluyen este diodo. Si usas un rele suelto, tienes que poner el diodo tu mismo.

**Active-HIGH vs Active-LOW**

Algunos modulos de rele se activan cuando el GPIO esta en HIGH (3.3V), otros cuando esta en LOW (0V). Si tu firmware asume lo contrario, el rele estara encendido cuando deberia estar apagado. Verifica siempre la polaridad de tu modulo y configurala correctamente en el firmware.

---

### Histeresis: evitando el efecto "ping-pong"

Imagina esta situacion: has configurado una alerta que dice "si la temperatura supera 28 grados, enciende el sistema de enfriamiento". Parece razonable. Pero observa que pasa en la practica:

    Temperatura real: oscila naturalmente entre 27.8 y 28.2 grados

    Segundo 0:  Temp = 27.9 C  -->  Sistema: APAGADO
    Segundo 1:  Temp = 28.1 C  -->  Sistema: ENCENDIDO
    Segundo 2:  Temp = 27.9 C  -->  Sistema: APAGADO   (se enfrio un poco)
    Segundo 3:  Temp = 28.0 C  -->  Sistema: ENCENDIDO
    Segundo 4:  Temp = 27.8 C  -->  Sistema: APAGADO
    ...

El actuador se enciende y apaga cada pocos segundos. Esto se llama "chattering" o "efecto ping-pong". Es terrible por varias razones:

- **Desgaste mecanico**: Los contactos del rele se desgastan con cada conmutacion. Un rele tiene una vida util medida en numero de conmutaciones (tipicamente 100.000). Si conmuta cada segundo, dura poco mas de un dia.
- **Estres electrico**: Arrancar un motor consume mucha mas corriente que mantenerlo girando. Arranques frecuentes pueden quemar el motor.
- **Ruido electrico**: Cada conmutacion genera interferencias que pueden afectar las lecturas de los sensores.

**La solucion: histeresis**

En vez de un solo umbral, usas dos:

    Umbral de activacion:    28.0 C  (enciende el sistema de enfriamiento)
    Umbral de desactivacion: 26.0 C  (apaga el sistema de enfriamiento)

    La diferencia (2.0 C) es la "banda de histeresis"

El comportamiento ahora es:

    Temp sube:   25 -> 26 -> 27 -> 28 --> ENCIENDE el sistema de enfriamiento
    Temp baja:   28 -> 27 -> 26 --> APAGA el sistema de enfriamiento

    Mientras la temperatura esta entre 26 y 28, el actuador
    mantiene su estado anterior (lo que estuviera haciendo, sigue haciendolo)

    Temperatura
    30 |
    29 |
    28 |------- Umbral de activacion ----------- ON -------
    27 |   ^^^^                                    ^^^^
    26 |------- Umbral de desactivacion -------- OFF ------
    25 |
       +-------------------------------------------> Tiempo
              Zona de histeresis:
              el actuador mantiene su estado anterior

Ahora el sistema de enfriamiento se enciende cuando la temperatura sube a 28 y no se apaga hasta que baja a 26. Las pequenas oscilaciones alrededor de un valor no causan conmutaciones.

**Como elegir la banda de histeresis**: No hay una formula magica; depende de la aplicacion. Para temperatura en la mayoria de procesos industriales, una banda de 1-2 grados suele ser adecuada. El criterio es: la banda debe ser mayor que el ruido normal de la medicion pero menor que lo que seria un cambio significativo que justifique accion.

**Analogia**: Piensa en el termostato de tu casa. Cuando pones 22 grados, la calefaccion no se enciende y apaga exactamente a 22.0. Se enciende cuando baja de 21 y se apaga cuando llega a 23. Si no tuviera esa banda, estarias oyendo el "clic" de la caldera cada pocos segundos. La histeresis es esa banda de tolerancia que da estabilidad al sistema.

---

## Como encaja en el proyecto

La Fase 3 es donde el sistema de monitoreo cobra vida. Hasta ahora tenias hardware que sabe comunicarse (Fase 2: ESP-NOW) y un gateway que sabe conectarse a una red (Fase 1: WiFi, HTTP). Pero sin sensores calibrados y actuadores fiables, lo unico que puedes enviar y recibir son numeros inventados.

Esta fase le da al sistema sus "sentidos" y sus "manos":

    Fase 0: Entiendes el hardware y las herramientas
    Fase 1: El gateway se conecta al mundo (WiFi, REST API, panel web)
    Fase 2: Los nodos hablan con el gateway (ESP-NOW, deep sleep)
    --> Fase 3: Los nodos MIDEN el mundo real y ACTUAN sobre el <--
    Fase 4+: Dashboard embebido avanzado, OTA, optimizacion...

Concretamente, el driver DS18B20 que se crea en esta fase es el que usa el firmware del nodo (Fase 2) para rellenar los paquetes ESP-NOW con datos reales de temperatura. Antes de la Fase 3, un nodo enviaba datos de prueba. Despues de la Fase 3, envia la temperatura real del entorno monitorizado.

Y con el control de actuadores y el sistema de alertas con histeresis, el sistema pasa de ser un observador pasivo a un sistema autonomo capaz de reaccionar: si la temperatura sube, activa el sistema de enfriamiento; si baja, activa el calentador. Sin intervencion humana, las 24 horas del dia.

    Nodo sensor (ESP32-C3)
    +--------------------------------------------------+
    |                                                    |
    |  DS18B20 --[1-Wire]--> Driver temp --> Lectura     |
    |                                          |         |
    |  Filtrado (media recortada) ----------> Paquete    |
    |                                        ESP-NOW    |
    +--------------------------------------------------+

    Gateway (ESP32-S3)
    +--------------------------------------------------+
    |  Recibe paquete ESP-NOW                           |
    |      |                                            |
    |      v                                            |
    |  Sistema de alertas (con histeresis)              |
    |       |                                           |
    |       v                                           |
    |  Rele --> Actuador / Bomba / Ventilador           |
    +--------------------------------------------------+

Sin mediciones fiables, todo el sistema es inutil. La calibracion, el filtrado y la gestion de errores no son lujos academicos: son la diferencia entre un sistema que funciona y uno que miente.

> **Extensiones de dominio especifico**: Si tu proyecto requiere sensores analogicos complejos (pH, oxigeno disuelto, ORP, nivel ultrasonico), estos siguen los mismos patrones de driver, calibracion y filtrado que has aprendido aqui, pero con pasos adicionales de acondicionamiento de senal y compensacion. Ver los ejemplos completos en `examples/fish-farm/`.

---

## Errores tipicos y como evitarlos

1. **Olvidar la resistencia pull-up del bus 1-Wire.** Sin la resistencia de 4.7K ohmios entre la linea de datos y VCC, el bus 1-Wire no funciona. El ESP32 no puede "ver" ningun DS18B20 y los comandos de descubrimiento devuelven cero dispositivos. Es el error mas comun con sensores 1-Wire y lo primero que debes verificar cuando no detectas sensores.

2. **No reinicializar el bus 1-Wire tras deep sleep.** El ESP32 pierde el estado de los perifericos al entrar en deep sleep. Si el nodo usa ciclos de deep sleep, hay que volver a llamar `ds18b20_init()` cada vez que se despierta.

3. **Usar valores crudos del ADC en vez de valores calibrados.** El ADC del ESP32 puede tener errores de decenas de milivoltios en las lecturas crudas. Si calculas unidades fisicas con voltajes sin calibrar, tu lectura puede tener errores significativos. Usa siempre la API de calibracion de ESP-IDF que aprovecha los datos de eFuse de tu chip.

4. **No usar filtrado y confiar en lecturas individuales.** Una lectura aislada de un sensor analogico puede incluir un spike de ruido electrico que te de un valor absurdo. Si tu sistema de alertas se dispara con una sola lectura anomala, encenderas y apagaras actuadores sin motivo. Aplica siempre un filtro (preferiblemente media recortada) y basa tus decisiones en el valor filtrado.

5. **Configurar alertas sin histeresis.** Un umbral unico provoca chattering: el actuador se enciende y apaga repetidamente cuando la lectura oscila alrededor del umbral. Esto destruye reles, estresa motores y genera ruido electrico. Usa siempre dos umbrales separados (activacion y desactivacion) con una banda de histeresis adecuada.

6. **Omitir el diodo flyback en un rele sin modulo integrado.** Si usas un rele suelto (no un modulo comercial que ya incluye proteccion), necesitas poner un diodo de proteccion en paralelo con la bobina. El pico de back-EMF al desactivar la bobina puede superar los 100V durante microsegundos, suficiente para destruir el transistor de control y potencialmente daniar el GPIO del ESP32.

7. **Confundir active-high y active-low en el modulo de rele.** Algunos modulos activan el rele con un nivel logico alto (3.3V en el pin de control) y otros con un nivel bajo (0V). Si tu firmware asume lo contrario, el rele estara encendido cuando deberia estar apagado. Verifica el datasheet del modulo o pruebalo manualmente antes de escribir el driver.

---

## Experimenta

> **Reto 1: Compara lecturas crudas vs calibradas del ADC.** Conecta un potenciometro a un pin ADC del ESP32. Pon el potenciometro a una posicion fija y lee 100 valores crudos (sin calibracion) y 100 valores calibrados (usando la API de ESP-IDF). Calcula el promedio y la desviacion estandar de cada grupo. Cuanto difieren los promedios? Cual grupo tiene menor dispersion? Repite con el potenciometro en varias posiciones (cerca de 0V, en el medio, cerca de 3.3V) y observa si el error es uniforme o peor en los extremos del rango.

> **Reto 2: Visualiza el efecto del filtrado.** Lee un sensor (puede ser un LDR, un potenciometro, o el propio DS18B20) 200 veces y envia los valores por el monitor serie. En paralelo, aplica una media movil de 5 muestras, un filtro de mediana de 5 muestras y una media recortada de 5 muestras (descartando el mayor y el menor). Imprime los cuatro valores en cada lectura. Pega los datos en una hoja de calculo y grafica las cuatro curvas. Durante la prueba, genera un "spike" artificial (para DS18B20, toca el sensor con los dedos). Cual filtro rechaza mejor el spike?

> **Reto 3: Experimenta con histeresis.** Usa un potenciometro para simular una variable de proceso. Configura un LED (simulando un actuador) con un umbral unico de activacion (por ejemplo, "si el valor ADC pasa de 2000, enciende"). Gira el potenciometro lentamente alrededor de ese valor y observa el parpadeo. Ahora implementa histeresis con dos umbrales (activacion en 2000, desactivacion en 1800). Repite la prueba. El parpadeo desaparece?

> **Reto 4: Multi-sensor en el mismo bus 1-Wire.** Si tienes dos o mas DS18B20, conectalos todos al mismo pin de datos (con una sola resistencia pull-up). Usa el comando de descubrimiento de bus (`onewire_bus_search_rom`) para encontrar todos los sensores. Calienta uno con los dedos mientras el otro queda a temperatura ambiente. Verifica que puedes leer los dos sensores de forma independiente usando sus direcciones ROM unicas.

---

## Para profundizar

- Documentacion de la API del ADC (oneshot y calibracion) en ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc_oneshot.html
- Documentacion de la API de ADC calibracion continua: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc_calibration.html
- Documentacion del driver GPIO para ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html
- Datasheet del DS18B20 (sensor de temperatura 1-Wire): https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf
- Componente ESP-IDF onewire_bus: https://components.espressif.com/components/espressif/onewire_bus
- API de NVS (almacenamiento no volatil) para guardar coeficientes de calibracion: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html
- Articulo sobre filtros de mediana en sistemas embebidos: https://embeddedgurus.com/stack-overflow/tag/median-filter/

---

## Preguntas de reflexion

1. El DS18B20 necesita 750 ms para completar una conversion de temperatura a 12 bits. Tu nodo despierta del deep sleep y necesita leer el sensor lo antes posible para minimizar el tiempo activo y ahorrar bateria. Como estructurarias el codigo para no "desperdiciar" esos 750 ms esperando activamente?

2. El ADC del ESP32 con atenuacion de 11 dB tiene un rango teorico hasta 3.3V, pero la precision se degrada por encima de 2.6V. Si tu sensor analogico produce voltajes entre 0 y 3.0V, que estrategias podrias usar para mejorar la precision de las lecturas en la parte alta del rango?

3. Tu sistema usa una media recortada de 5 lecturas para filtrar la temperatura. Pero notas que cuando hay un cambio real brusco (alguien abre una ventana y la temperatura baja 5 grados en pocos segundos), el sistema tarda demasiado en reflejar el cambio real. Que podrias modificar en el filtrado sin perder la proteccion contra spikes?

4. Has implementado histeresis en el control del actuador de temperatura con umbral de activacion a 30 grados C y desactivacion a 28 grados C. Un dia de mucho calor, la temperatura oscila entre 29 y 31 grados durante horas. El actuador esta encendido o apagado? Es el comportamiento correcto?

5. Tu modulo de rele es active-low (se activa cuando el GPIO esta en LOW). Cuando el ESP32 se reinicia, durante los primeros milisegundos antes de que tu firmware configure los GPIO, en que estado estan los pines? Que implicacion tiene esto para la seguridad del actuador? Como lo resolveras?

6. Quieres monitorizar la temperatura en dos puntos distintos de una instalacion usando dos nodos, cada uno con un DS18B20. El gateway tiene una regla de alerta: "si cualquiera de los dos nodos supera 35 grados, activar el sistema de ventilacion". Como implementarias esto en el sistema de alertas? Que pasa con la histeresis cuando los dos nodos tienen temperaturas diferentes?
