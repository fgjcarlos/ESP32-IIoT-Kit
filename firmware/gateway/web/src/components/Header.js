import { h } from 'preact';

/**
 * Cabecera del portal: identifica la entrega (W02) y su carácter de servicio
 * local. Visible en todas las vistas, cerrada o abierta.
 */
export function Header() {
  return h('header', { class: 'cabecera' },
    h('p', { class: 'kicker' }, 'W02 · Portal local del gateway'),
    h('h1', { class: 'titulo' }, 'Acceso y estado de red del gateway'),
    h('p', { class: 'entradilla' }, 'Panel servido por el propio equipo en su red local: funciona sin Internet, sin cuentas y sin servicios externos.')
  );
}
