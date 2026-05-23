# NVS (Non-Volatile Storage)

#concepto #fase/0 #fase/1

## Que es
Sistema de almacenamiento clave-valor en la memoria flash del ESP32. Los datos sobreviven a reinicios, deep sleep, y cortes de alimentacion. Es el equivalente a un "mini base de datos" en el chip.

## Por que importa en este proyecto
- Almacena credenciales WiFi (SSID, password)
- Almacena configuracion de nodos (intervalo sleep, tipo sensor)
- Almacena calibracion de sensores (coeficientes pH)
- WiFi lo necesita internamente para datos de calibracion del PHY

## Como funciona
1. Inicializar con `nvs_flash_init()` (obligatorio antes de WiFi)
2. Abrir un namespace: `nvs_open("wifi", NVS_READWRITE, &handle)`
3. Leer/escribir con `nvs_get_str()` / `nvs_set_str()`
4. Confirmar escritura con `nvs_commit(handle)`
5. Cerrar con `nvs_close(handle)`

## API clave
```c
nvs_flash_init()                          // Inicializar NVS (primera llamada!)
nvs_open("namespace", mode, &handle)      // Abrir namespace (max 15 chars)
nvs_set_str(handle, "key", "value")       // Escribir string
nvs_get_str(handle, "key", buf, &len)     // Leer string
nvs_set_i32(handle, "key", value)         // Escribir entero
nvs_commit(handle)                        // OBLIGATORIO despues de set
nvs_close(handle)                         // Cerrar (en TODOS los paths!)
nvs_flash_erase()                         // Borrar todo NVS
```

## Gotchas
- `nvs_flash_init()` DEBE llamarse antes que `esp_wifi_init()`, siempre
- `nvs_commit()` es OBLIGATORIO despues de cada `nvs_set_*()` — sin el, los datos no se persisten
- `nvs_close()` debe llamarse en TODOS los paths de ejecucion (incluidos los de error)
- Namespace maximo: 15 caracteres
- NVS tiene wear leveling interno, pero no es infinito — no escribir en cada wake cycle
- Para contadores que cambian frecuentemente, usar `RTC_DATA_ATTR` en vez de NVS → ver [[Deep-Sleep]]
- NVS almacena datos en texto plano en flash — credenciales expuestas con acceso fisico

## Convencion del proyecto
Un namespace por modulo:
- `"wifi"` — credenciales WiFi
- `"espnow"` — configuracion ESP-NOW
- `"sensor"` — calibracion de sensores
- `"mqtt"` — configuracion MQTT
- `"system"` — configuracion general

## Enlaces
- [[Deep-Sleep]] - NVS sobrevive a deep sleep; RTC memory tambien pero es mas rapida
- [[WiFi-Modos]] - WiFi requiere NVS inicializado
- [[ESP-IDF]] - NVS es un componente core de ESP-IDF
