import { h } from 'preact';

/**
 * Cabecera del panel: identifica el diseño, el carácter de demostración y el
 * alcance de solo lectura. Sin navegación: esta entrega no tiene vistas.
 */
export function Header() {
  return h('header', { class: 'cabecera' },
    h('p', { class: 'kicker' }, 'W01-v0.1 · Demostración local · Aprobación pendiente'),
    h('h1', { class: 'titulo' }, 'Estado de red y acceso local del gateway'),
    h('p', { class: 'entradilla' }, 'Panel de solo lectura con datos de demostración: no hay dispositivo conectado, ni llamadas a la red, ni escritura en el firmware.')
  );
}
