# Tutorial 5: Actualizaciones Seguras y Sistema Robusto

## Que vas a aprender

- Como actualizar el firmware de un microcontrolador sin cable USB (OTA)
- Por que se necesitan dos copias del firmware y como funciona el rollback
- Como funciona TLS en sistemas embebidos con recursos limitados
- Las diferentes capas de seguridad en un sistema IoT
- Metodologia para probar un sistema bajo condiciones adversas
- Conceptos basicos de ingenieria de fiabilidad

---

## Conceptos clave

### OTA: actualizaciones sin cables

Imagina que tienes 15 nodos sensores distribuidos por una instalacion industrial, algunos en lugares de dificil acceso. Descubres un bug en el firmware. ¿Vas a ir nodo por nodo con un cable USB? OTA (Over-The-Air) resuelve esto: el dispositivo descarga el nuevo firmware a traves de la red inalambrica y se actualiza solo.

El proceso simplificado es asi:

```
Estado actual                    Proceso OTA                    Resultado
┌──────────┐                                                 ┌──────────┐
│ Firmware  │  1. Conectar a servidor                        │ Firmware  │
│ v1.0      │  2. Descargar v1.1          ──────────►        │ v1.1      │
│ (running) │  3. Escribir en flash                          │ (running) │
│           │  4. Reiniciar                                  │           │
└──────────┘                                                 └──────────┘
```

El desafio es: ¿como cambias las ruedas de un coche mientras conduces? No puedes sobreescribir el firmware que esta ejecutandose. La solucion es tener dos espacios separados en la flash.

### Particiones A/B: el truco de la doble copia

La memoria flash del ESP32 se divide en particiones, como un disco duro con varias particiones. Para OTA, necesitas dos particiones de aplicacion: `ota_0` y `ota_1`. Solo una esta activa a la vez.

```
Flash del ESP32 (8MB en el gateway)
┌────────────────────────────────────────────────┐
│ Bootloader │ Part. Table │ otadata              │
├────────────┴─────────────┴──────────────────────┤
│                                                  │
│   ota_0 (3MB)          │    ota_1 (3MB)          │
│   [Firmware v1.0]      │    [vacio]              │
│   ◄── ACTIVA           │                         │
│                         │                         │
├─────────────────────────┴────────────────────────┤
│  SPIFFS (1.9MB) - archivos web                   │
└──────────────────────────────────────────────────┘
```

Cuando descargas una actualizacion, se escribe en la particion INACTIVA. Si ota_0 esta ejecutandose, la nueva version se escribe en ota_1. Tras reiniciar, el bootloader arranca desde ota_1. Si algo sale mal, ota_0 sigue intacta.

El proceso paso a paso:

1. Firmware v1.0 corre desde ota_0
2. Se descarga v1.1 y se escribe en ota_1
3. Se marca ota_1 como "siguiente arranque" (en la particion otadata)
4. Se reinicia el ESP32
5. El bootloader lee otadata y arranca desde ota_1
6. ¡Ahora corre v1.1!

Si quieres otra actualizacion (v1.2), se escribira en ota_0 (que ahora es la inactiva). Y asi sucesivamente, alternando entre las dos particiones.

### Rollback: la red de seguridad

¿Que pasa si v1.1 tiene un bug critico y el ESP32 crashea nada mas arrancar? Sin proteccion, quedarias con un dispositivo inutilizable que solo puedes recuperar con USB.

El mecanismo de rollback funciona asi: tras arrancar con el nuevo firmware, este tiene un "periodo de prueba". Debe confirmar explicitamente que funciona correctamente. Si no lo hace dentro de un tiempo limite (por ejemplo, 60 segundos), el bootloader asume que el firmware es defectuoso y vuelve al anterior automaticamente.

```
Escenario exitoso:
  Arranque v1.1 → WiFi OK → ESP-NOW OK → Nodos responden → CONFIRMAR ✓
  → v1.1 queda como firmware permanente

Escenario fallido:
  Arranque v1.1 → Crash! → Reinicio → Bootloader detecta: "no confirmado"
  → Vuelve a v1.0 automaticamente
```

Es como un periodo de prueba en un trabajo: si el nuevo firmware demuestra que funciona bien (pasando todos los checks), se queda. Si falla, se vuelve al anterior. La clave es que la confirmacion debe hacerse DESPUES de verificar que los sistemas criticos funcionan, no inmediatamente al arrancar.

### TLS en sistemas embebidos

TLS (Transport Layer Security) es el protocolo que cifra las comunicaciones de red. Es lo que hace que HTTPS tenga la "S" de seguro. Cuando el gateway envia datos de sensores por MQTT o descarga firmware OTA, esa informacion viaja por WiFi y potencialmente por internet. Sin cifrado, cualquier persona en la red puede:

- Leer las mediciones de los sensores
- Interceptar credenciales MQTT
- Inyectar comandos falsos a los actuadores
- Sustituir el firmware OTA por uno malicioso

TLS resuelve esto con tres mecanismos:

**Cifrado**: Los datos se cifran antes de enviar. Solo el receptor con la clave correcta puede leerlos. Es como enviar una carta en un sobre cerrado con llave.

**Autenticacion**: Los certificados digitales prueban que el servidor es quien dice ser. Cuando el gateway se conecta al broker MQTT, verifica su certificado. Es como verificar el DNI de alguien antes de darle informacion confidencial.

**Integridad**: Se detecta si alguien modifica los datos en transito. Es como un sello de lacre en una carta: si esta roto, sabes que alguien la abrio.

En el ESP32, TLS se implementa con mbedTLS, una libreria criptografica diseñada para dispositivos con recursos limitados. El coste: cada conexion TLS consume unos 40KB de RAM adicionales y las operaciones criptograficas usan CPU. En un ESP32-S3 con 512KB de RAM, esto es manejable pero hay que tenerlo en cuenta.

### Capas de seguridad en IoT

La seguridad no es una unica barrera sino multiples capas, como las defensas de un castillo medieval:

```
Capa 5: Verificacion de firmware OTA (¿es autentico?)
    │
Capa 4: Autenticacion de usuarios (¿quien eres?)
    │
Capa 3: TLS para MQTT y HTTP (datos en transito cifrados)
    │
Capa 2: WiFi WPA2 (red inalambrica protegida)
    │
Capa 1: ESP-NOW AES-128 (comunicacion local cifrada)
    │
[Datos de sensores y comandos]
```

Cada capa protege contra un tipo de ataque diferente:

- **Capa 1 - ESP-NOW cifrado**: Protege la comunicacion entre nodos y gateway. Un atacante con un receptor WiFi no puede leer los datos de los sensores ni enviar datos falsos.
- **Capa 2 - WiFi WPA2**: Protege la red AP del gateway. Solo quien conozca la contraseña puede conectarse al panel web.
- **Capa 3 - TLS**: Protege los datos que viajan por internet (MQTT al broker, descargas OTA). Incluso si alguien intercepta el trafico, no puede leerlo.
- **Capa 4 - Autenticacion**: El panel web y la API requieren usuario y contraseña. Protege contra acceso no autorizado incluso dentro de la red local.
- **Capa 5 - Firma OTA**: Verifica que el firmware descargado no ha sido modificado. Protege contra la instalacion de firmware malicioso.

Ningun sistema es 100% seguro, pero cada capa que añades multiplica la dificultad para un atacante. Es la estrategia de "defensa en profundidad": si una capa falla, las demas siguen protegiendo.

### Pruebas de estres: rompiendo el sistema a proposito

Las pruebas de estres son como los crash-tests de los coches: sometemos el sistema a condiciones extremas para encontrar sus limites y debilidades ANTES de desplegarlo en produccion. Un sistema que funciona perfectamente en tu escritorio con 2 nodos puede fallar con 10 nodos en un entorno real con interferencias.

Tipos de pruebas que debes realizar:

**Test de carga**: ¿Que pasa cuando 10-15 nodos envian datos simultaneamente? El gateway podria quedarse sin memoria, perder paquetes, o no dar abasto procesando datos. Mides: uso de RAM, tasa de paquetes perdidos, latencia.

**Test de desconexion**: ¿Que pasa cuando desaparece el WiFi? ¿Y cuando vuelve? El sistema debe degradar graciosamente (ESP-NOW sigue funcionando sin WiFi) y recuperarse automaticamente. Mides: tiempo de deteccion, tiempo de recuperacion, datos perdidos.

**Test de alimentacion**: ¿Que pasa con un corte de luz del gateway? Debe arrancar desde cero, recuperar configuracion de NVS, y aceptar conexiones de nodos. Mides: tiempo de recovery total.

**Test de duracion**: ¿El sistema es estable durante dias o semanas? Los memory leaks son insidiosos: pierdes 100 bytes por hora, no se nota en una hora, pero en un mes son 72KB — suficiente para crashear. Mides: heap libre a lo largo del tiempo (debe ser estable).

**Test de alcance**: ¿Los nodos se comunican bien a la distancia real de la instalacion? Mueve los nodos a los puntos mas lejanos y verifica que los datos llegan.

La metodologia es siempre la misma:
1. Define que pruebas y que metricas de exito/fallo
2. Configura el entorno de prueba
3. Ejecuta la prueba durante el tiempo definido
4. Recopila y analiza resultados
5. Documenta todo (sirve para comparar con futuras versiones)

### Ingenieria de fiabilidad basica

Un sistema que monitoriza una instalacion industrial funciona 24 horas al dia, 7 dias a la semana, posiblemente en un lugar remoto. Si falla a las 3 de la mañana, nadie va a ir a reiniciarlo. La ingenieria de fiabilidad busca que el sistema sea robusto y se recupere solo.

Conceptos clave:

**MTBF (Mean Time Between Failures)**: El tiempo medio entre fallos. Si el gateway crashea una vez al mes, su MTBF es 30 dias. El objetivo es maximizarlo. Para v1.0, un MTBF de 30+ dias es un buen objetivo inicial.

**Degradacion graciosa**: Cuando algo falla, el sistema debe seguir funcionando en lo que pueda. Si se pierde WiFi:
- ESP-NOW sigue funcionando (los nodos siguen enviando datos)
- El dashboard embebido sigue accesible via AP
- Solo se pierde MQTT (datos al servidor)
- Cuando WiFi vuelve, todo se reconecta

Si se pierde un nodo:
- Los demas siguen enviando datos
- El gateway marca ese nodo como "offline"
- Se genera una alerta

**Watchdog como ultimo recurso**: Si a pesar de todo el software se bloquea (deadlock, bucle infinito), el watchdog reinicia el sistema automaticamente. No es elegante, pero es mejor que quedar bloqueado indefinidamente.

**Logging para diagnostico remoto**: Los logs son la "caja negra" del sistema. Cuando algo falla, los logs te dicen que paso. Usa diferentes niveles: ERROR para cosas que necesitan atencion, WARN para situaciones sospechosas, INFO para flujo normal, DEBUG para detalle fino. En produccion, activa solo ERROR e INFO. Si necesitas investigar un problema, puedes activar DEBUG remotamente via MQTT.

---

## Como encaja en el proyecto

La Fase 5 transforma un prototipo funcional en un sistema listo para produccion. Es la diferencia entre "funciona en mi mesa" y "funciona en una instalacion industrial durante meses".

OTA permite mantener y mejorar el sistema sin acceso fisico. La seguridad protege los datos y los actuadores contra acceso no autorizado. Las pruebas validan que el sistema funciona bajo condiciones reales. La documentacion permite que otra persona despliegue el sistema.

Despues de esta fase, el sistema esta listo para su primera instalacion real. No sera perfecto — ningun sistema lo es en v1.0 — pero sera funcional, seguro, actualizable y estable.

---

## Errores tipicos y como evitarlos

1. **Hacer OTA sin verificar la tabla de particiones**: Si el ESP32 fue flasheado con tabla "Single factory", no tiene particiones ota_0/ota_1 y el OTA fallara con error críptico. Verifica SIEMPRE la tabla antes de intentar OTA. ¿Por que ocurre? Porque la tabla de particiones se flashea independientemente del firmware y puede no coincidir.

2. **Confirmar el firmware OTA demasiado pronto**: Si llamas a la funcion de confirmacion inmediatamente al arrancar (antes de verificar WiFi, ESP-NOW, etc.), pierdes la capacidad de rollback automatico ante bugs que solo aparecen segundos despues del arranque. ¿Por que? Porque una vez confirmado, el bootloader no hara rollback aunque el firmware falle despues.

3. **Ignorar el consumo de RAM de TLS**: Cada conexion TLS consume ~40KB de RAM. Si tienes MQTT TLS + HTTPS OTA activos simultaneamente, son ~80KB solo en TLS. En un ESP32 con 320KB de RAM disponible, esto es significativo. ¿Por que importa? Porque no queda RAM para el buffer de datos, el HTTP server y los demas componentes.

4. **No probar con la cantidad real de nodos**: Un sistema que funciona con 3 nodos puede fallar con 10. Los problemas de concurrencia, buffers llenos y colisiones de paquetes solo aparecen bajo carga. ¿Por que? Porque 10 nodos que despiertan aleatoriamente cada 5 minutos generaran colisiones que 3 nodos rara vez causan.

5. **Asumir que un test corto valida estabilidad**: Una prueba de 1 hora no detecta memory leaks lentos ni degradacion gradual. Necesitas pruebas de dias. ¿Por que? Porque un leak de 50 bytes por ciclo de recepcion, con 10 nodos cada 5 minutos, son 600 bytes/hora: invisible en una hora, critico en una semana.

6. **Usar HTTP Basic Auth sin entender sus limitaciones**: Basic Auth envia usuario y contraseña codificados en base64, que NO es cifrado. Cualquiera que intercepte el trafico puede decodificarlo. En la red local del AP del gateway es aceptable para v1.0, pero en una red publica seria inseguro sin HTTPS.

---

## Experimenta

> **Experimenta 1**: Flashea un firmware intencionadamente defectuoso (que haga un crash a los 5 segundos) via OTA. Observa como el rollback lo detecta y vuelve al firmware anterior. ¿Cuanto tarda todo el proceso?

> **Experimenta 2**: Con Wireshark (o un sniffer WiFi), captura trafico MQTT sin TLS y luego con TLS. Compara: ¿puedes leer los datos de sensores sin TLS? ¿Y con TLS? Esto te dara una apreciacion visceral de por que el cifrado importa.

> **Experimenta 3**: Ejecuta tu sistema durante 48 horas y grafíca el heap libre del gateway cada minuto. ¿La linea es plana (estable) o tiene tendencia a bajar (leak)? Si baja, ¿puedes identificar que operacion causa el leak?

---

## Para profundizar

- ESP-IDF OTA Updates: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/ota.html
- ESP-IDF HTTPS OTA Example: https://github.com/espressif/esp-idf/tree/master/examples/system/ota/advanced_https_ota
- ESP-NOW Security: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/network/esp_now.html
- mbedTLS en ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/protocols/mbedtls.html
- Mosquitto SSL/TLS: https://mosquitto.org/man/mosquitto-tls-7.html
- OWASP IoT Security Guide: https://owasp.org/www-project-internet-of-things/
- ESP-IDF Watchdog Timers: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/wdts.html

---

## Preguntas de reflexion

1. ¿Por que no se puede simplemente sobreescribir el firmware que esta ejecutandose? ¿Que pasaria si el proceso se interrumpe a mitad?

2. Si el rollback automatico revierte al firmware anterior, pero ese firmware anterior tambien tiene un bug... ¿que sucede? ¿Como saldrias de esa situacion?

3. ¿Por que es importante que SNTP sincronice la hora ANTES de intentar una conexion TLS? ¿Que tiene que ver la hora con los certificados?

4. Un atacante instala un sniffer WiFi cerca de tu instalacion IIoT. ¿Que informacion podria obtener si tienes: a) ninguna seguridad, b) solo ESP-NOW cifrado, c) todas las capas de seguridad?

5. ¿Que diferencia practica hay entre QoS 1 y QoS 2 en MQTT cuando se combina con TLS? ¿Son redundantes o complementarios?

6. Si tu gateway tiene un MTBF de 15 dias, ¿cuantas veces se reiniciaria en un año? ¿Es aceptable si el tiempo de recovery es de 30 segundos?

7. ¿Por que el test de duracion (ejecutar durante dias/semanas) revela problemas que no aparecen en un test de 1 hora? ¿Que tipos de bugs se manifiestan solo con el tiempo?

8. ¿En que orden aplicarias las mejoras de seguridad si tuvieras tiempo limitado? ¿Cual aporta mas proteccion por el esfuerzo invertido?
