import { h } from 'preact';

/**
 * Cabecera del panel: identifica el diseño, el carácter de demostración y
 * la aprobación pendiente. Visible en todas las vistas, cerrada o abierta.
 */
export function Header() {
  return h('header', { class: 'cabecera' },
    h('p', { class: 'kicker' }, 'W01-v0.2 · Demostración local · Aprobación pendiente'),
    h('h1', { class: 'titulo' }, 'Acceso y estado de red del gateway'),
    h('p', { class: 'entradilla' }, 'Panel con datos de demostración: no hay dispositivo conectado, ni llamadas a la red, ni escritura en el firmware.')
  );
}
