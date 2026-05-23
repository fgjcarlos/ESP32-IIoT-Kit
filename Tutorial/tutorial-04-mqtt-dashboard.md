# Tutorial 4: Del Microcontrolador a la Pantalla

## Que vas a aprender

En las fases anteriores construimos los cimientos: los nodos leen sensores, los empaquetan en un protocolo binario eficiente y los envian por ESP-NOW al gateway. Pero hasta ahora, esos datos se quedaban atrapados en el mundo de los microcontroladores. Nadie podia verlos desde un navegador, un telefono o una oficina remota. Era como tener un termometro excelente encerrado en una caja sin ventana.

En esta fase, abrimos esa caja. Vamos a llevar los datos desde el gateway hasta una pantalla web donde un operador de la piscifactoria puede ver en tiempo real que esta pasando en cada estanque. Para eso necesitamos varias piezas nuevas:

- **Protocolo MQTT**: el sistema de mensajeria que usan millones de dispositivos IoT para comunicarse de forma ligera y organizada. Aprenderemos como funciona el modelo publicar/suscribir, como se organizan los "topics" (canales) y que significan los niveles de calidad de servicio.
- **Diseno de jerarquia de topics MQTT**: como nombrar y organizar los canales para que el sistema escale sin volverse un caos.
- **Broker MQTT (Mosquitto)**: el intermediario central que recibe y distribuye todos los mensajes. Por que es imprescindible y como funciona.
- **Patron bridge**: como el gateway actua de traductor entre el mundo ESP-NOW (local, binario, sin internet) y el mundo MQTT (internet, JSON, basado en topics).
- **WebSocket**: la tecnologia que permite que el navegador reciba datos en tiempo real sin tener que preguntar constantemente "hay algo nuevo?".
- **Conceptos basicos de React**: la libreria JavaScript con la que construiremos el dashboard visual.
- **Datos de series temporales**: como almacenar y consultar eficientemente miles de lecturas de sensores indexadas por tiempo.

Al terminar esta fase, tendras una comprension clara del camino completo que recorre un dato: desde que un sensor de temperatura lo mide en el agua, hasta que aparece como un numero actualizado en la pantalla de un navegador web.

---

## Conceptos clave

### MQTT: el protocolo de mensajeria IoT

MQTT son las siglas de Message Queuing Telemetry Transport, aunque el nombre importa menos que entender para que sirve. Fue disenado especificamente para el Internet de las Cosas: situaciones donde los dispositivos tienen poca memoria, poco ancho de banda y conexiones inestables. Es extraordinariamente ligero: una cabecera MQTT puede ocupar tan solo 2 bytes, frente a los cientos de bytes de una cabecera HTTP.

La diferencia fundamental con HTTP es el paradigma de comunicacion. HTTP funciona como una conversacion directa: el cliente pregunta, el servidor responde, y la conexion se cierra. Es un modelo de peticion-respuesta. MQTT funciona de forma completamente distinta: usa el modelo publicar/suscribir (publish/subscribe).

Imaginalo asi. MQTT es como un sistema de emisoras de radio:

```
  [DJ 1]---\                         /---[Oyente A]
             \                       /
  [DJ 2]------> [TORRE DE RADIO] -----> [Oyente B]
             /    (el broker)        \
  [DJ 3]---/                         \---[Oyente C]
```

Los **publicadores** (publishers) son como los DJs: envian su contenido a un canal especifico. No saben ni les importa quien esta escuchando. Los **suscriptores** (subscribers) son como los oyentes: sintonizan los canales que les interesan. No saben ni les importa quien esta emitiendo. Y la **torre de radio** es el broker: recibe las senales de los DJs y las retransmite a todos los oyentes que estan sintonizados en ese canal.

Lo importante es que publicadores y suscriptores nunca hablan directamente entre si. Toda la comunicacion pasa por el broker. Esto tiene ventajas enormes: un publicador puede enviar datos aunque no haya ningun suscriptor conectado en ese momento, y un suscriptor puede recibir datos de multiples publicadores sin conocerlos de antemano.

Un mismo dispositivo puede ser publicador y suscriptor a la vez. Por ejemplo, nuestro gateway publica datos de sensores en ciertos canales, pero tambien se suscribe a canales de comandos para recibir ordenes de control.

Los mensajes se envian a "topics" (temas o canales). Un topic es simplemente una cadena de texto que identifica el canal, como "temperatura/estanque1" o "alertas/general". Veremos en detalle como disenarlos en la siguiente seccion.

**Niveles de calidad de servicio (QoS)**

No todos los mensajes tienen la misma importancia. MQTT ofrece tres niveles de garantia de entrega:

**QoS 0: "Dispara y olvida"**. El publicador envia el mensaje una vez y no verifica si llego. Es como gritar algo en una plaza llena de gente: quiza alguien lo escucho, quiza no. Es el mas rapido y el que menos ancho de banda consume, pero no hay garantia de entrega. Perfecto para datos de sensores que se envian cada pocos segundos: si se pierde una lectura, la siguiente llegara pronto.

**QoS 1: "Al menos una vez"**. El publicador envia el mensaje y espera una confirmacion (ACK) del broker. Si no la recibe, reenvia el mensaje. Es como enviar una carta certificada: sabes que llego porque te firman el acuse de recibo. La desventaja es que el mensaje puede llegar duplicado si el ACK se perdio pero el mensaje original si llego. Bueno para alertas importantes donde un duplicado es tolerable.

**QoS 2: "Exactamente una vez"**. Usa un intercambio de cuatro pasos para garantizar que el mensaje llega una y solo una vez. Es como un contrato legal: hay multiples verificaciones para asegurar que ambas partes estan de acuerdo. Es el mas lento y el que mas ancho de banda consume. Reservalo para acciones criticas como activar una bomba o abrir una valvula, donde un duplicado podria causar problemas.

```
  QoS 0:  Publicador ---mensaje---> Broker           (1 paso)
  QoS 1:  Publicador ---mensaje---> Broker ---ACK---> Publicador   (2 pasos)
  QoS 2:  Publicador <==== intercambio de 4 pasos ====> Broker    (4 pasos)
```

**Mensajes retenidos (retained messages)**

Cuando un publicador envia un mensaje con la bandera "retained" activada, el broker guarda ese mensaje como el ultimo valor conocido para ese topic. Cuando un nuevo suscriptor se conecta y se suscribe a ese topic, recibe inmediatamente el mensaje retenido sin tener que esperar a que el publicador envie uno nuevo. Es como llegar a una pizarra donde alguien ya escribio el ultimo valor: no tienes que esperar a que lo vuelvan a escribir.

Esto es muy util para datos de estado. Si un nodo publica su temperatura cada 30 segundos con la bandera retained, un dashboard que se conecte en cualquier momento vera inmediatamente la ultima temperatura conocida en lugar de una pantalla vacia durante hasta 30 segundos.

---

### Diseno de topics MQTT

Los topics MQTT son cadenas de texto jerarquicas separadas por el caracter barra `/`. Disenar bien la jerarquia de topics es como disenar bien la estructura de carpetas de un disco duro: si lo haces bien desde el principio, todo es facil de encontrar y de organizar. Si lo haces mal, terminas con un caos inmanejable.

**Nuestra jerarquia de topics**

Para la piscifactoria, usamos esta estructura:

```
piscifactoria / {gateway_id} / nodo / {nodo_id} / {tipo_sensor}
```

Ejemplos concretos:

```
piscifactoria/gw01/nodo/n03/temperatura
piscifactoria/gw01/nodo/n03/ph
piscifactoria/gw01/nodo/n07/oxigeno_disuelto
piscifactoria/gw02/nodo/n01/temperatura
piscifactoria/gw01/estado/conexion
piscifactoria/gw01/comando/nodo/n03/actuador
```

Cada nivel de la jerarquia tiene un proposito claro:
- **piscifactoria**: raiz del sistema, identifica el dominio
- **gateway_id**: identifica que gateway recogio el dato (util si tienes varias zonas)
- **nodo**: nivel fijo que indica que lo siguiente es un identificador de nodo
- **nodo_id**: identifica el nodo sensor especifico
- **tipo_sensor**: que magnitud se esta midiendo

**Comodines (wildcards)**

MQTT ofrece dos comodines para suscripciones, y son extraordinariamente poderosos:

El comodin `+` sustituye exactamente un nivel. Por ejemplo, `piscifactoria/+/nodo/+/temperatura` te suscribe a las lecturas de temperatura de TODOS los nodos de TODOS los gateways. El `+` dice "me da igual que haya en este nivel, quiero cualquier valor".

El comodin `#` sustituye todos los niveles restantes desde su posicion. Solo puede aparecer al final. Por ejemplo, `piscifactoria/gw01/#` te suscribe a absolutamente todo lo que publique el gateway 01: temperaturas, pH, estados, comandos, todo.

```
piscifactoria/gw01/#               -> todo del gateway 01
piscifactoria/+/nodo/+/temperatura  -> temperatura de todos los nodos
piscifactoria/#                     -> absolutamente todo el sistema
piscifactoria/gw01/nodo/n03/+      -> todos los sensores del nodo 03
```

**Por que los topics planos son una mala idea**

Podrias pensar: "por que no usar simplemente temperatura_nodo1, ph_nodo1, temperatura_nodo2 y listo?". El problema es que los topics planos no escalan. Si tienes 10 nodos con 4 sensores cada uno, necesitas conocer de antemano los 40 nombres de topics y suscribirte a cada uno individualmente. Si anadesun nodo nuevo, tienes que modificar el codigo del suscriptor. Con la jerarquia y los comodines, suscribirte a `piscifactoria/#` te da todo automaticamente, sin importar cuantos nodos anadas en el futuro.

Piensalo como la diferencia entre tener todas las fotos sueltas en el escritorio del ordenador (topics planos) versus tenerlas organizadas en carpetas por ano, mes y evento (topics jerarquicos). Con pocas fotos da igual, pero con miles es imprescindible la organizacion.

---

### El broker MQTT: Mosquitto

Si MQTT es un sistema de correo postal, el broker es la oficina central de correos. Recibe cartas (mensajes) de los remitentes (publicadores), mira la direccion (topic), y las entrega a todos los buzones que pidieron recibir correo de esa direccion (suscriptores). Sin la oficina de correos, nadie puede enviar ni recibir nada.

```
                    +------------------+
  Gateway --------->|                  |--------> Backend
  (publica)         |    MOSQUITTO     |--------> Dashboard
                    |    (broker)      |
  Dashboard ------->|                  |--------> Gateway
  (publica cmds)    +------------------+   (recibe cmds)
```

**Mosquitto** es el broker MQTT de codigo abierto mas popular del mundo. Es desarrollado por la fundacion Eclipse y es increiblemente versatil: puede correr en una Raspberry Pi con recursos minimos o en un servidor en la nube manejando miles de conexiones simultaneas.

Mosquitto ofrece varias capacidades importantes para nuestro sistema:

**Autenticacion**: puedes configurar usuarios y contrasenas para que solo dispositivos autorizados puedan conectarse. Sin esto, cualquiera que conozca la direccion del broker podria leer todos los datos de tu piscifactoria o, peor aun, enviar comandos falsos a los actuadores.

**Cifrado TLS**: los datos viajan encriptados entre los dispositivos y el broker. Igual que HTTPS protege tu navegacion web, TLS protege las comunicaciones MQTT. Especialmente importante si el broker esta en la nube y los datos viajan por internet.

**Sesiones persistentes**: si un suscriptor se desconecta brevemente (por ejemplo, por un corte de red), el broker puede recordar su suscripcion y guardar los mensajes que llegaron durante la desconexion. Cuando el suscriptor vuelve a conectarse, recibe los mensajes acumulados. Es como pedir al vecino que te recoja el correo mientras estas de vacaciones.

**Gestion de QoS**: el broker gestiona los intercambios de confirmacion necesarios para QoS 1 y QoS 2, asegurandose de que las garantias de entrega se cumplan.

En nuestro sistema, Mosquitto juega un papel central:
- El gateway publica datos de sensores HACIA el broker
- El backend se suscribe para recibir datos DESDE el broker y guardarlos en la base de datos
- El dashboard puede tambien suscribirse directamente via WebSocket (Mosquitto soporta esto con un listener adicional)
- Los comandos de control viajan en sentido inverso: el dashboard publica, el gateway se suscribe

El broker es el unico punto por el que pasan todos los mensajes. Esto lo convierte en un componente critico: si se cae, la comunicacion se detiene. Por eso es importante monitorizarlo y, en instalaciones de produccion, considerar configuraciones de alta disponibilidad con brokers redundantes.

---

### Patron bridge: el gateway como traductor

Piensa en el gateway como un interprete en una conferencia internacional. A un lado tiene a los delegados que hablan ESP-NOW: un protocolo local, binario, sin internet, optimizado para bajo consumo. Al otro lado tiene a los delegados que hablan MQTT: un protocolo de internet, basado en texto JSON, organizado por topics. El gateway es el unico que entiende ambos idiomas y traduce en tiempo real.

```
  Mundo ESP-NOW                    Mundo MQTT/Internet
  (local, binario)                 (JSON, topics)
                     +---------+
  [Nodo 1] )))      |         |    piscifactoria/gw01/nodo/n01/temp
  [Nodo 2] )))  --> | GATEWAY | -->  piscifactoria/gw01/nodo/n02/ph
  [Nodo 3] )))      | (bridge)|    piscifactoria/gw01/nodo/n03/o2
                     +---------+
                     <-- (comandos en sentido inverso) <--
```

**Flujo de datos: del sensor al broker**

Cuando un nodo envia datos de sensor via ESP-NOW, el gateway ejecuta los siguientes pasos:

1. Recibe la trama binaria por ESP-NOW (los bytes crudos que definimos en la Fase 2)
2. Deserializa esos bytes usando el protocolo que disenamos: extrae el ID del nodo, el tipo de mensaje, los valores de los sensores, la marca de tiempo
3. Formatea esos datos como un objeto JSON legible, por ejemplo: el ID del nodo, la temperatura con su valor y unidad, la marca de tiempo en formato estandar
4. Construye el topic MQTT adecuado concatenando la raiz, el ID del gateway, el ID del nodo y el tipo de sensor
5. Publica el mensaje JSON en ese topic a traves del broker

**Flujo de comandos: del broker al nodo**

En sentido inverso, cuando alguien envia un comando de control (por ejemplo, "activa el aireador del estanque 3"):

1. El gateway, que esta suscrito a topics de comando, recibe un mensaje JSON del broker
2. Analiza el JSON para extraer el nodo destino y la accion a realizar
3. Serializa el comando en el formato binario de nuestro protocolo ESP-NOW
4. Envia la trama binaria al nodo correspondiente via ESP-NOW

**Por que esta separacion es poderosa**

Esta arquitectura de puente tiene beneficios enormes:

Los nodos no necesitan saber nada sobre MQTT, ni sobre internet, ni sobre JSON. Solo conocen ESP-NOW y el protocolo binario. Esto los mantiene simples, con bajo consumo y faciles de programar.

El servidor y el dashboard no necesitan saber nada sobre ESP-NOW ni sobre el protocolo binario. Solo ven topics MQTT con datos JSON limpios. Podrias cambiar toda la tecnologia inalambrica de los nodos (de ESP-NOW a LoRa, por ejemplo) y el servidor no se enteraria: solo el gateway necesitaria adaptarse.

Es el mismo principio que un enchufe universal de viaje: adapta la forma del enchufe (protocolo) sin cambiar la electricidad (datos) que pasa por dentro.

---

### WebSocket: comunicacion bidireccional en tiempo real

Para entender WebSocket, primero hay que entender el problema que resuelve.

HTTP, el protocolo de la web, funciona asi: el navegador pide algo al servidor, el servidor responde, y la conexion se cierra. Si quieres saber si hay datos nuevos, tienes que volver a preguntar. Y otra vez. Y otra vez. Esto se llama "polling" y es como estar llamando a alguien por telefono cada 5 segundos para preguntar "tienes algo que decirme?". Funciona, pero es tremendamente ineficiente: cada llamada consume ancho de banda, tiempo y recursos del servidor, incluso cuando la respuesta es "no, no hay nada nuevo".

```
  HTTP (polling):
  Navegador: "Hay datos nuevos?"  --> Servidor: "No"
  Navegador: "Hay datos nuevos?"  --> Servidor: "No"
  Navegador: "Hay datos nuevos?"  --> Servidor: "Si, aqui tienes"
  Navegador: "Hay datos nuevos?"  --> Servidor: "No"
  (... y asi todo el rato)

  WebSocket:
  Navegador: "Quiero conectarme"  --> Servidor: "OK, conexion abierta"
  (... silencio hasta que hay algo ...)
  Servidor: "Oye, dato nuevo!"    --> Navegador: "Recibido"
  (... silencio ...)
  Servidor: "Otro dato!"          --> Navegador: "Recibido"
  Navegador: "Activa aireador"    --> Servidor: "Hecho"
```

WebSocket es como pasar de enviar cartas (HTTP) a tener una llamada telefonica abierta (WebSocket). La conexion se establece una vez y permanece abierta. Ambos lados pueden enviar mensajes en cualquier momento sin pedir permiso ni establecer una nueva conexion. Es verdaderamente bidireccional: tanto el servidor puede enviar datos al navegador como el navegador puede enviar comandos al servidor.

El proceso de conexion empieza con HTTP normal: el navegador hace una peticion HTTP especial que dice "quiero actualizar esta conexion a WebSocket". Si el servidor acepta, la conexion HTTP se transforma en una conexion WebSocket persistente. A partir de ese momento, los datos fluyen libremente en ambas direcciones.

En nuestro sistema, WebSocket es la pieza que conecta el backend con el dashboard:

1. El backend recibe datos de sensores via MQTT (suscrito al broker)
2. Inmediatamente, sin esperar a que nadie pregunte, empuja esos datos a traves de la conexion WebSocket a todos los navegadores que tienen el dashboard abierto
3. Los navegadores actualizan la interfaz en tiempo real: los numeros cambian, las graficas se extienden, las alertas aparecen
4. Si el operador envia un comando desde el dashboard (por ejemplo, activar un actuador), el mensaje viaja por WebSocket al backend, que lo publica en MQTT, llega al broker, y de ahi al gateway

El resultado es una experiencia fluida: el operador ve los datos cambiar en tiempo real, como si estuviera mirando directamente el sensor. La latencia tipica del sistema completo (sensor a pantalla) es de unos pocos cientos de milisegundos.

---

### React basico para el dashboard

React es una libreria JavaScript creada por Meta (Facebook) para construir interfaces de usuario. Es una de las herramientas mas populares para desarrollo web moderno, y es ideal para nuestro dashboard porque esta disenada para manejar interfaces que cambian frecuentemente, exactamente lo que necesitamos con datos de sensores en tiempo real.

No necesitas ser experto en React para entender el dashboard, pero si necesitas conocer cuatro conceptos fundamentales:

**Componentes**

Un componente es una pieza reutilizable de la interfaz. Piensa en ellos como piezas de LEGO: cada una tiene una forma y funcion especifica, y las combinas para construir algo mas grande. En nuestro dashboard, tendriamos componentes como: una tarjeta que muestra el valor actual de un sensor, una grafica que muestra el historico de temperatura, un indicador de estado que cambia de color (verde/amarillo/rojo) segun los umbrales, una tabla con los ultimos eventos.

Cada componente es independiente y encapsula su propia logica de presentacion. Puedes tener 20 tarjetas de sensor en pantalla, y cada una es una instancia del mismo componente mostrando datos diferentes.

**Estado (State)**

El estado es la informacion que cambia dentro de un componente. La temperatura actual del estanque 3 es un estado. Si la conexion con el broker esta activa o no es un estado. La pagina que el usuario tiene seleccionada en el menu es un estado. Cuando el estado cambia, React automaticamente actualiza solo la parte de la pantalla que necesita cambiar, sin recargar toda la pagina. Esto es lo que hace que la interfaz sea tan fluida.

**Props (Propiedades)**

Las props son datos que un componente padre pasa a un componente hijo. Es como llenar un formulario: el formulario (componente hijo) tiene campos vacios, y el padre los rellena con valores concretos. Por ejemplo, un componente "TarjetaSensor" podria recibir como props el nombre del sensor, el valor actual, la unidad de medida y el color del umbral. El mismo componente muestra temperaturas, pH o niveles de oxigeno dependiendo de las props que reciba.

**Hooks**

Los hooks son funciones especiales que permiten a los componentes usar estado y ejecutar efectos secundarios. Los dos mas importantes son: useState, que permite a un componente recordar y actualizar datos (como el ultimo valor recibido de un sensor), y useEffect, que permite ejecutar acciones cuando el componente aparece en pantalla o cuando algo cambia (como abrir la conexion WebSocket cuando el dashboard se carga y cerrarla cuando el usuario lo cierra).

**Estructura del dashboard**

Nuestro dashboard tiene varias paginas, y cada pagina es un componente que contiene sub-componentes:

```
  Dashboard
    |
    |-- Pagina: Vista general en tiempo real
    |     |-- Mapa/diagrama de estanques
    |     |-- Tarjetas de resumen por estanque
    |     |-- Indicadores de alerta
    |
    |-- Pagina: Graficas historicas
    |     |-- Selector de rango de tiempo
    |     |-- Graficas de linea (temperatura, pH, O2...)
    |     |-- Tabla de datos exportable
    |
    |-- Pagina: Configuracion
          |-- Umbrales de alerta
          |-- Gestion de nodos
          |-- Estado del sistema
```

La libreria Recharts (construida sobre React) nos permite crear graficas interactivas de forma sencilla: graficas de linea para temperaturas a lo largo del tiempo, graficas de barras para comparar estanques, indicadores de tipo gauge para valores actuales.

---

### Datos de series temporales

Los datos de sensores tienen una naturaleza especial: son "series temporales" (time series). Esto significa que cada dato es un valor asociado a un momento en el tiempo: "a las 14:32:07 la temperatura del estanque 3 era 22.4 grados". Esta naturaleza los hace fundamentalmente diferentes de los datos que almacena, por ejemplo, una tienda en linea (nombre del producto, precio, stock).

**Caracteristicas de las series temporales**

**Solo se anade, casi nunca se modifica (append-only)**: cuando llega una nueva lectura de temperatura, se agrega al final de la serie. Nadie va a "editar" la temperatura que habia ayer a las 3 de la tarde. Los datos historicos son inmutables.

**Las consultas son basadas en tiempo**: las preguntas tipicas son "muestrame la temperatura de las 2 a las 4 de la tarde", "cual fue el maximo pH de la ultima semana", "cuando fue la ultima vez que el oxigeno bajo de 5 mg/L". El tiempo es siempre el eje principal.

**La agregacion es fundamental**: rara vez quieres ver los 86.400 valores individuales de un dia (uno por segundo). Lo normal es querer promedios por hora, maximos por dia, minimos por semana. Agregar (resumir) los datos es una operacion constante.

**Los datos recientes son mas valiosos**: la temperatura de hace 5 minutos es critica. La de hace 6 meses es interesante para tendencias pero no para operar. Esto sugiere politicas de retencion: guardar datos detallados (cada segundo) durante una semana, promedios por minuto durante un mes, promedios por hora durante un ano.

**SQLite como punto de partida**

SQLite es una base de datos relacional embebida: no necesita un servidor separado, es un solo archivo en disco, y viene incluida en practicamente todo. Para empezar, es perfecta: cero configuracion, facil de respaldar (copiar un archivo), y suficiente rendimiento para una piscifactoria de tamano pequeno o mediano.

Una tabla simple con columnas como marca de tiempo, identificador del nodo, tipo de sensor y valor numerico es suficiente para almacenar todas las lecturas. Las consultas por rango de tiempo funcionan bien si creamos un indice sobre la columna de tiempo.

**InfluxDB para crecer**

Cuando el volumen de datos crece (muchos nodos, muchos sensores, mucho historico), SQLite empieza a sufrir. InfluxDB es una base de datos disenada especificamente para series temporales. Sus ventajas incluyen: consultas sobre rangos de tiempo que son ordenes de magnitud mas rapidas, politicas de retencion automaticas (por ejemplo, borrar datos de mas de un ano sin intervencion manual), funciones de agregacion integradas (promedio, maximo, minimo, percentiles) optimizadas para este tipo de datos, y un lenguaje de consulta disenado para preguntas temporales.

Nuestro backend empieza con SQLite porque la simplicidad importa al principio. La arquitectura esta disenada para que la migracion a InfluxDB sea sencilla cuando sea necesario: solo cambia la capa de almacenamiento, el resto del sistema (MQTT, WebSocket, React) no se entera.

---

## Como encaja en el proyecto

La Fase 4 cierra el circuito completo. Hasta ahora, los datos vivian unicamente en el mundo de los microcontroladores: se median, se empaquetaban, se transmitian de nodo a gateway... y ahi se quedaban. Ahora, por primera vez, los datos escapan del hardware y llegan a donde un ser humano puede verlos, analizarlos y actuar sobre ellos.

El camino completo que recorre un dato, de principio a fin:

```
  Sensor fisico (termistor en el agua)
       |
       v
  Nodo ESP32 (lee el sensor, empaqueta en protocolo binario)
       |  [ESP-NOW, inalambrico, local]
       v
  Gateway ESP32 (recibe, traduce de binario a JSON)
       |  [WiFi/Internet, MQTT]
       v
  Broker Mosquitto (distribuye el mensaje a los suscriptores)
       |              \
       v               v
  Backend             Dashboard (suscripcion directa opcional)
  (recibe por MQTT,
   guarda en SQLite)
       |  [WebSocket]
       v
  Frontend React (recibe en tiempo real, muestra graficas)
       |
       v
  Pantalla del navegador del operador
```

Esta es la fase donde el sistema se convierte en algo verdaderamente util para los operadores de la piscifactoria. Ya no necesitan ir fisicamente a cada estanque para saber la temperatura o el nivel de oxigeno. Desde un navegador web, en cualquier lugar, pueden:

- Ver los valores actuales de todos los sensores en tiempo real
- Revisar el historico de cualquier variable con graficas interactivas
- Configurar umbrales de alerta (por ejemplo: "avisame si la temperatura sube de 28 grados")
- Enviar comandos a los actuadores (activar aireadores, valvulas, alimentadores)
- Detectar tendencias problematicas antes de que se conviertan en emergencias

Cada fase anterior fue un requisito para esta: sin el protocolo binario eficiente (Fase 2), los nodos no podrian comunicarse de forma fiable. Sin ESP-NOW (Fase 3), no habria red inalambrica local. Y sin esta Fase 4, los datos no llegarian nunca al ser humano que toma decisiones.

---

## Errores tipicos y como evitarlos

**1. No proteger el broker con autenticacion**

Dejar Mosquitto sin usuario ni contrasena es como dejar la puerta de la granja abierta. Cualquier persona que conozca la IP del broker puede leer todos los datos de la piscifactoria o, peor, enviar comandos falsos. Desde el primer dia, configura al menos autenticacion basica con usuario y contrasena. Para produccion, anade cifrado TLS.

**2. Usar QoS 2 para todo "por si acaso"**

Es tentador usar siempre el nivel maximo de garantia, pero QoS 2 consume significativamente mas ancho de banda y tiempo que QoS 0. Para lecturas de sensores que llegan cada pocos segundos, QoS 0 es suficiente: si se pierde una, la siguiente viene enseguida. Reserva QoS 1 o 2 para comandos criticos y alertas.

**3. Disenar topics planos sin jerarquia**

Empezar con topics como "temp_nodo1", "ph_nodo2" parece mas simple, pero cuando tienes 30 nodos con 5 sensores cada uno, tienes 150 topics individuales a los que suscribirte y mantener. Invierte tiempo en disenar una buena jerarquia desde el principio. El esfuerzo se paga cien veces cuando el sistema crece.

**4. No manejar la desconexion del WebSocket en el frontend**

Las conexiones WebSocket se cortan: el usuario cambia de red WiFi, el servidor se reinicia, hay un corte momentaneo de internet. Si el dashboard no detecta la desconexion y no intenta reconectarse automaticamente, el operador se queda mirando datos congelados creyendo que todo esta bien. Implementa siempre logica de reconexion con backoff exponencial (esperar 1 segundo, luego 2, luego 4, luego 8... para no saturar el servidor con intentos).

**5. Guardar datos crudos sin marca de tiempo del servidor**

Los nodos ESP32 no tienen reloj de tiempo real preciso. Si guardas la marca de tiempo que envia el nodo, puedes tener datos desordenados o con tiempos incorrectos. Es mas fiable que el backend anade su propia marca de tiempo en el momento de recibir el dato (ademas de conservar la del nodo como referencia). Asi garantizas un reloj consistente.

**6. No limitar el historico de datos en la interfaz**

Pedirle al dashboard que dibuje una grafica con un millon de puntos colgara el navegador del usuario. Siempre agrega los datos antes de enviarlos al frontend: si el usuario pide un ano de datos, envia promedios por hora (8.760 puntos), no lecturas individuales por segundo (31 millones de puntos). El backend debe hacer la agregacion, no el navegador.

**7. Olvidar el "Last Will and Testament" de MQTT**

MQTT tiene una funcion llamada LWT (Ultimo Testamento): al conectarse, un cliente puede decir "si me desconecto inesperadamente, publica este mensaje en este topic". Es ideal para que el gateway publique automaticamente un estado "desconectado" si se cae. Sin esto, el dashboard no sabe si el gateway esta desconectado o simplemente no hay datos nuevos.

**8. No separar la logica del gateway en capas claras**

Es facil escribir el gateway como un programa monolitico donde la recepcion ESP-NOW, la conversion y la publicacion MQTT estan mezcladas en una sola funcion. Esto funciona al principio pero se vuelve imposible de depurar y modificar. Separa claramente: una capa para recibir ESP-NOW, otra para traducir formatos, otra para publicar MQTT. Asi puedes probar cada parte por separado.

---

## Experimenta

> **Experimento 1: Suscribete a topics MQTT con mosquitto_sub y observa el flujo de datos**
>
> Instala las herramientas de linea de comandos de Mosquitto en tu ordenador. Abre una terminal y ejecuta mosquitto_sub con el topic comodin `piscifactoria/#` apuntando a la direccion de tu broker. Veras aparecer en tiempo real cada mensaje que el gateway publica. Prueba a cambiar el filtro: suscribete solo a un nodo especifico, solo a lecturas de temperatura, o solo a un gateway. Observa como los comodines `+` y `#` filtran los mensajes. Luego, en otra terminal, usa mosquitto_pub para publicar un mensaje manualmente en un topic de comando y comprueba que el gateway lo recibe y actua.

> **Experimento 2: Que pasa si el broker se cae?**
>
> Con el sistema funcionando (nodos enviando datos, gateway publicando, dashboard mostrando), detente el servicio de Mosquitto. Observa que ocurre: el gateway intenta publicar y falla? Se acumulan mensajes? El dashboard se congela o muestra un error? Ahora vuelve a iniciar Mosquitto. Se reconectan automaticamente el gateway y el dashboard? Se perdieron mensajes durante la caida? Este experimento te ensenara la importancia de manejar desconexiones graciosamente y te motivara a implementar colas locales en el gateway para buffering.

> **Experimento 3: Mide la latencia extremo a extremo**
>
> Quieres saber cuanto tarda un dato en viajar desde el sensor hasta la pantalla. Una forma simple: haz que el nodo incluya su marca de tiempo (millis) en el mensaje. En el dashboard, cuando llega un dato, registra el tiempo actual del navegador. La diferencia te da una aproximacion de la latencia total (no sera exacta porque los relojes no estan sincronizados, pero te da un orden de magnitud). Prueba con distintas condiciones: con el broker en la misma red local versus en la nube, con QoS 0 versus QoS 1, con un solo nodo versus diez nodos publicando simultaneamente.

> **Experimento 4: Explora los mensajes retenidos**
>
> Publica un mensaje con la bandera "retained" en un topic de sensor usando mosquitto_pub. Luego cierra el suscriptor y vuelve a abrirlo. Observa como recibe inmediatamente el ultimo mensaje retenido sin esperar a que el publicador envie uno nuevo. Ahora publica un mensaje SIN la bandera retained y repite el proceso. Nota la diferencia: sin retained, el nuevo suscriptor no recibe nada hasta que llega un mensaje nuevo. Esto te ayudara a decidir que topics de tu sistema deberian usar mensajes retenidos.

---

## Para profundizar

- **Especificacion oficial de MQTT 5.0** (docs.oasis-open.org/mqtt): el documento tecnico completo del protocolo. Denso pero es la referencia definitiva. Las secciones sobre QoS y sesiones persistentes son especialmente relevantes para nuestro sistema.

- **Documentacion de Eclipse Mosquitto** (mosquitto.org): guias de configuracion, opciones de seguridad, configuracion de listeners WebSocket, bridges entre brokers. Imprescindible para poner en produccion el broker.

- **Documentacion oficial de React** (react.dev): el tutorial interactivo "Learn React" es excelente para principiantes. Las secciones sobre estado, efectos y pensamiento en componentes son las mas relevantes para nuestro dashboard.

- **Documentacion de Recharts** (recharts.org): la libreria de graficas que usamos en el dashboard. Los ejemplos de LineChart y AreaChart son directamente aplicables a nuestras graficas de sensores.

- **API de WebSocket del navegador** (developer.mozilla.org): la referencia de la API nativa de WebSocket en JavaScript. Cubre el ciclo de vida de la conexion (open, message, close, error) y como enviar y recibir datos.

- **Documentacion de InfluxDB** (docs.influxdata.com): para cuando el sistema crezca y necesites migrar de SQLite. Las guias sobre modelado de datos de series temporales y politicas de retencion son particularmente utiles.

- **HiveMQ MQTT Essentials** (hivemq.com/mqtt-essentials): una serie de articulos cortos y claros que explican cada aspecto de MQTT con ejemplos practicos. Excelente como complemento a la especificacion formal.

---

## Preguntas de reflexion

**1.** Si el broker Mosquitto se cae durante 5 minutos y hay 10 nodos enviando datos cada 10 segundos, que ocurre con esos 300 mensajes? Depende la respuesta del QoS configurado? Que mecanismos podrias implementar en el gateway para no perder datos durante la caida?

**2.** Hemos dicho que los nodos no necesitan saber nada sobre MQTT y que el servidor no necesita saber nada sobre ESP-NOW. Que ventajas concretas tiene esta separacion si manana decides reemplazar ESP-NOW por LoRa (un protocolo de largo alcance)? Que partes del sistema tendrias que modificar y cuales permanecerian intactas?

**3.** Para el dashboard de una piscifactoria con 50 estanques y 5 sensores por estanque, cada sensor enviando datos cada 5 segundos: cuantos mensajes MQTT por minuto genera el sistema? Cuantos puntos de datos se acumulan en un dia? En un ano? A partir de que volumen crees que SQLite dejaria de ser suficiente?

**4.** Un operador abre el dashboard a las 10 de la manana. Sin mensajes retenidos, que veria en pantalla hasta que lleguen los primeros datos? Como mejoran los mensajes retenidos esta experiencia? Hay algun caso en que un mensaje retenido podria ser danino (por ejemplo, mostrando informacion obsoleta)?

**5.** Hemos elegido WebSocket para la comunicacion en tiempo real entre backend y frontend. Que pasaria si usaramos polling HTTP (preguntar cada segundo)? Calcula aproximadamente cuanto ancho de banda adicional consumiria el polling con 50 estanques y 5 sensores cada uno, considerando que cada peticion HTTP tiene unos 500 bytes de cabeceras.

**6.** En nuestro sistema, el dato pasa por muchas manos: sensor fisico, nodo ESP32, gateway, broker, backend, WebSocket, frontend. Cada salto anade latencia y un punto potencial de fallo. Si tuvieras que simplificar la arquitectura eliminando un componente, cual eliminarias? Que perderiaspor? Hay alguno que sea verdaderamente imposible de eliminar?

**7.** Las politicas de retencion de datos son un compromiso entre almacenamiento y detalle historico. Si guardas todo con resolucion de un segundo durante un ano, cuanto espacio ocuparia (estima el tamano de un registro y multiplica)? Disena una politica de retencion por niveles que equilibre detalle reciente con historico a largo plazo sin consumir demasiado disco.

**8.** El broker MQTT es un punto unico de fallo: si se cae, todo se detiene. Que estrategias existen para hacer el sistema mas resiliente? Investiga sobre "MQTT broker bridging" y clusters. Cual seria la solucion mas simple para una piscifactoria pequena y cual para una instalacion industrial grande?
