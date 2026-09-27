
# Embedded Dashboard Specification

## Purpose

The ESP32-S3 gateway MUST serve a complete interactive dashboard without any external server. A Preact SPA is stored in SPIFFS and delivered to browsers over WiFi AP. REST API and WebSocket endpoints on the gateway are the sole data interfaces.

## Requirements

### Requirement: SPIFFS-Served Preact SPA

The documentation for Fase 4 MUST describe a Preact SPA built with Vite, gzip-compressed, and stored in the gateway's SPIFFS partition (~1.9 MB). The docs MUST specify a size budget (target ≤200 KB gzipped) and reference the `web/` source directory under `firmware/gateway/`.

#### Scenario: Developer reads Fase 4 for dashboard setup

- GIVEN a developer opens `Fases/fase-04-mqtt-dashboard.md`
- WHEN they read the embedded dashboard section
- THEN they find a Preact + Vite build pipeline explained
- AND a SPIFFS partition size budget is documented
- AND the docs reference `firmware/gateway/web/` as the SPA source directory

#### Scenario: Size budget exceeded scenario is addressed

- GIVEN the SPA source size exceeds the SPIFFS budget
- WHEN the developer reads the troubleshooting section
- THEN they find guidance on reducing bundle size (gzip, code splitting, asset limits)

### Requirement: REST API Documentation

The documentation MUST describe a REST API served by `esp_http_server` on the gateway covering at minimum: sensor data retrieval, node configuration, and OTA trigger endpoints. MQTT MUST be documented as optional, not required for basic operation.

#### Scenario: Developer designs a REST client for the gateway

- GIVEN a developer reads Fase 4 REST API section
- WHEN they look for endpoint definitions
- THEN they find at least GET /api/sensors, GET /api/nodes, POST /api/config, and POST /api/ota documented
- AND each endpoint shows request/response format

#### Scenario: Developer checks if MQTT is mandatory

- GIVEN a developer reads Fase 4
- WHEN they search for MQTT requirements
- THEN they find MQTT documented in a clearly labeled optional integration section
- AND the core dashboard workflow does not depend on MQTT being configured

### Requirement: WebSocket Real-Time Data Push

The documentation MUST describe a WebSocket endpoint on the gateway that pushes sensor events to connected browsers. The docs MUST specify the message format and connection lifecycle.

#### Scenario: Developer implements real-time sensor display

- GIVEN a developer reads the WebSocket section in Fase 4
- WHEN they look for how to connect and receive data
- THEN they find a WebSocket URL pattern, a message format (JSON), and reconnection guidance

#### Scenario: No active WebSocket client

- GIVEN no browser is connected to the WebSocket endpoint
- WHEN a sensor data message arrives at the gateway
- THEN the docs clarify that data is still processed and stored internally (WebSocket is push-only, not required for data collection)

---

