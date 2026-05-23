# Tutorial 3: Midiendo el Mundo Real

## Que vas a aprender

- Diferencia entre sensores analogicos y digitales, y cuando usar cada tipo.
- Como funciona el protocolo 1-Wire que usa el DS18B20 para medir la temperatura del agua.
- Por que el ADC del ESP32 no es preciso "de fabrica" y como calibrarlo para obtener lecturas fiables.
- Como convertir milivoltios en unidades reales (pH, mg/L) mediante calibracion de sensores.
- Tecnicas de filtrado de senal (media movil, mediana, media recortada) para limpiar lecturas ruidosas.
- Por que ciertas mediciones dependen de otras (compensacion de temperatura en oxigeno disuelto).
- Como controlar actuadores de potencia (reles, bombas, aireadores) de forma segura desde un GPIO de 3.3V.
- Que es la histeresis y por que los umbrales de activacion y desactivacion deben ser diferentes.

---

## Conceptos clave

### Sensores analogicos vs digitales

En el mundo de los sensores existen dos grandes familias, y la diferencia fundamental esta en como le comunican la informacion al microcontrolador.

Un **sensor analogico** produce un voltaje continuo que es proporcional a lo que mide. Una sonda de pH, por ejemplo, genera un voltaje que sube o baja segun la acidez del agua. Un sensor de oxigeno disuelto produce otro voltaje proporcional a la concentracion de oxigeno. Un sensor de ORP (potencial de oxidacion-reduccion) hace lo mismo. En todos estos casos, la salida es una senal electrica que tu tienes que interpretar.

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

En nuestro proyecto de acuicultura, usamos ambos tipos. Los sensores de pH, oxigeno disuelto y ORP son analogicos (producen un voltaje que leemos con el ADC). Los DS18B20 de temperatura del agua son digitales (hablan por 1-Wire). Cada tipo requiere un tratamiento diferente en el firmware, y entender esa diferencia es el primer paso para escribir drivers fiables.

---

### El protocolo 1-Wire

El DS18B20 es probablemente el sensor de temperatura mas popular del mundo para proyectos con microcontroladores, y con razon: es sumergible, preciso (0.5 grados de exactitud de fabrica), barato y usa un protocolo que solo necesita un cable de datos.

El protocolo se llama 1-Wire ("un cable") porque toda la comunicacion ocurre a traves de un unico hilo. En la practica necesitas dos conexiones: el cable de datos y tierra (GND). Existe incluso un modo llamado "alimentacion parasita" donde el sensor toma la energia del propio cable de datos, eliminando la necesidad de un tercer cable de alimentacion. Aunque para instalaciones fijas como las de una granja acuicola, es mas fiable usar los tres cables (datos, VCC y GND).

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

En el proyecto, los DS18B20 miden la temperatura del agua en cada estanque. Esa lectura de temperatura no solo es valiosa por si misma: es indispensable para compensar la lectura de oxigeno disuelto, como veremos mas adelante.

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

**Atenuacion**: Nativamente, el ADC del ESP32 solo puede medir voltajes entre 0 y aproximadamente 1.1V. Si tu sensor de pH produce 2.5V, necesitas "atenuar" esa senal para que quepa en el rango del ADC. ESP-IDF ofrece cuatro niveles de atenuacion:

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

**Regla de oro**: Nunca uses valores crudos del ADC para tomar decisiones. Siempre pasa por la API de calibracion. En un proyecto de acuicultura donde un error de 0.2 en el pH puede significar la diferencia entre peces sanos y peces estresados, la calibracion del ADC no es opcional.

**Analogia**: El ADC es como una regla de madera artesanal. El fabricante sabe que las marcas no estan perfectas, asi que incluye una hoja con correcciones: "cuando la regla dice 15 cm, en realidad son 14.8 cm". La API de calibracion es esa hoja de correcciones, y los datos de eFuse son las mediciones especificas de tu regla concreta.

---

### Calibracion de sensores: de voltios a unidades reales

La calibracion del ADC te da milivoltios. Pero tu no quieres saber que hay 1847 mV en el pin del sensor de pH. Tu quieres saber que el pH del agua es 7.2. La calibracion de sensores es el paso que convierte voltaje en unidades con significado fisico.

**El concepto fundamental**: Cualquier sensor analogico tiene una funcion de transferencia, una relacion matematica entre la magnitud fisica que mide y el voltaje que produce. En el caso mas simple, esa relacion es lineal:

    pH = pendiente * voltaje + offset

Donde "pendiente" y "offset" son dos numeros que definen la recta. El problema es que no los conoces de antemano porque dependen de tu sensor concreto, del circuito acondicionador, del desgaste de la sonda, de la temperatura ambiente... La calibracion consiste en determinar esos dos numeros.

**Calibracion de 2 puntos (la mas comun para pH)**:

Necesitas dos soluciones de referencia con pH conocido, llamadas "soluciones buffer". Las tipicas son pH 4.0 y pH 7.0 (se compran en frascos, son baratas).

    Paso 1: Sumerge la sonda en solucion buffer pH 7.0
            Lee el voltaje del ADC (calibrado): por ejemplo, 1500 mV
            Ahora tienes el punto (1500, 7.0)

    Paso 2: Sumerge la sonda en solucion buffer pH 4.0
            Lee el voltaje del ADC (calibrado): por ejemplo, 2100 mV
            Ahora tienes el punto (2100, 4.0)

    Paso 3: Con dos puntos defines una recta
            pendiente = (7.0 - 4.0) / (1500 - 2100) = 3.0 / (-600) = -0.005
            offset = 7.0 - (-0.005 * 1500) = 7.0 + 7.5 = 14.5

    Resultado: pH = -0.005 * voltaje_mV + 14.5

    Paso 4: Guarda pendiente y offset en NVS (flash)
            Asi sobreviven a reinicios y cortes de luz

    Verificacion: si lees 1800 mV
                  pH = -0.005 * 1800 + 14.5 = 5.5
                  Comprueba con una tercera solucion buffer si es correcto

**Por que dos puntos y no uno**: Un solo punto te da el offset pero no la pendiente. Es como saber que la regla marca bien a 10 cm pero no saber si las marcas estan bien separadas. Podria marcar bien a 10 cm pero mal a 20 cm. Dos puntos definen una recta unica.

**Por que a veces querrias tres puntos**: Con tres puntos puedes detectar si la relacion NO es lineal. Si el tercer punto cae sobre la recta definida por los otros dos, la relacion es lineal y tu calibracion de 2 puntos es valida. Si el tercer punto se desvia significativamente, la relacion tiene curvatura y necesitarias un polinomio de segundo grado o una tabla de busqueda.

    2 puntos:                     3 puntos (lineal):           3 puntos (no lineal):
    pH                            pH                           pH
    |  *                          |  *                         |  *
    |      *                      |      *                     |       *
    |                             |          *                 |            *
    +--------> mV                 +--------> mV                +--------> mV
    Define una recta              El tercer punto cae          El tercer punto
    (asume linealidad)            sobre la recta: bien         se desvie: problema

**Almacenamiento en NVS**: Los coeficientes de calibracion (pendiente y offset) se guardan en el almacenamiento no volatil (NVS) del ESP32. NVS funciona como un diccionario: guardas pares clave-valor que sobreviven a reinicios y cortes de corriente. Asi no necesitas recalibrar cada vez que el nodo se reinicia o despierta del deep sleep.

**Deriva del sensor**: Las sondas de pH se degradan con el tiempo. El gel de la membrana de vidrio se contamina, los electrodos se corroen. Esto hace que la relacion voltaje-pH cambie lentamente. Por eso la calibracion no es algo que se hace una vez y se olvida: hay que repetirla periodicamente. En un entorno acuicola, una calibracion mensual es razonable; en condiciones exigentes, quincenal.

---

### Filtrado de senal: limpiando lecturas ruidosas

Acabas de calibrar tu sensor de pH. Le pides una lectura y obtienes 7.14. Le pides otra y obtienes 7.08. Otra: 7.21. Otra: 7.11. Otra: 9.87. Otra: 7.15. Los valores saltan, y de vez en cuando aparece un valor disparatado que claramente no es real.

Esto es normal. Las lecturas de sensores analogicos siempre tienen ruido: interferencias electricas de la bomba de agua, fluctuaciones en la fuente de alimentacion, imprecision inherente del sensor, vibraciones mecanicas en la sonda. El ruido es inevitable; lo que puedes hacer es filtrarlo.

**Media movil (Moving Average)**

La tecnica mas sencilla. Mantienes un buffer con las ultimas N lecturas y calculas su promedio. Cada vez que llega una lectura nueva, entra en el buffer y la mas antigua sale.

    Lecturas entrantes:  7.14  7.08  7.21  7.11  9.87  7.15  7.09  7.18
                         ----  ----  ----  ----
                         Buffer de 4 lecturas

    Media movil (N=4):
    Paso 1: (7.14 + 7.08 + 7.21 + 7.11) / 4 = 7.135
    Paso 2: (7.08 + 7.21 + 7.11 + 9.87) / 4 = 7.817  <-- el outlier contamina
    Paso 3: (7.21 + 7.11 + 9.87 + 7.15) / 4 = 7.835  <-- sigue afectado
    Paso 4: (7.11 + 9.87 + 7.15 + 7.09) / 4 = 7.805  <-- aun afectado

El problema es evidente: un solo valor anomalo (9.87) arrastra el promedio durante N pasos. La media movil es buena para suavizar fluctuaciones pequenas pero mala para rechazar valores extremos. Ademas, si el valor real cambia bruscamente (la temperatura sube de verdad), la media movil tarda N pasos en reflejar el cambio.

**Filtro de mediana (Median Filter)**

En vez de promediar, ordenas las ultimas N lecturas de menor a mayor y tomas la del centro.

    Lecturas en el buffer:  7.08  7.21  7.11  9.87  7.15
    Ordenadas:              7.08  7.11  7.15  7.21  9.87
                                         ^
                                      Mediana = 7.15

El outlier de 9.87 queda relegado a un extremo y no afecta al valor central. La mediana es excelente para rechazar picos aislados (que en electronica se llaman "spikes"). Su debilidad es que no suaviza tanto las fluctuaciones normales como la media.

**Media recortada (Trimmed Mean) -- la que usamos en este proyecto**

Combina lo mejor de ambas tecnicas. Tomas N lecturas, descartas las mas altas y las mas bajas, y promedias el resto.

    Lecturas: 7.14  7.08  7.21  7.11  9.87  7.15  7.09
    Ordenadas: 7.08  7.09  7.11  7.14  7.15  7.21  9.87
                      ^^^^  ^^^^  ^^^^  ^^^^  ^^^^
               descarta                              descarta
               (minimo)                              (maximo)

    Media recortada = (7.09 + 7.11 + 7.14 + 7.15 + 7.21) / 5 = 7.14

El valor anomalo 9.87 fue descartado automaticamente. Y el suavizado del promedio se aplica sobre los valores restantes.

**Analogia**: Imagina que cinco personas estiman la temperatura de una habitacion. Cuatro dicen valores entre 22 y 24 grados. Una persona dice 100 grados (claramente esta bromeando o tiene fiebre). Si haces la media de los cinco, obtienes 31.2 grados, un disparate. Si descartas el valor mas alto y el mas bajo y promedias los tres centrales, obtienes algo alrededor de 23 grados, que es la realidad. Eso es una media recortada.

**Cual elegir depende de la situacion**: Para un sensor que necesita respuesta rapida (detector de fuga), la mediana es mejor. Para un sensor donde importa la precision a largo plazo (pH en un estanque que cambia lentamente), la media recortada es ideal. En nuestro proyecto, los sensores de calidad de agua cambian lentamente, asi que la media recortada nos da estabilidad sin sacrificar precision.

---

### Compensacion de temperatura en sensores

Este concepto es uno de los mas importantes y a la vez mas ignorados por principiantes: algunas mediciones dependen de la temperatura, y si no compensas, tu lectura es incorrecta.

El ejemplo mas claro es el **oxigeno disuelto (DO)**. La cantidad de oxigeno que el agua puede retener depende fuertemente de la temperatura:

    Temperatura     Oxigeno disuelto maximo (a nivel del mar)
    del agua        (saturacion al 100%)
    ============    ==========================================
       5 C            12.8 mg/L
      10 C            11.3 mg/L
      15 C            10.1 mg/L
      20 C             9.1 mg/L
      25 C             8.3 mg/L
      30 C             7.5 mg/L
      35 C             6.9 mg/L

Observa: a 5 grados, el agua puede contener 12.8 mg/L de oxigeno. A 30 grados, solo 7.5 mg/L. Casi el doble de diferencia. Ahora imagina que tu sensor de DO mide un voltaje que corresponde a 8.0 mg/L. Si el agua esta a 25 grados, eso es un 96% de saturacion (casi el maximo), los peces estan bien. Pero si el agua esta a 15 grados, eso es solo un 79% de saturacion, lo cual podria ser preocupante.

**El mismo voltaje del sensor tiene un significado completamente diferente dependiendo de la temperatura.**

    Sin compensacion:                    Con compensacion:

    Sensor DO: 450 mV                   Sensor DO: 450 mV
         |                               Sensor temp: 28.5 C
         v                                    |         |
    Formula simple:                           v         v
    DO = f(voltaje)                      Formula completa:
         |                               DO = f(voltaje, temperatura)
         v                                    |
    DO = 8.2 mg/L                             v
    (puede estar MUY mal)                DO = 7.6 mg/L
                                         (valor correcto)

Esto es la razon por la que muchos nodos de nuestro proyecto necesitan **dos sensores**: el sensor de oxigeno disuelto y un DS18B20 para medir la temperatura del agua simultaneamente. La lectura de temperatura se usa para corregir la lectura de DO.

El procedimiento en el firmware es:

    1. Leer temperatura del agua (DS18B20)
    2. Leer voltaje del sensor de DO (ADC calibrado)
    3. Aplicar formula de compensacion que usa ambos valores
    4. Obtener el valor real de DO en mg/L

El **pH** tambien se ve afectado por la temperatura, aunque en menor medida. La pendiente de la sonda de pH (la relacion milivoltios-por-unidad-de-pH, llamada "pendiente de Nernst") cambia con la temperatura. A 25 grados, la pendiente teorica es de -59.16 mV por unidad de pH. A 0 grados, es -54.20 mV. A 50 grados, -64.12 mV. Si calibraste a 25 grados y luego mides a 10 grados sin compensar, tendras un error en la lectura.

**Analogia**: Imagina que tienes una balanza para pesar sacos de grano. Pero la balanza lee diferente si hace calor o frio porque el metal de la bascula se dilata. Si solo miras el peso sin considerar la temperatura, tus sacos "pesan" diferente en verano y en invierno, aunque tengan el mismo grano. Compensar es tener un termometro junto a la balanza y aplicar una correccion: "a 35 grados, restar 200 gramos al peso mostrado".

---

### Reles y control de actuadores

Hasta ahora hemos hablado de medir. Pero un sistema de monitoreo que solo mide y no actua es como un detector de humo que suena pero no activa los aspersores. En acuicultura, cuando el oxigeno baja, necesitas encender un aireador. Cuando la temperatura sube, necesitas activar una bomba de circulacion. Eso requiere actuadores: dispositivos que hacen algo fisico en el mundo real.

El problema es que un GPIO del ESP32 produce 3.3V y puede suministrar unos 12 miliamperios como maximo. Un aireador de estanque consume 220V y varios amperios. No puedes conectar el aireador directamente al ESP32. Necesitas un intermediario: el rele.

**Que es un rele**

Un rele es un interruptor controlado electricamente. Dentro tiene una bobina de cobre. Cuando pasa corriente por la bobina, genera un campo magnetico que mueve mecanicamente un contacto metalico, cerrando (o abriendo) un circuito externo.

    Circuito de control               Circuito de potencia
    (bajo voltaje, ESP32)             (alto voltaje, aireador)

    GPIO --> [Transistor] --> [Bobina del rele]
                                    |
                              [Contacto movil]
                                    |
                              /     |     \
                          NC   COM   NO
                                    |
                          220V ---- Aireador ---- 220V

    NC  = Normalmente Cerrado (conectado cuando el rele esta apagado)
    COM = Comun
    NO  = Normalmente Abierto (conectado cuando el rele esta encendido)

En la mayoria de aplicaciones de acuicultura, conectas el actuador entre COM y NO: asi el actuador esta apagado por defecto y se enciende cuando activas el rele. Es mas seguro: si el ESP32 se cuelga o pierde alimentacion, el rele se desactiva y el actuador se apaga.

**Por que necesitas un transistor u optoacoplador**

El GPIO del ESP32 no tiene suficiente corriente para activar la bobina del rele directamente (la bobina tipica necesita 70-100 mA). Necesitas un transistor que actue como amplificador de corriente: la pequena corriente del GPIO controla una corriente mayor que fluye hacia la bobina.

Los modulos de reles comerciales ya incluyen este transistor (y a menudo un optoacoplador). Un **optoacoplador** es un componente que aisle electricamente el circuito del ESP32 del circuito del rele mediante luz: un LED interno ilumina un fototransistor. Asi, aunque el lado de potencia tenga un problema electrico, el ESP32 esta protegido porque no hay conexion electrica directa.

    ESP32                   Optoacoplador              Rele
    +------+           +---[LED]---[FotoTR]---+       +------+
    | GPIO |---------->| Lado ESP32 | Lado rele|----->| Bobina|
    +------+           +---(aislado)----------+       +------+

**El diodo flyback: proteccion esencial**

Aqui viene un detalle critico que muchos principiantes ignoran. Una bobina de rele almacena energia en su campo magnetico mientras esta encendida. Cuando cortas la corriente (apagas el rele), esa energia tiene que ir a algun sitio. Se manifiesta como un pico de voltaje transitorio (llamado back-EMF o "fuerza contra-electromotriz") que puede alcanzar decenas o cientos de voltios durante microsegundos.

    Bobina apagandose:

    Voltaje
      ^
      |    /\  <-- Pico de back-EMF (puede ser >100V)
      |   /  \     durante microsegundos
      |  /    \
      |_/      \___  Voltaje normal
      +-------------> Tiempo
           ^
           Momento en que
           cortas la corriente

Ese pico puede destruir el transistor que controla la bobina, y por extension daniar el GPIO del ESP32. La solucion es un **diodo flyback** (tambien llamado "diodo de proteccion" o "snubber diode") conectado en paralelo con la bobina, en sentido inverso. En operacion normal, el diodo no conduce (esta en polarizacion inversa). Cuando aparece el pico de back-EMF, el diodo conduce y disipa la energia de forma segura.

    Sin diodo flyback:              Con diodo flyback:

    +------+                        +------+
    | Bobina| -->  pico de          | Bobina| --+
    +------+      voltaje           +------+   |
                  (peligroso)          ^       |
                                       |  [Diodo]
                                       |       |
                                       +-------+
                                       El pico circula por el diodo
                                       en vez de danar el transistor

Los modulos de reles de buena calidad ya incluyen este diodo. Si usas un rele suelto, tienes que poner el diodo tu mismo.

**Active-HIGH vs Active-LOW**

Un detalle que causa confusion: algunos modulos de rele se activan cuando el GPIO esta en HIGH (3.3V), otros cuando esta en LOW (0V). Esto depende del diseno del circuito del modulo. Si tu firmware asume active-high pero el modulo es active-low, el rele estara encendido cuando deberia estar apagado y viceversa. Verifica siempre la polaridad de tu modulo y configurala correctamente en el firmware.

---

### Histeresis: evitando el efecto "ping-pong"

Imagina esta situacion: has configurado una alerta que dice "si la temperatura del agua supera 28 grados, enciende el aireador de emergencia". Parece razonable. Pero observa que pasa en la practica:

    Temperatura real del agua: oscila naturalmente entre 27.8 y 28.2 grados

    Segundo 0:  Temp = 27.9 C  -->  Aireador: APAGADO
    Segundo 1:  Temp = 28.1 C  -->  Aireador: ENCENDIDO
    Segundo 2:  Temp = 27.9 C  -->  Aireador: APAGADO   (se enfrio un poco)
    Segundo 3:  Temp = 28.0 C  -->  Aireador: ENCENDIDO
    Segundo 4:  Temp = 27.8 C  -->  Aireador: APAGADO
    Segundo 5:  Temp = 28.2 C  -->  Aireador: ENCENDIDO
    ...

El aireador se enciende y apaga cada pocos segundos. Esto se llama "chattering" o "efecto ping-pong". Es terrible por varias razones:

- **Desgaste mecanico**: Los contactos del rele se desgastan con cada conmutacion. Un rele tiene una vida util medida en numero de conmutaciones (tipicamente 100.000). Si conmuta cada segundo, dura poco mas de un dia.
- **Estres electrico**: Arrancar un motor (como el de un aireador) consume mucha mas corriente que mantenerlo girando. Arranques frecuentes pueden quemar el motor.
- **Ruido electrico**: Cada conmutacion genera interferencias que pueden afectar las lecturas de los sensores.

**La solucion: histeresis**

En vez de un solo umbral, usas dos:

    Umbral de activacion:    28.0 C  (enciende el aireador)
    Umbral de desactivacion: 26.0 C  (apaga el aireador)

    La diferencia (2.0 C) es la "banda de histeresis"

El comportamiento ahora es:

    Temp sube:   25 -> 26 -> 27 -> 28 --> ENCIENDE el aireador
    Temp baja:   28 -> 27 -> 26 --> APAGA el aireador

    Mientras la temperatura esta entre 26 y 28, el aireador
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

Ahora el aireador se enciende cuando la temperatura sube a 28 y no se apaga hasta que baja a 26. Las pequenas oscilaciones alrededor de un valor (27.8, 28.1, 27.9) no causan conmutaciones porque una vez encendido, el aireador permanece encendido hasta que la temperatura baja significativamente.

**Como elegir la banda de histeresis**: No hay una formula magica; depende de la aplicacion. Para temperatura del agua en acuicultura, una banda de 1-2 grados suele ser adecuada. Para pH, podrias usar 0.2-0.3 unidades. El criterio es: la banda debe ser mayor que el ruido normal de la medicion pero menor que lo que seria un cambio significativo que justifique accion.

**Analogia**: Piensa en el termostato de tu casa. Cuando pones 22 grados, la calefaccion no se enciende y apaga exactamente a 22.0. Se enciende cuando baja de 21 y se apaga cuando llega a 23. Si no tuviera esa banda, estarias oyendo el "clic" de la caldera cada pocos segundos. La histeresis es esa banda de tolerancia que da estabilidad al sistema.

En nuestro proyecto, cada alerta tiene sus dos umbrales configurables: umbral de activacion y umbral de desactivacion. La diferencia entre ambos es la histeresis. Se almacenan en NVS junto con los demas parametros del nodo.

---

## Como encaja en el proyecto

La Fase 3 es donde el sistema de monitoreo cobra vida. Hasta ahora tenias hardware que sabe comunicarse (Fase 2: ESP-NOW) y un gateway que sabe conectarse a Internet (Fase 1: WiFi, MQTT). Pero sin sensores calibrados y actuadores fiables, lo unico que puedes enviar y recibir son numeros inventados.

Esta fase le da al sistema sus "sentidos" y sus "manos":

    Fase 0: Entiendes el hardware y las herramientas
    Fase 1: El gateway se conecta al mundo (WiFi, MQTT, panel web)
    Fase 2: Los nodos hablan con el gateway (ESP-NOW, deep sleep)
    --> Fase 3: Los nodos MIDEN el mundo real y ACTUAN sobre el <--
    Fase 4+: Inteligencia, alertas avanzadas, OTA...

Concretamente, los drivers de sensores que se crean en esta fase son los que usa el firmware del nodo (Fase 2) para rellenar los paquetes ESP-NOW con datos reales. Antes de la Fase 3, un nodo enviaba datos de prueba. Despues de la Fase 3, envia la temperatura real del agua, el pH real, la concentracion de oxigeno disuelto real.

Y con el control de actuadores y el sistema de alertas con histeresis, el sistema pasa de ser un observador pasivo a un sistema autonomo capaz de reaccionar: si el oxigeno baja, enciende el aireador; si la temperatura sube, activa la circulacion de agua. Sin intervencion humana, las 24 horas del dia.

    Nodo sensor (ESP32-C3)
    +--------------------------------------------------+
    |                                                    |
    |  DS18B20 --[1-Wire]--> Driver temp --> Lectura     |
    |                                          |         |
    |  Sonda pH --[ADC calibrado]--> Driver pH |         |
    |                                  |       |         |
    |  Sonda DO --[ADC calibrado]------+       |         |
    |              + compensacion temp         |         |
    |                                          v         |
    |  Filtrado (media recortada) ----------> Paquete    |
    |                                        ESP-NOW    |
    |  Sistema de alertas (con histeresis)               |
    |       |                                            |
    |       v                                            |
    |  Rele --> Aireador / Bomba / Alimentador           |
    +--------------------------------------------------+

Sin mediciones fiables, todo el sistema es inutil. Puedes tener la mejor infraestructura de comunicacion del mundo, pero si el sensor de pH te dice 7.0 cuando en realidad hay 6.2, tus peces estan en peligro y tu no lo sabes. La calibracion, el filtrado y la compensacion no son lujos academicos: son la diferencia entre un sistema que funciona y uno que miente.

---

## Errores tipicos y como evitarlos

1. **Olvidar la resistencia pull-up del bus 1-Wire.** Sin la resistencia de 4.7K ohmios entre la linea de datos y VCC, el bus 1-Wire no funciona. El ESP32 no puede "ver" ningun DS18B20 y los comandos de descubrimiento devuelven cero dispositivos. Es el error mas comun con sensores 1-Wire y lo primero que debes verificar cuando no detectas sensores.

2. **Usar valores crudos del ADC en vez de valores calibrados.** El ADC del ESP32 puede tener errores de decenas de milivoltios en las lecturas crudas. Si calculas el pH con voltajes sin calibrar, tu lectura puede desviarse 0.3 o mas unidades de pH, suficiente para tomar decisiones erroneas. Usa siempre la API de calibracion de ESP-IDF que aprovecha los datos de eFuse de tu chip.

3. **Calibrar el sensor de pH una sola vez y olvidarse.** Las sondas de pH se degradan con el uso y el tiempo. La membrana de vidrio se contamina, el electrodo de referencia pierde potencial. Una calibracion que era perfecta hace tres meses puede tener un error significativo hoy. Establece un calendario de recalibracion (quincenal o mensual) y anotalo como parte del mantenimiento del sistema.

4. **Medir oxigeno disuelto sin compensar por temperatura.** La misma lectura de voltaje del sensor de DO puede corresponder a 8.5 mg/L a 20 grados o a 7.2 mg/L a 30 grados. Sin el dato de temperatura, la lectura de DO es un numero sin significado real. Asegurate de que cada nodo que mide DO tenga tambien un sensor de temperatura y de que el firmware aplique la compensacion antes de reportar el valor.

5. **No usar filtrado y confiar en lecturas individuales.** Una lectura aislada de un sensor analogico puede incluir un spike de ruido electrico que te de un valor absurdo. Si tu sistema de alertas se dispara con una sola lectura anomala, encenderas y apagaras actuadores sin motivo. Aplica siempre un filtro (preferiblemente media recortada) y basa tus decisiones en el valor filtrado.

6. **Configurar alertas sin histeresis.** Un umbral unico provoca chattering: el actuador se enciende y apaga repetidamente cuando la lectura oscila alrededor del umbral. Esto destruye reles, estresa motores y genera ruido electrico. Usa siempre dos umbrales separados (activacion y desactivacion) con una banda de histeresis adecuada.

7. **Omitir el diodo flyback en un rele sin modulo integrado.** Si usas un rele suelto (no un modulo comercial que ya incluye proteccion), necesitas poner un diodo de proteccion en paralelo con la bobina. El pico de back-EMF al desactivar la bobina puede superar los 100V durante microsegundos, suficiente para destruir el transistor de control y potencialmente daniar el GPIO del ESP32.

8. **Confundir active-high y active-low en el modulo de rele.** Algunos modulos activan el rele con un nivel logico alto (3.3V en el pin de control) y otros con un nivel bajo (0V). Si tu firmware asume lo contrario, el rele estara encendido cuando deberia estar apagado. Verifica el datasheet del modulo o pruebalo manualmente antes de escribir el driver. Un rele que se queda activado por un error de polaridad puede mantener encendido un actuador de forma indefinida.

---

## Experimenta

> **Reto 1: Compara lecturas crudas vs calibradas del ADC.** Conecta un potenciometro a un pin ADC del ESP32. Pon el potenciometro a una posicion fija y lee 100 valores crudos (sin calibracion) y 100 valores calibrados (usando la API de ESP-IDF). Calcula el promedio y la desviacion estandar de cada grupo. Cuanto difieren los promedios? Cual grupo tiene menor dispersion? Repite con el potenciometro en varias posiciones (cerca de 0V, en el medio, cerca de 3.3V) y observa si el error es uniforme o peor en los extremos del rango.

> **Reto 2: Visualiza el efecto del filtrado.** Lee un sensor analogico (puede ser un LDR o un potenciometro) 500 veces y envia los valores crudos por el monitor serie. En paralelo, aplica una media movil de 10 muestras, un filtro de mediana de 5 muestras y una media recortada de 7 muestras (descartando el mayor y el menor). Imprime los cuatro valores en cada lectura (crudo, media movil, mediana, media recortada). Pega los datos en una hoja de calculo y grafica las cuatro curvas. Durante la prueba, genera un "spike" artificial (toca el cable con el dedo). Cual filtro rechaza mejor el spike? Cual sigue mas rapido un cambio real?

> **Reto 3: Experimenta con histeresis.** Usa un potenciometro para simular una temperatura variable. Configura un LED (simulando un actuador) con un umbral unico de activacion (por ejemplo, "si el valor pasa de 2000, enciende"). Gira el potenciometro lentamente alrededor de ese valor y observa el parpadeo. Ahora implementa histeresis con dos umbrales (activacion en 2000, desactivacion en 1800). Repite la prueba. El parpadeo desaparece?

> **Reto 4: Calibracion de dos puntos con un sensor real.** Si tienes una sonda de pH y soluciones buffer, realiza una calibracion de dos puntos. Mide el voltaje en buffer pH 4.0, mide el voltaje en buffer pH 7.0, calcula pendiente y offset, guarda los coeficientes en NVS. Luego verifica con una tercera solucion buffer (pH 10.0 si la tienes). Cuanto se desvio tu lectura del valor esperado? Si no tienes sonda de pH, puedes simular el ejercicio con un potenciometro: asigna "pH 4.0" a una posicion y "pH 7.0" a otra, calibra, y verifica que posiciones intermedias dan valores intermedios coherentes.

---

## Para profundizar

- Documentacion de la API del ADC (oneshot y calibracion) en ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc_oneshot.html
- Documentacion de la API de ADC calibracion continua: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc_calibration.html
- Documentacion del driver GPIO para ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html
- Datasheet del DS18B20 (sensor de temperatura 1-Wire): https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf
- Tutorial de Atlas Scientific sobre calibracion de sensores de pH: https://atlas-scientific.com/blog/ph-probe-calibration/
- Guia de Atlas Scientific sobre compensacion de temperatura en oxigeno disuelto: https://atlas-scientific.com/blog/dissolved-oxygen-temperature-compensation/
- Nota de aplicacion de Espressif sobre el ADC del ESP32: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc_oneshot.html#adc-attenuation
- Articulo sobre filtros de mediana en sistemas embebidos: https://embeddedgurus.com/stack-overflow/tag/median-filter/
- API de NVS (almacenamiento no volatil) para guardar coeficientes de calibracion: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html

---

## Preguntas de reflexion

1. Tienes un sensor de pH analogico y un DS18B20 digital conectados al mismo nodo. El nodo despierta del deep sleep, necesita leer ambos sensores y enviar los datos por ESP-NOW. En que orden leerias los sensores y por que? Pista: recuerda cuanto tarda la conversion del DS18B20 y piensa en como aprovechar ese tiempo.

2. El ADC del ESP32 con atenuacion de 11 dB tiene un rango teorico hasta 3.3V, pero la precision se degrada por encima de 2.6V. Tu sensor de oxigeno disuelto produce voltajes entre 0 y 3.0V. Que estrategias podrias usar para mejorar la precision de las lecturas en la parte alta del rango?

3. Has calibrado tu sensor de pH con soluciones buffer de pH 4.0 y pH 7.0. Al verificar con una solucion de pH 10.0, obtienes una lectura de 10.4 en vez de 10.0. Que te dice esto sobre la linealidad de tu sensor? Que harias para mejorar la precision en todo el rango?

4. Tu sistema usa una media recortada de 7 lecturas para filtrar el pH. Pero notas que cuando anade acido al estanque de forma intencional (para corregir el pH), el sistema tarda demasiado en reflejar el cambio real. Que podrias modificar en el filtrado sin perder la proteccion contra spikes?

5. Un nodo tiene un sensor de oxigeno disuelto pero no tiene sensor de temperatura. El nodo vecino (a 2 metros) si tiene un DS18B20. Seria aceptable usar la temperatura del nodo vecino para compensar la lectura de DO? Que problemas podrias encontrar con esta estrategia?

6. Has implementado histeresis en el control del aireador con umbrales de activacion a 5.0 mg/L de DO y desactivacion a 7.0 mg/L de DO. Un dia de mucho calor, el oxigeno del estanque oscila entre 5.5 y 6.5 mg/L durante horas. El aireador esta encendido o apagado? Es el comportamiento correcto? Que pasaria si la banda de histeresis fuera demasiado estrecha (por ejemplo, activacion a 5.0, desactivacion a 5.5)?

7. Tu modulo de rele es active-low (se activa cuando el GPIO esta en LOW). Cuando el ESP32 se reinicia, durante los primeros milisegundos antes de que tu firmware configure los GPIO, en que estado estan los pines? Que implicacion tiene esto para la seguridad del actuador? Como lo resolveras?

8. Imagina que necesitas medir el pH en un estanque que esta a 50 metros del nodo sensor. El cable entre la sonda de pH y la placa del ESP32 tiene 50 metros de longitud. Que problemas esperas encontrar? Seria mejor un sensor analogico o digital para esta distancia? Que alternativas de diseno podrias considerar?
