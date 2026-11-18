import { h } from 'preact';

/**
 * Tarjeta de estado reutilizable: título, estado y detalle.
 * El estado siempre acompaña a texto, para no depender solo del color.
 */
export function StatusCard({ status }) {
  return h('article', { class: 'tarjeta' },
    h('h3', { class: 'tarjeta-titulo' }, status.title),
    h('p', { class: 'pilula', 'data-clase': status.tone }, status.state),
    h('p', { class: 'detalle' }, status.detail)
  );
}
