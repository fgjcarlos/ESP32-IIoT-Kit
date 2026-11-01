# Base visual provisional para wireframes

**Propuesta de trabajo, no identidad visual aprobada del gateway.** Revisión: W01-v0.1.

## Sincronización de fuentes

- `site/assets/stylesheets/main.484c7ddc.min.css`: tema generado por Material para MkDocs; excluido de la UI del gateway.
- `mkdocs.yml`: configura la documentación, no el producto.
- `firmware/gateway/web/`: no existe todavía; no hay tokens de UI que extraer.
- `firmware/gateway/components/brand_profile/README.md`: la marca es configurable; esta propuesta no fija su identidad ni sustituye perfiles.

Valores extraídos del producto: **0**. Los siguientes tokens son una propuesta neutra para comparar recorridos; requieren aceptación junto con el diseño, no describen firmware entregado.

## Tokens propuestos

| Grupo | Token | Valor | Uso |
|---|---|---|---|
| Color | `--paper` | `#ffffff` | Superficie |
| Color | `--canvas` | `#f3f4f6` | Fondo |
| Color | `--ink` | `#111827` | Texto y acción principal |
| Color | `--muted` | `#374151` | Texto secundario |
| Color | `--line` | `#6b7280` | Bordes visibles |
| Color | `--focus` | `#1d4ed8` | Foco |
| Color | `--error` | `#991b1b` | Errores, siempre con texto |
| Color | `--success` | `#166534` | Éxito, siempre con texto |
| Tipografía | fuente | `system-ui, sans-serif` | Todo el texto; sin descargas |
| Tipografía | escala | `0.875rem`, `1rem`, `1.25rem`, `1.75rem`, `2rem` | Auxiliar, cuerpo, subtítulos, títulos |
| Espaciado | escala | `0.25rem`, `0.5rem`, `0.75rem`, `1rem`, `1.5rem`, `2rem` | Ritmo |
| Radio | esquinas | `0`, `0.5rem` | Controles y tarjetas |
| Sombra | ninguna | `none` | Wireframes sin decoración |

Fuente de todos los valores propuestos: este documento; no proceden de MkDocs ni de una marca aprobada.

## Reglas de componentes

- HTML autónomo, CSS/JS/SVG en línea; sin red, fuentes, imágenes o librerías externas.
- Navegación usable con teclado, foco visible, etiquetas explícitas y regiones semánticas.
- Texto normal con contraste AA; botones con altura mínima 44px. Éxito/error no dependen solo del color.
- Unidades fluidas y media query para móvil; sin animaciones necesarias.
- Toda pantalla identifica W01-v0.1, simulación y aprobación pendiente.
- No se aceptan credenciales reales ni se persiste información; controles «Simular» para los escenarios.
- Las tres alternativas cambian la estructura, no la paleta: asistente, panel, lista de puesta en marcha.
