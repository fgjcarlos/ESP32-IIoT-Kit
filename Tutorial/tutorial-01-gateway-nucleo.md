# Tutorial 1: Construyendo el Cerebro del Sistema

> **Fase 1 -- Gateway (Nucleo)**
> Este tutorial te guia por los conceptos fundamentales que hacen funcionar el gateway de nuestro sistema de monitorizacion IIoT. No vamos a copiar y pegar codigo: vamos a *entender* que hace cada pieza y por que esta ahi.

---

## Que vas a aprender

- Como funciona NVS (almacenamiento persistente en flash)
- Como configurar WiFi en modo APSTA (AP + STA simultaneo)
- Programacion basada en eventos en ESP-IDF
- Como crear un servidor HTTP en un microcontrolador
- Diseno basico de API REST
- Sincronizacion horaria con SNTP
- Que es un watchdog y por que es esencial en sistemas embebidos
- Estrategia de reconexion con backoff exponencial

Al final de este tutorial tendras un mapa mental claro de como arranca el gateway, como se comunica con el mundo exterior y como se protege a si mismo de fallos.

---

## Conceptos clave

---

### NVS: almacenamiento que sobrevive a reinicios

NVS significa *Non-Volatile Storage* (almacenamiento no volatil). Para entenderlo, primero hay que saber que la RAM del ESP32 se borra cada vez que el chip se apaga o reinicia. Todo lo que guardaste en variables normales desaparece. NVS resuelve ese problema: es una pequena base de datos que vive dentro de la memoria flash del chip y que **sobrevive a reinicios y cortes de luz**.

**Analogia**: imagina las notas adhesivas que pegas en la puerta del frigorifico. Puedes apagar la luz de la cocina, salir de casa, volver al dia siguiente, y las notas siguen ahi. NVS es exactamente eso para el ESP32: notas persistentes que el firmware puede leer y escribir.

NVS funciona con un modelo de **clave-valor**. Piensa en un diccionario:

    +---------------------+---------------------------+
    | Clave               | Valor                     |
    +---------------------+---------------------------+
    | "wifi_ssid"         | "IIoT-Red-Local"          |
    | "wifi_pass"         | "clave-segura"            |
    | "sensor_intervalo"  | 30                        |
    | "nombre_gateway"    | "GW-Zona-Norte"           |
    +---------------------+---------------------------+

Cada entrada tiene un nombre (la clave) y un dato (el valor). Los tipos de datos soportados incluyen enteros de 8, 16, 32 y 64 bits, strings de texto y blobs (datos binarios arbitrarios, como una tabla de calibracion).

**Namespaces**: NVS organiza las claves en "namespaces", que funcionan como carpetas. Puedes tener un namespace "wifi" con las credenciales, otro "sensores" con parametros de calibracion, y otro "sistema" con configuracion general. Esto evita colisiones: dos componentes distintos pueden tener una clave llamada "timeout" sin pisarse entre si, siempre que esten en namespaces diferentes.

**Proteccion contra corrupcion**: que pasa si se corta la luz justo cuando NVS estaba escribiendo un dato? NVS usa una tecnica llamada *journaling*: antes de modificar un dato, escribe la intencion en un registro temporal. Si la operacion se interrumpe, al reiniciar NVS puede detectar la inconsistencia y recuperarse. No es perfecto (la flash tiene un numero limitado de escrituras), pero es mucho mas robusto que escribir directamente en la flash sin proteccion.

En nuestro gateway, NVS guarda las credenciales WiFi, el nombre de la red AP, parametros del sistema y cualquier configuracion que el usuario modifique desde la pagina web. Sin NVS, cada vez que el gateway se reinicia (por un corte de luz, por un watchdog, por una actualizacion) habria que reconfigurarlo manualmente. En una instalacion industrial remota, eso no es una opcion.

---

### WiFi APSTA: ser router y cliente a la vez

El ESP32 tiene un unico chip de radio WiFi, pero puede operar en un modo muy util llamado **APSTA**, que combina dos roles simultaneamente:

- **AP (Access Point)**: el ESP32 crea su propia red WiFi, como un router. Puedes conectar tu movil o portatil directamente a el.
- **STA (Station)**: el ESP32 se conecta a un router existente, como cualquier dispositivo normal.

Por que necesitamos ambos? Piensa en el escenario real:

    +-----------+          +-------------+          +-----------+
    |  Tu movil | )))  ((( |   GATEWAY   | )))  ((( |  Router   |
    |  o portatil|  (AP)   |   ESP32     |  (STA)   |  de la    |
    +-----------+          +-------------+          | instalacion|
                                                     +-----------+
         Conexion local                          Conexion a internet
         (configuracion,                         (enviar datos al
          emergencias)                            servidor, MQTT)

El **modo AP** es tu puerta de emergencia. Imagina que el router de la instalacion se estropea. Sin el modo AP, no tendrias forma de acceder al gateway para ver que esta pasando o cambiar la configuracion. Con el AP, simplemente te conectas directamente al gateway con tu movil, abres el navegador y accedes al panel web. Funciona aunque no haya internet.

El **modo STA** es la conexion al mundo exterior. A traves del router de la instalacion, el gateway envia datos a la nube, sincroniza la hora, y en fases futuras publicara datos via MQTT.

**La restriccion del canal**: aqui viene un detalle tecnico importante. Como ambos modos (AP y STA) comparten el mismo chip de radio fisico, **deben operar en el mismo canal WiFi**. No puede transmitir en el canal 6 como AP y en el canal 11 como STA al mismo tiempo. Quien decide el canal? El router al que se conecta como STA. Cuando la conexion STA se establece, el AP automaticamente se ajusta al canal que usa ese router. Si no hay ningun router disponible (modo STA desconectado), el AP puede elegir cualquier canal libremente.

Esto tiene una implicacion practica: si cambias el router de la instalacion por uno que opera en un canal diferente, los dispositivos conectados al AP del gateway pueden sufrir una desconexion momentanea mientras el AP cambia de canal.

El flujo de inicializacion del WiFi en el gateway es:

    1. Inicializar el stack TCP/IP
    2. Crear las interfaces de red (AP y STA)
    3. Configurar parametros del AP (SSID, contrasena, canal por defecto)
    4. Configurar parametros del STA (SSID y contrasena del router)
    5. Arrancar el WiFi en modo APSTA
    6. Esperar eventos de conexion

---

### Programacion basada en eventos (Event Loop)

Si vienes de Arduino, estas acostumbrado a esto: una funcion setup() que se ejecuta una vez, y una funcion loop() que se ejecuta una y otra vez, eternamente. Dentro de loop() compruebas manualmente si hay datos WiFi, si un boton se pulso, si un sensor tiene lectura nueva. Esto se llama **polling** (sondeo constante).

**Analogia**: polling es como un nino en el asiento trasero del coche preguntando cada diez segundos "ya llegamos? ya llegamos? ya llegamos?". Funciona, pero es agotador e ineficiente.

ESP-IDF usa un modelo diferente: **programacion basada en eventos**. En lugar de preguntar constantemente, registras funciones (llamadas *handlers*) que se ejecutan **solo cuando algo ocurre**.

**Analogia**: en lugar del nino preguntando, le dices "cuando lleguemos, te aviso". Tu funcion handler es la accion que el nino hara cuando le avisen: desabrocharse el cinturon, coger la mochila, etc.

Asi funciona el flujo:

    +------------------+
    | Sistema ESP-IDF  |
    | (publica eventos)|-----> WIFI_EVENT_STA_CONNECTED
    +------------------+            |
                                    v
                          +-----------------------+
                          | Tu handler registrado |
                          | "Genial, WiFi conectado|
                          |  ahora pido IP..."     |
                          +-----------------------+

El ESP-IDF tiene un **default event loop** (bucle de eventos por defecto) donde los componentes del sistema publican sus eventos. Algunos eventos tipicos son:

    - WIFI_EVENT_STA_START         --> el WiFi STA se ha iniciado
    - WIFI_EVENT_STA_CONNECTED     --> conectado al router
    - WIFI_EVENT_STA_DISCONNECTED  --> desconectado del router
    - IP_EVENT_STA_GOT_IP          --> se ha obtenido direccion IP
    - WIFI_EVENT_AP_STACONNECTED   --> un dispositivo se conecto al AP
    - WIFI_EVENT_AP_STADISCONNECTED--> un dispositivo se desconecto del AP

Tu trabajo como programador es: (1) crear el event loop, (2) registrar handlers para los eventos que te interesan, y (3) dejar que el sistema haga el resto.

Las ventajas son significativas:

- **Eficiencia**: el CPU puede entrar en estados de bajo consumo cuando no hay eventos que procesar, en vez de estar comprobando cosas en un bucle.
- **Claridad**: cada handler tiene una responsabilidad clara. El handler de desconexion WiFi se encarga de reconectar, el handler de peticion HTTP se encarga de responder, etc.
- **Desacoplamiento**: el componente WiFi no necesita saber que haces cuando se conecta; simplemente publica el evento y los interesados reaccionan.

En nuestro gateway, el event loop es el corazon de la logica: gestiona conexiones y desconexiones WiFi, controla el backoff exponencial de reconexion, y coordina la inicializacion de los servicios que dependen de tener red disponible.

---

### Servidor HTTP embebido

Esto puede parecer sorprendente al principio: un microcontrolador de pocos dolares ejecutando un servidor web, como si fuera un servidor de Amazon. Pero es exactamente lo que hace el gateway. Cuando te conectas al AP del ESP32 y abres un navegador, estas accediendo a un servidor HTTP real que corre dentro del chip.

El concepto basico es sencillo. El servidor escucha conexiones en un puerto (normalmente el 80). Cuando llega una peticion HTTP (por ejemplo, un navegador pidiendo una pagina), el servidor busca un **URI handler** registrado para esa URL y ese metodo, y ejecuta la funcion asociada.

    Navegador pide: GET /
         |
         v
    +---------------------------+
    |   Servidor HTTP del ESP32 |
    |                           |
    |  Busca handler para       |
    |  GET "/"                  |
    |       |                   |
    |       v                   |
    |  handler_pagina_principal |
    |  --> responde con HTML    |
    +---------------------------+
         |
         v
    Navegador muestra la pagina

Los **URI handlers** son el equivalente a las rutas en un framework web como Flask o Express, pero mucho mas basico. Registras pares de (metodo + URL) con funciones:

    GET  /              --> servir pagina web principal
    GET  /api/status    --> devolver estado del sistema en JSON
    GET  /api/nodes     --> devolver lista de nodos en JSON
    POST /api/config    --> recibir nueva configuracion

**Limitaciones importantes**: el ESP32 no es un servidor de produccion. Tiene poca RAM (tipicamente unos 300 KB disponibles) y solo puede manejar un numero reducido de conexiones simultaneas, normalmente entre 4 y 7. Esto significa que si quince personas intentan acceder a la pagina web al mismo tiempo, algunas conexiones seran rechazadas. En la practica, esto no es un problema: el panel del gateway lo usa una o dos personas a la vez, no cien.

**Archivos embebidos**: un servidor web normal lee archivos HTML, CSS y JavaScript del disco duro. El ESP32 no tiene disco duro. La solucion es **embeber** estos archivos directamente en el firmware. En el proceso de compilacion, CMake toma los archivos de la carpeta web, los convierte en arrays de bytes que se almacenan en la flash, y el servidor los sirve directamente desde ahi. Es como llevar la pagina web "cosida" dentro del firmware.

El servidor HTTP de nuestro gateway cumple dos funciones fundamentales:

1. **Panel web de configuracion**: una pagina que puedes abrir en el navegador para ver el estado del sistema, cambiar credenciales WiFi, ver que nodos estan conectados y ajustar parametros. Es tu interfaz de emergencia cuando no hay internet.

2. **API REST**: endpoints que otros programas (no humanos) pueden consultar para obtener datos o enviar configuracion de forma estructurada.

---

### API REST basica

REST (*Representational State Transfer*) es una forma estandar y ampliamente aceptada de disenar APIs web. Si alguna vez has usado una app del movil que carga datos de internet, probablemente esta usando una API REST detras.

Los conceptos fundamentales son pocos:

**Endpoints**: son URLs que representan "recursos". Cada URL es una puerta a cierta informacion o accion.

    /api/status   --> el estado general del sistema
    /api/nodes    --> los nodos de sensores conectados
    /api/config   --> la configuracion del gateway

**Metodos HTTP**: indican que quieres hacer con el recurso.

    GET   = leer (dame informacion)
    POST  = escribir/crear (toma esta informacion nueva)
    PUT   = actualizar (modifica esto)
    DELETE = borrar (elimina esto)

**JSON**: el formato de los datos. JSON es texto estructurado que tanto humanos como maquinas pueden leer facilmente.

    Ejemplo de respuesta a GET /api/status:

    {
      "uptime": 3600,
      "wifi_connected": true,
      "nodes_count": 4,
      "free_heap": 180000
    }

Analogia: piensa en un restaurante. El menu (la documentacion de la API) te dice que platos hay disponibles. El camarero (HTTP) lleva tu pedido (la peticion) a la cocina (el servidor) y te trae el resultado (la respuesta). GET es pedir la carta, POST es hacer un pedido nuevo.

En nuestro gateway, la API REST permite:

    GET  /api/status  --> ver estado del gateway (uptime, memoria, WiFi)
    GET  /api/nodes   --> ver los nodos ESP-NOW registrados y sus ultimos datos
    POST /api/config  --> cambiar configuracion (nuevas credenciales WiFi, etc.)

ESP-IDF incluye **cJSON**, una libreria ligera y eficiente para crear y leer JSON dentro del microcontrolador. Dado que el ESP32 tiene poca RAM, cJSON esta disenado para usar la memoria con cuidado. Aun asi, hay que ser consciente de no crear estructuras JSON enormes que agoten la memoria disponible.

Por que no simplemente poner todo en la pagina web? Porque la API permite la **automatizacion**. Un script en un ordenador, una app del movil, u otro microcontrolador pueden consultar la API sin necesidad de un navegador. Separa la interfaz visual (HTML) de los datos (JSON), lo cual es una practica de diseno solida.

---

### SNTP: la importancia del tiempo

Cuando enciendes un ESP32, su reloj interno marca **1 de enero de 1970, 00:00:00**. Esto se conoce como la "epoca Unix" y es el punto de referencia a partir del cual los sistemas informaticos cuentan el tiempo. El ESP32 no tiene un reloj con bateria propia (como el que tiene tu ordenador en la placa base), asi que cada vez que se enciende, "piensa" que estamos en 1970.

**SNTP** (*Simple Network Time Protocol*) resuelve esto. Es un protocolo que permite al ESP32 preguntar la hora exacta a servidores de internet especializados, como pool.ntp.org, que mantienen relojes atomicos sincronizados.

    +----------+        "Que hora es?"        +----------------+
    |  ESP32   | ---------------------------> | pool.ntp.org   |
    |  Gateway |                              | (servidor NTP) |
    |          | <--------------------------- |                |
    +----------+     "Son las 14:32:07"       +----------------+
                     del 15 de marzo de 2026

Por que importa tener la hora correcta? Por varias razones criticas:

1. **Timestamps en los datos**: cuando un sensor reporta que la temperatura del agua es 18.5 grados, necesitas saber *cuando* ocurrio esa lectura. Sin hora correcta, los datos pierden la mitad de su valor. "La temperatura subio a 25 grados" no es lo mismo que "la temperatura subio a 25 grados a las 3 de la madrugada".

2. **Logging y diagnostico**: cuando algo falla, los logs con hora real te permiten reconstruir la secuencia de eventos. "WiFi se desconecto a las 03:14, reconecto a las 03:16, nodo 3 dejo de reportar a las 03:17" cuenta una historia. Sin hora real, solo tendrias "paso primero esto, luego esto" sin saber cuando.

3. **Certificados TLS**: si en el futuro el gateway se conecta a servicios en la nube usando HTTPS (conexion segura), los certificados TLS tienen fechas de validez. Si el ESP32 cree que es 1970, todos los certificados pareceran "del futuro" y seran rechazados.

**Analogia**: el gateway es como el **reloj central de una estacion de tren**. Ese reloj grande que esta en el vestibulo y que todos los relojes de los andenes usan como referencia. El gateway sincroniza su hora con internet (SNTP) y luego distribuye esa hora a todos los nodos de sensores a traves de los mensajes ESP-NOW. Cuando el gateway envia un ACK (confirmacion de recepcion) a un nodo, incluye la hora actual. Asi, incluso los nodos que no tienen conexion a internet saben que hora es.

El flujo de sincronizacion es:

    Internet (NTP) --> Gateway (SNTP) --> Nodos (via ESP-NOW ACK)

    Reloj atomico      Reloj central      Relojes de los
    global             del sistema          andenes/sensores

La sincronizacion no es instantanea ni perfecta, pero para nuestro caso de uso (lecturas de sensores cada 30 segundos o mas), una precision de unos pocos cientos de milisegundos es mas que suficiente.

---

### Watchdog: el guardian del sistema

Imagina este escenario: tu gateway lleva funcionando tres meses sin problemas en una instalacion industrial remota a 40 km de la ciudad mas cercana. Una noche, un bug raro en el software que nunca se habia manifestado causa un bloqueo. El firmware entra en un bucle infinito, o un deadlock entre dos tareas deja el sistema completamente congelado. El WiFi deja de funcionar. Los nodos siguen enviando datos pero nadie los recibe. Los parametros monitorizados podrian estar fuera de rango y nadie se entera.

En un PC, simplemente reiniciarias. Pero nadie va a conducir 40 km a las 3 de la madrugada para pulsar un boton de reset.

El **watchdog** (perro guardian) es la solucion. Es un **temporizador hardware** independiente del software principal. Funciona asi:

    Firmware normal:

    [Ejecuta tareas] --> alimenta watchdog --> [Ejecuta tareas] --> alimenta watchdog
         |                    ^                     |                    ^
         |     "Estoy vivo"   |                     |     "Estoy vivo"   |
         +--------------------+                     +--------------------+

    Firmware bloqueado:

    [Bucle infinito...........................................................
         |
         |   (no alimenta al watchdog)
         |
         +--- Watchdog: "No me han alimentado en 30 segundos"
                         "REINICIO EL CHIP"
                                |
                                v
                         [ESP32 se reinicia limpiamente]

El firmware, en su ejecucion normal, debe periodicamente "alimentar" al watchdog (en ingles, *to feed the watchdog* o *to kick the watchdog*). Esto le dice al temporizador hardware: "todo va bien, sigo funcionando". Si el firmware se bloquea, deja de alimentar al watchdog, y cuando el temporizador expira, el hardware reinicia el chip automaticamente.

**Configurar el timeout**: este es un equilibrio delicado.

- **Muy corto** (por ejemplo, 2 segundos): el watchdog podria dispararse durante operaciones legitimamente largas, como una conexion WiFi que tarda 5 segundos, causando reinicios innecesarios.
- **Muy largo** (por ejemplo, 5 minutos): si el sistema se bloquea, pasaran 5 minutos sin recoger datos de los sensores antes de que el watchdog actue.

Para nuestro proyecto, **30 segundos** es un buen compromiso. Es tiempo suficiente para que las operaciones mas largas terminen (conexion WiFi, sincronizacion NTP) pero no tan largo como para dejar los sensores desatendidos demasiado tiempo.

El watchdog es la **ultima linea de defensa**. No arregla el bug que causo el bloqueo, pero al menos devuelve el sistema a un estado funcional. Combinado con NVS (que preserva la configuracion) y el backoff exponencial (que reconecta WiFi de forma inteligente), el gateway puede recuperarse de casi cualquier fallo sin intervencion humana.

---

### Backoff exponencial: reconectarse sin saturar

Cuando la conexion WiFi STA se pierde (el router se reinicio, hubo un corte de luz, interferencias), el gateway necesita reconectarse. La pregunta es: con que frecuencia lo intenta?

**Enfoque ingenuo**: reintentar cada segundo. Problema: si el router tarda 2 minutos en reiniciarse, el gateway habra hecho 120 intentos fallidos, consumiendo CPU, energia, y generando trafico innecesario. Peor aun: si hay 10 gateways haciendo lo mismo, cuando el router vuelva todos intentaran conectarse simultaneamente, potencialmente saturandolo.

**Enfoque inteligente**: backoff exponencial. La espera entre reintentos se duplica cada vez:

    Intento 1: esperar  1 segundo
    Intento 2: esperar  2 segundos
    Intento 3: esperar  4 segundos
    Intento 4: esperar  8 segundos
    Intento 5: esperar 16 segundos
    Intento 6: esperar 32 segundos
    Intento 7: esperar 60 segundos  (tope maximo)
    Intento 8: esperar 60 segundos
    ... (sigue a 60s hasta que conecte)

Cuando finalmente conecta, el contador se resetea a 1 segundo para la proxima vez.

    Tiempo -->

    Ingenuo:  | X X X X X X X X X X X X X X X X X X X X X X
    (cada 1s)

    Backoff:  | X . X . . X . . . . X . . . . . . . . X
    (exponencial)

    X = intento de conexion
    . = espera

**Analogia**: es como llamar a alguien que no contesta al telefono. Si es un amigo y no contesta, esperas 1 minuto y vuelves a llamar. Si sigue sin contestar, esperas 2 minutos. Luego 5. Luego 10. No llamas cada 10 segundos sin parar, porque eso seria acoso y ademas gastarias toda tu bateria.

Este patron no lo inventamos nosotros. Se usa en practicamente todos los protocolos de red: TCP, HTTP, MQTT, DNS, sistemas de colas de mensajes... Es uno de esos patrones que todo ingeniero de software deberia conocer.

En nuestro gateway, el backoff se implementa con un simple multiplicador. Cada vez que el handler del evento WIFI_EVENT_STA_DISCONNECTED se ejecuta, calcula el tiempo de espera actual, programa un temporizador para el siguiente intento, y duplica el multiplicador (hasta el tope). Cuando llega IP_EVENT_STA_GOT_IP, resetea el multiplicador a 1.

---

## Como encaja en el proyecto

Esta Fase 1 construye el **cerebro** del sistema de monitorizacion. Piensa en la instalacion IIoT como un organismo:

    +-----------------------------------------------------------+
    |               INSTALACION INDUSTRIAL / IIoT               |
    |                                                           |
    |   [Nodo 1]    [Nodo 2]    [Nodo 3]    [Nodo 4]          |
    |   Temp/pH     Temp/pH     Oxigeno     Nivel               |
    |      \           |           |           /                |
    |       \          |           |          /                 |
    |        \  (ESP-NOW, Fase 2) |         /                  |
    |         \        |          |        /                    |
    |          v       v          v       v                     |
    |        +---------------------------+                      |
    |        |        GATEWAY            |                      |
    |        |     (esta fase!)          |                      |
    |        |                           |                      |
    |        |  NVS: configuracion       |                      |
    |        |  WiFi AP: acceso local    |------> Tu movil      |
    |        |  WiFi STA: internet       |------> Nube/MQTT     |
    |        |  HTTP: panel web + API    |        (Fase 4)      |
    |        |  SNTP: hora correcta      |                      |
    |        |  Watchdog: proteccion     |                      |
    |        +---------------------------+                      |
    +-----------------------------------------------------------+

El gateway es el **punto central** de todo. Sin el, los nodos de sensores estarian gritando datos al vacio y nadie los escucharia. Sus responsabilidades son:

1. **Recibir datos** de todos los nodos via ESP-NOW (se implementa plenamente en Fase 2, pero la infraestructura WiFi se prepara aqui)
2. **Ofrecer un panel local** de emergencia via WiFi AP + servidor web
3. **Conectar con el mundo exterior** via WiFi STA (para MQTT en Fase 4, para SNTP ahora)
4. **Mantener la configuracion** persistente en NVS
5. **Protegerse de fallos** con watchdog y reconexion inteligente

La **secuencia de inicializacion** es la columna vertebral del firmware. Cada paso depende del anterior:

    NVS (leer config) --> WiFi (APSTA) --> HTTP Server --> SNTP --> ESP-NOW
         |                    |                |             |         |
         |                    |                |             |         |
    Sin NVS no hay       Sin WiFi no       Sin WiFi AP   Sin WiFi  Sin WiFi
    credenciales WiFi    hay red            no hay panel   STA no   inicializado
                                            web local      hay      no hay
                                                           hora     comunicacion
                                                           real     con nodos

Si NVS falla, nada funciona. Si WiFi falla, no hay ni panel web ni sincronizacion horaria ni datos. Cada pieza es un cimiento sobre el que se construye la siguiente. Por eso es fundamental entender cada componente individualmente antes de ver como interactuan.

---

## Errores tipicos y como evitarlos

### 1. No inicializar NVS antes de WiFi

El subsistema WiFi de ESP-IDF usa NVS internamente para almacenar datos de calibracion de la radio. Si intentas iniciar WiFi sin haber inicializado NVS primero, obtendras errores crípticos o comportamiento inestable. **Siempre** inicializa NVS como primer paso del firmware. Este es probablemente el error mas comun en proyectos ESP-IDF nuevos y el mas dificil de diagnosticar si no lo conoces.

### 2. Olvidar alimentar el watchdog en tareas de larga duracion

Si tienes una tarea que tarda mas de lo esperado (por ejemplo, una conexion WiFi lenta o una operacion de NVS grande), el watchdog puede dispararse y reiniciar el chip. Esto parece un "reinicio aleatorio" y es extremadamente confuso de depurar. La solucion es alimentar al watchdog dentro de bucles largos, o asegurarte de que las tareas que no pueden alimentarlo tienen el timeout del watchdog configurado adecuadamente.

### 3. No manejar el evento de desconexion WiFi STA

Muchos tutoriales basicos solo manejan el evento "conectado" y asumen que la conexion nunca se pierde. En la vida real, la conexion WiFi se pierde *constantemente*: interferencias, reinicios del router, cortes de luz. Si no manejas WIFI_EVENT_STA_DISCONNECTED, el gateway simplemente deja de tener internet y no intenta reconectar. Maneja siempre la desconexion con una estrategia de reconexion (idealmente con backoff exponencial).

### 4. Agotar la memoria con JSON grandes

cJSON reserva memoria dinamica del heap para construir objetos JSON. Si intentas crear una respuesta JSON con datos de 50 nodos, cada uno con 10 lecturas historicas, puedes agotar la RAM y causar un crash. Monitoriza el heap libre antes y despues de operaciones JSON intensas. Mantiene las respuestas JSON ligeras: envia solo los datos necesarios, pagina los resultados si hay muchos nodos.

### 5. Usar el mismo namespace de NVS para todo

Es tentador meter todas las claves en un unico namespace "config" o "data". El problema llega cuando dos componentes distintos del firmware usan accidentalmente la misma clave (por ejemplo, "timeout" puede significar cosas distintas para WiFi y para el servidor HTTP). Usa namespaces descriptivos y separados: "wifi_cfg", "http_cfg", "system", etc.

### 6. No configurar correctamente el canal WiFi en modo APSTA

Si configuras el AP con un canal fijo (por ejemplo, canal 6) pero el router STA esta en el canal 11, la conexion STA fallara o el AP sera inaccesible. No fijes el canal del AP manualmente cuando usas modo APSTA; deja que el sistema lo ajuste automaticamente al canal del router STA. Si necesitas configurar un canal por defecto para cuando no hay router, hazlo solo como valor inicial.

### 7. Registrar URI handlers despues de arrancar el servidor

Si registras un handler despues de que el servidor ya esta corriendo y un cliente hace una peticion a esa URL en el intervalo, recibira un error 404. Registra todos tus handlers *antes* de iniciar el servidor, o al menos ten en cuenta que hay una ventana donde las URLs no estan disponibles.

### 8. Asumir que SNTP sincroniza instantaneamente

La sincronizacion SNTP requiere una conexion WiFi STA activa y puede tardar varios segundos (o mas si hay problemas de red). No uses la hora del sistema para timestamps criticos hasta que hayas confirmado que SNTP ha sincronizado. Puedes verificar esto comprobando que el ano actual es mayor que 2020, por ejemplo. Mientras tanto, usa el uptime del sistema o marca los datos como "hora no sincronizada".

---

## Experimenta

---

> **Experimento 1: Observa NVS en accion**
>
> Conectate al gateway por puerto serie (monitor). Cambia las credenciales WiFi desde la pagina web. Observa los logs que confirman la escritura en NVS. Ahora desconecta la alimentacion del ESP32 completamente (no solo reset, sino desenchufar). Espera 10 segundos. Vuelve a conectar. Observa en los logs como al arrancar lee las *nuevas* credenciales de NVS, no las originales. Los datos sobrevivieron al corte total de energia. Intenta lo mismo cambiando el nombre del AP y verifica que tras reiniciar el AP aparece con el nuevo nombre.

---

> **Experimento 2: Explora el modo APSTA con tu movil**
>
> Con el gateway funcionando, busca redes WiFi desde tu movil. Deberias ver la red del AP del gateway (algo como "IIoT-Gateway" o el nombre configurado). Conectate a ella. Abre un navegador y accede a la IP del gateway (normalmente 192.168.4.1). Deberias ver el panel web. Ahora, *sin desconectarte del AP*, observa en el panel web si el gateway esta tambien conectado al router (modo STA). Si lo esta, veras la IP STA y la intensidad de senal. Has confirmado que ambos modos funcionan simultaneamente. Prueba a acceder a la misma pagina web usando la IP del modo STA desde un dispositivo conectado al router de la instalacion.

---

> **Experimento 3: Provoca al watchdog**
>
> Este experimento requiere modificar temporalmente el firmware (asegurate de revertir los cambios despues). Introduce un retraso artificial mayor que el timeout del watchdog en alguna tarea. Observa en los logs como el watchdog detecta la falta de alimentacion y reinicia el chip. Veras un mensaje de panico del sistema con la causa del reinicio. Luego revierte el cambio y verifica que el sistema arranca correctamente. Este experimento te dara confianza en que el watchdog realmente funciona y te protege de bloqueos reales.

---

## Para profundizar

Estos son los recursos oficiales de Espressif para cada uno de los temas tratados. La documentacion de ESP-IDF es extensa y de buena calidad; no dudes en explorarla cuando necesites mas detalles.

- **NVS (Non-Volatile Storage)**
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html

- **WiFi Driver (incluye modo APSTA)**
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html

- **Event Loop Library**
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_event.html

- **HTTP Server**
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_http_server.html

- **SNTP Time Synchronization**
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html

- **Watchdog Timers**
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/wdts.html

- **cJSON Library**
  https://github.com/DaveGamble/cJSON

- **ESP-IDF Getting Started Guide**
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html

---

## Preguntas de reflexion

Estas preguntas no tienen una unica respuesta correcta. Su objetivo es que pienses sobre las decisiones de diseno y entiendas el *por que* detras de cada eleccion.

**1.** Si el gateway pierde la conexion WiFi STA y el backoff exponencial llega al tope de 60 segundos, significa eso que los nodos de sensores dejan de enviar datos? Por que si o por que no? Piensa en que protocolos usa cada conexion.

**2.** Imagina que NVS se corrompe por completo y no se puede leer ninguna configuracion al arrancar. Que deberia hacer el firmware? Quedarse parado y no hacer nada? Arrancar con valores por defecto? Como afecta esto al modo AP (pista: el usuario necesita poder acceder al gateway para reconfigurar)?

**3.** Por que es importante que el AP y el STA compartan el mismo canal WiFi? Que pasaria fisicamente si el chip de radio intentara transmitir en dos frecuencias distintas simultaneamente?

**4.** En nuestro diseno, el servidor HTTP tiene dos roles: servir una pagina web para humanos y una API REST para maquinas. Seria mejor tener dos servidores separados (uno en el puerto 80, otro en el 8080)? Que ventajas y desventajas tendria eso en un sistema con recursos limitados?

**5.** El watchdog reinicia el chip cuando detecta un bloqueo, pero no soluciona el bug que lo causo. Que pasaria si hay un bug que siempre se manifiesta 5 minutos despues de arrancar? El sistema estaria reiniciandose cada 5 minutos eternamente. Como podrias detectar y manejar esta situacion (pista: piensa en un contador de reinicios en NVS)?

**6.** SNTP necesita conexion WiFi STA para funcionar. Si el gateway esta en un lugar sin internet (solo modo AP), como podrian los nodos tener timestamps utiles? Hay alguna alternativa a SNTP para mantener una referencia temporal basica?

**7.** El backoff exponencial con un tope de 60 segundos significa que, en el peor caso, el gateway puede tardar hasta 60 segundos en reconectar despues de que el router vuelva. Es esto aceptable para una instalacion IIoT en produccion? Que factores considerarias para elegir un tope diferente?

**8.** Cuando un usuario cambia las credenciales WiFi desde la pagina web (conectado al AP), el gateway debe desconectarse de la red STA actual y conectarse a la nueva. Que pasa con la peticion HTTP del usuario durante ese proceso? Piensa en que interfaz (AP o STA) esta usando el usuario para comunicarse.

---

*Este tutorial es parte del proyecto ESP32-IIoT-Kit — Plataforma generica de monitorizacion industrial con ESP32.*
*Siguiente: Tutorial 2 -- Nodos de sensores y comunicacion ESP-NOW (Fase 2)*
