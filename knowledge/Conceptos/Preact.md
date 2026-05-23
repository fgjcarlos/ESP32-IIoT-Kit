# Preact

#concepto #fase/4

## Que es
Preact es una biblioteca de interfaz de usuario de **3 KB** (minificada y comprimida) que reimplementa la API de React usando el DOM nativo en lugar de un Virtual DOM completo. Ofrece los mismos conceptos (componentes funcionales, hooks, JSX) con una fracción del tamaño.

En el contexto de ESP32-IIoT-Kit, Preact es la biblioteca elegida para construir el dashboard embebido que se sirve desde SPIFFS.

## Por que importa en este proyecto

El dashboard del gateway se almacena en la partición SPIFFS (1,9 MB disponibles). Con React (~40 KB gzipped sin dependencias, >100 KB con react-dom) no queda margen para el resto de la aplicación. Con Preact (~3 KB gzipped) el presupuesto de tamaño es completamente viable:

| Biblioteca | Tamaño gzipped |
|------------|----------------|
| React + ReactDOM | ~40–45 KB |
| Preact | ~3–4 KB |
| Vanilla JS (app compleja) | 10–20 KB (sin mantenimiento) |

Preact permite escribir el dashboard con la ergonomía de React (componentes, estado reactivo, JSX) sin exceder el presupuesto de flash.

## Como funciona

Preact usa el mismo modelo de componentes que React:

```jsx
// Componente funcional con hook de estado
import { h } from 'preact';
import { useState, useEffect } from 'preact/hooks';

export function NodoCard({ nodoId }) {
  const [lectura, setLectura] = useState(null);

  useEffect(() => {
    fetch(`/api/nodes/${nodoId}/readings`)
      .then(r => r.json())
      .then(data => setLectura(data));
  }, [nodoId]);

  return (
    <div class="nodo-card">
      <h3>Nodo {nodoId}</h3>
      {lectura ? <p>Temperatura: {lectura.temp / 10} °C</p> : <p>Cargando...</p>}
    </div>
  );
}
```

### Diferencias clave respecto a React

| Característica | React | Preact |
|---------------|-------|--------|
| Tamaño | ~40 KB | ~3 KB |
| Virtual DOM | Propio (reconciliador Fiber) | Propio simplificado |
| API | `React.createElement`, `ReactDOM.render` | `h`, `render` de preact |
| Hooks | `react` package | `preact/hooks` |
| Compat layer | N/A | `preact/compat` simula API exacta de React |
| JSX pragma | `React` | `h` (o configurar Vite para autodetectar) |

Con `preact/compat` puedes usar librerías de React sin modificarlas, a costa de ~2 KB adicionales.

## Flujo de build con Vite y SPIFFS

1. `npm create preact` → scaffold en `firmware/gateway/web/`
2. `vite build` → genera `dist/` con archivos JS/CSS gzipped
3. Script CMake copia `dist/` a la imagen SPIFFS antes de flashear
4. El gateway sirve los archivos con `httpd_resp_set_hdr("Content-Encoding", "gzip")`

## Gotchas

- Preact no incluye `preact/compat` por defecto; si usas librerías React, añade el alias en `vite.config.js`
- El pragma JSX debe configurarse: en Vite añade `{ "jsxImportSource": "preact" }` en `tsconfig.json`
- Los event handlers usan `class` en lugar de `className` (DOM nativo)
- `useEffect` con array de dependencias vacío (`[]`) se ejecuta solo al montar, igual que React

## Cuándo usar Preact vs alternativas

| Alternativa | Cuándo usarla |
|-------------|--------------|
| **Preact** | Dashboard embebido con lógica reactiva, <1,9 MB SPIFFS |
| **Vanilla JS** | Páginas muy simples, sin estado reactivo complejo |
| **HTMX** | Si prefieres hipermedia sobre API REST; añade ~15 KB |
| **React** | Solo si el bundle puede ir en una CDN (no embebido) |

## Enlaces

- [[WiFi-Modos]] — El gateway sirve el SPA por WiFi AP
- Fase 4: `Fases/fase-04-mqtt-dashboard.md` — Subtarea 4.2
- Tutorial 4: `Tutorial/tutorial-04-mqtt-dashboard.md`
- [Documentación oficial Preact](https://preactjs.com/guide/v10/getting-started)
