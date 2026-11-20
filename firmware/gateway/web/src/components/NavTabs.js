import { h } from 'preact';

/**
 * Navegación entre vistas del portal abierto. Botones reales (no enlaces):
 * sin router, el estado de vista vive en App. aria-current marca la activa.
 * La configuración Wi-Fi no se ofrece en W02: W03 la reactiva.
 */
export function NavTabs({ view, onChange }) {
  return h('nav', { class: 'vistas', 'aria-label': 'Vistas del portal' },
    h('button', {
      type: 'button',
      class: 'vista',
      'aria-current': view === 'estado' ? 'page' : null,
      onClick: () => onChange('estado')
    }, 'Estado de red'),
    h('button', {
      type: 'button',
      class: 'vista',
      'aria-current': view === 'credencial' ? 'page' : null,
      onClick: () => onChange('credencial')
    }, 'Credencial')
  );
}
