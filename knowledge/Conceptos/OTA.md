# OTA

#concepto

## Que es

Actualizar el firmware de un dispositivo ya instalado, a través de la red,
sin necesidad de tocarlo físicamente. En este proyecto el OTA del **gateway**
(ESP32-S3) es la parte implementada en la issue #13; el de los nodos
(ESP32-C3) sigue siendo un plan de la Fase 5.

El OTA no es sólo "bajar un binario". Es un problema de **confianza**: la
unidad tiene que poder distinguir una imagen legítima de una imagen ajena, y
tiene que poder volver atrás si la imagen nueva no funciona.

## Por que importa en este proyecto

El gateway levanta su propio punto de acceso WiFi. Cualquiera que alcance
la URL de la imagen podría, sin más, instalar firmware ajeno en una
instalación remota que ya no se puede ir a flashear en persona.

Por eso la issue #13 define tres cosas a la vez:

1. **Autenticidad** — la imagen viene firmada, y la firma se verifica
   *antes* de descargar la imagen.
2. **Compatibilidad** — la imagen declara para qué producto, marca, modelo
   y revisión de hardware es, y la unidad lo comprueba.
3. **Reversibilidad** — si la imagen no funciona, la unidad vuelve a la
   anterior sin intervención.

## Como funciona

### Las dos mitades

| Componente | Qué hace | Dónde |
|---|---|---|
| `ota_manifest` | Modelo del manifiesto, parser, forma canónica, reglas | C puro, portable |
| `ota_manager` | Firma, descarga, escritura, activación, rollback | ESP-IDF |

La separación no es estética: las reglas (compatibilidad, canal,
antirregresión) son las interesantes y se prueban en el host, sin hardware.
La criptografía y la flash necesitan el dispositivo.

### El manifiesto

Un JSON plano con producto, marca, modelo, revisión de hardware, target,
versión, canal, tamaño, SHA-256, URL y firma. La firma es un **ECDSA P-256**
sobre la *forma canónica* del manifiesto: una serialización determinista de
sus campos, en un orden fijo, una línea por campo.

Se firma la forma canónica y no el documento porque así el JSON puede
viajar con cualquier formato y el contenido firmado sigue siendo
inequívoco. Las dos implementaciones (la del firmware y la de la release)
se comprueban contra el mismo **vector dorado**, así que no pueden separarse
sin que falle una de las dos.

### El orden de las comprobaciones

Es la propiedad de seguridad del conjunto:

```
manifiesto → parsear → verificar firma → comprobar compatibilidad
           → comprobar canal → comprobar versión → DESCARGAR imagen
           → escribir en la ranura inactiva → releer y comparar digest
           → activar
```

Compatibilidad, canal y versión se comprueban **antes de descargar un solo
byte**. Una imagen de otra marca cuesta una petición pequeña.

### Los canales

`dev` (0) < `beta` (1) < `production` (2). Una unidad sólo acepta imágenes
al menos tan estables como ella: `rango(imagen) >= rango(unidad)`.

- una unidad de producción **nunca** acepta una imagen de `beta` o `dev`;
- una unidad de `beta` **nunca** acepta una imagen de `dev`.

El canal es un dato de compilación, no algo que el OTA pueda cambiar.

### El rollback

Una imagen recién instalada arranca en `PENDING_VERIFY`. El bootloader sólo
la acepta si la aplicación la confirma con
`esp_ota_mark_app_valid_cancel_rollback()`.

- Si la imagen **entra en pánico**, el bootloader la marca `ABORTED` en el
  siguiente arranque y arranca la otra ranura.
- Si la imagen **se queda colgada**, un vigilante reinicia la unidad tras
  120 s; el bootloader hace el rollback.

La confirmación se pide **después** de que los subsistemas críticos estén
vivos. Un firmware que no llega a la red no es un firmware que funciona.

## API clave

```c
// El gestor (ESP-IDF)
esp_err_t ota_manager_init(void);
esp_err_t ota_manager_start_update(const char *manifest_url);
esp_err_t ota_manager_start_configured_update(void);
esp_err_t ota_manager_confirm_pending_image(void);
esp_err_t ota_manager_get_status(ota_status_t *out);
esp_err_t ota_manager_set_manifest_url(const char *url);

// El manifiesto (portable)
esp_err_t ota_manifest_parse(const char *json, size_t len, ota_manifest_t *out,
                             ota_reject_reason_t *reason);
esp_err_t ota_manifest_canonical_payload(const ota_manifest_t *m, char *out, size_t len);
esp_err_t ota_manifest_check_compatible(const ota_manifest_t *m,
                                        const ota_device_identity_t *device,
                                        ota_reject_reason_t *reason);
esp_err_t ota_manifest_check_channel(const ota_manifest_t *m, const char *device_channel,
                                     ota_reject_reason_t *reason);
```

ESP-IDF: `esp_ota_ops.h` (`esp_ota_begin`, `esp_ota_write`, `esp_ota_end`,
`esp_ota_set_boot_partition`, `esp_ota_mark_app_valid_cancel_rollback`).

## Gotchas

- **El CRC del `otadata` no es el CRC-32 habitual.** El bootloader usa
  `esp_rom_crc32_le(0xFFFFFFFF, ...)`, que complementa la semilla antes del
  bucle y el resultado al final. `zlib.crc32(data)` da un valor distinto;
  `zlib.crc32(data, 0xFFFFFFFF)` da el correcto. La diferencia hizo que el
  bootloader rechazara un `otadata` escrito a mano.
- **La secuencia del `otadata` decide la ranura**: `(ota_seq - 1) % nº de
  ranuras`. Impar → `ota_0`, par → `ota_1`.
- **La clave privada nunca está en el repositorio**, pero la pública sí: la
  embebe el firmware. Rotar la clave obliga a recompilar.
- **Un manifiesto con un campo desconocido se rechaza.** La firma sólo
  cubre los campos conocidos: un campo que el parser ignorase no estaría
  autenticado.
- **La imagen se relee de la flash para el digest**, no se confía en los
  bytes que llegaron por la red.
- **La unidad necesita 8 KB de pila** para el cliente HTTP y la sesión TLS;
  el OTA corre en su propia tarea por eso.

## Enlaces
- [[`_MOC`]]
- [[Watchdog]] - el vigilante que reinicia la unidad si la imagen no se confirma
- [[NVS]] - donde se guarda la URL del servidor de releases
- [[White-label]] - de dónde salen producto, marca, modelo y revisión
- [[005-ota-manifiesto-firmado]]
