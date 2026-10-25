# Issue #15 — Sincronización de la documentación con el hardware y el firmware reales

> Plan de trabajo. La documentacion activa del repositorio se contradia en
> varios puntos con el hardware adoptado (ESP32-S3-WROOM-1-N16R8) y con la
> base modular del firmware (`firmware/gateway/components/`). Esta issue
> deja una sola version de la verdad.

## Alcance

- `README.md`, `project.md`, `CLAUDE.md` y `AGENTS.md`: alinear con el target
  S3 y con el estado real de `firmware/gateway/`.
- `Fases/` y `Tutorial/`: eliminar referencias a hardware no adoptado
  (ESP32-S3-DevKitC, ESP32-C3-DevKitM y el GPIO del LED propio de cada placa)
  y mantener solo las compatibles con S3.
- `docs_site/` y `mkdocs.yml`: espejo completo de las fuentes canonicas y
  build en modo estricto.
- `openspec/changes/iiot-kit-transformation/`: sincronizar el estado con las
  tareas realmente completadas.

## Restricciones

- La documentacion narrativa va en castellano europeo; los artefactos de
  `openspec/` y `.atl/` van en ingles.
- `docs_site/` es un espejo por enlaces simbolicos: se edita el fichero
  canonico, nunca la copia del espejo.
- No se declara implementada ninguna funcion que no tenga evidencia en
  `firmware/gateway/` o `firmware/common/`.
- Los valores reportados por el vendedor (pinout de imagen, dimensiones,
  3,3 V, 20 mA) siguen siendo **pendientes de verificacion fisica** y se
  mantienen marcados como tales.
- El pitch retirado `0.1155 mm` no se reintroduce, ni su geometria derivada.

## Diagnostico

| Hallazgo | Evidencia |
|----------|-----------|
| `mkdocs build --strict` fallaba con 18 avisos | AC-05 del cambio archivado `iiot-kit-transformation`; lo registro `openspec/changes/archive/2026-09-27-iiot-kit-transformation/archive-report.md` como riesgo residual |
| `docs_site/` no contenia las notas de concepto ni ADR-004/005 | Los enlaces `[[ESP-NOW]]`, `[[Deep-Sleep]]`, `[[004-iiot-kit-transformation]]` no tenian destino en el sitio |
| Las paginas de referencia usaban nombres distintos de los del vault | `referencia/protocolo.md` frente a `knowledge/Referencia/Protocolo-Mensajes.md`, lo que rompia la resolucion de `[[Protocolo-Mensajes]]` |
| `README.md` declaraba `Bun + TypeScript + SQLite` y un tutorial en otro repositorio | `README.md`, tabla de stack y enlace del tutorial |
| `Fases/fase-04` declaraba `8MB flash, 2MB PSRAM` | Contradice el modulo adoptado (16 MB flash, 8 MB PSRAM Octal) |
| `Fases/fase-04` y `Tutorial/tutorial-04` describian particiones de 8 MB | `firmware/gateway/partitions.csv` es de 16 MB |
| `Fases/fase-00/01/02` nombraban placas no adoptadas | `ESP32-S3-DevKitC`, `ESP32-C3-DevKitM` |
| `Fases/fase-05` daba por hecho un broker Mosquitto y un backend | Contradice lagateway autonomo |
| `Fases/fase-06`/`Tutorial/tutorial-06` asumian un backend con SQLite y JWT | Contradice la gateway autonomo |
| `project.md` describia un firmware plano en `main/` | El firmware real es modular, con componentes por subsistema |
| `project.md` fijaba un cronograma de 22 semanas | `docs/replanificacion/03-fases-corregidas.md` fija 25 |
| `openspec/changes/iiot-kit-transformation/state.yaml` declaraba 9 de 22 tareas | El cambio se completo y se archivo el 2026-09-27 con 22 de 22 |
| Sparkplug B no aparecia en Fase 6 | El componente existe y esta desactivado por defecto |

## Trabajo

1. **Espejo de `docs_site/`**
   - Enlazar `knowledge/Conceptos/*` en `docs_site/conceptos/`.
   - Enlazar ADR-004 y ADR-005 en `docs_site/decisiones/`.
   - Renombrar las paginas de referencia a su nombre canonico del vault.
   - Enlazar `knowledge/Proyecto/{Arquitectura,Progreso}.md` y
     `knowledge/_MOC.md`.
   - Anadir las entradas correspondientes al `nav` de `mkdocs.yml`.
2. **Enlaces rotos**
   - Normalizar `[[`_MOC`]]` a `[[_MOC]]` en las notas de concepto.
   - Convertir en texto plano las cuatro entradas de `_MOC.md` que apuntan a
     notas que no existen.
   - Anadir `tools/ci/check-roamlinks.py`, que reproduce la resolucion del
     plugin y falla con codigo 1 ante un enlace sin destino.
3. **Stack y arquitectura**
   - Quitar `Bun + TypeScript + SQLite` de `README.md` y del diagrama de
     `docs_site/index.md` y `Tutorial/README.md`.
   - Corregir el enlace del tutorial, que apuntaba a otro repositorio.
   - Anadir el hardware del gateway con su estado de verificacion.
4. **Fuente de verdad del hardware**
   - Declarar `docs/hardware/esp32-s3-wroom-1-n16r8.md` como fuente unica en
     `project.md`, `CLAUDE.md` y `AGENTS.md`.
   - Sustituir la tabla de hardware de `project.md` por un resumen con
     enlace, en lugar de duplicar el perfil.
5. **Estado real del firmware**
   - Describir la estructura modular en `project.md` y `CLAUDE.md`.
   - Marcar como implementados solo los componentes con codigo real, y como
     contratos los que son solo interfaz.
   - Corregir memoria y particiones en `Fases/fase-04`,
     `Tutorial/tutorial-04` y `knowledge/Proyecto/Arquitectura.md`.
6. **Hardware no adoptado**
   - Sustituir `ESP32-S3-DevKitC` y `ESP32-C3-DevKitM` por el carrier
     adoptado y por una placa de nodo sin modelo fijado.
7. **Sparkplug B como integracion opcional**
   - Anadir la sub-tarea 6.0 en `Fases/fase-06-mejoras-futuras.md`.
   - Marcarla como opcional en `project.md`, `CLAUDE.md` y `AGENTS.md`.
8. **OpenSpec**
   - Poner `state.yaml` en `completed`/`archive` con las 22 tareas hechas y
     `tasks_remaining: []`.
   - Sincronizar `tasks.md` y `apply-progress.md` con la version archivada.
   - Registrar la remediacion de AC-05 en el propio cambio.
9. **Puerta de calidad**
   - Anadir `.github/workflows/docs-ci.yml` con `mkdocs build --strict` y el
     comprobador de enlaces.
   - Pasar `deploy-docs.yml` a modo estricto.

## Fuera de alcance

- La carpeta activa `openspec/changes/issue-9-gateway-firmware-foundation/`:
  solo conserva `apply-progress.md` y `tasks.md` sin `state.yaml`, asi que no
  declara un estado que contradiga al archivo.

## Segunda pasada: tres huecos que quedaban

Una segunda entrega cerro tres incoherencias que sobrevivieron a la primera:

- **Rutas planas en `Fases/` y `Tutorial/`**: 63 referencias a
  `firmware/gateway/main/<componente>.c` describian un firmware plano que ya no
  existe. Ahora apuntan a `firmware/gateway/components/<nombre>/<nombre>.c` y a
  `firmware/gateway/components/<nombre>/include/<nombre>.h`. Los ficheros que si
  viven en `main/` (`main.c`, `nvs_config.c/.h`, `CMakeLists.txt`,
  `Kconfig.projbuild`, `idf_component.yml`) se dejan intactos. Las tareas que
  decian "agregar X.c a SRCS en `main/CMakeLists.txt`" decian una cosa y el build
  hacia otra: ahora dicen "crear `components/<nombre>/CMakeLists.txt`", que es lo
  que hace `EXTRA_COMPONENT_DIRS "components"`. La traduccion esta en
  `tools/docs/fix-flat-gateway-paths.py`.
- **Cifra de memoria contradictoria en una pagina publicada**:
  `knowledge/Conceptos/Preact.md` seguia diciendo que la particion SPIFFS tiene
  1,9 MB, cuando son 4 MiB.
- **El trabajo sin aterrizar**: la cadena de PRs quedaba abierta, de modo que la
  rama canonica seguia arrastrando el backend `Bun + TypeScript + SQLite` del
  `README.md`. La cadena se fusiona de abajo arriba y `main` queda con el estado
  corregido.

## Criterios de aceptacion

| Criterio | Como se comprueba |
|----------|-------------------|
| Sin Bun, Node, SQLite ni Mosquitto obligatorio en la documentacion activa | `grep -riE "bun\|sqlite" README.md project.md CLAUDE.md AGENTS.md docs_site/index.md Tutorial/README.md Fases/ Tutorial/` -> solo menciones historicas explicitas o negaciones |
| Fuente de verdad unica de placa, pinout y memoria | `docs/hardware/esp32-s3-wroom-1-n16r8.md` referenciado desde `project.md`, `CLAUDE.md` y `AGENTS.md`; sin cifras de memoria contradictorias |
| Sparkplug B como integracion opcional en Fase 6 | Sub-tarea 6.0 en `Fases/fase-06-mejoras-futuras.md`; `CONFIG_SPARKPLUG_B_ENABLED` por defecto en `n` |
| Display y carrier pendientes de verificacion fisica | Sin afirmaciones de geometria, niveles logicos o corriente de retroiluminacion como hechos |
| Estado OpenSpec = tareas completadas | `state.yaml` con `status: completed`, 22 tareas en `tasks_done`, `tasks_remaining: []` |
| `mkdocs build --strict` correcto | `mkdocs build --strict` devuelve 0; `python3 tools/ci/check-roamlinks.py` devuelve 0 |
