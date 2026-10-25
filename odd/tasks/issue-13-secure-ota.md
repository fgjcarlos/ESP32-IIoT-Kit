# Issue #13 — OTA segura por producto y revisión

## Objetivo

Implementar la actualización remota del gateway de modo que **sólo** pueda
instalar una imagen firmada que sea exactamente para esa marca, ese modelo y
esa revisión de hardware, y que la unidad pueda volver atrás si la imagen
instalada no funciona.

## Problema y justificación

El diseño actual contempla OTA A/B y rollback (`Fases/fase-05-ota-optimizacion.md`),
pero no define manifiestos firmados ni protección frente a una imagen de
otra marca o de otra revisión. Con una red local abierta —el gateway
levanta su propio AP— cualquiera que alcance la URL de la imagen podría
instalar firmware ajeno en la unidad.

La issue pide, además, una **estrategia de canales** que impida la
promoción accidental entre entornos, una **estrategia de TLS y
certificados**, y una **ruta de aprovisionamiento** de credenciales y
claves.

## Nota sobre la premisa de la issue

La issue se redactó sobre la placa de 4 MB. La issue #8 sustituyó esa
placa por la **ESP32-S3-WROOM-1-N16R8** (16 MB) y `project.md` es la fuente
autoritativa del diseño actual. El criterio «cabe en la tabla de particiones
aprobada de 4 MB» se interpreta como «cabe en la tabla de particiones
aprobada», que es la de 16 MB, y se comprueba además que la imagen cabría
en el reparto de 4 MB del nodo (dos ranuras de 1,5 MiB).

## Alcance

Incluye:

- Componente nuevo `firmware/gateway/components/ota_manifest` (C puro,
  portable): modelo del manifiesto, parser estricto, forma canónica,
  digest, comparación de digest y las reglas de **compatibilidad**,
  **canal** y **antirregresión**.
- `firmware/gateway/components/ota_manager` pasa de *contract-only* a
  implementación real: verificación ECDSA P-256 con mbedTLS, descarga HTTPS
  con el bundle de certificados, escritura en la ranura inactiva con
  `esp_ota_ops`, verificación del digest leído de la flash, activación,
  confirmación del rollback y vigilante de la ventana de confirmación.
- La identidad de la unidad como fuente única: `brand_profile` gana
  `product_id` y `hw_revision`, porque el manifiesto se compara contra
  ellos.
- Publicación: `tools/ota/` con generación del par de claves, firma del
  manifiesto y las dos utilidades de diagnóstico (`make-otadata.py`,
  `make-defective-image.sh`).
- Documentación: `docs/ota/ota-security.md`.

No incluye:

- Claves privadas ni certificados de producción en el repositorio.
- OTA de nodos por ESP-NOW (fuera de alcance en la primera entrega, según
  la propia issue).
- Un endpoint HTTP para disparity la actualización: el arranque del
  servidor HTTP completo es de la Fase 4 y esta issue entrega el gestor y
  su API, no el servidor.
- Secure Boot de ESP-IDF: es una capa del bootloader, ortogonal al
  manifiesto de publicación.

## Restricciones y evidencia

- ESP-IDF v5.4, xtensa. Toolchain en `$HOME/esp/esp-idf`. `source export.sh`
  antes de `idf.py`.
- `chain_strategy: feature-branch-chain`. Una slice por PR, ≤400 líneas.
- TDD: compilación limpia + etiqueta de Unity + símbolo exportado es la
  evidencia equivalente cuando no hay hardware; el dispositivo
  (`/dev/ttyACM0`) está disponible para el *boot smoke* y para la
  demostración del rollback.
- Sin claves privadas en Git: la mitad pública se versiona con una
  excepción explícita en `.gitignore`; la privada sigue cubierta por
  `*.pem`.

## Mapa de la separación público / secreto

| Dato | Dónde vive | Versionado | Secreto |
|---|---|---|---|
| Clave pública de firma | `ota_manager/certs/ota_signing_key.pem` | sí | no |
| Clave privada de firma | `ota_manager/certs/ota_signing_private.pem` | **no** | **sí** |
| URL del servidor de releases | NVS `ota`/`manifest_url` | no | no |
| Certificado CA del servidor | bundle de ESP-IDF en el firmware | sí | no |

## Plan de trabajo

### [x] ODD-1 — Componente `ota_manifest` (modelo, parser, forma canónica)

**Archivos**:
- `firmware/gateway/components/ota_manifest/include/ota_manifest.h`
- `firmware/gateway/components/ota_manifest/ota_manifest.c`
- `firmware/gateway/components/ota_manifest/CMakeLists.txt`
- `firmware/gateway/components/ota_manifest/test/CMakeLists.txt`
- `firmware/gateway/components/ota_manifest/test/test_ota_manifest.c`

**Aceptación**:
- El parser lee un manifiesto completo y rechaza campo desconocido,
  duplicado, ausente, esquema no soportado y URL sin HTTPS.
- La forma canónica es byte a byte la que firma la release.
- Las reglas de compatibilidad, canal y antirregresión rechazan lo que
  toca.
- Test bajo la etiqueta `[ota_manifest]`.

**Commit**: `feat(gateway): add the ota_manifest component (the release manifest)`.

### [x] ODD-2 — Identidad de producto y revisión de hardware

**Archivos**:
- `firmware/gateway/components/brand_profile/Kconfig.projbuild`
- `firmware/gateway/components/brand_profile/include/brand_profile.h`
- `firmware/gateway/components/brand_profile/brand_profile.c`
- `firmware/gateway/components/brand_profile/brand_profile_esp.c`
- `firmware/gateway/components/brand_profile/test/test_brand_profile.c`

**Aceptación**:
- `CONFIG_BRAND_PRODUCT_ID` y `CONFIG_BRAND_HW_REVISION` existen.
- `brand_profile_validate()` los exige.
- El payload JSON los publica.

**Commit**: `feat(gateway): add the product id and the hardware revision to the brand profile`.

### [x] ODD-3 — `ota_manager`: verificación, descarga, instalación

**Archivos**:
- `firmware/gateway/components/ota_manager/include/ota_manager.h`
- `firmware/gateway/components/ota_manager/ota_manager.c`
- `firmware/gateway/components/ota_manager/CMakeLists.txt`
- `firmware/gateway/components/ota_manager/certs/ota_signing_key.pem`
- `firmware/gateway/components/ota_manager/test/*`

**Aceptación**:
- La firma ECDSA P-256 se verifica contra la clave embebida.
- El manifiesto se comprueba **antes** de descargar la imagen.
- La imagen se escribe en la ranura inactiva y se relee para comparar el
  digest.
- El estado de la unidad y la URL del servidor se guardan en NVS.

**Commit**: `feat(gateway): implement the secure OTA manager`.

### [x] ODD-4 — Rollback y ventana de confirmación

**Archivos**:
- `firmware/gateway/components/ota_manager/ota_manager.c`
- `firmware/gateway/main/main.c`
- `firmware/gateway/sdkconfig.defaults`

**Aceptación**:
- `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`.
- La imagen nueva arranca en `PENDING_VERIFY` y se confirma sólo cuando
  los subsistemas críticos están vivos.
- Un vigilante reinicia la unidad si la imagen no se confirma en 120 s.

**Commit**: `feat(gateway): confirm the new image and roll back when it does not work`.

### [x] ODD-5 — Publicación: clave, firma y utilidades de diagnóstico

**Archivos**:
- `tools/ota/gen-ota-keys.sh`
- `tools/ota/sign-manifest.py`
- `tools/ota/make-otadata.py`
- `tools/ota/make-defective-image.sh`
- `tools/ota/README.md`
- `.gitignore`

**Aceptación**:
- La mitad privada nunca está en el índice de Git; la pública sí.
- `sign-manifest.py --self-test` comprueba la forma canónica contra el
  vector dorado.
- `make-otadata.py` escribe un registro que el bootloader acepta.

**Commit**: `build(ota): add the release signing and the diagnostic tooling`.

### [x] ODD-6 — Documentación y evidencia acumulada

**Trabajo**:
- `docs/ota/ota-security.md`.
- Evidencia en el host y en el dispositivo.
- Demostración del rollback con una imagen defectuosa.

**Aceptación**:
- Los seis criterios de aceptación de la issue tienen evidencia.

**Commit**: `docs(odd): record the evidence of the secure OTA of issue 13`.

## Ledger de estado

| ODD | Alcance | Estado | Evidencia |
|---|---|---|---|
| ODD-1 | `ota_manifest` (modelo, parser, reglas) | hecho | componente `firmware/gateway/components/ota_manifest/`, etiqueta `[ota_manifest]` |
| ODD-2 | Identidad de producto y revisión | hecho | `CONFIG_BRAND_PRODUCT_ID`, `CONFIG_BRAND_HW_REVISION` |
| ODD-3 | `ota_manager` (firma, descarga, instalación) | hecho | etiqueta `[ota_manager]`, verificación ECDSA real en el dispositivo |
| ODD-4 | Rollback y ventana de confirmación | hecho | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, demostración en `/dev/ttyACM0` |
| ODD-5 | Publicación y diagnóstico | hecho | `tools/ota/`, clave privada fuera del índice |
| ODD-6 | Documentación y evidencia | hecho | `docs/ota/ota-security.md`, 68 tests en host, 110 en dispositivo |

## Evidencia acumulada (ODD-6)

### Criterios de aceptación de la issue

| Criterio | Cómo se cumple | Evidencia |
|---|---|---|
| Una imagen incompatible por marca, modelo o revisión se rechaza **antes** del flasheo | `ota_manifest_check_compatible()` se ejecuta antes de la descarga | test `[ota_manifest]` «A foreign image is refused before it is downloaded»; el log del dispositivo muestra el rechazo con su motivo |
| La firma y el hash del manifiesto se validan | ECDSA P-256 sobre la forma canónica + SHA-256 de la imagen releída de la flash | test `[ota_manager]` «A manifest signed by the release key is accepted» y «A manifest that was altered after the signature is refused» |
| El rollback se demuestra ante una imagen defectuosa | imagen que entra en pánico, escrita en `ota_1` y armada con `otadata` | log completo del dispositivo: arranca `0x320000`, entra en pánico, el bootloader marca `ABORTED` y arranca `0x20000` |
| Los canales no permiten promoción accidental | `rango(manifiesto) >= rango(unidad)`, canal desconocido rechazado | test `[ota_manifest]` «Release channels never promote a unit by accident» |
| La solución cabe en la tabla de particiones aprobada | la imagen mide 1.021.120 B en una ranura de 3 MiB | `check_sizes.py` del build; también cabe en 1,5 MiB (reparto de 4 MB) |
| Credenciales y claves tienen una ruta de provisioning documentada | tabla del apartado 5 de `docs/ota/ota-security.md` | `git check-ignore` sobre la clave privada; la pública versionada |

### Pruebas

- En el host (`tools/host-tests`): **68 tests, 0 fallos**. Los 11 tests de
  `ota_manifest` se compilan contra las fuentes reales del componente.
- En el dispositivo (`/dev/ttyACM0`, `gateway_test.bin`): **110 tests, 0
  fallos**. La firma del manifiesto se verifica con mbedTLS y la clave
  pública embebida: es criptografía real, no un doble.

### Demostración del rollback

Imagen defectuosa: una copia del firmware del gateway cuyo `app_main` entra
en pánico. Escrita en `ota_1` (0x320000) y armada con un registro `otadata`
en estado `new`.

```
I (210)  boot: Loaded app from partition at offset 0x320000
E (1103) ota_defective: this image is defective: it never confirms itself
E (1113) ota_defective: the bootloader must roll back to the previous slot

abort() was called at PC 0x42009d44 on core 0
Backtrace: 0x40375c4d:0x3fc99ea0 ...
Rebooting...
rst:0xc (RTC_SW_CPU_RST),boot:0x8 (SPI_FAST_FLASH_BOOT)
I (154)  boot: No factory image, trying OTA 0
I (354)  boot: Loaded app from partition at offset 0x20000
I (1536) ota_manager: ota: running ota_0, boot ota_0, version 1.0.0, channel stable
```

Y el estado que dejó el bootloader en `otadata`:

```
otadata[0]: seq=2 label='ota_1' state=4 (ABORTED)
otadata[1]: seq=4294967295 state=4294967295 (UNDEFINED)
```

`ABORTED` es `ESP_OTA_IMG_ABORTED`: el bootloader descartó la imagen
defectuosa y volvió a la ranura anterior.

### Presupuesto de flash

```
gateway.bin binary size 0xf94c0 bytes. Smallest app partition is 0x300000 bytes. 0x206b40 bytes (68%) free.
```

1.021.120 bytes en una ranura de 3 MiB: queda el 68 % libre. La misma
imagen cabría en la ranura de 1,5 MiB del reparto de 4 MB del nodo (64 %
ocupado, 35 % libre).

### Secretos

- `git check-ignore -v .../certs/ota_signing_private.pem` → `*.pem`.
- `git ls-files -- .../certs/` → sólo la clave **pública**.

## Guardarraíles

- `partitions.csv` del gateway no se toca: el OTA usa las dos ranuras que
  ya existían.
- La clave privada no se versiona ni se imprime.
- Ningún test necesita red: la verificación de la firma es local.
- La demostración del rollback no borra la NVS ni toca la ranura buena.

## Referencias

- GitHub issue: <https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/13>
- Issue #8 (placa N16R8): `odd/tasks/issue-8-gateway-board-support.md`
- Issue #12 (marca): `odd/tasks/issue-12-brand-profiles.md`
- Fase 5: `Fases/fase-05-ota-optimizacion.md`
- Decisiones: `docs/replanificacion/01-decisiones-corregidas.md`
- Contrato de seguridad: `docs/ota/ota-security.md`
- ESP-IDF, OTA: <https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/ota.html>
- ESP-IDF, rollback: <https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/bootloader.html>
