import { h } from 'preact';
import { Header } from './components/Header.js';
import { StatusCard } from './components/StatusCard.js';
import { DEMO_STATUSES } from './data/demo-statuses.js';

/**
 * Panel de estado de red y acceso local (demo local, solo lectura).
 * No hay estado propio ni acciones: la lista de estados es inmutable.
 */
export function App() {
  return h('div', { class: 'app' },
    h(Header, null),
    h('main', { class: 'contenido', id: 'contenido' },
      h('section', { class: 'estado', 'aria-labelledby': 'titulo-estado' },
        h('h2', { id: 'titulo-estado' }, 'Estado de red y acceso local'),
        h('p', { class: 'meta' }, 'Demostración local con datos de ejemplo: no proceden del firmware, no hay dispositivo conectado ni API real.'),
        h('div', { class: 'tarjetas' },
          DEMO_STATUSES.map((status) => h(StatusCard, { key: status.id, status }))),
        h('p', { class: 'nota' }, 'Contexto: AP y STA comparten canal. El equipo fija el canal y la STA debe seguirlo; aquí es un dato informativo, no un campo editable.')
      )
    )
  );
}
