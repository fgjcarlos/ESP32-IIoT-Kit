# Acceso y estado de red — demo local (W01-v0.2)

Aplicación **Preact + Vite** con **datos de demostración** que simula el primer
acceso protegido y muestra el estado de red y de acceso local del gateway.

**No es el dashboard del producto.** No hay API, ni servidor, ni firmware, ni
SPIFFS, ni flasheo, ni persistencia: el panel no produce ningún efecto sobre el
dispositivo y no escribe nada.

- Versión del diseño: `W01-v0.2` (propuesta, **aprobación pendiente**).
- Tokens visuales: [`design/system.md`](../../../design/system.md) (base visual provisional).
- Tarea ODD: `odd/tasks/issue-68-web-wireframes.md` (T14).

## Alcance de esta entrega

La demo arranca **cerrada**: una puerta de primer acceso pide la credencial
única del equipo (valor de demostración `acceso-inicial`, mostrado en pantalla
porque no hay equipo real). Desbloqueada, ofrece tres vistas:

| Tarjeta | Estado mostrado |
|---|---|
| AP del equipo | Disponible |
| STA de la red del usuario | Desconectada · opcional |
| Acceso local | Disponible por AP |
| Internet | No necesario |

- **Estado de red**: cuatro tarjetas con datos de ejemplo fijos (AP, STA
  opcional, acceso local, Internet no necesario), de solo lectura.
- **Configuración**: formulario AP/STA con SSID y claves editables y canal e
  IP de solo lectura (los fija el equipo). La aplicación es simulada: un
  control de demo permite forzar el fallo; con fallo, la configuración
  anterior sigue activa (esa es la vía de recuperación).
- **Credencial**: cambio de la credencial única, con propuesta en el primer
  uso. Desde la puerta cerrada, «Recuperar acceso» simula reentrar por el AP
  del equipo y restaura la credencial inicial sin tocar la configuración.

Quedan **fuera de esta entrega**: la integración real con el gateway (API,
SPIFFS, dispositivo), que vive en otra issue.

## Requisitos

- Node 24 (probado con `v24.14.0`) y npm 11. Las dependencias están fijadas y
  verificadas con `--engine-strict`: ninguna exige un Node posterior.

## Instalación

```bash
npm install --prefix firmware/gateway/web --cache "$PWD/build/issue-68-npm-cache" --ignore-scripts --engine-strict
```

Para instalar exactamente lo que fija el lockfile:

```bash
npm ci --prefix firmware/gateway/web --cache "$PWD/build/issue-68-npm-cache" --ignore-scripts --engine-strict
```

La caché de npm queda dentro del proyecto (`build/issue-68-npm-cache`, ya
ignorado por `.gitignore`); no se toca la caché ni la configuración global.
`node_modules/` y `dist/` están ignorados por `firmware/gateway/web/.gitignore`.

## Scripts

| Comando | Qué hace |
|---|---|
| `npm --prefix firmware/gateway/web test` | Pruebas de render con el runner de Node |
| `npm --prefix firmware/gateway/web run build` | Bundle de producción en `firmware/gateway/web/dist` |
| `npm --prefix firmware/gateway/web run dev` | Servidor de desarrollo en `http://127.0.0.1:5174` |
| `npm --prefix firmware/gateway/web run preview` | Sirve `dist/` en `http://127.0.0.1:4174` |

Todos los servidores escuchan **solo en loopback** (`127.0.0.1`).

## Pruebas

```bash
npm --prefix firmware/gateway/web test
```

Las pruebas usan el runner de Node (`node --test`) y `preact-render-to-string`:
renderizan la puerta, el formulario y las vistas, y comprueban la **salida
pública** de render y las **transiciones puras** del estado simulado
(desbloqueo, aplicación de configuración con éxito y fallo, cambio y
recuperación de credencial). No consultan el estado interno ni el historial de
Git. Los valores de red aparecen solo en la vista de configuración, marcados
como demostración; la vista de estado sigue sin fijar valores.

Son comprobaciones de render, **no** de navegador: el layout, los viewports y
la navegación con teclado quedan para revisión humana.

## Limitaciones

- Sin dispositivo, sin red y sin API: el código no hace llamadas de red.
- Sin persistencia: el estado vive en memoria y recargar vuelve a cerrar la puerta.
- Comprobaciones estáticas de accesibilidad (controles nativos, foco visible,
  media query móvil); sin verificación visual automatizada: no hay navegador en
  este entorno y la revisión visual queda para el humano.
