# Tutorial 2: Nodos Autonomos y Comunicacion Fiable

## Que vas a aprender en este tutorial

En el tutorial anterior conociste el panorama general del proyecto: un gateway central, nodos sensores distribuidos y el protocolo ESP-NOW como medio de comunicacion. Ahora vamos a sumergirnos en el corazon del sistema: los nodos autonomos.

Un nodo autonomo es como un trabajador de campo en una instalacion industrial. Se despierta, toma las mediciones del entorno monitorizado, envia su informe a la oficina central (el gateway) y se vuelve a dormir. No necesita supervision constante. No necesita estar conectado a la corriente. Funciona con bateria durante meses.

En este tutorial vas a entender:

- Deep sleep en detalle: que partes del ESP32 se apagan, cuales sobreviven y como se despierta
- RTC memory: la pequena memoria que recuerda datos entre suenos
- Como disenar un protocolo binario eficiente (y por que no usamos JSON)
- Fiabilidad sobre enlaces no fiables: ACK, reintentos y numeros de secuencia
- Auto-discovery: como el gateway aprende automaticamente sobre nuevos nodos
- Presupuestos de energia: como calcular cuanto durara una bateria
- Conceptos basicos de FreeRTOS que necesitas para entender el firmware de los nodos

No vamos a ver codigo completo aqui. Vamos a construir la base conceptual que necesitas para entender por que el firmware esta escrito como esta.

---

## Conceptos clave

### Deep sleep en profundidad

Para entender el deep sleep, primero hay que entender que tiene dentro un ESP32.

Un ESP32 (ya sea el S3 o el C3) no es solo un procesador. Es un sistema completo en un chip, con varias "zonas" internas:

    +-----------------------------------------------+
    |                  ESP32-S3/C3                   |
    |                                                |
    |  +----------+  +----------+  +-----------+    |
    |  |   CPU    |  |   WiFi   |  | Bluetooth |    |
    |  | (1 o 2   |  |  radio   |  |   radio   |    |
    |  |  nucleos)|  +----------+  +-----------+    |
    |  +----------+                                  |
    |  +----------+  +----------+  +-----------+    |
    |  |   RAM    |  | Periph.  |  |   Flash   |    |
    |  | 400-520KB|  | (SPI,I2C |  |  externa  |    |
    |  |          |  |  UART...) |  |   4MB+    |    |
    |  +----------+  +----------+  +-----------+    |
    |                                                |
    |  +-------------------------------------------+ |
    |  |        Dominio RTC (siempre vivo)         | |
    |  |  +--------+  +--------+  +----------+    | |
    |  |  | Reloj  |  |  RTC   |  | Algunos  |    | |
    |  |  | RTC    |  | Memory |  | GPIOs    |    | |
    |  |  +--------+  | (~8KB) |  +----------+    | |
    |  |               +--------+                  | |
    |  +-------------------------------------------+ |
    +-----------------------------------------------+

Cuando el ESP32 entra en deep sleep, ocurre algo drastico: casi todo se apaga. La CPU deja de ejecutar instrucciones. La RAM principal pierde todo su contenido. El WiFi y Bluetooth se desconectan. Los perifericos (SPI, I2C, UART) dejan de funcionar. La flash externa queda inaccesible.

Lo unico que permanece vivo es el dominio RTC: un controlador muy simple con su propio reloj, una pequena memoria (~8KB) y opcionalmente algunos pines GPIO.

La analogia perfecta: deep sleep es como la hibernacion de un oso. El cuerpo se apaga casi por completo. El metabolismo cae al minimo. Pero un reloj biologico interno sigue funcionando, y cuando llega la primavera (o el tiempo configurado), el oso se despierta.

Y aqui viene lo mas importante que debes entender: cuando el ESP32 se despierta del deep sleep, NO retoma la ejecucion donde la dejo. Es como un arranque en frio. El bootloader se ejecuta desde cero. La funcion app_main() comienza desde su primera linea. Para el programa, es como si el ESP32 se encendiera por primera vez.

Esto cambia completamente como disenas el firmware. No puedes pensar "hago una lectura, espero 5 minutos, hago otra lectura" como harias en Arduino con un delay(). En lugar de eso, piensas: "cada vez que me despierto, leo sensores, envio datos y me vuelvo a dormir".

**Fuentes de despertar (wake sources)**

El ESP32 puede despertar del deep sleep por varias razones:

1. Temporizador (timer): la mas comun para nodos sensores. Le dices "despiertame en 300 segundos" y el reloj RTC cuenta el tiempo. Cuando se cumple, arranca de nuevo.

2. Pin GPIO externo: un sensor puede generar una senal que despierta al ESP32. Util para detectar eventos como un nivel de agua critico.

3. Touchpad: los pines capacitivos del ESP32 pueden detectar un toque incluso en deep sleep. No lo usamos en este proyecto, pero existe.

4. Coprocesador ULP: un procesador ultra simple dentro del dominio RTC que puede ejecutar pequenos programas mientras la CPU principal duerme. Puede leer sensores analogicos y despertar a la CPU solo si el valor es interesante.

Para nuestros nodos IIoT, usamos exclusivamente el temporizador. El nodo se despierta cada N minutos (configurable, por defecto 5 minutos), hace su trabajo y vuelve a dormir.

**Como sabe el programa si es arranque en frio o despertar de deep sleep?**

ESP-IDF proporciona una funcion que retorna la causa del reset. Si la causa es "despertar de deep sleep por timer", sabes que tus datos en RTC memory son validos. Si la causa es "power on" o "reset externo", sabes que es la primera vez y debes inicializar todo desde cero.

---

### RTC memory: la memoria persistente del sueno

Ya dijimos que la RAM principal se pierde en deep sleep. En un ESP32-S3 son 520KB de RAM y en un C3 son 400KB. Todo eso desaparece.

Pero la RTC memory (~8KB) sobrevive. Es poca, si. Pero es suficiente para lo que necesitamos.

Piensa en la RTC memory como la mesita de noche de alguien que duerme. No puedes poner un armario entero ahi, pero si puedes dejar una nota con lo esencial: "manana tengo que hacer X, llevo Y intentos fallidos, mi ultimo numero de secuencia fue Z".

**Que guardamos en RTC memory para nuestros nodos:**

- Contador de secuencia de paquetes: un numero que incrementa con cada envio. Permite al gateway detectar paquetes perdidos o duplicados.
- Contador de fallos consecutivos: cuantas veces seguidas no hemos recibido ACK del gateway. Si llega a 5, activamos modo ahorro de energia.
- Intervalo de sueno actual: normalmente 5 minutos, pero puede cambiar a 30 minutos en modo ahorro.
- Timestamp del ultimo envio exitoso: para saber cuanto tiempo llevamos sin comunicacion.
- Flags de estado: si estamos en modo ahorro, si necesitamos re-enviar discovery, etc.

**El ciclo de vida segun la RTC memory:**

    Primer encendido (cold boot):
    +---------------------------+
    | RTC memory = basura       |
    | Detectar: causa = POWERON |
    | Inicializar todo a cero   |
    | seq_num = 0               |
    | fail_count = 0            |
    | sleep_interval = 300s     |
    +---------------------------+
             |
             v
    Funcionamiento normal (warm boot):
    +---------------------------+
    | RTC memory = valores      |
    | previos conservados       |
    | Detectar: causa = TIMER   |
    | Leer seq_num, incrementar |
    | Enviar datos              |
    | Si ACK: fail_count = 0    |
    | Si no ACK: fail_count++   |
    | Dormir sleep_interval     |
    +---------------------------+
             |
             v
    (se repite cada despertar)

En el cold boot, la RTC memory contiene valores aleatorios (basura). Por eso es critico verificar la causa del arranque antes de confiar en esos datos. Si es power-on, inicializas. Si es despertar de deep sleep, confias en los valores existentes.

Para colocar una variable en RTC memory, se usa un atributo especial en la declaracion de la variable que le dice al compilador y al linker: "esta variable no va en la RAM normal, va en la seccion RTC". Es una sola palabra clave antes de la declaracion; no cambia en nada como usas la variable despues.

La limitacion es clara: solo ~8KB. No puedes almacenar buffers grandes ni historiales. Solo lo esencial para mantener la continuidad entre ciclos de sueno.

---

### Disenando un protocolo binario

**Por que no JSON?**

Si vienes del mundo web, tu primer instinto seria enviar algo como {"temp":25.3,"ph":7.1,"node":"AA:BB:CC"}. En un servidor con 16GB de RAM y un procesador a 3GHz, eso funciona perfecto.

Pero en un ESP32 con 400KB de RAM, un procesador a 160MHz y un limite de 250 bytes por paquete ESP-NOW, JSON es un desperdicio:

- Parsing de JSON requiere una biblioteca que ocupa flash y RAM.
- Las cadenas de texto son mucho mas largas que los datos puros. El texto "temperature" ocupa 11 bytes. El valor como numero flotante ocupa 4 bytes.
- Cada par de llaves, comillas, comas y dos puntos son bytes desperdiciados.
- Hay que manejar errores de parsing (JSON malformado).

La alternativa: un protocolo binario con estructuras de tamano fijo.

**La analogia: carta vs. telegrama**

JSON es como enviar una carta: "Estimado senor, le informo que la temperatura de la zona numero tres es de veinticinco punto tres grados celsius y el pH medido a las catorce horas fue de siete punto uno".

Un protocolo binario es como un telegrama donde cada caracter cuesta: "E3 T25.3 P7.1 H14". Cada posicion tiene un significado predefinido, ambas partes conocen el formato, no se necesitan etiquetas.

**Estructura de nuestro protocolo**

Nuestro protocolo tiene dos partes: un encabezado (header) comun a todos los mensajes, y un payload (carga util) que varia segun el tipo de mensaje.

    Encabezado (13 bytes):
    +------+------+--------+--------+-----------+
    | Byte | Long | Campo  | Tipo   | Proposito |
    +------+------+--------+--------+-----------+
    |  0   |  1   | msg_type| uint8 | Tipo msg  |
    |  1-6 |  6   | src_mac | bytes | MAC origen|
    |  7-8 |  2   | seq_num | uint16| Secuencia |
    | 9-12 |  4   | timestamp| uint32| Tiempo   |
    +------+------+--------+--------+-----------+
    Total encabezado: 13 bytes

    Tipos de mensaje:
    0x01 = DATA        (datos de sensores)
    0x02 = DISCOVERY   (nodo nuevo se presenta)
    0x03 = ACK         (confirmacion del gateway)
    0x04 = CONFIG      (gateway envia configuracion)

    Payload DATA (ejemplo: 20 bytes):
    +------+------+----------+---------+
    | Byte | Long | Campo    | Tipo    |
    +------+------+----------+---------+
    | 0-3  |  4   | temp     | float32 |
    | 4-7  |  4   | pH       | float32 |
    | 8-11 |  4   | OD       | float32 |
    | 12-15|  4   | turbidez | float32 |
    | 16-17|  2   | bateria  | uint16  |
    | 18   |  1   | status   | uint8   |
    | 19   |  1   | reservado| uint8   |
    +------+------+----------+---------+

    Paquete total: 13 + 20 = 33 bytes
    (muy lejos del limite de 250 bytes de ESP-NOW)

**Consideraciones de empaquetado (struct packing)**

Los compiladores de C a veces insertan bytes de "relleno" entre campos de una estructura para alinearlos en memoria. Por ejemplo, despues de un campo de 1 byte, el compilador puede agregar 3 bytes vacios para que el siguiente campo de 4 bytes quede alineado.

Esto es un problema para protocolos de comunicacion: si el nodo y el gateway usan diferente relleno, interpretan los bytes de manera distinta. La solucion es decirle al compilador "empaqueta esta estructura sin relleno, cada campo va justo despues del anterior". Se hace con un atributo de compilador que fuerza el empaquetado sin espacios.

**Orden de bytes (byte order)**

Los numeros de mas de un byte (como uint16 o float32) se pueden almacenar de dos formas: little-endian (el byte menos significativo primero) o big-endian (el byte mas significativo primero). Como todos nuestros dispositivos son ESP32 (todos little-endian), no hay problema. Pero si algun dia el gateway se comunica con otros sistemas, habria que definir un orden de bytes para el protocolo.

---

### Fiabilidad: ACK, reintentos y numeros de secuencia

ESP-NOW es una maravilla de simplicidad: envias un paquete y llega al otro lado en milisegundos, sin necesidad de conectarte a un access point WiFi. Pero no es perfecto.

Los paquetes se pueden perder por:
- Interferencia de otras redes WiFi
- Distancia excesiva entre nodo y gateway
- Obstaculos fisicos (paredes, estructuras metalicas de la instalacion)
- Colisiones si dos nodos transmiten al mismo tiempo
- El gateway estaba ocupado procesando otro paquete

Si un nodo envia una lectura de pH critico y ese paquete se pierde, nadie se entera. En una instalacion industrial, eso puede significar un proceso fuera de control sin alerta.

Por eso necesitamos un protocolo de fiabilidad encima de ESP-NOW. Nuestro protocolo tiene cuatro mecanismos:

**1. ACK (acknowledgement / acuse de recibo)**

Cuando el gateway recibe un paquete, envia una confirmacion al nodo. Es como firmar un acuse de recibo cuando te entregan un paquete.

    Nodo                    Gateway
      |                        |
      |--- Datos (seq #5) ---->|
      |                        | (procesa datos)
      |<---- ACK (seq #5) ----|
      |                        |
      | (recibio ACK, todo OK) |
      | (se va a dormir)       |

El ACK incluye el numero de secuencia del paquete que confirma. Asi el nodo sabe exactamente cual paquete fue recibido.

**2. Reintentos (retries)**

Si el nodo no recibe ACK dentro de un tiempo limite (500ms en nuestro caso), asume que el paquete se perdio y lo reenvia. Maximo 3 intentos.

    Nodo                    Gateway
      |                        |
      |--- Datos (seq #5) ---->|  (se pierde en el aire)
      |                        |
      | (500ms sin ACK...)     |
      |                        |
      |--- Datos (seq #5) ---->|  (esta vez llega)
      |                        | (procesa datos)
      |<---- ACK (seq #5) ----|
      |                        |
      | (recibio ACK, OK)      |
      | (se va a dormir)       |

Si despues de 3 intentos no hay ACK, el nodo incrementa su contador de fallos y se va a dormir. Lo intentara en el proximo ciclo.

    Nodo                    Gateway
      |                        |
      |--- Datos (seq #5) --->X  (perdido)
      | (500ms...)             |
      |--- Datos (seq #5) --->X  (perdido)
      | (500ms...)             |
      |--- Datos (seq #5) --->X  (perdido)
      | (500ms...)             |
      |                        |
      | fail_count++           |
      | Si fail_count >= 5:    |
      |   sleep = 30 minutos   |
      | Dormir                 |

**3. Numeros de secuencia**

Cada paquete lleva un numero que incrementa con cada envio. Esto sirve para dos cosas:

Detectar duplicados: si el ACK se pierde (pero el dato si llego), el nodo reenvia. El gateway recibe el mismo paquete dos veces. Gracias al numero de secuencia, detecta: "este #5 ya lo procese, no lo guardo de nuevo, solo reenvio el ACK".

Detectar huecos: si el gateway recibe #5 y luego #7, sabe que #6 se perdio. Puede registrar esta perdida para monitoreo de calidad del enlace.

    Secuencia normal:
    #1 -> #2 -> #3 -> #4 -> #5   (todo bien)

    Duplicado detectado:
    #1 -> #2 -> #3 -> #3 -> #4   (el segundo #3 se ignora)

    Hueco detectado:
    #1 -> #2 -> #4 -> #5         (falta #3, se registra como perdido)

**4. Idempotencia**

Procesar el mismo mensaje dos veces debe producir el mismo resultado que procesarlo una vez. Si el gateway recibe dos veces la lectura "temp=25.3, seq=#5", solo guarda una. No suma dos lecturas, no crea dos registros. Este principio se llama idempotencia.

Es como depositar un cheque con numero: si el banco recibe el cheque numero 1234 dos veces, solo lo procesa una vez. El numero identifica de forma unica la operacion.

**El flujo completo de un ciclo exitoso:**

    1. Nodo despierta (deep sleep -> warm boot)
    2. Lee seq_num de RTC memory (ej: 42)
    3. Inicializa WiFi y ESP-NOW
    4. Lee sensores (temp, pH, OD, turbidez)
    5. Construye paquete binario con seq_num=42
    6. Envia paquete al gateway
    7. Espera ACK (max 500ms)
    8. Recibe ACK con seq_num=42 -> exito
    9. Incrementa seq_num a 43, guarda en RTC memory
    10. fail_count = 0, guarda en RTC memory
    11. Entra en deep sleep por 300 segundos
    12. Vuelve al paso 1

---

### Auto-discovery: nodos que se presentan solos

Imagina que tienes 20 zonas monitorizadas y vas a instalar un nodo sensor en cada una. No quieres tener que conectar cada nodo a un computador para configurarlo, ni editar un archivo en el gateway con las MAC de los 20 nodos.

La solucion: auto-discovery. Cada nodo sabe presentarse solo.

**Como funciona:**

Cuando un nodo se enciende por primera vez (cold boot, no despertar de deep sleep), no sabe si el gateway lo conoce. Entonces, antes de enviar datos, envia un mensaje de tipo DISCOVERY.

    Mensaje DISCOVERY:
    +---------------------------+
    | Header:                   |
    |   msg_type = 0x02         |
    |   src_mac = AA:BB:CC:...  |
    |   seq_num = 0             |
    |   timestamp = 0           |
    +---------------------------+
    | Payload:                  |
    |   sensor_type = "pH+T+OD" |
    |   firmware_version = 2.1  |
    |   wake_interval = 300s    |
    |   battery_mv = 3850      |
    +---------------------------+

El gateway recibe este DISCOVERY y:
1. Busca si ya tiene esa MAC registrada
2. Si no, la agrega a su tabla de nodos conocidos
3. Responde con un ACK (opcionalmente con configuracion)

**La tabla de nodos del gateway:**

El gateway mantiene una tabla interna con informacion de cada nodo:

    +-------------------+--------+----------------+--------+------+
    | MAC               | Tipo   | Ultimo contacto| Pkts   | RSSI |
    +-------------------+--------+----------------+--------+------+
    | AA:BB:CC:DD:EE:01 | pH+T   | hace 5 min     | 1,247  | -45  |
    | AA:BB:CC:DD:EE:02 | pH+T+OD| hace 2 min     |   832  | -62  |
    | AA:BB:CC:DD:EE:03 | pH+T   | hace 35 min    |   156  | -78  |
    +-------------------+--------+----------------+--------+------+

Con esta tabla, el gateway puede:
- Detectar nodos que dejaron de reportar (alerta)
- Calcular la tasa de perdida de paquetes por nodo (huecos en secuencia)
- Monitorear la calidad del enlace (RSSI) para cada nodo
- Saber que tipo de sensores tiene cada nodo

**Beneficios:**

- Plug-and-play: enciendes un nodo nuevo y se integra automaticamente
- Sin configuracion manual: no hay archivos que editar ni bases de datos que llenar
- Resistente a reemplazos: si un nodo se dana y lo reemplazas, el nuevo nodo se presenta solo
- El gateway se convierte en la fuente de verdad sobre el estado de la red

**Re-discovery:**

Un nodo tambien puede enviar DISCOVERY si detecta que lleva muchos ciclos sin recibir ACK. Tal vez el gateway se reinicio y perdio su tabla. El DISCOVERY es la forma de decir "hola, estoy aqui, recuerdame".

---

### Presupuestos de energia

Una de las preguntas mas importantes en un proyecto IoT con baterias es: cuanto va a durar la bateria?

No queremos adivinar. Queremos calcular.

**Los dos estados de un nodo:**

Un nodo solo tiene dos estados relevantes para el consumo:

    Estado ACTIVO (despierto):
    - CPU ejecutando codigo
    - WiFi radio transmitiendo
    - Sensores encendidos
    - Consumo: ~120mA
    - Duracion: ~200ms (0.2 segundos)

    Estado DORMIDO (deep sleep):
    - Casi todo apagado
    - Solo dominio RTC
    - Consumo: ~10uA (0.01mA)
    - Duracion: 300 segundos (5 minutos)

**El calculo:**

Para calcular la vida de la bateria, necesitamos el consumo promedio.

Carga consumida por ciclo activo:
120mA multiplicado por 0.2 segundos = 24 mAs (miliamperio-segundo)

Carga consumida en deep sleep (5 minutos):
0.01mA multiplicado por 300 segundos = 3 mAs

Carga total por ciclo (5 min + 0.2s):
24 + 3 = 27 mAs por ciclo de ~300 segundos

Corriente promedio:
27 mAs dividido entre 300.2 segundos = 0.090 mA = 90 uA

Con una bateria de 3400 mAh (tipica 18650):
3400 mAh dividido entre 0.090 mA = 37,778 horas = 1,574 dias = aprox. 52 meses

En la practica, hay perdidas (autodescarga de la bateria, regulador de voltaje, variaciones de temperatura), asi que un estimado conservador es alrededor del 30-40% de la cifra teorica. Aun asi, estamos hablando de mas de 15 meses de vida con una sola bateria.

**Tabla: vida de bateria segun intervalo de despertar**

    Bateria: 3400mAh | Tiempo activo: 200ms @ 120mA

    +------------------+-------------------+---------------------+
    | Intervalo sueno  | Corriente promedio| Vida estimada (real)|
    +------------------+-------------------+---------------------+
    | 1 minuto         |     0.41 mA       |   ~8 meses          |
    | 5 minutos        |     0.09 mA       |   ~15 meses         |
    | 10 minutos       |     0.05 mA       |   ~18 meses         |
    | 30 minutos       |     0.02 mA       |   ~22 meses         |
    +------------------+-------------------+---------------------+

    (Valores reales considerando ~35% de eficiencia practica
     respecto al maximo teorico)

**Impacto de los reintentos:**

Cada reintento agrega ~200ms de tiempo activo a 120mA. Si un nodo tiene que reintentar 3 veces en cada ciclo, su tiempo activo pasa de 200ms a 800ms, multiplicando por 4 el consumo activo.

Por eso el modo ahorro de energia (power-save) es tan importante: si el gateway no esta disponible, el nodo pasa a despertar cada 30 minutos en vez de cada 5. Sin este mecanismo, un nodo sin gateway podria agotar su bateria en pocos meses por los reintentos continuos:

    Situacion normal (5 min, 1 intento):
    Corriente promedio: ~0.09 mA -> 15+ meses

    Gateway caido SIN modo ahorro (5 min, 3 reintentos cada vez):
    Tiempo activo: 200ms x 3 + esperas = ~1.7s
    Corriente promedio: ~0.69 mA -> ~5 meses

    Gateway caido CON modo ahorro (30 min, 3 reintentos):
    Corriente promedio: ~0.12 mA -> ~13 meses

El modo ahorro es un seguro: protege la bateria cuando las cosas van mal.

**Otros factores que afectan la bateria:**

- Temperatura: las baterias de litio rinden menos en frio. En entornos con temperatura controlada esto generalmente no es problema.
- Autodescarga: una bateria 18650 pierde ~2-3% de carga por mes estando quieta.
- Regulador de voltaje: el circuito que convierte los 3.7V de la bateria a los 3.3V del ESP32 consume algo de corriente.
- Sensores: si tus sensores consumen mucho, o tardan en estabilizarse, el tiempo activo aumenta.

---

### FreeRTOS basico para nodos

ESP-IDF, el framework oficial de Espressif para programar ESP32, esta construido sobre FreeRTOS. Esto significa que, te guste o no, tu codigo se ejecuta dentro de un sistema operativo en tiempo real.

Para los nodos, no necesitas ser un experto en FreeRTOS. Pero hay conceptos que debes entender para no tropezar.

**Tareas (tasks)**

En FreeRTOS, una "tarea" es como un hilo de ejecucion independiente. Cada tarea tiene:
- Su propia pila (stack) de memoria
- Una prioridad (numero mayor = mas importante)
- Un estado (ejecutando, lista, bloqueada, suspendida)

Cuando enciendes el ESP32, FreeRTOS crea varias tareas internas (WiFi, timer, idle) y luego ejecuta tu funcion app_main() dentro de la tarea "main".

Para los nodos, todo nuestro trabajo ocurre en esta tarea main. No creamos tareas adicionales. El flujo es completamente secuencial:

    app_main() {
        inicializar WiFi y ESP-NOW
        leer sensores
        enviar paquete
        esperar ACK
        entrar en deep sleep
    }

No hay hilos paralelos, no hay sincronizacion compleja. Es simple a proposito.

**Pero entonces, por que importa FreeRTOS?**

Por tres razones:

1. Los callbacks de ESP-NOW se ejecutan en otra tarea. Cuando registras una funcion para que se llame cuando llega un paquete ESP-NOW, esa funcion no se ejecuta en tu tarea main. Se ejecuta en la tarea interna de WiFi. Esto significa que no puedes simplemente poner una variable global y asumir que tu tarea main la vera inmediatamente. Necesitas un mecanismo de sincronizacion.

2. Las funciones de espera usan "ticks" de FreeRTOS. FreeRTOS tiene su propia unidad de tiempo: el "tick". Normalmente un tick = 1 milisegundo en ESP-IDF. Las funciones de espera y timeout trabajan en ticks, no en milisegundos directamente. Hay una macro que convierte milisegundos a ticks.

3. Event Groups para sincronizacion. Aqui esta el mecanismo clave para nuestros nodos. Un Event Group es como un tablero de senales donde diferentes tareas pueden poner y leer banderas.

**Como usamos Event Groups para esperar el ACK:**

    Tarea main                     Callback ESP-NOW
        |                               |
        | Envia paquete                  |
        |                               |
        | Espera evento "ACK_RECIBIDO"   |
        | con timeout de 500ms           |
        | (se bloquea, no consume CPU)   |
        |                               |
        |         (llega paquete ACK)    |
        |                               |
        |    Pone bandera "ACK_RECIBIDO" |
        |                               |
        | Despierta, ve la bandera       |
        | Sabe que el ACK llego          |
        | Continua con el flujo          |

La funcion xEventGroupWaitBits es la que hace la magia: bloquea la tarea main (sin consumir CPU) hasta que el callback de ESP-NOW pone la bandera de ACK recibido, o hasta que pasa el timeout de 500ms. Si pasa el timeout sin ACK, la funcion retorna y el nodo sabe que tiene que reintentar.

Es como esperar un paquete: te sientas a leer un libro (no gastas energia mirando por la ventana) y el timbre te avisa cuando llega.

**Que NO necesitas saber de FreeRTOS para los nodos:**

- Mutexes y semaforos: nuestro flujo secuencial no los necesita
- Colas (queues): utiles para el gateway, no para nodos simples
- Software timers: usamos el timer de hardware para deep sleep
- Task notifications: Event Groups son suficientes para nuestro caso

Conforme avancemos al gateway en tutoriales futuros, necesitaras mas de FreeRTOS. Pero por ahora, con entender tareas, callbacks en contexto diferente y Event Groups, estas listo.

---

## Como encaja en el proyecto

La Fase 2 construye a los "trabajadores de campo" del sistema. Si la Fase 1 fue el plano general, la Fase 2 es donde construimos los sensores que van a estar en las zonas monitorizadas, midiendo 24/7.

    Vista general del sistema:

    Zona 1            Zona 2            Zona 3
    +--------+        +--------+        +--------+
    | Nodo 1 |        | Nodo 2 |        | Nodo 3 |
    | pH+Temp|        | pH+T+OD|        | pH+Temp|
    +---+----+        +---+----+        +---+----+
        |                 |                 |
        |    ESP-NOW      |    ESP-NOW      |
        |   (inalambrico) |   (inalambrico) |
        v                 v                 v
    +----------------------------------------+
    |            GATEWAY (ESP32-S3)           |
    |  Recibe datos, guarda, envia a la nube |
    +----------------------------------------+

Estos nodos autonomos son los ojos y oidos de la instalacion IIoT. Cada uno:

- Se despierta periodicamente (cada 5 minutos)
- Mide los parametros del agua (temperatura, pH, oxigeno disuelto, turbidez)
- Reporta al gateway usando nuestro protocolo binario fiable
- Se vuelve a dormir para conservar bateria

El protocolo de fiabilidad asegura que los datos no se pierdan silenciosamente. Si un paquete se pierde, se reintenta. Si el gateway esta caido, el nodo entra en modo ahorro para proteger la bateria.

El auto-discovery hace que desplegar nuevos nodos sea trivial: sacas un nodo de la caja, lo enciendes en la zona correspondiente y se integra al sistema solo.

La gestion de energia asegura meses de vida con una sola bateria, incluso en ubicaciones remotas donde cambiar baterias frecuentemente seria impráctico.

Todo esto junto crea un sistema robusto: funciona sin intervencion humana, sobrevive a fallos temporales del gateway, escala facilmente y dura meses sin mantenimiento.

---

## Errores tipicos y como evitarlos

**1. Confiar en la RAM normal despues del deep sleep**

Declaras una variable global, le asignas un valor, el nodo entra en deep sleep y al despertar esperas que el valor siga ahi. No esta. La RAM principal se borra completamente. Si necesitas que un dato sobreviva al deep sleep, debe estar en RTC memory con el atributo correspondiente. Este error es silencioso: el programa no falla, simplemente usa valores aleatorios.

**2. No verificar la causa del arranque antes de usar RTC memory**

La RTC memory sobrevive al deep sleep, pero en un cold boot (primera vez que se enciende o reset fisico) contiene basura. Si no verificas por que se desperto el ESP32 y asumes que los datos de RTC memory son validos, tu nodo puede empezar con un numero de secuencia de 47,231 o un contador de fallos de 255. Siempre verifica: si es cold boot, inicializa. Si es warm boot, usa los valores existentes.

**3. Olvidar empaquetar las estructuras del protocolo**

Sin el atributo de empaquetado, el compilador puede insertar bytes de relleno en tu estructura. El nodo envia 33 bytes creyendo que el campo de temperatura empieza en el byte 13, pero en realidad empieza en el byte 16 porque hay 3 bytes de relleno despues del msg_type. El gateway interpreta basura. Este error es especialmente dificil de diagnosticar porque todo parece funcionar, pero los valores numericos son incorrectos.

**4. No implementar timeout al esperar el ACK**

Si esperas el ACK indefinidamente (sin timeout), y el gateway no responde, el nodo se queda despierto para siempre consumiendo ~120mA. La bateria se agota en un dia. Siempre usa xEventGroupWaitBits con un timeout definido (500ms en nuestro caso). Si no llega el ACK, sigue adelante con la logica de reintentos.

**5. Reintentar indefinidamente**

Si tu logica dice "reintenta hasta que funcione", un nodo sin gateway va a estar despierto permanentemente, quemando bateria. Siempre define un limite maximo de reintentos (3 en nuestro caso) y despues acepta el fallo, guarda el estado y duerme. El modo ahorro de energia es la segunda linea de defensa: si fallas muchas veces seguidas, aumentas el intervalo de sueno.

**6. No hacer idempotente el procesamiento en el gateway**

Si el gateway guarda un dato cada vez que lo recibe sin verificar el numero de secuencia, un reintento genera datos duplicados. De repente tienes dos lecturas de temperatura identicas con 200ms de diferencia, lo que no tiene sentido fisico. El gateway debe verificar: "ya procese este seq_num de esta MAC?" Si si, solo reenvia el ACK sin guardar de nuevo.

**7. Inicializar WiFi/ESP-NOW antes de leer sensores**

El radio WiFi consume mucha energia. Si lo enciendes primero y luego lees sensores que tardan 100ms en estabilizarse, estas desperdiciando energia. Orden optimo: encender sensores, esperar estabilizacion, leer valores, encender WiFi, enviar, apagar. Cada milisegundo con el radio encendido cuenta.

**8. Ignorar el RSSI en la tabla de nodos del gateway**

El RSSI (Received Signal Strength Indicator) te dice que tan fuerte llega la senal. Si un nodo que normalmente tiene RSSI de -45 dBm empieza a reportar -80 dBm, algo cambio: movieron el nodo, hay un nuevo obstaculo, o la antena tiene un problema. Monitorear el RSSI te permite detectar problemas antes de que se conviertan en paquetes perdidos.

---

## Experimenta

Estas actividades te ayudan a internalizar los conceptos. No necesitas la instalacion industrial completa, solo un ESP32 y curiosidad.

---

**Experimento 1: Que pasa si desconectas el gateway mientras un nodo envia?**

Configura un nodo y un gateway funcionando. Luego desconecta el gateway (quitale la corriente). Observa en los logs del nodo: deberia enviar, esperar ACK, no recibirlo, reintentar 3 veces, incrementar el contador de fallos y dormir. Vuelve a conectar el gateway y observa como el nodo se recupera en su siguiente ciclo. Si dejas el gateway desconectado lo suficiente, el nodo deberia entrar en modo ahorro (30 minutos de sueno). Reconecta el gateway y verifica que el nodo eventualmente vuelve al intervalo normal.

---

**Experimento 2: Cual es el tiempo maximo de deep sleep?**

El timer de deep sleep del ESP32 usa un contador de 64 bits. Investiga en la documentacion de ESP-IDF: cual es el valor maximo que puedes pasar a la funcion de deep sleep? Convertido a unidades humanas, cuanto tiempo es? Pista: es mucho mas de lo que jamas necesitarias. Prueba configurar un deep sleep de 1 hora y verifica que el nodo despierta correctamente.

---

**Experimento 3: Mide el tiempo real de despertar-a-dormir**

Usa la funcion de timestamp al inicio de app_main() y justo antes de entrar en deep sleep. La diferencia es tu tiempo activo real. Comparalo con los 200ms que usamos en los calculos de bateria. Es mas? Es menos? Que factores afectan este tiempo? Prueba desconectar los sensores y mide solo el tiempo de arranque + envio ESP-NOW. Luego agrega los sensores y compara.

---

**Experimento 4: Observa la perdida de paquetes**

Si tienes dos ESP32, configura el nodo para enviar cada 5 segundos (para no esperar 5 minutos entre pruebas). Pon el gateway a registrar el numero de secuencia de cada paquete recibido. Luego aleja el nodo progresivamente. A que distancia empiezan a aparecer huecos en la secuencia? Pon un obstaculo metalico entre ambos. Como cambia? Este experimento te da intuicion real sobre por que necesitamos el protocolo de fiabilidad.

---

## Para profundizar

Si quieres ir mas alla de lo que cubre este tutorial, estos recursos son el siguiente paso:

- **Documentacion de deep sleep de ESP-IDF**: la guia oficial de Espressif explica todas las fuentes de despertar, el coprocesador ULP y los modos de sueno intermedios (light sleep). Busca "ESP-IDF Sleep Modes" en la documentacion oficial de Espressif.

- **Documentacion de ESP-NOW de ESP-IDF**: detalla los limites del protocolo (250 bytes, maximo 20 peers, encriptacion opcional), funciones de envio y recepcion, y como funciona el callback. Busca "ESP-IDF ESP-NOW" en docs.espressif.com.

- **Documentacion de FreeRTOS**: el sitio oficial freertos.org tiene tutoriales excelentes sobre tareas, Event Groups, colas y sincronizacion. El capitulo sobre Event Groups es especialmente relevante para entender como esperamos el ACK.

- **Hoja de datos del ESP32-S3 y ESP32-C3**: las secciones sobre consumo de energia (power consumption) dan valores exactos para cada modo de sueno y cada periferico. Utiles para hacer calculos de bateria mas precisos.

- **"Making Embedded Systems" de Elecia White**: un libro excelente que cubre protocolos binarios, presupuestos de energia y muchos otros temas relevantes para sistemas embebidos.

---

## Preguntas de reflexion

Estas preguntas no tienen una unica respuesta correcta. Estan disenadas para que pienses mas alla de lo que se explico en el tutorial.

**1.** Si el ESP32 pierde toda la RAM principal al entrar en deep sleep, por que no simplemente guardar todo en flash (que es persistente)? Que desventaja tendria escribir en flash cada 5 minutos durante meses?

**2.** Nuestro protocolo usa un numero de secuencia de 16 bits (uint16), lo que permite valores de 0 a 65,535. Si un nodo envia un paquete cada 5 minutos, cuanto tiempo pasara antes de que el contador de vuelta a cero? Es un problema? Como lo manejas?

**3.** Dijimos que el timeout para esperar el ACK es 500ms. Que pasaria si lo pones muy corto (50ms)? Y si lo pones muy largo (5 segundos)? Cual es el equilibrio?

**4.** El auto-discovery asume que cualquier nodo que se presente es bienvenido. En un entorno de produccion, esto podria ser un problema de seguridad. Como podrias anadir autenticacion sin complicar demasiado el sistema?

**5.** Si un nodo esta en modo ahorro de energia (30 minutos de sueno) porque el gateway estuvo caido, y el gateway se recupera, el nodo no se entera hasta su proximo despertar (que puede ser en 29 minutos). Como podrias reducir este tiempo de recuperacion?

**6.** Nuestro calculo de bateria asume que el ESP32 consume 10uA en deep sleep. Pero si le conectas sensores que consumen 1mA permanentemente, ese numero cambia drasticamente. Como diseñarias el circuito para que los sensores tambien se apaguen durante el deep sleep?

**7.** Si dos nodos envian un paquete exactamente al mismo tiempo, hay una colision y ambos se pierden. Con 20 nodos despertando cada 5 minutos, cual es la probabilidad de colision? Como podrias reducirla?

**8.** El protocolo binario que describimos es eficiente pero rigido: si quieres agregar un nuevo campo (por ejemplo, nivel de agua), tienes que cambiar la estructura en el nodo Y en el gateway. Como diseñarias el protocolo para que sea extensible sin romper compatibilidad con versiones anteriores?
