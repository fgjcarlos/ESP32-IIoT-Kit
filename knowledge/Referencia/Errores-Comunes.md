# Errores Comunes ESP32 — Quick Reference

#referencia

## Entorno y toolchain
| Error | Solucion |
|-------|----------|
| `idf.py: command not found` | `source ~/esp/esp-idf/export.sh` |
| Compilacion falla con errores de toolchain | `idf.py set-target esp32s3` (o esp32c3) |
| Submodulos faltantes | `git submodule update --init --recursive` |

## Hardware
| Error | Solucion |
|-------|----------|
| `dmesg` no detecta la placa | Cable USB de solo carga. Cambiar cable |
| Permission denied en /dev/ttyACM0 | `sudo usermod -aG dialout $USER` + cerrar sesion completa |
| Sensor I2C/1-Wire no responde | Verificar pull-up de 4.7K ohm |
| Sensor no lee bien | Verificar alimentacion 3.3V (NO 5V!) |

## Programacion
| Error | Solucion |
|-------|----------|
| `delay()` no existe | Usar `vTaskDelay(pdMS_TO_TICKS(ms))` |
| `printf` sin timestamp | Usar `ESP_LOGx(TAG, ...)` |
| NVS datos no persisten | Olvidaste `nvs_commit()` despues de `nvs_set_*()` |
| WiFi falla al init | `nvs_flash_init()` ANTES de `esp_wifi_init()` |
| ESP-NOW no init | `esp_wifi_start()` ANTES de `esp_now_init()` |
| ESP-NOW no recibe | Verificar que sender y receiver estan en el MISMO canal WiFi |
| Broadcast siempre "SUCCESS" | Normal — broadcast no tiene ACK. Usar unicast para verificar |
| ADC satura en 1V | Configurar atenuacion: `ADC_ATTEN_DB_12` para rango 0-3.3V |
| ADC da basura con WiFi | Usar ADC1, no ADC2 (ADC2 comparte hardware con WiFi) |
| Struct tiene tamano incorrecto | Agregar `__attribute__((packed))` |
| deep sleep consume mucho | UART/GPIO no desactivados antes de dormir |
| RTC variable siempre 0 | Falta `static` o `RTC_DATA_ATTR`, o es cold boot |

## Memoria
| Error | Solucion |
|-------|----------|
| Stack overflow (crash) | Aumentar stack de la tarea (default 4KB) |
| Heap se agota | Verificar `cJSON_Delete()` y `free()` despues de cada uso |
| `guru meditation error` | Null pointer, stack overflow, o division por cero. Ver backtrace |

## Dashboard embebido (Preact SPA en SPIFFS)
| Error | Solucion |
|-------|----------|
| HTTP 404 al pedir `/` | SPIFFS no montada, o archivo `index.html` no en la particion SPIFFS |
| Build Vite no genera `dist/` | Ejecutar `npm run build` dentro de `firmware/gateway/web/` |
| SPIFFS llena al flashear | Verificar `idf.py partition-table`: la particion SPIFFS debe tener suficiente espacio |
| WebSocket se desconecta | El servidor no envia keepalive. Implementar ping cada 30s o reconectar en cliente |
| CORS en fetch desde la SPA | Agregar `Access-Control-Allow-Origin: *` en las respuestas REST del HTTP server |
| Valores de sensor no actualizan | WebSocket no suscrito al topic correcto, o el gateway no publica al conectar un nuevo cliente |

## Ver tambien
- [[ESP-IDF]]
- [[FreeRTOS]]
- [[NVS]]
- [[REST-API]]
- [[WebSocket]]
