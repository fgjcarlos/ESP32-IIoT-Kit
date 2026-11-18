# Estado de red y acceso local — demo local (W01-v0.1)

Aplicación **Preact + Vite** de **solo lectura** que muestra el estado de red y
de acceso local del gateway con **datos de demostración**.

**No es el dashboard del producto.** No hay API, ni servidor, ni firmware, ni
SPIFFS, ni flasheo, ni autenticación, ni persistencia: el panel no produce
ningún efecto sobre el dispositivo y no escribe nada.

- Versión del diseño: `W01-v0.1` (propuesta, **aprobación pendiente**).
- Tokens visuales: [`design/system.md`](../../../design/system.md) (base visual provisional).
- Tarea ODD: `odd/tasks/issue-68-web-wireframes.md` (T11).

## Alcance de esta entrega

Cuatro tarjetas de estado, con datos de ejemplo fijos:

| Tarjeta | Estado mostrado |
|---|---|
| AP del equipo | Disponible |
| STA de la red del usuario | Desconectada · opcional |
| Acceso local | Disponible por AP |
| Internet | No necesario |

Quedan **fuera de esta entrega**: los formularios de configuración de AP y STA,
el cambio de credenciales, los escenarios de error y recuperación, y el panel de
operaciones completo.

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
renderizan el panel y comprueban la **salida pública** de render (las cuatro
tarjetas y sus estados, la ausencia de controles de configuración y la ausencia
de valores de red fijados). No consultan el estado interno ni el historial de
Git.

Son comprobaciones de render, **no** de navegador: el layout, los viewports y
la navegación con teclado quedan para revisión humana.

## Limitaciones

- Sin dispositivo, sin red y sin API.
- Sin persistencia ni estado propio: la lista de estados es inmutable.
- Sin verificación visual automatizada: no hay navegador en este entorno.
