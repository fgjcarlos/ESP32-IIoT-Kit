# Portal local del gateway (W02)

Aplicación **Preact + Vite** que es el portal local protegido que sirve el
propio gateway desde su flash, accesible por el AP del equipo sin STA ni
Internet.

- Historia: W02 — [issue #69](https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/69).
- Diseño: `W01-v0.2` (wireframes aprobados en la issue #68); las vistas de W02
  son las de W01, sin pantallas nuevas.
- Tokens visuales: [`design/system.md`](../../../design/system.md).
- Tarea ODD: `odd/tasks/issue-69-portal-local-protegido.md` (T6).
- Decisión de credencial: [ADR-006](../../../knowledge/Decisiones/006-portal-provision-inicial.md).

## Qué hace

El recorrido es el de W01-v0.2, contra la API real del firmware:

| Pantalla | Comportamiento |
|---|---|
| Puerta de acceso | Pide la credencial única; `POST /api/login` |
| Estado | Cuatro tarjetas con los datos en vivo de `GET /api/status` |
| Credencial | Cambia la credencial; `POST /api/credential` |

La sesión vive en el servidor (cookie `portal_session`, `HttpOnly`). La
interfaz no guarda el estado: cada vista pide lo que necesita.

## La credencial inicial

El equipo genera una credencial aleatoria en su primer arranque y **solo la
muestra una vez por la consola serie** (UART0 / USB-CDC), junto con el aviso
de cambiarla. En la imagen del firmware no hay ninguna credencial: se guarda
únicamente su resumen SHA-256 en la partición NVS.

```text
I (1234) portal_auth: portal initial credential: 4f2a-91c7-0b8e-6d35 (shown once; change it after the first login)
```

Para recuperarla hay que acceder al equipo físicamente y borrar su NVS: el
portal vuelve a aprovisionar una credencial nueva.

## Desarrollo local

```bash
npm install
npm run dev      # vite en 127.0.0.1:5174
npm test         # node --test
npm run build    # genera dist/ (se commitea: el firmware lo embebe)
```

El `dist/` se commitea a propósito: el firmware embebe esos bytes en la
imagen y el build de firmware no debe depender de Node.

## W03

La vista de configuración de Wi-Fi (`ConfigView.js`) no se ofrece en W02: la
configuración de la red del usuario es la historia siguiente, que debe
conservar el acceso local. El archivo se conserva para esa entrega.

## Límites

- Un solo propietario: no hay cuentas, ni historial, ni permisos por rol.
- Sin WebSocket ni push: el estado se lee cuando la vista lo pide.
- El portal no sustituye al LCD: el runtime de display sigue diferido (#16).
