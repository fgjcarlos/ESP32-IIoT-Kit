# FreeRTOS

#concepto #fase/0 #fase/1

## Que es
Sistema operativo en tiempo real que corre por debajo de ESP-IDF. Gestiona tareas (hilos), semaforos, colas, event groups y temporizadores. Toda aplicacion ESP-IDF corre sobre FreeRTOS.

## Por que importa en este proyecto
- `app_main()` es una tarea FreeRTOS
- Los callbacks de WiFi y ESP-NOW corren en tareas separadas
- El servidor HTTP corre en su propia tarea
- Sincronizacion entre tareas (event groups, mutexes) es necesaria para el gateway

## Conceptos clave

### Tareas
```c
xTaskCreate(funcion, "nombre", stack_size, param, prioridad, &handle);
vTaskDelay(pdMS_TO_TICKS(1000));  // Delay no bloqueante (1 segundo)
```
- Stack default: 4KB — no poner arrays grandes en el stack
- `app_main()` tiene un stack de 8KB (configurable)

### Event Groups (para sincronizacion)
```c
EventGroupHandle_t eg = xEventGroupCreate();
xEventGroupSetBits(eg, BIT0);                              // Desde callback
EventBits_t bits = xEventGroupWaitBits(eg, BIT0, true, false, timeout); // Desde main
```
Uso en el proyecto: nodo espera ACK de ESP-NOW con timeout

### Mutexes (para exclusion mutua)
```c
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
xSemaphoreTake(mutex, portMAX_DELAY);
// seccion critica
xSemaphoreGive(mutex);
```
Uso en el proyecto: proteger array de nodos en el gateway (accedido desde callback ESP-NOW y HTTP handler)

## Gotchas
- `delay()` NO existe en ESP-IDF. Usar `vTaskDelay(pdMS_TO_TICKS(ms))`
- `pdMS_TO_TICKS()` convierte milisegundos a ticks de FreeRTOS
- NO llamar a `vTaskDelay()` dentro de un callback o event handler — bloquea la tarea del sistema
- Los callbacks ESP-NOW y WiFi corren en la tarea WiFi, no en `app_main()`
- Stack overflow = crash silencioso. Si crashea en una funcion con arrays grandes, aumentar stack
- En nodos con [[Deep-Sleep]], NO se usa `while(1)` ni tareas persistentes — el ciclo es una sola ejecucion

## Cuando usar cada primitiva

| Necesidad | Primitiva |
|-----------|-----------|
| Esperar un evento de otro contexto | Event Group |
| Proteger datos compartidos | Mutex |
| Pasar datos entre tareas | Queue (xQueueCreate) |
| Ejecutar algo periodicamente | Timer (xTimerCreate) |
| Delay sin bloquear el sistema | vTaskDelay |

## Enlaces
- [[ESP-IDF]] - FreeRTOS es parte integral de ESP-IDF
- [[Deep-Sleep]] - En nodos, no se usa FreeRTOS loop
- [[Watchdog]] - FreeRTOS tiene task watchdog integrado
