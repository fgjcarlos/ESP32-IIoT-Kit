import { h } from 'preact';

/**
 * Navegación entre vistas de la demo abierta. Botones reales (no enlaces):
 * sin router, el estado de vista vive en App. aria-current marca la activa.
 */
export function NavTabs({ view, onChange }) {
  return h('nav', { class: 'vistas', 'aria-label': 'Vistas de la demo' },
    h('button', {
      type: 'button',
      class: 'vista',
      'aria-current': view === 'estado' ? 'page' : null,
      onClick: () => onChange('estado')
    }, 'Estado de red'),
    h('button', {
      type: 'button',
      class: 'vista',
      'aria-current': view === 'configuracion' ? 'page' : null,
      onClick: () => onChange('configuracion')
    }, 'Configuración')
  );
}
