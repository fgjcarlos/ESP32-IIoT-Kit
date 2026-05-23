# Tutorial 6: Hacia Donde Seguir

## Que vas a aprender

- Conceptos de redes mesh y por que son dificiles en dispositivos con recursos limitados
- Que es machine learning en el edge y como puede mejorar la deteccion de anomalias
- IoT celular: modulos 4G, comandos AT, y conectividad de respaldo
- Que cambia cuando escalas de 5 a 50 nodos
- Store-and-forward: como no perder datos cuando la red falla
- Diseño de APIs publicas y sistemas multi-usuario

---

## Conceptos clave

### Mesh networking: mas alla del punto a punto

En el sistema v1.0, cada nodo habla directamente con el gateway. Es una topologia de estrella:

```
        Nodo A ──┐
                  │
        Nodo B ──┼── Gateway
                  │
        Nodo C ──┘
```

Esto funciona bien cuando todos los nodos estan al alcance del gateway. Pero ¿que pasa si Nodo D esta detras de un edificio, demasiado lejos?

En una red mesh, los nodos pueden reenviar mensajes de otros nodos:

```
        Nodo A ──────── Gateway
          │
        Nodo D            (D no alcanza al gateway, pero si a A)
          │
        Nodo E            (E no alcanza ni al gateway ni a A, pero si a D)
```

Nodo E envia a D, D reenvia a A, A reenvia al gateway. El mensaje "salta" por la red hasta llegar a destino. Es como el juego del telefono, pero con verificacion en cada paso.

Suena elegante, pero es enormemente complejo de implementar correctamente:

**Routing**: ¿Como sabe un nodo por donde enviar un mensaje? Necesita descubrir quien es su vecino, quien esta mas cerca del gateway, y elegir la mejor ruta. Si un nodo intermedio desaparece (bateria agotada), hay que recalcular las rutas.

**Bucles**: Sin proteccion, un mensaje podria rebotar entre nodos infinitamente (A envia a B, B a C, C a A...). La solucion es el TTL (Time-To-Live): cada salto decrementa un contador. Cuando llega a cero, el mensaje se descarta.

**Energia**: Un nodo relay no puede estar en deep sleep — necesita estar despierto para recibir y reenviar mensajes de otros. Esto reduce dramaticamente su autonomia de bateria. Un nodo normal dura 12+ meses; un nodo relay quizas 1-2 meses.

**Colisiones**: Si dos nodos intentan enviar al mismo relay simultaneamente, los mensajes colisionan y se pierden. Se necesitan mecanismos de acceso al medio (similar a como varias personas no pueden hablar a la vez por un walkie-talkie).

ESP-NOW no tiene mesh nativo. Espressif ofrece ESP-MESH (basado en WiFi, no ESP-NOW), pero consume mucha mas energia. Para nuestro proyecto, la solucion practica es relay estatico (configurado manualmente) o relay semi-automatico con descubrimiento de vecinos, pero sin routing dinamico completo. El mesh completo es un proyecto de investigacion en si mismo.

### Machine Learning en el edge: inteligencia local

Hasta ahora, las alertas se basan en umbrales fijos: si la temperatura supera 28°C, alerta. Pero los problemas reales en una piscifactoria rara vez son tan simples. A veces es una combinacion sutil: el pH baja ligeramente mientras la temperatura sube y el oxigeno disminuye — individualmente cada valor esta dentro del rango, pero la combinacion indica un problema.

Machine Learning (ML) puede detectar estos patrones. La idea es:

1. **Entrenar**: En un PC potente, alimentar al modelo con semanas de datos "normales" de la piscifactoria. El modelo aprende que patrones son normales.

2. **Desplegar**: Convertir el modelo a un formato ultraligero (TFLite Micro, ~50KB) y cargarlo en el ESP32-S3 del gateway.

3. **Inferir**: Cuando llegan nuevas lecturas, el modelo las compara con lo que aprendio como "normal". Si la lectura no encaja, genera una alerta de anomalia.

La diferencia con los umbrales fijos:

```
Umbrales fijos:           Machine Learning:
"temp > 28°C = alerta"   "Esta combinacion de temp + pH + DO
                          no se parece a nada que haya visto
                          antes = anomalia"
```

El ESP32-S3 es especialmente adecuado para esto: tiene instrucciones vectoriales (SIMD) que aceleran las operaciones matematicas que el modelo necesita. Un modelo pequeño puede ejecutar inferencia en menos de 100ms.

Las limitaciones son reales: el entrenamiento requiere un PC (no puedes entrenar en el ESP32), necesitas datos historicos suficientes (minimo 2 semanas), y el modelo solo puede detectar anomalias respecto a lo que aprendio como "normal" — si las condiciones normales cambian (por ejemplo, cambio de estacion), hay que reentrenar.

### IoT celular: cuando WiFi no es una opcion

No todas las piscifactorias tienen cobertura WiFi. Algunas estan en ubicaciones remotas donde la unica conectividad disponible es la red movil. Un modulo 4G/LTE como el SIM7600 permite al gateway conectarse a internet a traves de la red de telefonia movil.

La comunicacion funciona via comandos AT: instrucciones de texto que el ESP32 envia al modulo por UART (conexion serie). Son herencia de los modems telefonicos de los años 80. El proceso:

```
ESP32-S3 ←─ UART ─→ SIM7600 ←─ 4G ─→ Torre movil ←→ Internet
```

El ESP32 envia comandos como "registrate en la red" y "establece conexion de datos". Una vez establecida la conexion PPP (Point-to-Point Protocol), el ESP32 tiene acceso a internet como si tuviera WiFi, y MQTT funciona transparentemente.

La estrategia ideal es usar 4G como backup:

- **Modo normal**: WiFi STA conectado al router local. Bajo coste, alta velocidad.
- **Modo backup**: WiFi caido > 5 minutos. Se activa 4G automaticamente. Los datos fluyen por red movil.
- **Recuperacion**: WiFi vuelve. Se desactiva 4G para ahorrar datos/coste. MQTT reconecta por WiFi.

Consideraciones importantes: el 4G consume significativamente mas energia que WiFi, los datos moviles tienen coste, y la latencia es mayor (~100-200ms vs ~5ms). Por eso es un backup, no el medio principal.

### Escalabilidad: de 5 a 50 nodos

Un sistema que funciona bien con 5 nodos puede fallar con 50. Los problemas de escala no son lineales — aparecen de golpe al superar ciertos umbrales:

**Limite de peers ESP-NOW**: Con cifrado, el maximo es 6 peers (no 20 como a veces se indica). Con 50 nodos, necesitas estrategias: usar broadcast en vez de unicast, rotar peers, o aceptar peers sin cifrado (menos seguro).

**Concurrencia**: 50 nodos que despiertan aleatoriamente cada 5 minutos generan, en promedio, un paquete cada 6 segundos. Pero por la distribucion aleatoria, habra momentos con 3-4 paquetes en un segundo. El gateway debe manejar estos picos sin perder datos.

**Memoria**: El registro de 50 nodos con su historial reciente consume mas RAM. Si cada nodo almacena 10 lecturas recientes con metadatos, son 50 x 10 x ~64 bytes = 32KB solo para el registro.

**Base de datos**: Con 50 nodos cada 5 minutos, son 600 lecturas por hora, 14.400 por dia, 5 millones al año. SQLite manejara esto, pero las consultas de historico se ralentizaran. Es el momento de considerar InfluxDB (base de datos diseñada para series temporales).

**Dashboard**: Mostrar 50 nodos en una pantalla requiere diseño cuidadoso: agrupacion por zona, filtros, paginacion. No puedes tener 50 tarjetas en una sola pagina.

La leccion principal: diseña para la escala que necesitas ahora, pero con puntos de extension claros para crecer. No optimices prematuramente para 500 nodos si vas a tener 10.

### Store-and-forward: no perder datos nunca

En el mundo real, las conexiones fallan. El router se reinicia, el broker MQTT se actualiza, internet se cae durante una tormenta. Si el gateway no puede publicar por MQTT, ¿que pasa con los datos de los sensores que siguen llegando por ESP-NOW?

Sin proteccion: se pierden. El gateway los recibe, intenta publicar, falla, y los descarta.

Con store-and-forward: el gateway detecta que MQTT esta offline y guarda los datos localmente (tarjeta SD o flash). Cuando la conexion vuelve, envía los datos almacenados con sus timestamps originales. Para el servidor, es como si los datos hubieran llegado en tiempo real, solo que con un retraso.

```
Flujo normal:
  Nodo → ESP-NOW → Gateway → MQTT → Servidor ✓

Sin conexion (store):
  Nodo → ESP-NOW → Gateway → [MQTT offline] → Guardar en SD

Reconexion (forward):
  Gateway → Leer SD → MQTT → Servidor ✓
  (con timestamps originales)
```

Es como el buzon de correo de tu casa: si no estas cuando llega el cartero, las cartas se acumulan en el buzon. Cuando vuelves, las recoges todas. No se pierden, solo llegan con retraso.

Consideraciones: la SD tiene capacidad finita (32GB son muchos meses de datos), hay que gestionar la rotacion (borrar datos viejos), y al reconectar no debes enviar todo el backlog de golpe (saturarias el broker y la red). Enviar en lotes pequeños con pausas entre ellos.

### APIs publicas y sistemas multi-usuario

Un sistema de produccion necesita integrarse con otros sistemas y servir a multiples usuarios con diferentes necesidades:

**Multi-usuario con roles**: El gerente de la piscifactoria necesita ver todo y configurar alertas. El operario necesita ver datos y controlar actuadores. El visitante solo necesita ver datos. Cada uno debe tener su cuenta con permisos adecuados.

Los roles tipicos son:
- **Admin**: Gestiona usuarios, configura todo el sistema
- **Operador**: Ve datos, controla actuadores, configura alertas
- **Visor**: Solo lectura, ve datos y graficos

La autenticacion se implementa con JWT (JSON Web Tokens): el usuario se identifica una vez (login), recibe un token firmado que incluye su identidad y rol. En cada peticion posterior, el servidor verifica el token sin necesidad de consultar la base de datos. Es como un pase de acceso: lo muestras al entrar y el guardia verifica que es autentico y que tu nivel de acceso te permite entrar a esa zona.

**API publica**: Permite que otros programas (no solo el dashboard) accedan a los datos. Un ERP de la piscifactoria podria consultar datos de sensores. Un servicio de SMS podria suscribirse a alertas. Un investigador podria descargar historicos para analisis.

La API publica debe ser:
- Documentada (OpenAPI/Swagger genera documentacion interactiva automaticamente)
- Versionada (`/api/v1/...`) para poder evolucionar sin romper integraciones existentes
- Limitada (rate limiting: maximo N peticiones por minuto para evitar abuso)
- Autenticada (API keys en vez de usuario/password, mas facil de revocar)

---

## Como encaja en el proyecto

La Fase 6 representa la evolucion natural de un sistema IoT desplegado. Cada mejora aborda una limitacion que descubriras en el uso real:

- **Mesh**: "Los nodos del fondo de la instalacion no alcanzan al gateway"
- **ML**: "Los umbrales fijos no detectan problemas graduales"
- **4G**: "No hay WiFi en esta ubicacion"
- **SD backup**: "Perdimos datos del fin de semana por un corte de internet"
- **Multi-usuario**: "El operario no deberia poder cambiar la configuracion del sistema"
- **API publica**: "El sistema de gestion de la empresa necesita acceder a los datos de agua"

No necesitas implementar todo. Elige lo que resuelve tu problema mas urgente y avanza paso a paso, igual que has hecho en las 6 fases anteriores.

---

## Errores tipicos y como evitarlos

1. **Implementar mesh antes de tener el sistema basico estable**: Mesh añade complejidad exponencial. Si el sistema punto-a-punto no es fiable, mesh solo multiplicara los problemas. Asegurate de que v1.0 funciona perfectamente antes de añadir mesh.

2. **Entrenar el modelo ML con datos insuficientes o contaminados**: Un modelo entrenado con solo 2 dias de datos "normales" no ha visto suficiente variabilidad (dia/noche, lluvia/sol, alimentacion de peces). Un modelo entrenado con datos que incluyen anomalias las considerara normales. Necesitas semanas de datos limpios.

3. **Usar 4G como conexion principal**: El coste de datos y el consumo de energia lo hacen inviable como medio principal. Diseñalo siempre como backup.

4. **No versionar la API publica**: Si cambias el formato de respuesta de `/api/nodes`, todos los clientes que la usan se rompen. Con versionado (`/api/v1/`), puedes crear `/api/v2/` con el nuevo formato mientras v1 sigue funcionando.

5. **Store-and-forward sin limite de almacenamiento**: Si nunca borras datos viejos de la SD, eventualmente se llena y el sistema falla. Implementa rotacion desde el primer dia.

---

## Experimenta

> **Experimenta 1**: Coloca dos ESP32-C3 a una distancia donde uno no alcance al gateway pero si al otro. ¿Puedes implementar un relay manual (el nodo intermedio reenvia los datos del nodo lejano)? ¿Cuanto afecta al consumo de bateria del nodo relay?

> **Experimenta 2**: Exporta datos historicos de tus sensores a un CSV. Abrelo en una hoja de calculo y busca manualmente patrones. ¿Puedes identificar momentos "normales" y momentos "raros"? Esto es esencialmente lo que el modelo ML hace automaticamente.

> **Experimenta 3**: Desconecta el router y observa cuanto tiempo tarda el gateway en detectar la perdida. ¿Los datos de ESP-NOW se siguen procesando? Ahora imagina que tienes una SD: ¿cuantos datos podrias almacenar en una tarjeta de 8GB a razon de 10 nodos cada 5 minutos?

---

## Para profundizar

- ESP-MESH (referencia de Espressif): https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/esp-wifi-mesh.html
- TFLite Micro para ESP32: https://github.com/espressif/esp-tflite-micro
- ESP-IDF Modem (PPP/4G): https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/network/esp_netif.html
- SD Card en ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/storage/sdmmc.html
- OpenAPI Specification: https://swagger.io/specification/
- JWT Introduction: https://jwt.io/introduction
- SIMCom SIM7600 documentacion: https://www.simcom.com/product/SIM7600X.html

---

## Preguntas de reflexion

1. ¿Por que una red mesh con nodos que duermen la mayor parte del tiempo es fundamentalmente diferente de una red mesh con nodos siempre encendidos? ¿Que compromiso de energia implica ser un nodo relay?

2. Un modelo de ML detecta una "anomalia" que resulta ser simplemente una condicion climatica nueva (primera ola de calor del año). ¿Es un falso positivo? ¿Como ajustarias el modelo?

3. Si el coste de datos moviles es 0.01€ por MB, y tu gateway envia un mensaje MQTT de 200 bytes cada 30 segundos via 4G, ¿cuanto costaria al mes? ¿Como reducirias el coste?

4. ¿Que ventaja tiene usar JWT en vez de sesiones de servidor para autenticacion? ¿Y que desventaja tiene (pista: revocacion)?

5. ¿Por que es mala idea enviar todo el backlog de la SD de golpe cuando MQTT reconecta? ¿Que estrategia de envio usarias?

6. Si tu sistema crece a 100 nodos, ¿que componente sera el primero en convertirse en cuello de botella: el gateway, el broker MQTT, la base de datos, o el dashboard? ¿Por que?
