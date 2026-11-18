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
porque no hay equipo real). Desbloqueada, muestra cuatro tarjetas de estado,
con datos de ejemplo fijos:

| Tarjeta | Estado mostrado |
|---|---|
| AP del equipo | Disponible |
| STA de la red del usuario | Desconectada · opcional |
| Acceso local | Disponible por AP |
| Internet | No necesario |

Quedan **fuera de esta entrega**: los formularios de configuración de AP y STA
con su aplicación simulada, el cambio de credenciales y la recuperación (cortes
siguientes de la cadena de #68).

No se fija ningún valor de red: no hay SSID, contraseña, dirección IP ni canal,
porque todavía no hay valores aprobados. El canal compartido entre AP y STA se
menciona solo como contexto informativo.

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
renderizan la puerta y la vista de estado y comprueban la **salida pública** de
render (el campo etiquetado de la puerta, las transiciones puras de desbloqueo,
las cuatro tarjetas y sus estados, la ausencia de controles en la vista de
estado y la ausencia de valores de red fijados). No consultan el estado interno
ni el historial de Git.

Son comprobaciones de render, **no** de navegador: el layout, los viewports y
la navegación con teclado quedan para revisión humana.

## Limitaciones

- Sin dispositivo, sin red y sin API.
- Sin persistencia: el estado vive en memoria y recargar vuelve a cerrar la puerta.
- Sin verificación visual automatizada: no hay navegador en este entorno.
