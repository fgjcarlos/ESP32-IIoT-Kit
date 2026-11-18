import { h } from 'preact';
import { StatusCard } from './StatusCard.js';
import { DEMO_STATUSES } from '../data/demo-statuses.js';

/**
 * Vista de estado (desbloqueada): hereda sin cambios el contenido de solo
 * lectura de W01-v0.1. Aquí no hay controles: la edición vive en otras
 * vistas y ninguna comparte esta sección.
 */
export function StatusView() {
  return h('main', { class: 'contenido', id: 'contenido' },
    h('section', { class: 'estado', 'aria-labelledby': 'titulo-estado' },
      h('h2', { id: 'titulo-estado' }, 'Estado de red y acceso local'),
      h('p', { class: 'meta' }, 'Demostración local con datos de ejemplo: no proceden del firmware, no hay dispositivo conectado ni API real.'),
      h('div', { class: 'tarjetas' },
        DEMO_STATUSES.map((status) => h(StatusCard, { key: status.id, status }))),
      h('p', { class: 'nota' }, 'Contexto: AP y STA comparten canal. El equipo fija el canal y la STA debe seguirlo; aquí es un dato informativo, no un campo editable.')
    )
  );
}
