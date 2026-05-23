# Deep Sleep

#concepto #fase/0 #fase/2

## Que es
Modo de consumo ultra-bajo del ESP32. Apaga casi todo el chip excepto el controlador RTC. Al despertar, el programa arranca desde cero (como un reset).

## Por que importa en este proyecto
Los nodos sensores funcionan con bateria 18650 (3400mAh) y deben durar 12+ meses. Deep sleep consume ~5uA, vs ~130mA en modo activo con WiFi. Sin deep sleep, la bateria dura ~26 horas.

## Como funciona
1. `app_main()` se ejecuta (init, leer sensor, enviar datos)
2. Se llama a `esp_deep_sleep_start()` — no retorna nunca
3. El chip entra en deep sleep (~5uA)
4. El timer RTC despierta al chip despues de N segundos
5. El chip reinicia: ejecuta `app_main()` desde cero
6. Toda la RAM se pierde, excepto RTC memory (8KB)

## API clave
```c
esp_sleep_enable_timer_wakeup(us)     // Configurar timer (microsegundos!)
esp_deep_sleep_start()                 // Entrar en deep sleep (no retorna)
esp_sleep_get_wakeup_cause()           // Detectar si es cold boot o wake
RTC_DATA_ATTR static int counter = 0;  // Variable que sobrevive al deep sleep
```

## Gotchas
- `esp_sleep_enable_timer_wakeup()` usa MICROSEGUNDOS, no milisegundos. 10 segundos = 10000000
- `RTC_DATA_ATTR` solo funciona con variables `static` o globales
- RTC memory se pierde en un reset por alimentacion (solo sobrevive a deep sleep)
- NO poner `while(1)` en `app_main()` de un nodo con deep sleep — el ciclo es una sola ejecucion
- Llamar a `esp_now_deinit()` y `esp_wifi_stop()` antes de dormir
- UART consume corriente durante deep sleep si no se desactiva

## Calculo de autonomia
```
Consumo medio = (t_activo * I_activo + t_sleep * I_sleep) / t_total
Autonomia = Capacidad_bateria / Consumo_medio * 0.8 (factor eficiencia)
```

## Enlaces
- [[ESP-NOW]] - Se envia datos antes de dormir
- [[NVS]] - Persistencia que sobrevive a todo (no solo deep sleep)
- [[FreeRTOS]] - No se usa loop FreeRTOS en nodos con deep sleep
