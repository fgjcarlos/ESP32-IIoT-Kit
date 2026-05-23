# Protocolo de Mensajes ESP-NOW — Quick Reference

#referencia

Source of truth: `docs/replanificacion/02-protocolo-unificado.md`

## Tipos de mensaje

| Tipo | Valor | Direccion | Descripcion |
|------|-------|-----------|-------------|
| DATA | 0x01 | nodo → gateway | Datos de sensor |
| ACK | 0x02 | gateway → nodo | Confirmacion |
| DISCOVERY | 0x03 | nodo → gateway | Auto-registro |
| CONFIG | 0x04 | gateway → nodo | Configuracion remota |
| OTA_PREPARE | 0x05 | gateway → nodo | Preparar nodo para OTA WiFi |

## Tipos de sensor

| Tipo | Valor | values[] |
|------|-------|----------|
| TEMPERATURE | 0x01 | [0]=grados C |
| PH | 0x02 | [0]=pH, [1]=mV raw |
| DO | 0x03 | [0]=mg/L, [1]=temp compensacion |
| ORP | 0x04 | [0]=mV |
| LEVEL | 0x05 | [0]=centimetros |
| TURBIDITY | 0x06 | [0]=NTU |
| CUSTOM | 0xFF | Punto de extension para tipos definidos por el usuario. La interpretacion de values[] es especifica de la aplicacion. `protocol_sensor_type_str()` retorna `"CUSTOM"`. |

## Header comun (13 bytes)

```
[version:1][msg_type:1][node_mac:6][sequence:2][reserved:1][payload_len:2]
```

## Tamanos

| Mensaje | Bytes | Cabe en ESP-NOW (250) |
|---------|-------|-----------------------|
| Header | 13 | - |
| DATA | 37 | Si |
| ACK | 20 | Si |
| DISCOVERY | 21 | Si |
| CONFIG | 16 | Si |
| OTA_PREPARE | 112 | Si |

## Flujo tipico

```
Cold boot:  Nodo --DISCOVERY--> Gateway --ACK--> Nodo
Normal:     Nodo --DATA-------> Gateway --ACK--> Nodo (sleep)
Config:     Nodo --DATA-------> Gateway --CONFIG-> Nodo (aplica, sleep)
OTA:        Gateway --OTA_PREPARE-> Nodo (conecta WiFi, descarga firmware)
```

## Ver tambien
- [[ESP-NOW]]
- [[003-protocolo-unificado]]
