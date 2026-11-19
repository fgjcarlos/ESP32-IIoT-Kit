# ADR-006: Provisión inicial del portal local por credencial aleatoria y revelación serie

## Estado
Aceptada (2026-11-19)

## Contexto
La historia W02 (issue #69) exige un portal local protegido servido por el
gateway desde flash. El criterio de aceptación 2 dice literalmente: "Rechazar
acceso no autorizado a operaciones y datos protegidos; acordar el mecanismo de
primera provisión antes de implementarlo, sin publicar secretos". El gateway no
tiene runtime LCD (diferido por #16), así que la credencial inicial no puede
mostrarse en pantalla del equipo, y el proyecto es educativo: no existe línea
de manufactura que imprima etiquetas.

## Decisión
El gateway genera una **credencial inicial aleatoria en el primer arranque**
(`esp_fill_random`), persiste en NVS **solo su digest SHA-256** (namespace
`"portal"`, claves X-macro siguiendo el patrón de `sparkplug_b_nvs.h`) y la
**revela al propietario por la consola serie** (UART0 / USB-CDC). El primer
login exitoso ofrece el cambio de credencial (flujo W01-v0.2). Comparación en
tiempo constante sobre el digest.

## Alternativas consideradas

1. **Aleatoria al primer arranque + serie** (elegida)
   - Pro: ningún secreto en Git ni en la imagen del firmware
   - Pro: aislamiento real por dispositivo (cada gateway nace con una distinta)
   - Pro: la recuperación exige acceso físico, coherente con un portal del propietario
   - Pro: no requiere manufactura ni etiquetas
   - Contra: el primer acceso necesita un monitor serie (`idf.py monitor`)

2. **Derivada de identidad del equipo** (MAC/eFuse + sal de manufactura)
   - Pro: imprimible en etiqueta, sin interacción serie
   - Contra: quien conozca la regla de derivación y la identidad del equipo
     puede calcular la credencial
   - Contra: exige manufactura real (etiquetas) para aportar valor; hoy no existe

3. **Fija por perfil de marca** (brand secrets locales)
   - Pro: la más simple para aula/laboratorio
   - Contra: todos los equipos del despliegue nacen con la misma credencial;
     si nadie la cambia, aislamiento entre dispositivos = 0

## Consecuencias
- `portal_auth` provisiona con `esp_fill_random` cuando NVS no tiene digest
- El reveal serie se documenta como paso operativo del primer acceso
  (pendiente de verificación física, tag `[hw]` donde corresponda)
- El cambio de credencial exige la credencial vigente; un cambio rechazado no
  altera lo almacenado
- W03 y posteriores heredan este mecanismo sin cambios

Related: [[ADR-004-iiot-kit-transformation]] · issue #69 · `openspec/changes/issue-69-local-portal/design.md` (D3)
