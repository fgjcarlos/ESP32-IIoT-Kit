# Tutorial 0: Primeros Pasos con ESP32 y ESP-IDF

## Que vas a aprender

- Que es un ESP32, en que se diferencian las variantes S3 y C3, y por que el proyecto usa cada una.
- Que es ESP-IDF, como se compara con Arduino, y como funciona su sistema de build (CMake, idf.py, menuconfig).
- Como el ESP32 organiza su memoria flash en particiones, como usa sus pines GPIO y como se comunica con sensores mediante I2C, SPI y 1-Wire.
- Que es ESP-NOW, como funcionan los modos WiFi (STA, AP, APSTA) y los modos de energia (active, modem sleep, light sleep, deep sleep).
- Como usar UART y el monitor serie para depurar tu firmware desde el primer dia.

---

## Conceptos clave

### Que es ESP32 y sus variantes

El ESP32 es un microcontrolador fabricado por Espressif Systems. Un microcontrolador es un ordenador diminuto: tiene procesador, memoria RAM, almacenamiento (flash) y perifericos de entrada/salida, todo en un solo chip. Lo que hace especial al ESP32 es que ademas incluye WiFi y Bluetooth integrados, lo que lo convierte en una pieza perfecta para proyectos de Internet de las Cosas (IoT).

Ahora bien, "ESP32" es una familia. Las dos variantes que nos interesan son la S3 y la C3:

    +-------------------------------+-------------------------------+
    |          ESP32-S3             |          ESP32-C3             |
    +-------------------------------+-------------------------------+
    | Arquitectura: Xtensa LX7     | Arquitectura: RISC-V         |
    | Nucleos: 2 (dual-core)       | Nucleos: 1 (single-core)     |
    | RAM: hasta 512 KB            | RAM: 400 KB                  |
    | Aceleracion IA (vectorial)   | Sin aceleracion IA           |
    | WiFi 802.11 b/g/n + BLE 5   | WiFi 802.11 b/g/n + BLE 5   |
    | Consumo mayor en activo      | Consumo menor, mas eficiente |
    +-------------------------------+-------------------------------+

La diferencia fundamental esta en la arquitectura del procesador. La S3 usa Xtensa, que es un diseno propietario de Cadence, con dos nucleos y capacidades extra para procesamiento vectorial. La C3 usa RISC-V, una arquitectura abierta, con un solo nucleo y orientada a la eficiencia energetica.

**Analogia**: Piensa en la S3 como el "gerente de oficina". Tiene un escritorio grande (mas RAM), dos manos para hacer varias cosas a la vez (dual-core) y herramientas avanzadas (aceleracion IA). La C3 es el "trabajador de campo": lleva una mochila ligera (menos RAM), trabaja con una mano (single-core), pero es extremadamente eficiente con la bateria y cumple su tarea sin desperdiciar recursos.

En nuestro proyecto de acuicultura:

- El **ESP32-S3** actua como **gateway** (puerta de enlace). Necesita gestionar WiFi, ESP-NOW, MQTT, una interfaz web y coordinar multiples nodos. Necesita musculo.
- El **ESP32-C3** actua como **nodo sensor**. Su trabajo es leer sensores, enviar datos por ESP-NOW y dormir. Necesita eficiencia, no potencia bruta.

---

### ESP-IDF vs Arduino

Si has programado un Arduino alguna vez, sabes que es muy agradable: escribes setup(), loop(), subes el codigo y funciona. Pero Arduino es un framework de abstraccion: oculta lo que pasa debajo para que sea facil empezar. Eso esta bien para prototipos simples, pero cuando necesitas control real sobre el hardware, te quedas corto.

ESP-IDF (Espressif IoT Development Framework) es el SDK oficial de Espressif. Es lo que Espressif usa internamente para desarrollar. Te da acceso completo a:

- **FreeRTOS**: un sistema operativo en tiempo real que permite ejecutar multiples tareas concurrentes con prioridades.
- **Sistema de componentes**: codigo modular y reutilizable, como paquetes en otros lenguajes.
- **menuconfig**: una herramienta visual para configurar opciones del firmware sin tocar codigo.
- **Control avanzado de WiFi y Bluetooth**: configurar canales, potencia de transmision, modos de ahorro de energia, callbacks de bajo nivel.
- **APIs de perifericos de bajo nivel**: acceso directo a timers, DMA, ADC con calibracion, etc.

**Analogia**: Arduino es como conducir un coche automatico. Giras la llave, pisas el acelerador y llegas a tu destino. ESP-IDF es como conducir un coche con cambio manual. Tienes que aprender a usar el embrague, gestionar las marchas, entender las revoluciones del motor. Requiere mas aprendizaje, pero sabes exactamente que esta haciendo el coche en cada momento. Y cuando necesitas exprimir el rendimiento (como en una carrera o subiendo un puerto de montana cargado), el control manual marca la diferencia.

ESP-IDF usa CMake como sistema de build e idf.py como herramienta de linea de comandos. Mas sobre esto a continuacion.

---

### El sistema de build: CMake e idf.py

Cuando escribes codigo en C para el ESP32, necesitas un proceso para convertir ese codigo legible por humanos en un binario que el chip pueda ejecutar. Este proceso se llama "build" y en ESP-IDF involucra varios pasos:

    Tu codigo (.c, .h)
         |
         v
    +------------------+
    | idf.py build     |  <-- Tu ejecutas este comando
    +------------------+
         |
         v
    +------------------+
    | CMake configura  |  Analiza CMakeLists.txt, detecta componentes,
    | el proyecto      |  elige el toolchain correcto
    +------------------+
         |
         v
    +------------------+
    | Compilador       |  xtensa-esp32s3-elf-gcc  (para S3)
    | (toolchain)      |  riscv32-esp-elf-gcc     (para C3)
    +------------------+
         |
         v
    +------------------+
    | Enlazador        |  Junta todo: tu codigo + librerias + FreeRTOS
    | (linker)         |  en un unico binario
    +------------------+
         |
         v
    firmware.bin (listo para grabar en el chip)

Cada variante de ESP32 tiene un procesador con un juego de instrucciones diferente. Por eso el compilador es distinto para S3 (Xtensa) y para C3 (RISC-V). CMake se encarga de elegir el correcto segun el "target" que hayas configurado.

Los tres comandos fundamentales de idf.py:

- **idf.py build** -- Compila tu proyecto y genera el binario.
- **idf.py flash** -- Escribe (graba) el binario en la memoria flash del ESP32 a traves del USB.
- **idf.py monitor** -- Abre una conexion serie para ver lo que el ESP32 imprime (logs, mensajes de depuracion).

Y hay un cuarto comando muy especial:

- **idf.py menuconfig** -- Abre un menu interactivo en la terminal donde puedes activar/desactivar funciones, cambiar parametros (tamano de pila, velocidad de WiFi, nivel de log, pines por defecto) sin modificar ni una linea de tu codigo fuente. Es como el menu de ajustes de un videojuego: cambias la configuracion, no el juego.

**Analogia**: CMake es el arquitecto que lee los planos (CMakeLists.txt) y organiza el trabajo. El compilador es el albanil que construye ladrillo a ladrillo. idf.py es el jefe de obra que coordina todo con un solo comando.

---

### Memoria flash y particiones

El ESP32 no tiene un disco duro ni una tarjeta SD por defecto. Lo que tiene es memoria flash, tipicamente 4 MB u 8 MB, integrada en la placa. Esta memoria es no volatil: su contenido se mantiene aunque se corte la alimentacion.

Pero 4 MB de flash no son un bloque monolitico. Estan divididos en **particiones**, cada una con un proposito especifico:

    Direccion 0x0000
    +------------------------------+
    |  Bootloader (segunda etapa)  |  Arranca el chip, decide que app ejecutar
    +------------------------------+
    |  Tabla de particiones         |  El "mapa" que dice donde esta cada cosa
    +------------------------------+
    |  NVS (Non-Volatile Storage)  |  Almacen de pares clave-valor (ajustes)
    +------------------------------+
    |  app (factory)               |  Tu firmware principal
    +------------------------------+
    |  OTA_0                       |  Espacio para una actualizacion remota
    +------------------------------+
    |  OTA_1                       |  Espacio para otra actualizacion (ping-pong)
    +------------------------------+
    Direccion final (ej. 0x400000 para 4 MB)

**Analogia**: La memoria flash es un edificio de apartamentos. Cada particion es un apartamento con un proposito:

- El **bootloader** es el portero: es lo primero que se activa cuando enciendes el chip y decide a que "apartamento" (particion de app) dirigir la ejecucion.
- La **tabla de particiones** es el directorio del edificio: dice que hay en cada planta y en que direccion.
- **NVS** es el trastero: aqui se guardan configuraciones persistentes como credenciales WiFi, calibraciones de sensores o el canal de ESP-NOW. Usa un formato clave-valor, como un diccionario.
- **app (factory)** es el apartamento principal: aqui vive tu firmware.
- **OTA_0 y OTA_1** son apartamentos de reserva para actualizaciones por aire (Over-The-Air). Mientras ejecutas desde OTA_0, escribes la nueva version en OTA_1. Si la actualizacion falla, el bootloader vuelve a la version anterior. Es un mecanismo de seguridad tipo "ping-pong".

Comprender las particiones es importante porque cuando tu firmware crece, puedes quedarte sin espacio en la particion de app. Y cuando implementemos OTA en fases futuras, necesitaras entender este esquema para no pisar una particion con otra.

---

### GPIO: pines de proposito general

GPIO significa General Purpose Input/Output. Son los pines fisicos del ESP32 que puedes usar para interactuar con el mundo exterior: leer el estado de un boton, encender un LED, activar un rele que controla una bomba de agua, o comunicarte con un sensor.

Cada pin GPIO tiene dos modos fundamentales:

    +-------------------+         +-------------------+
    |  Modo ENTRADA     |         |  Modo SALIDA      |
    |  (INPUT)          |         |  (OUTPUT)         |
    |                   |         |                   |
    |  Lee el mundo:    |         |  Actua en el      |
    |  - Boton pulsado? |         |  mundo:           |
    |  - Sensor digital?|         |  - Encender LED   |
    |                   |         |  - Activar rele   |
    +-------------------+         +-------------------+

Ahora bien, hay un problema clasico con los pines de entrada. Imagina que conectas un boton a un pin GPIO pero el boton esta sin pulsar. El cable esta "al aire", flotando. El pin no esta conectado ni a voltaje alto (3.3V) ni a tierra (0V). En ese estado, el pin lee valores aleatorios: a veces 1, a veces 0, influenciado por ruido electromagnetico, la humedad, incluso tu mano cerca. Es lo que se llama un pin "flotante".

La solucion son las resistencias pull-up y pull-down:

    Con pull-up:                  Con pull-down:

    3.3V                          GPIO ---+--- Boton --- 3.3V
     |                                    |
     R (resistencia)                      R (resistencia)
     |                                    |
    GPIO ---+--- Boton --- GND           GND

    Cuando el boton NO esta          Cuando el boton NO esta
    pulsado: GPIO lee ALTO (1)       pulsado: GPIO lee BAJO (0)

    Cuando el boton SI esta          Cuando el boton SI esta
    pulsado: GPIO lee BAJO (0)       pulsado: GPIO lee ALTO (1)

La buena noticia es que el ESP32 tiene resistencias pull-up y pull-down internas. No necesitas componentes externos; las activas por software al configurar el pin.

**Analogia**: Los pines GPIO son las "manos" del ESP32. Con el modo entrada, palpa el mundo (lee sensores, botones). Con el modo salida, manipula el mundo (enciende, apaga, mueve). Las resistencias pull-up/pull-down son como poner la mano en una posicion de reposo conocida: si no hay nadie que la mueva, sabes donde esta.

---

### Buses de comunicacion: I2C, SPI y 1-Wire

Los sensores de nuestro proyecto (temperatura, pH, oxigeno disuelto) no hablan el mismo idioma que el procesador del ESP32. Necesitan un protocolo de comunicacion, un "idioma comun". Los tres mas habituales en el mundo de microcontroladores son I2C, SPI y 1-Wire.

**I2C (Inter-Integrated Circuit)**

Usa solo 2 cables:
- SDA (datos) -- por aqui viaja la informacion
- SCL (reloj) -- marca el ritmo al que se envian los datos

    ESP32                Sensor A         Sensor B         Sensor C
    [SDA] --------+----------+----------------+----------------+
    [SCL] --------+----------+----------------+----------------+
                  |          |                |                |
              (pull-up)  Direccion: 0x48   Dir: 0x76        Dir: 0x44

Multiples dispositivos comparten los mismos 2 cables. Cada dispositivo tiene una direccion unica (como un numero de telefono). El ESP32 dice "quiero hablar con el dispositivo en la direccion 0x48" y solo ese dispositivo responde.

**Analogia**: I2C es como una linea telefonica compartida (de las antiguas). Todos estan conectados al mismo cable, pero cada uno tiene un numero de telefono. Cuando el ESP32 marca un numero, solo contesta el dispositivo correspondiente. Es sencillo de cablear (solo 2 cables) pero no es el mas rapido.

**SPI (Serial Peripheral Interface)**

Usa 4 cables principales:
- MOSI (Master Out, Slave In) -- datos del ESP32 al sensor
- MISO (Master In, Slave Out) -- datos del sensor al ESP32
- SCK (Serial Clock) -- reloj
- CS (Chip Select) -- uno por dispositivo, para seleccionar con quien hablas

    ESP32             Sensor A          Sensor B
    [MOSI] ------------>|<----------------->|
    [MISO] <------------|<------------------|
    [SCK]  ------------>|<----------------->|
    [CS_A] ------------>|                   |
    [CS_B] -------------------------------->|

Para hablar con el Sensor A, el ESP32 pone CS_A en bajo. Para hablar con B, pone CS_B en bajo. Es como tener lineas telefonicas dedicadas: mas cables, pero comunicacion mucho mas rapida y sin "colisiones".

**Analogia**: SPI es como tener una linea telefonica directa con cada persona. Mas cables (mas instalacion), pero puedes hablar a toda velocidad sin esperar turno.

**1-Wire**

Un solo cable de datos (mas tierra). Usado por sensores como el DS18B20 (sensor de temperatura sumergible, muy comun en acuicultura).

    ESP32              DS18B20 #1       DS18B20 #2       DS18B20 #3
    [GPIO] ------+----------+----------------+----------------+
                 |
             (pull-up)
                 |
               3.3V

Cada dispositivo 1-Wire tiene un identificador unico de 64 bits grabado de fabrica. El ESP32 puede descubrir todos los dispositivos del bus y hablar con cada uno individualmente.

**Analogia**: 1-Wire es como un walkie-talkie con un solo canal. Todos estan en la misma frecuencia y se turnan para hablar. Es lento, pero para un sensor de temperatura que se lee cada pocos segundos, es mas que suficiente.

**Cuando usar cada uno:**

    +-----------+--------+-----------+---------------------------+
    | Protocolo | Cables | Velocidad | Uso tipico en el proyecto |
    +-----------+--------+-----------+---------------------------+
    | I2C       |   2    | Media     | pH, oxigeno disuelto      |
    | SPI       |   4+   | Alta      | Pantallas, SD cards       |
    | 1-Wire    |   1    | Baja      | DS18B20 (temperatura agua)|
    +-----------+--------+-----------+---------------------------+

---

### ADC: convirtiendo el mundo analogico

El mundo real es analogico. La temperatura no salta de 20.0 a 21.0 grados; pasa por todos los valores intermedios (20.001, 20.002...). Los sensores analogicos producen un voltaje continuo proporcional a lo que miden: mas temperatura, mas voltaje.

Pero el ESP32 es digital. Solo entiende numeros. El ADC (Analog to Digital Converter, conversor analogico-digital) es el puente entre estos dos mundos: mide un voltaje y lo convierte en un numero.

    Mundo analogico         ADC          Mundo digital

    Voltaje continuo   +-----------+    Numero entero
    0.0V ... 3.3V  --> | Muestreo  | --> 0 ... 4095
                       | 12 bits   |
                       +-----------+

El ADC del ESP32 tiene resolucion de 12 bits, lo que significa que puede distinguir 2^12 = 4096 niveles diferentes (del 0 al 4095). Si el rango es 0 a 3.3V, cada "paso" representa aproximadamente 0.8 mV.

**Atenuacion**: Por defecto, el ADC del ESP32 solo puede leer voltajes hasta aproximadamente 1.1V. Si tu sensor produce hasta 3.3V, necesitas configurar una atenuacion. La atenuacion es como ponerle gafas de sol al ADC: reduce la "intensidad" de la senal para que entre en su rango de medida.

    Sin atenuacion:  rango 0 - ~1.1V   (para senales pequenas)
    Atenuacion 2.5dB:  rango 0 - ~1.5V
    Atenuacion 6dB:    rango 0 - ~2.2V
    Atenuacion 11dB:   rango 0 - ~3.1V  (el mas comun, cubre casi todo el rango)

**Calibracion**: Aqui viene el problema. El ADC del ESP32 no es perfecto. Si le aplicas exactamente 1.65V (la mitad de 3.3V), no necesariamente vas a leer 2048 (la mitad de 4095). Hay no-linealidades, variaciones de fabrica y ruido. La calibracion corrige estas imperfecciones.

**Analogia**: El ADC es como una regla para medir voltajes. Tiene marcas del 0 al 4095. Pero imagina que las marcas de la regla no estan perfectamente espaciadas: algunas estan mas juntas, otras mas separadas. Si mides con esa regla sin corregir, tus medidas tendran error. La calibracion es como crear una tabla de correcciones: "cuando la regla dice 2000, en realidad son 1980". ESP-IDF incluye rutinas de calibracion que hacen esto automaticamente usando datos almacenados de fabrica en el eFuse del chip.

Para un proyecto de acuicultura donde medimos pH con precision de 0.1, la calibracion del ADC no es opcional: es imprescindible.

---

### ESP-NOW: comunicacion sin router

En una red WiFi normal, todos los dispositivos se conectan a un router (punto de acceso). Sin router, no hay comunicacion. ESP-NOW rompe esta dependencia: permite que dos ESP32 se comuniquen directamente, sin necesidad de ningun router intermedio.

    Red WiFi tradicional:          ESP-NOW:

        [Router]                   [Nodo A] <------> [Nodo B]
       /   |    \                     |
     [A]  [B]  [C]                    +----------> [Nodo C]

    Todos dependen del router.     Comunicacion directa punto a punto.
    Si el router cae, nadie habla. No hay punto unico de fallo.

Caracteristicas clave de ESP-NOW:

- Maximo **250 bytes por mensaje**. Parece poco, pero una lectura de sensor (tipo + valor + timestamp) cabe sobradamente.
- Latencia muy baja: **menos de 10 milisegundos** desde que envias hasta que llega.
- **Bajo consumo**: no necesita mantener una conexion WiFi activa.
- Soporta hasta **20 peers cifrados** (o 6 cifrados + 14 sin cifrar si hay limitaciones de memoria).
- Funciona en la capa de enlace de datos (no necesita IP, no necesita TCP/UDP).

**Requisito critico**: Todos los dispositivos que se comunican por ESP-NOW deben estar en el **mismo canal WiFi**. Si el gateway esta en el canal 6, todos los nodos deben estar en el canal 6. Si el gateway cambia de canal (por ejemplo, al reconectarse al router), los nodos pierden la comunicacion. Esto hay que gestionarlo con cuidado.

**Analogia**: ESP-NOW es como un walkie-talkie digital. No necesitas una antena de telefonia (router), solo que ambos walkie-talkies esten en la misma frecuencia (canal). Pulsas el boton, hablas, y el otro te escucha casi al instante. El mensaje es corto (250 bytes es como una frase), pero para decir "la temperatura es 24.5 grados" es mas que suficiente.

En nuestro proyecto, los nodos ESP32-C3 leen sensores y envian los datos por ESP-NOW al gateway ESP32-S3. No necesitan conectarse a Internet. No necesitan IP. Solo gritan su dato al aire y el gateway lo recoge.

---

### Modos WiFi: STA, AP y APSTA

El ESP32 tiene un modulo WiFi muy versatil que puede funcionar en tres modos. Entender estos modos es crucial para el diseno del gateway.

**STA (Station) -- "Soy un cliente"**

    [Router de la granja]
           |
           | WiFi
           |
    [ESP32 en modo STA]

El ESP32 se comporta como tu movil o tu portatil: se conecta a un punto de acceso existente (un router), recibe una IP y puede acceder a Internet. En este modo puede enviar datos por MQTT a un servidor en la nube, sincronizar la hora por NTP, etc.

**AP (Access Point) -- "Soy el router"**

    [ESP32 en modo AP]
       /       \
      /   WiFi  \
    [Movil]  [Portatil]

El ESP32 crea su propia red WiFi. Otros dispositivos se conectan a el. Util para configuracion inicial: si llegas a la granja y el router no funciona, puedes conectarte directamente al ESP32 con tu movil para ver datos o cambiar ajustes. Pero en este modo, el ESP32 NO tiene acceso a Internet.

**APSTA (Access Point + Station) -- "Soy ambos a la vez"**

    [Router de la granja]
           |
           | WiFi (STA: conectado al router)
           |
    [ESP32 en modo APSTA]
       /       \
      / WiFi AP \
    [Movil]  [Portatil de mantenimiento]

Este es el modo estrella del gateway. El ESP32-S3 se conecta al router de la granja como cliente (STA) para tener Internet y enviar datos por MQTT, y al mismo tiempo crea su propia red WiFi (AP) para que un tecnico pueda conectarse localmente con el movil y acceder a un panel de control web.

    +-------------------------------------------------------+
    |                  ESP32-S3 Gateway                      |
    |                                                        |
    |  [STA] --WiFi--> Router --> Internet --> MQTT Broker   |
    |                                                        |
    |  [AP] <--WiFi-- Movil del tecnico (panel web local)    |
    |                                                        |
    |  [ESP-NOW] <-- Datos de nodos C3                       |
    +-------------------------------------------------------+

El gateway vive en tres mundos a la vez: recibe datos de los nodos por ESP-NOW, los reenvia a la nube por WiFi STA, y atiende peticiones locales por WiFi AP. Esa es su complejidad y su potencia.

---

### Modos de energia: del activo al deep sleep

Un ESP32 alimentado por bateria no puede estar siempre encendido a tope. Consumiria la bateria en horas. Por eso el chip ofrece varios modos de energia, cada uno con un compromiso diferente entre consumo y capacidad:

    Consumo          Capacidad
    alto                completa
     |   +----------------------------------+
     |   | ACTIVO (~240 mA)                 |  Todo encendido: CPU, WiFi, RAM,
     |   | La casa con todas las luces      |  perifericos. Maximo rendimiento.
     |   +----------------------------------+
     |
     |   +----------------------------------+
     |   | MODEM SLEEP (~20 mA)             |  CPU funciona, pero la radio WiFi
     |   | Apagaste las luces del jardin    |  se apaga entre beacons. Ahorra
     |   +----------------------------------+  bastante si no transmites datos
     |                                         constantemente.
     |   +----------------------------------+
     |   | LIGHT SLEEP (~0.8 mA)            |  CPU pausada, RAM intacta. Como
     |   | Casa en standby: TV apagada      |  hibernar un portatil. Al despertar,
     |   | pero todo listo para encender    |  continua exactamente donde lo dejo.
     |   +----------------------------------+
     |
     v   +----------------------------------+
    bajo | DEEP SLEEP (~10 uA)              |  Casi todo apagado. Solo queda vivo
         | Casa con la luz cortada;         |  el RTC (Real Time Clock) y 8 KB de
         | solo el despertador funciona     |  memoria RTC. Al despertar, el
         +----------------------------------+  programa arranca DESDE CERO, como
                                               un reinicio. Solo los 8 KB de RTC
                                               sobreviven.

**Detalle critico del deep sleep**: Cuando el ESP32 entra en deep sleep, todo el contenido de la RAM principal se pierde. Las variables, las conexiones, el estado de tu programa: todo desaparece. Al despertar, ejecuta el bootloader y arranca app_main() desde el principio, como si hubieras pulsado el boton de reset. La unica forma de pasar informacion entre ciclos de sueno es escribirla en la **memoria RTC** (8 KB) antes de dormir, o en NVS (flash) si necesitas mas espacio.

Fuentes de despertar del deep sleep:
- **Timer**: "despiertame en 60 segundos"
- **GPIO**: "despiertame si alguien pulsa este boton"
- **ULP** (Ultra Low Power coprocessor): un coprocesador diminuto que puede hacer mediciones simples mientras el ESP32 principal duerme

Para los nodos de bateria del proyecto, el ciclo de vida es:

    Despertar --> Leer sensores --> Enviar por ESP-NOW --> Dormir
       |                                                     |
       +--------- 60 segundos de deep sleep -----------------+

Este ciclo puede mantener un nodo C3 funcionando durante meses con una bateria modesta.

---

### UART y el monitor serie

UART (Universal Asynchronous Receiver-Transmitter) es un protocolo de comunicacion serie, uno de los mas antiguos y sencillos que existen. Cuando conectas tu placa ESP32 al ordenador por USB, el cable USB lleva internamente senales UART convertidas a USB por un chip adaptador (normalmente un CP2102 o CH340 en las placas de desarrollo).

    +----------+       Cable USB       +----------+
    |  ESP32   |  TX ------>  RX  ---> | Ordenador|
    |          |  RX <------  TX  <--- |          |
    +----------+                       +----------+
                  Dentro del cable:
                  UART <-> USB (chip conversor)

Cuando ejecutas idf.py monitor, tu ordenador se conecta al puerto serie (normalmente /dev/ttyUSB0 en Linux o /dev/ttyACM0) y muestra todo lo que el ESP32 envia por UART. Es tu ventana al interior del microcontrolador.

**Baud rate**: Es la velocidad a la que se transmiten los datos. Debe ser la misma en ambos extremos. El valor por defecto en ESP-IDF es 115200 baudios. Si configuras uno diferente en el ESP32 y no cambias el del monitor, veras caracteres basura en pantalla, como si alguien hablara a doble velocidad y no lo entendieras.

**El sistema de logging de ESP-IDF**: En vez de usar printf() a pelo, ESP-IDF ofrece macros de logging con niveles y colores:

- ESP_LOGE(tag, ...) -- Error (rojo). Algo fallo gravemente.
- ESP_LOGW(tag, ...) -- Warning (amarillo). Algo raro pero no fatal.
- ESP_LOGI(tag, ...) -- Info (verde). Informacion general, el mas comun.
- ESP_LOGD(tag, ...) -- Debug (sin color). Detalle para depuracion.
- ESP_LOGV(tag, ...) -- Verbose (sin color). Detalle extremo, raramente usado.

El "tag" es una etiqueta de texto que identifica el modulo que genera el mensaje. Cuando tienes 15 componentes imprimiendo mensajes, el tag te permite filtrar y encontrar lo que buscas.

En menuconfig puedes configurar el nivel de log global: si lo pones en "Info", los mensajes de Debug y Verbose no se compilaran (ni siquiera ocupan espacio en flash). Esto es util en produccion para reducir el tamano del binario.

**Analogia**: El monitor serie es como la ventanilla de cristal de una sala de servidores. No puedes entrar fisicamente al ESP32 a ver que pasa, pero a traves de esta ventanilla puedes leer todos los carteles que el ESP32 va pegando en las paredes (los mensajes de log). Los colores y etiquetas te ayudan a encontrar rapidamente el cartel que buscas.

---

## Como encaja en el proyecto

La Fase 0 es los cimientos. Aqui no vas a construir el sistema de monitoreo de peces todavia. Lo que vas a hacer es aprender las herramientas, validar que tu hardware funciona y sentar las bases sobre las que se construira todo lo demas.

Cada concepto de este tutorial tiene un destino claro en las fases futuras:

    Fase 0 (tu estas aqui)          Fases futuras
    ========================        ===============================

    GPIO .......................... Fase 3: Control de actuadores
                                    (reles para bombas, alimentadores)

    I2C / ADC .................... Fase 3: Lectura de sensores
                                    (pH, oxigeno disuelto, turbidez)

    1-Wire ....................... Fase 3: Sensores DS18B20
                                    (temperatura del agua)

    ESP-NOW ...................... Fase 2: Comunicacion nodo -> gateway
                                    (envio de datos de sensores)

    WiFi APSTA .................. Fase 1: Gateway conectado a Internet
                                    (MQTT + panel web local)

    Deep sleep .................. Fase 2: Nodos de bateria
                                    (ciclos despertar-medir-enviar-dormir)

    Particiones / OTA ........... Fases avanzadas: Actualizaciones
                                    remotas del firmware

    UART / Monitor .............. TODAS las fases: Tu herramienta
                                    de depuracion constante

Sin entender estos fundamentos, las fases posteriores seran como construir un edificio sin saber que es el hormigon. Tomate el tiempo necesario aqui. Cuando llegues a la Fase 1 y configures el WiFi APSTA del gateway, vas a agradecer haber entendido la diferencia entre STA y AP. Cuando en la Fase 2 los nodos no se comuniquen, sabras que lo primero que hay que verificar es que estan en el mismo canal WiFi.

---

## Errores tipicos y como evitarlos

1. **Seleccionar el target equivocado al compilar.** Si configuras el target como esp32s3 pero tu placa es un C3 (o viceversa), el binario compilado usara instrucciones de procesador incompatibles. El chip no arrancara o se comportara de forma impredecible. Verifica siempre el target antes de compilar. El toolchain de Xtensa genera codigo que un RISC-V no puede ejecutar y viceversa.

2. **Baud rate del monitor no coincide con el del firmware.** Veras caracteres ilegibles en el terminal. Esto ocurre porque emisor y receptor hablan a velocidades diferentes, como intentar escuchar un audio a doble velocidad. Asegurate de que el baud rate configurado en menuconfig coincida con el del comando idf.py monitor (por defecto ambos usan 115200).

3. **Pin GPIO flotante leyendo valores erraticos.** Si configuras un pin como entrada pero olvidas activar el pull-up o pull-down (y no tienes resistencia externa), el pin capta ruido electromagnetico y da lecturas aleatorias. Activa siempre la resistencia interna correspondiente al configurar un pin de entrada.

4. **Olvidar que deep sleep reinicia el programa.** Un error muy comun es asumir que las variables se mantienen despues de despertar del deep sleep. No es asi. El ESP32 reinicia completamente, como si le hubieras quitado la corriente y se la hubieras vuelto a poner. Solo la memoria RTC (8 KB) y NVS (flash) sobreviven. Si necesitas guardar un contador de ciclos, guardalo en RTC antes de dormir.

5. **Nodos ESP-NOW en canales WiFi diferentes.** Si el gateway esta en modo APSTA y el router le asigna el canal 11, pero los nodos estan configurados para el canal 1, no se comunicaran. ESP-NOW opera en la capa de enlace de datos y necesita que todos los dispositivos esten en el mismo canal de radio. Implementa un mecanismo para que los nodos descubran el canal del gateway o fija el canal de forma explicita.

6. **No calibrar el ADC y confiar en valores crudos.** Los valores sin calibrar del ADC del ESP32 pueden tener errores significativos, especialmente en los extremos del rango. Si lees un pH de 7.2 pero en realidad es 6.8, puedes tomar decisiones erroneas sobre la calidad del agua. Usa siempre las funciones de calibracion que ofrece ESP-IDF.

7. **Intentar enviar mas de 250 bytes en un mensaje ESP-NOW.** El limite es estricto. Si tu estructura de datos supera los 250 bytes, el envio fallara silenciosamente o con error. Disena tus mensajes para que sean compactos. Si necesitas enviar mas datos, fragmentalos en multiples mensajes.

8. **Usar el modo AP esperando tener acceso a Internet.** El modo AP convierte al ESP32 en un punto de acceso local, pero no tiene conexion a Internet. Si necesitas enviar datos a un servidor MQTT en la nube, necesitas el modo STA (o APSTA). El modo AP solo sirve para comunicacion local entre el ESP32 y los dispositivos conectados a el.

---

## Experimenta

> **Reto 1**: Configura dos ESP32 con ESP-NOW, uno como emisor y otro como receptor. Luego cambia el canal WiFi solo en uno de ellos. Que ocurre? Puedes verificar la perdida de paquetes?

> **Reto 2**: Pon un ESP32-C3 en deep sleep con un timer de 10 segundos. Mide con un multimetro el consumo en deep sleep y comparalo con el consumo en activo. Si tienes un analizador de corriente, mide cuanto tarda desde que despierta hasta que envia un paquete ESP-NOW. Cuanta bateria necesitarias para un anio?

> **Reto 3**: Conecta un sensor analogico (un potenciometro o un LDR sirven) a un pin ADC. Lee los valores crudos (sin calibracion) y luego activala. Compara los resultados. Cuanto error habia sin calibracion?

> **Reto 4**: Configura un ESP32 en modo APSTA. Conectate a su AP con el movil y al mismo tiempo verifica que el ESP32 tiene conexion a Internet haciendo un ping a un servidor. Puedes ver el ESP32 en ambas redes simultaneamente?

---

## Para profundizar

- Documentacion oficial de ESP-IDF (version estable): https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/
- Guia de inicio rapido ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/
- Referencia de la API de GPIO: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html
- Referencia de la API de ADC (calibracion incluida): https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc_oneshot.html
- Guia de ESP-NOW: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html
- Documentacion de modos de WiFi: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/wifi.html
- Guia de sleep modes: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html
- Datasheet del ESP32-S3: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- Datasheet del ESP32-C3: https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf
- Documentacion de particiones: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/partition-tables.html
- Guia del sistema de build (CMake): https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/build-system.html
- Blog de Espressif (articulos tecnicos y tutoriales): https://blog.espressif.com/

---

## Preguntas de reflexion

1. Si el gateway ESP32-S3 esta en modo APSTA conectado a un router en el canal 6, y un nodo ESP32-C3 intenta comunicarse por ESP-NOW en el canal 1, que pasara? Como lo solucionarias sin cambiar la configuracion del router?

2. Un nodo sensor entra en deep sleep durante 60 segundos, luego despierta, lee los sensores y envia los datos por ESP-NOW. Necesitas contar cuantos ciclos lleva desde la ultima vez que se reseteo manualmente. Donde guardarias ese contador y por que?

3. Tienes un sensor de pH conectado por I2C y tres sondas de temperatura DS18B20 en la misma pecera. Usarias el mismo bus de comunicacion para todos? Que protocolo usarias para cada uno y por que?

4. Imagina que tu ADC lee un valor de 2048 cuando aplicas exactamente 1.65V (la mitad del rango). Eso significa que el ADC esta bien calibrado? Que comprobaciones adicionales harias para asegurarte?

5. Un tecnico de la granja necesita ver los datos de los sensores en su movil sin tener cobertura de Internet. Que modo WiFi del gateway le permitiria hacerlo? Podria al mismo tiempo el gateway seguir enviando datos a la nube si hay un router disponible?

6. Explica con tus propias palabras por que ESP-NOW es mejor opcion que WiFi tradicional para los nodos sensor de bateria. Menciona al menos tres ventajas concretas.

7. Si tu firmware ocupa 1.5 MB y la flash es de 4 MB, podrias implementar OTA con dos particiones de aplicacion (OTA_0 y OTA_1)? Haz el calculo considerando bootloader, tabla de particiones y NVS.

8. Un companero ha configurado un pin GPIO como entrada para leer un boton, pero el boton "se activa solo" de vez en cuando sin que nadie lo toque. Cual es la causa mas probable y como lo arreglarias?
