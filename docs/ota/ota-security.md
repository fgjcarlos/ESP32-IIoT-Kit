# OTA segura por producto y revisión

> Issue #13 — «Implementar actualizaciones OTA seguras y compatibles con
> marca, modelo y revisión de hardware».
>
> Este documento es la referencia del contrato de seguridad de la
> actualización remota del gateway: qué es un manifiesto, qué decide la
> unidad antes de descargar nada, cómo se validan los canales, qué
> estrategia de TLS y certificados se usa y cómo se aprovisionan las
> claves.

## 1. La idea

Una actualización remota tiene dos riesgos: que la unidad instale una
imagen que **no es suya**, y que instale una imagen que **está dañada** o
que **no funciona**. La respuesta a los dos es la misma: nada se instala
sin una **firma** que lo autorice, y nada se activa sin comprobar que la
flash coincide con lo que se firmó.

El flujo completo es:

```
   ┌────────────────┐  HTTPS   ┌──────────────┐
   │  manifiesto    │ ◄────────┤   gateway    │
   │  (JSON, SSO)   │          │              │
   └────────────────┘          │  1. parsear  │  → mal formado = fuera
        │                      │  2. verificar │  → firma mala = fuera
        │                      │     la firma  │
        │                      │  3. comprobar │  → marca, modelo,
        │                      │     la        │    revisión, canal
        │                      │     identidad │    = fuera
        ▼                      │  4. descargar │  ┐
   ┌────────────────┐          │  5. escribir  │  │ la imagen va a la
   │    imagen      │ ─────────►│     el slot  │  │ ranura inactiva
   │    (.bin)      │  HTTPS   │  6. releer y  │  ┘
   └────────────────┘          │     comparar │
                               │  7. activar  │  → solo si todo coincide
                               └──────────────┘
```

Los pasos 1 a 3 ocurren **antes de descargar un solo byte de la imagen**. Una
imagen de otra marca, de otro modelo, de otra revisión de hardware o de otro
canal cuesta una petición pequeña y ningún byte de la imagen.

## 2. El manifiesto

El manifiesto es un objeto JSON plano. La firma cubre su **forma
canónica**, no el documento: el receptor reconstruye la forma canónica a
partir de los campos y verifica la firma sobre ella. Así el documento puede
viajar con cualquier formato, espacios o orden de claves, y el contenido
firmado sigue siendo inequívoco.

### 2.1 Campos

| Campo | Tipo | Obligatorio | Qué es |
|---|---|---|---|
| `schema` | número | sí | Versión del esquema del manifiesto. Hoy `1`. |
| `product` | texto | sí | Identificador estable del producto (`CONFIG_BRAND_PRODUCT_ID`). |
| `brand` | texto | sí | Marca (`CONFIG_BRAND_NAME`). |
| `model` | texto | sí | Modelo comercial (`CONFIG_BRAND_MODEL`). |
| `revision` | texto | sí | Revisión **de hardware** del carrier (`CONFIG_BRAND_HW_REVISION`). |
| `target` | texto | sí | Chip de destino, `esp32s3`. |
| `version` | texto | sí | Versión del firmware de la imagen. |
| `channel` | texto | sí | Canal de publicación: `dev`, `beta` o `production`. |
| `size` | número | sí | Tamaño exacto de la imagen en bytes. |
| `sha256` | texto | sí | SHA-256 de la imagen, 64 hex en minúsculas. |
| `url` | texto | sí | URL **HTTPS** de la imagen. |
| `signature` | texto | sí | Firma ECDSA P-256 de la forma canónica, base64. |
| `key_id` | texto | sí | Identificador de la clave de firma. |
| `manifest_sha256` | texto | no | SHA-256 de la forma canónica, para fijar el manifiesto. |

El parser es **estricto a propósito**: rechaza el campo desconocido, el
campo duplicado, el valor anidado, el booleano, el `null`, el número con
decimales y la URL que no sea HTTPS. La razón es que la firma solo cubre
los campos conocidos: un campo que el parser ignorase no estaría
autenticado y no podría ser de fiar.

La definición normativa de la forma canónica está en
`firmware/gateway/components/ota_manifest/include/ota_manifest.h`. La
implementación de la release (`tools/ota/sign-manifest.py`) y la del
firmware están comprobadas contra el **mismo vector dorado**, de modo que
no pueden separarse sin que falle una de las dos.

### 2.2 La firma

- **Algoritmo**: ECDSA sobre P-256 (`secp256r1`), con SHA-256.
- **Qué se firma**: la forma canónica del manifiesto, described en el
  apartado 2.3.
- **Formato de la firma**: los dos enteros `r` y `s` en crudo, 32 bytes
  cada uno, en base64. El formato DER no se usa porque su parseo en C es
  mucho más delicado que el de dos enteros de tamaño fijo.
- **Clave**: la mitad **pública** de un par ECDSA P-256, embebida en el
  firmware en tiempo de compilación.

### 2.3 La forma canónica

```
iiot-ota-manifest/v1
schema=1
product=iiot-kit-gateway
brand=IIoT-Kit
model=GW-S3-1
revision=rev-a
target=esp32s3
version=1.1.0
channel=production
size=1011344
sha256=<64 hex en minúsculas>
url=https://releases.example.org/iiot-kit/1.1.0/gateway.bin
key_id=ota-2026
```

El orden de los campos es fijo, cada línea termina en `\n` y ningún valor
puede contener un salto de línea (el parser lo rechaza). El digest
`manifest_sha256` no forma parte de la forma canónica: es el resumen de ella.

## 3. Lo que decide la unidad antes de descargar

### 3.1 Compatibilidad

`product`, `brand`, `model`, `revision` y `target` tienen que coincidir
**exactamente** con los de la unidad:

| Dato del manifiesto | De dónde sale en la unidad |
|---|---|
| `product` | `CONFIG_BRAND_PRODUCT_ID` |
| `brand` | `CONFIG_BRAND_NAME` |
| `model` | `CONFIG_BRAND_MODEL` |
| `revision` | `CONFIG_BRAND_HW_REVISION` |
| `target` | `CONFIG_IDF_TARGET` |

La revisión de hardware es un dato nuevo que la issue #13 hace explícito: un
firmware que espera otro pinout, otra pantalla u otro carrier no puede
instalarse en esta unidad. `brand_profile` es el dueño de ese dato porque
`brand_profile` es la fuente única de la identidad pública del producto.

### 3.2 Canal

Los canales se ordenan por estabilidad:

| Canal | Rango | Qué instala |
|---|---|---|
| `dev` | 0 | imágenes de `dev`, `beta` y `production` |
| `beta` | 1 | imágenes de `beta` y `production` |
| `production` / `stable` | 2 | imágenes de `production` |

La regla es `rango(manifiesto) >= rango(unidad)`: **la imagen tiene que ser
al menos tan estable como la unidad que la instala**. De ahí salen las dos
garantías:

- una unidad de producción **nunca** acepta una imagen de `beta` o de `dev`;
- una unidad de `beta` **nunca** acepta una imagen de `dev`.

Un canal desconocido se rechaza en los dos lados. Una errata en la
configuración de compilación no puede abrir la puerta.

El canal es un dato de **compilación** (`CONFIG_BRAND_OTA_CHANNEL`), no algo
que el OTA pueda cambiar: una imagen no puede ascender a su propia unidad de
producción.

### 3.3 Antirregresión

La versión del manifiesto tiene que ser mayor o igual que la que está
corriendo. La comparación es numérica por componentes (`1.10.0` es más
nuevo que `1.9.0`), no lexicográfica.

### 3.4 Integridad de la imagen

El `sha256` del manifiesto es el SHA-256 de la imagen. La unidad lo calcula
**releyendo la ranura de la flash** después de escribirla, no confiando en
los bytes que le llegaban por la red: un error de escritura que el
controlador de flash no le ha reportado se detecta aquí. Si el digest no
coincide, la imagen **no se activa** y la unidad sigue con el firmware
anterior.

## 4. TLS y estrategia de certificados

- La URL del manifiesto y la de la imagen tienen que ser **HTTPS**. El
  parser del manifiesto rechaza una URL `http://` antes de que la unidad
  haga ninguna petición.
- El ancla de confianza es el **bundle de certificados de ESP-IDF**
  (`esp_crt_bundle_attach`), es decir, una autoridad pública. El bundle
  está habilitado en `sdkconfig.defaults` con
  `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`.
- Una autoridad privada es una decisión de despliegue. Se implementa
  sustituyendo el `crt_bundle_attach` por un `cert_pem` con el PEM de la CA
  embebido, en el mismo sitio del componente (`ota_http_config()`).

El bundle protege el **transporte**. Lo que protege el **contenido** es la
firma: aunque el servidor HTTPS fuese comprometido, no podría cambiar el
manifiesto sin invalidar la firma. Las dos capas son independientes y
ninguna sustituye a la otra.

## 5. Aprovisionamiento de claves y credenciales

| Dato | Dónde vive | ¿Secreto? | Cómo llega |
|---|---|---|---|
| Clave **pública** de firma | `certs/ota_signing_key.pem` | no | Se genera con `tools/ota/gen-ota-keys.sh` y se versiona. El firmware la embebe. |
| Clave **privada** de firma | `certs/ota_signing_private.pem` | **sí** | Se genera en el repositorio local o, mejor, en la canalización de publicación. **Nunca** entra en Git: lo cubre la regla `*.pem` de `.gitignore`. |
| URL del servidor de releases | NVS, namespace `ota`, clave `manifest_url` | no | Se aprovisiona en la fabricación o por la API. |
| Credenciales Wi-Fi | `wifi_credentials.local.h` | **sí** | `tools/gen-wifi-credentials.sh` desde `/.env`. |
| Secretos de fabricación | `brand_secrets.local.h` | **sí** | `tools/brand/gen-brand-secrets.sh` desde `/.env`. |

La rotación de la clave de firma es un cambio revisable: se genera el par
nuevo, se versiona la mitad pública y **se recompila el firmware**, porque
la clave está embebida. La mitad privada antigua se retira de la
canalización en el mismo cambio.

## 6. Presupuesto de flash

La tabla de particiones aprobada es la del gateway, 16 MB
(`firmware/gateway/partitions.csv`):

| Partición | Offset | Tamaño | Uso |
|---|---|---|---|
| `nvs` | `0x9000` | 24 KiB | Configuración persistente |
| `otadata` | `0xF000` | 8 KiB | Selección de ranura de arranque |
| `phy_init` | `0x11000` | 4 KiB | Calibración de radio |
| `ota_0` | `0x20000` | 3 MiB | Ranura activa |
| `ota_1` | `0x320000` | 3 MiB | Ranura inactiva (destino de la imagen) |
| `spiffs` | `0x620000` | 4 MiB | SPA y margen |

La imagen del gateway con la capa de OTA **mide 1.021.120 bytes**, es decir
un 32 % de una ranura de 3 MiB: quedan 2.124.608 bytes libres por ranura
(el 68 %).
La comprobación la hace el propio enlazador de ESP-IDF
(`check_sizes.py`), que aborta el build si la imagen no cabe.

El cambio de la issue #13 **no añade ninguna partición**: el OTA usa las dos
ranuras que ya existían. La partición `otadata` ya estaba en la tabla
aprobada.

> La issue #13 se redactó cuando la placa seleccionada todavía se suponía
> de 4 MB. La issue #8 la sustituyó por la ESP32-S3-WROOM-1-N16R8
> (16 MB), y `project.md` es la fuente autoritativa del diseño actual. El
> criterio de aceptación «cabe en la tabla de particiones aprobada de 4 MB»
> se interpreta, por tanto, como «cabe en la tabla de particiones
> aprobada», que es la de 16 MB. La misma imagen **también** cabría en el
> reparto de 4 MB del nodo (dos ranuras de 1,5 MiB), con un 35 % de
> margen.

## 7. El rollback

Una imagen recién instalada arranca en el estado `PENDING_VERIFY`. El
bootloader solo la da por buena cuando la aplicación la confirma con
`esp_ota_mark_app_valid_cancel_rollback()`.

Hay dos formas de que la imagen no llegue a confirmarse:

| Fallo | Qué lo detecta | Qué pasa |
|---|---|---|
| La imagen **falla al arrancar** | El watchdog del hardware | El bootloader marca la ranura como `ABORTED` en el siguiente arranque y arranca la otra. |
| La imagen **se queda colgada** | El vigilante de confirmación de `ota_manager` | Tras 120 s sin confirmar, la unidad se reinicia; el bootloader hace el rollback. |

La ventana de confirmación de 120 s es la de la Fase 5 corregida
(`docs/replanificacion/03-fases-corregidas.md`).

La confirmación se pide **después** de que los subsistemas críticos estén
vivos: la unidad confirma cuando el Wi-Fi está levantado y el MQTT ha
arrancado. Un firmware que no llega a la red es un firmware que no
funciona, y la unidad tiene que poder volver atrás.

## 8. Referencias

- Issue #13: <https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/13>
- Componente portable: `firmware/gateway/components/ota_manifest/`
- Componente ESP-IDF: `firmware/gateway/components/ota_manager/`
- Herramientas de publicación: `tools/ota/`
- Fase 5: `Fases/fase-05-ota-optimizacion.md`
- Decisiones: `docs/replanificacion/01-decisiones-corregidas.md`
- ESP-IDF, rollback: <https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/ota.html>
- ESP-IDF, particiones: <https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/partition-tables.html>
