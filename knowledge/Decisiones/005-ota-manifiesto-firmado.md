# ADR-005: Manifiesto de publicación firmado para el OTA

## Estado
Aceptada (2026-10-25)

## Contexto

La issue #13 pedía actualizaciones OTA «seguras y compatibles con marca,
modelo y revisión de hardware». El diseño heredado de la Fase 5 sólo
contemplaba un OTA A/B con rollback: `esp_https_ota()` bajaba un binario y
lo escribía en la ranura siguiente.

Eso deja tres huecos:

1. **Autenticidad**: cualquiera que alcance la URL de la imagen puede
   instalar firmware en la unidad. El gateway levanta su propio AP, así que
   la URL no es un secreto.
2. **Compatibilidad**: nada impedía instalar una imagen de otra marca, de
   otro modelo o de otra revisión de hardware. Un firmware que espera otro
   pinout o otra pantalla no puede funcionar en ese carrier.
3. **Canales**: nada impedía que una imagen de desarrollo llegara a una
   unidad de producción.

Además había una ambigüedad de vocabulario: `brand_profile` ya tenía un
campo `revision`, pero era la **revisión de compilación** (el SHA corto del
commit). La issue habla de la **revisión de hardware**. Son dos cosas
distintas y una hacen falta dos campos distintos.

## Decision

El OTA se apoya en un **manifiesto de publicación firmado**, con las
comprobaciones en el orden que da valor a la firma:

1. Se descarga el manifiesto (HTTPS) y se parsea con un parser **estricto**.
2. Se verifica su firma: **ECDSA P-256** sobre la *forma canónica* del
   manifiesto.
3. Se comprueban **compatibilidad**, **canal** y **antirregresión** — antes
   de descargar un solo byte de la imagen.
4. Se descarga la imagen, se escribe en la ranura inactiva y se **relee de
   la flash** para comparar su SHA-256 con el declarado.
5. Sólo entonces se activa la ranura. La imagen arranca en
   `PENDING_VERIFY` y la aplicación la confirma cuando los subsistemas
   críticos están vivos.

La implementación se reparte en dos componentes, por una razón que no es
estética: las *reglas* (compatibilidad, canal, versión) son la parte
interesante y se pueden probar en el host; la criptografía y la flash
necesitan el dispositivo.

- `ota_manifest`: C puro, portable, sin dependencias de ESP-IDF más allá
  de los códigos de error. Se prueba en `tools/host-tests`.
- `ota_manager`: la mitad ESP-IDF (mbedTLS, `esp_http_client`, `esp_ota_ops`,
  NVS).

La identidad que el manifiesto se compara (`product`, `brand`, `model`,
`revision` de hardware) sale de `brand_profile`, que ya es la fuente única
de la identidad pública. Por eso `brand_profile` gana `product_id` y
`hw_revision`, en lugar de que el OTA tenga su propia configuración.

El canal se ordena por estabilidad (`dev` < `beta` < `production`) y la
regla es `rango(imagen) >= rango(unidad)`: la imagen tiene que ser al menos
tan estable como la unidad que la instala.

## Alternativas consideradas

1. **Secure Boot de ESP-IDF** (firma de la imagen por el bootloader)
   - Pro: Es la solución del fabricante, ya auditada
   - Pro: Protege el binario entero, no sólo el metadato
   - Contra: Requiere eFuses quemados: es irreversible y por unidad
   - Contra: Un mismo binario sirve para todas las unidades: no distingue
     marca, modelo ni revisión
   - Contra: No permite publicar un manifiesto por versión con metadatos
   - Contra: Es una decisión de fabricación, no de publicación

2. **Firmar sólo el binario, sin manifiesto**
   - Pro: Menos piezas, un solo concepto
   - Contra: El binario no lleva dentro la marca, el modelo ni la revisión:
     hay que compararlos con algo externo
   - Contra: No hay forma de publicar «qué versión hay en cada canal» ni de
    .transportar el digest esperado

3. **Firma simétrica (HMAC) con una clave compartida**
   - Pro: Mucho más simple de verificar en el dispositivo
   - Contra: La clave compartida tiene que estar en **todas** las unidades:
     extraerla de una sola compromete a todas
   - Contra: No distingue «quién firmó» de «quién verifyó»

4. **Usar el certificado de Secure Boot como firma** (elegida: no)
   - Pro: Se reaprovecha material ya presente
   - Contra: La clave de Secure Boot está en eFuses, no se puede rotar ni
     versionar con el código
   - Contra: El certificado no se puede distribuir junto al manifiesto

5. **Cifrar el manifiesto en vez de firmarlo**
   - Pro: Oculta el contenido
   - Contra: Ocultar no es autenticar: quien tiene la clave puede cifrar
     cualquier cosa
   - Contra: La clave tendría que estar en la unidad, igual que el problema
     de la HMAC

6. **Firma Ed25519**
   - Pro: Firma de 64 bytes, muy rápida y sin parámetros de curva
   - Contra: mbedTLS 3.6 (la versión de ESP-IDF v5.4) **quitó** la API
     clásica `mbedtls_ed25519_write`; sólo queda la variante PSA, que
     exige inicializar el subsistema PSA
   - Contra: ECDSA P-256 ya está soportada, probada y es la que usan las
     PKI del mundo real

## Consecuencias

- **La clave pública de firma está en el repositorio**: no es un secreto, la
  necesita el firmware para verificar. La privada no, y lo cubre la regla
  `*.pem` de `.gitignore` con una excepción explícita para la pública.
- **Rotar la clave obliga a recompilar el firmware**, porque la clave está
  embebida. La rotación es un cambio revisable.
- **`brand_profile` crece** con `product_id` y `hw_revision`. Es una
  ampliación de la capa de marca, no un conflicto: el manifiesto se compara
  contra la identidad pública del producto.
- **La forma canónica es un contrato entre dos repositorios de código**
  (el firmware y la release). Se ata con un vector dorado comprobado por
  los dos lados.
- **El parser del manifiesto es deliberadamente estricto**: rechaza campo
  desconocido, duplicado, valor anidado y URL sin HTTPS. Un campo que el
  parser ignorase no estaría autenticado.
- **La imagen no se activa nunca sin releerla de la flash**: un error de
  escritura que el controlador no le ha reportado se detecta en el digest.
- **La revisión de hardware pasa a ser un dato de compilación**
  (`CONFIG_BRAND_HW_REVISION`), y cambiarlo es una decisión explícita.

## Referencias
- [[003-protocolo-unificado]] — mismo criterio: el formato en el cable es un contrato
- [[OTA]] — el concepto completo
- [[White-label]] — de dónde sale la identidad que el manifiesto declara
- `docs/ota/ota-security.md` — el contrato de seguridad
- `odd/tasks/issue-13-secure-ota.md` — el plan de trabajo y la evidencia
- `firmware/gateway/components/ota_manifest/include/ota_manifest.h` — la forma canónica
- Issue #13: <https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/13>
- ESP-IDF, OTA y rollback: <https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/ota.html>
