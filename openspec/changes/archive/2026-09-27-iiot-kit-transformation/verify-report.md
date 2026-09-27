# Verification Report: iiot-kit-transformation

**Run date**: 2026-09-23  
**Runner**: sdd-apply executor  
**Scope**: User-prescribed acceptance commands, executed in order. Each command was run as provided; exit code was captured immediately. No implementation fixes were made.

## Results

| AC | Verdict | Evidence |
|---|---|---|
| AC-01 | PASS | No matching output; exit code 1 is grep's expected no-match status. |
| AC-02 | PASS | All three requested example files listed; exit code 0. |
| AC-03 | PASS | `SENSOR_TYPE_CUSTOM = 0xFF` match; exit code 0. |
| AC-04 | PASS | Output includes `mqtt_ns` and default `iiot-kit`; exit code 0. |
| AC-05 | FAIL | `mkdocs build --strict` exited 1 after reporting 18 warnings. Per user direction, these warnings are understood as non-fatal individually, but strict-mode exit code is the criterion and remains a failure. |
| AC-06 | PASS | All four requested knowledge files listed; exit code 0. |
| AC-07 | PASS | `site_name` contains `ESP32-IIoT-Kit`; exit code 0. |
| AC-08 | PASS | No matching output; exit code 1 is grep's expected no-match status. |
| AC-09 | PASS | Match count was 1; exit code 0. |
| AC-10 | PASS | Match count was 22; exit code 0. |

## Commands and Observed Output

### AC-01 — terminology search

**Command**
```sh
grep -rIn --include='*.md' --include='*.yml' -E 'piscifactoria|estanque|acuicultura|acuicola|granja de peces|tanque de peces' Fases Tutorial docs project.md CLAUDE.md README.md mkdocs.yml docs_site knowledge | grep -v 'examples/fish-farm' | grep -v 'openspec/' | grep -v 'docs/replanificacion/00-resumen.md' | grep -v 'docs/replanificacion/02-protocolo-unificado.md' | grep -v 'knowledge/Decisiones/004-iiot-kit-transformation.md' | grep -v 'README.md:41'
```

**Observed output**: no output (empty).  
**Exit code**: 1 (expected grep no-match result).

### AC-02 — example files

**Command**
```sh
ls -la examples/fish-farm/README.md examples/fish-farm/sensors.md examples/fish-farm/mqtt-topics.md
```

**Observed output**
```text
-rw-rw-r-- 1 composedof2 composedof2  3772 sep 23 22:31 examples/fish-farm/mqtt-topics.md
-rw-rw-r-- 1 composedof2 composedof2  4323 sep 23 22:31 examples/fish-farm/README.md
-rw-rw-r-- 1 composedof2 composedof2 14273 sep 23 22:31 examples/fish-farm/sensors.md
```
**Exit code**: 0.

### AC-03 — custom sensor type

**Command**
```sh
grep 'SENSOR_TYPE_CUSTOM' docs/replanificacion/02-protocolo-unificado.md
```

**Observed output**
```text
    SENSOR_TYPE_CUSTOM      = 0xFF,  // Tipo de sensor personalizado para sensores definidos por el usuario.
```
**Exit code**: 0.

### AC-04 — MQTT namespace

**Command**
```sh
grep -E 'mqtt_ns|iiot-kit' docs/replanificacion/02-protocolo-unificado.md
```

**Observed output**
```text
{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}
| `mqtt_ns` | Namespace configurable. Se almacena en NVS con clave "mqtt_namespace". Valor por defecto: "iiot-kit". |
Con `mqtt_ns = "iiot-kit"` (valor por defecto):
iiot-kit/GW-001/nodo/AA:BB:CC:DD:EE:FF/temperature
iiot-kit/GW-001/alertas
iiot-kit/GW-001/control/actuador/0
Con `mqtt_ns = "greenhouse"` (instalacion de invernadero):
char mqtt_ns[32] = "iiot-kit";  // valor por defecto
size_t ns_len = sizeof(mqtt_ns);
nvs_get_str(h, "mqtt_namespace", mqtt_ns, &ns_len);
**Nota**: El prefijo historico `piscifactoria/` era el namespace por defecto en versiones anteriores del proyecto. Con la transformacion a ESP32-IIoT-Kit, ese prefijo se convierte en un ejemplo de namespace especifico de dominio. Ver `examples/fish-farm/mqtt-topics.md` para la configuracion completa del dominio de acuicultura (`mqtt_ns = "fish-farm"`).
```
**Exit code**: 0.

### AC-05 — strict MkDocs build

**Command**
```sh
mkdocs build --strict
```

**Observed output**
```text
 │  ⚠  Warning from the Material for MkDocs team
 │
 │  MkDocs 2.0, the underlying framework of Material for MkDocs,
 │  will introduce backward-incompatible changes, including:
 │  × All plugins will stop working – the plugin system has been removed
 │  × All theme overrides will break – the theming system has been rewritten
 │  × No migration path exists – existing projects cannot be upgraded
 │  × Closed contribution model – community members can't report bugs
 │  × Currently unlicensed – unsuitable for production use
 │  Our full analysis:
 │  https://squidfunk.github.io/mkdocs-material/blog/2026/02/18/mkdocs-2.0/
INFO - DeprecationWarning: warning_filter doesn't do anything since MkDocs 1.2 and will be removed soon. All messages on the `mkdocs` logger get counted automatically.
  File "/home/composedof2/.local/share/pipx/venvs/mkdocs-material/lib/python3.12/site-packages/mkdocs_roamlinks_plugin/plugin.py", line 10, in <module>
    log.addFilter(mkdocs.utils.warning_filter)
  File "/home/composedof2/.local/share/pipx/venvs/mkdocs-material/lib/python3.12/site-packages/mkdocs/utils/__init__.py", line 403, in __getattr__
    warnings.warn(
INFO - Cleaning site directory
INFO - Building documentation to directory: /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/site
WARNING - RoamLinksPlugin unable to find 004-iiot-kit-transformation in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find ESP-NOW in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find Protocolo-Mensajes in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find ESP-NOW in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find Deep-Sleep in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find NVS in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find FreeRTOS in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find ESP-IDF in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find ESP-NOW in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find Deep-Sleep in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find ESP-NOW in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find ESP-NOW in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find ESP-IDF in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find FreeRTOS in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find NVS in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find REST-API in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find WebSocket in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
WARNING - RoamLinksPlugin unable to find ESP-NOW in directory /home/composedof2/Dev/Codex/ESP32-IIoT-Kit/docs_site
Aborted with 18 warnings in strict mode!
```
**Exit code**: 1. Output contained 18 RoamLinksPlugin warnings plus the MkDocs Material notice and warning-filter deprecation details. The strict build is a failed AC; nothing was changed to mask or fix it.

### AC-06 — knowledge files

**Command**
```sh
ls -la knowledge/Decisiones/004-iiot-kit-transformation.md knowledge/Conceptos/Preact.md knowledge/Conceptos/REST-API.md knowledge/Conceptos/WebSocket.md
```

**Observed output**
```text
-rw-rw-r-- 1 composedof2 composedof2 3633 sep 23 22:31 knowledge/Conceptos/Preact.md
-rw-rw-r-- 1 composedof2 composedof2 5038 sep 23 22:31 knowledge/Conceptos/REST-API.md
-rw-rw-r-- 1 composedof2 composedof2 5555 sep 23 22:31 knowledge/Conceptos/WebSocket.md
-rw-rw-r-- 1 composedof2 composedof2 4340 sep 23 22:31 knowledge/Decisiones/004-iiot-kit-transformation.md
```
**Exit code**: 0.

### AC-07 — MkDocs site name

**Command**
```sh
grep 'site_name' mkdocs.yml
```

**Observed output**
```text
site_name: "ESP32-IIoT-Kit — Tutorial"
```
**Exit code**: 0.

### AC-08 — no SQLite/Express references in CLAUDE.md

**Command**
```sh
grep -iE 'sqlite|express' CLAUDE.md
```

**Observed output**: no output (empty).  
**Exit code**: 1 (expected grep no-match result).

### AC-09 — Fase 4 title

**Command**
```sh
grep -c 'Dashboard Embebido' Fases/fase-04-mqtt-dashboard.md
```

**Observed output**
```text
1
```
**Exit code**: 0.

### AC-10 — DS18B20 in Fase 3

**Command**
```sh
grep -c 'DS18B20' Fases/fase-03-sensores-actuadores.md
```

**Observed output**
```text
22
```
**Exit code**: 0.

## Conclusion

Nine acceptance criteria passed. AC-05 failed because `mkdocs build --strict` returned exit code 1, aborting after 18 warnings. AC-01 had no matching output and is a pass; there is no AC-01 residual failure. No fixes or other project-file edits were made. The proposed five-PR sequence is recorded in `apply-progress.md` and repeated here:

1. Wave 1 + anchors — T-01..T-03.
2. Wave 2 + Wave 3 core — T-04..T-10.
3. Wave 4 new files — T-11..T-15.
4. Wave 5 narrative sweep — T-16..T-21.
5. Wave 6 verification — T-22.

No real branches or PRs were created.
