# ADR-002: ESP-NOW sin cifrado en v1.0

## Estado
Aceptada (2026-05-23)

## Contexto
El plan original asumia 20 peers cifrados con ESP-NOW. Sin embargo, el hardware ESP-NOW solo soporta **6 peers cifrados** (con LMK/PMK). Con 20 peers no cifrados, se pueden tener hasta 20 nodos.

Una instalacion IIoT tipica necesita al menos 10-15 nodos para cubrir todas las zonas monitorizadas.

## Decision
Usar **ESP-NOW sin cifrado** en v1.0, con validacion de protocolo como medida de seguridad basica. Planificar cifrado por software (AES-256 a nivel de payload) para v2.0.

## Alternativas consideradas

1. **Cifrado ESP-NOW nativo (PMK/LMK)**
   - Pro: Cifrado AES-128 a nivel de hardware, transparente
   - Contra CRITICO: Limite de 6 peers cifrados. Insuficiente para el proyecto
   - Contra: No escala sin redesign

2. **Sin cifrado + validacion de protocolo** (elegida)
   - Pro: 20 peers disponibles (suficiente para v1.0)
   - Pro: Simple de implementar
   - Contra: Los datos viajan en texto plano por el aire
   - Mitigacion: Rango limitado de ESP-NOW (~200m), validacion de version/tipo/MAC

3. **Cifrado por software (AES-256-GCM a nivel de payload)**
   - Pro: Sin limite de peers, seguridad robusta
   - Pro: Independiente del hardware
   - Contra: Overhead de 32 bytes por mensaje (IV + tag)
   - Contra: Complejidad de implementacion, gestion de claves
   - Decision: Diferir a v2.0 (Fase 6)

## Consecuencias
- `peer_info.encrypt = false` en todas las configuraciones de ESP-NOW
- Gateway implementa whitelist de MACs aprobadas (modo OPEN/STRICT)
- Validacion de protocolo: version, msg_type, payload_len
- El riesgo de seguridad es aceptable para v1.0 (entorno controlado, rango limitado)
- v2.0 agrega cifrado por software sin cambiar la arquitectura

## Referencias
- [[ESP-NOW]]
- [ESP-NOW docs: Peer encryption](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/network/esp_now.html)
- `docs/replanificacion/01-decisiones-corregidas.md` (DC-01)
