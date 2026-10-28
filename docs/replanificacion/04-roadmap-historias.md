# Roadmap de historias completas hasta v1 y evolución opcional

Estado: planificación de 2026-10-28; las issues no equivalen a funcionalidades entregadas ni a diseños aprobados.
Objetivo: terminar el gateway autónomo ESP32-S3 y sus nodos C3 con historias pequeñas, verificables y completas para quien las usa.

## Por dónde empezar

1. H01 valida la conexión de la nueva placa y pantalla. W01 valida los wireframes de acceso/configuración web; puede avanzar sin conectar el LCD.
2. W02 entrega un portal protegido alojado realmente en el gateway; W03 permite configurar Wi-Fi conservando acceso local.
3. W04 valida los recorridos web restantes y L01 los wireframes LCD. Cada implementación espera la aprobación explícita de su recorrido.
4. N01–N04 entregan un nodo real comunicándose con el gateway. Después se incorporan sensores, control, monitorización y mantenimiento según las dependencias.
5. Q02 y Q03 cierran v1 con evidencias y puesta en marcha reproducible. F01–F08 son opciones posteriores, no requisitos de v1.

## Base que se reutiliza

La consulta completa de issues abiertas y cerradas encontró diez propuestas, todas cerradas, antes de publicar estas historias.
La base de firmware (#9), AP+STA (#21), Sparkplug B opcional (#11), marca (#12), OTA segura del gateway (#13) y CI (#14) se reutilizan; no se reimplementan.
Los cierres de #8/#10/#16 no demuestran un LCD ni una radio ESP-NOW operativos: el cierre de #16 dejó esos criterios diferidos.
Las nuevas historias cubren esos resultados pendientes o integraciones nuevas; no reabren ni modifican las propuestas históricas.

- [Arquitectura y estado real de la SPA](../../project.md): web embebida, API y datos locales; no servidor externo obligatorio.
- [Perfil canónico de hardware](../hardware/esp32-s3-wroom-1-n16r8.md): única autoridad para pines, memoria, pantalla y evidencias físicas.
- [Protocolo común](02-protocolo-unificado.md): contrato compartido entre gateway y nodos.
- [Fases y dependencias originales corregidas](03-fases-corregidas.md): contexto funcional; este roadmap descompone la entrega pendiente, no reactiva tareas ya resueltas.
- [Cierre y límites de #16](../../openspec/changes/archive/2026-10-26-issue-16-gateway-product-rollup/archive-report.md).

No se reutilizan automáticamente ejemplos antiguos de SQLite, servidores externos, GPIO o datos eléctricos del vendedor como decisiones de implementación.
Los presupuestos y límites se consultan en sus fuentes canónicas; no se duplican aquí.

## Regla de una historia

Una historia de implementación entrega una función utilizable de extremo a extremo: firmware y protocolo necesarios, API y vista cuando corresponda, persistencia, errores, pruebas y documentación.
No hay tickets separados de «crear API», «hacer frontend» o «añadir tests» para una misma función.
H01, W01, W04 y L01 son excepciones explícitas de validación física o diseño, necesarias antes de implementar de forma segura.

- Cada issue incluye alcance, exclusiones, tres criterios observables, dependencias y referencias bajo el formulario técnico del repositorio.
- Dependencia significa resultado demostrado, no simplemente código fusionado o checkbox marcado.
- Si falta una decisión de producto, de seguridad o de hardware, se resuelve antes de escribir la parte afectada; el backlog no autoriza suposiciones.
- Las pruebas de hardware se etiquetan y se ejecutan en hardware. Host/QEMU no sustituyen una prueba eléctrica, visual o de consumo.
- Una entrada rechazada no modifica la configuración válida; una orden pendiente o fallida nunca aparece como aplicada.
- Tests y documentación acompañan cada comportamiento. Se aplican el test-first, build y gates existentes, sin crear una CI paralela.
- Una historia no equivale necesariamente a una sola PR: si su implementación supera el límite del repositorio, se divide en entregas revisables conservando el objetivo funcional y sus pruebas.

## Historias y dependencias

Los identificadores H/W/N/S/A/L/O/Q/F son estables. La columna GitHub se completa sólo tras confirmar creación y lectura de vuelta.
Los criterios de aceptación detallados viven en cada issue; esta tabla es el mapa de orden y alcance.

### Configuración local y decisiones de interfaz

| ID | GitHub | Resultado | Depende de |
|---|---|---|---|
| H01 | pendiente de publicación | Validar la conexión segura de la nueva placa y pantalla | Ninguna |
| W01 | pendiente de publicación | Validar los wireframes web de acceso y configuración inicial | Ninguna |
| W02 | pendiente de publicación | Acceder a un portal local protegido servido por el gateway | W01 |
| W03 | pendiente de publicación | Guardar la red Wi-Fi desde la web y recuperar el acceso local | W02 |
| W04 | pendiente de publicación | Validar los wireframes web de nodos, lecturas y mantenimiento | W01 |
| L01 | pendiente de publicación | Validar datos, navegación y wireframes de la pantalla local | H01, W04 |

### Nodos, sensores y control

| ID | GitHub | Resultado | Depende de |
|---|---|---|---|
| N01 | pendiente de publicación | Dar de alta un nodo C3 y aprobarlo desde la web | W02, W04 |
| N02 | pendiente de publicación | Entregar una lectura C3 al gateway con ACK y reintentos | N01 |
| N03 | pendiente de publicación | Dormir el nodo y conservar su identidad entre ciclos | N02 |
| N04 | pendiente de publicación | Recuperar la entrega del nodo tras perder el canal del gateway | N02, W03 |
| S01 | pendiente de publicación | Mostrar temperatura real de un DS18B20 conectado al nodo | N02 |
| S02 | pendiente de publicación | Mostrar lecturas validadas y filtradas dentro de cada despertar | S01 |
| N05 | pendiente de publicación | Cambiar el intervalo de un nodo desde la web con confirmación | N03, W04 |
| A01 | pendiente de publicación | Accionar una salida de relé desde la web con estado seguro | N02, W04 |
| A02 | pendiente de publicación | Configurar y visualizar una alerta por umbral con histéresis | S02, W04 |
| A03 | pendiente de publicación | Vincular una alerta a una salida con recuperación segura | A01, A02 |

### Monitorización, pantalla y mantenimiento

| ID | GitHub | Resultado | Depende de |
|---|---|---|---|
| W05 | pendiente de publicación | Ver lecturas en directo y recuperar la conexión del navegador | N02, W04 |
| W06 | pendiente de publicación | Consultar un historial local acotado desde la web | W05 |
| W07 | pendiente de publicación | Configurar la integración Sparkplug B opcional desde la web | W02, W04, N02 |
| W08 | pendiente de publicación | Consultar diagnóstico y validez horaria desde la web | W02, W04 |
| L02 | pendiente de publicación | Mostrar el estado real del gateway en la pantalla verificada | H01, L01, W08 |
| L03 | pendiente de publicación | Consultar nodos, medidas y alarmas en la pantalla local | L02, W05, A02 |
| O01 | pendiente de publicación | Actualizar el gateway desde la web usando la OTA segura existente | W02, W04 |
| O02 | pendiente de publicación | Actualizar un nodo C3 mediante el AP del gateway | N05, O01 |
| Q01 | pendiente de publicación | Proteger las credenciales persistentes y verificar su recuperación | W03, W07 |
| Q02 | pendiente de publicación | Validar el funcionamiento conjunto y la recuperación del sistema | N04, N05, A03, W06, W07, W08, L03, O02, Q01 |
| Q03 | pendiente de publicación | Poner en marcha el sistema siguiendo una guía de operador | Q02 |

### Fase 6: opciones posteriores a v1

Estas historias se crean para conservar el plan completo, pero no están aprobadas para implementación ni bloquean v1.
Antes de iniciarlas se prioriza la opción, se valida hardware/seguridad/coste y se aprueban los recorridos de interfaz que añadan.
El relay se limita a un salto estático conforme a la replanificación; no se promete mesh automático.
La OTA por ESP-NOW no sustituye la primera OTA del nodo mediante AP; los roles múltiples y la API externa no son necesarios para operar localmente.

| ID | GitHub | Resultado | Depende de |
|---|---|---|---|
| F01 | pendiente de publicación | Comunicar un nodo autorizado mediante ESP-NOW cifrado | Q03 |
| F02 | pendiente de publicación | Recibir lecturas mediante un relay estático de un salto | Q03 |
| F03 | pendiente de publicación | Actualizar un nodo por ESP-NOW con transferencia recuperable | O02, Q03 |
| F04 | pendiente de publicación | Señalar una anomalía local con un modelo de ejemplo validado | Q03 |
| F05 | pendiente de publicación | Mantener la integración externa mediante respaldo LTE | W07, Q03 |
| F06 | pendiente de publicación | Conservar y recuperar lecturas desde una tarjeta SD validada | W06, Q03 |
| F07 | pendiente de publicación | Separar consulta y administración con dos roles locales | Q03 |
| F08 | pendiente de publicación | Consultar estado y lecturas desde una API externa de sólo lectura | F07 |

## Orden y puertas de aprobación

- **Web de configuración primero:** W01 → W02 → W03. No espera a sensores reales ni al LCD.
- **Primer dato completo:** W04 + W02 → N01 → N02 → S01. Se reutiliza el mismo estado de dominio para web y LCD.
- **Control seguro:** S02 → A02; A01 + A02 → A03. Datos caducados, órdenes fallidas y arranque tienen política acordada antes de actuar.
- **LCD real:** H01 + W04 → L01; H01 + L01 + W08 → L02; después L03. Ningún GPIO ni método de navegación se inventa.
- **Mantenimiento:** O01 reutiliza #13; O02 añade OTA del nodo por AP. Q01 endurece secretos persistentes sin posponer la protección básica de W02.
- **Cierre:** Q02 exige las dependencias v1 indicadas; Q03 demuestra que otra persona puede operar la versión entregada.

Las vistas nuevas que aparezcan al implementar una historia se incorporan al wireframe correspondiente y se validan antes de programarlas.
Validar wireframes no valida alimentación, consumo o comportamiento real del panel, y validar el LCD no aprueba su diseño de información.

## Qué no fija este plan

No fija fechas ni reutiliza las antiguas 25 semanas como estimación restante.
No selecciona sin pruebas mecanismos de autenticación, almacenamiento de histórico, hardware de actuación o valores de seguridad.
No autoriza compras, gastos LTE, publicación de firmware, conexiones a brokers ajenos, eFuses irreversibles ni despliegues remotos.
Las decisiones pendientes están dentro de las historias que las necesitan, como bloqueos explícitos; crear las issues no las resuelve.

## Seguimiento

- Planificación y evidencia de publicación: [registro ODD](../../odd/tasks/remaining-phases-roadmap.md).
- No se han creado wireframes, configurado GPIO ni implementado firmware por publicar este roadmap.
- El siguiente resultado a validar es H01/W01: evidencia física y prototipo web de primer acceso/configuración.
