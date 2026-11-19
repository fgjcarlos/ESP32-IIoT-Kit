import { h } from 'preact';
import { StatusCard } from './StatusCard.js';

/** Etiquetas en español de cada estado STA que puede devolver /api/status. */
export const ETIQUETAS_STA = Object.freeze({
  idle: 'No configurada · opcional',
  connecting: 'Conectando…',
  connected: 'Conectada',
  disconnected: 'Desconectada · reintento'
});

/** Formatea segundos de /api/status como «1 h 23 min» (pura). */
export function formatearUptime(segundos) {
  const minutos = Math.floor(segundos / 60);
  const horas = Math.floor(minutos / 60);
  if (horas > 0) return `${horas} h ${minutos % 60} min`;
  if (minutos > 0) return `${minutos} min`;
  return `${segundos} s`;
}

/** Mapea el documento de /api/status a las cuatro tarjetas aprobadas (pura). */
export function tarjetasDeEstado(status) {
  const { ap, sta } = status;
  const conectada = sta.state === 'connected';
  return [
    Object.freeze({
      id: 'ap',
      title: 'AP del equipo',
      state: ap.up ? 'Disponible' : 'No disponible',
      tone: ap.up ? 'ok' : 'neutro',
      detail: `Red propia del equipo para el acceso y la recuperación: ${ap.ssid} · ${ap.ip} · canal ${ap.channel}.`
    }),
    Object.freeze({
      id: 'sta',
      title: 'STA de la red del usuario',
      state: ETIQUETAS_STA[sta.state] ?? sta.state,
      tone: conectada ? 'ok' : 'neutro',
      detail: conectada
        ? `Conectada a ${sta.ssid} · ${sta.ip}.`
        : 'Es opcional: el equipo sigue siendo accesible por el AP local aunque no se una a ninguna red.'
    }),
    Object.freeze({
      id: 'acceso',
      title: 'Acceso local',
      state: 'Disponible por AP',
      tone: 'ok',
      detail: `El acceso no necesita Internet: desde la red del equipo, abre http://${ap.ip}.`
    }),
    Object.freeze({
      id: 'internet',
      title: 'Internet',
      state: 'No necesario',
      tone: 'neutro',
      detail: 'Todo el recorrido es local: no hay cuentas, servicios remotos ni actualizaciones en la nube.'
    })
  ];
}

/**
 * Vista de estado (desbloqueada): las cuatro tarjetas de W01-v0.2 con los
 * datos en vivo de /api/status. Solo lectura: la configuración no está en W02.
 */
export function StatusView({ status, error }) {
  return h('main', { class: 'contenido', id: 'contenido' },
    h('section', { class: 'estado', 'aria-labelledby': 'titulo-estado' },
      h('h2', { id: 'titulo-estado' }, 'Estado de red y acceso local'),
      h('p', { class: 'meta' }, status
        ? `Datos en vivo del equipo · activo desde hace ${formatearUptime(status.uptime_s)}.`
        : 'Datos en vivo del equipo, servidos por el propio gateway.'),
      error
        ? h('p', { class: 'fallo', role: 'alert' }, 'No se pudo leer el estado del equipo: comprueba la conexión y vuelve a entrar.')
        : null,
      status
        ? h('div', { class: 'tarjetas' },
          tarjetasDeEstado(status).map((tarjeta) => h(StatusCard, { key: tarjeta.id, status: tarjeta })))
        : h('p', { class: 'nota' }, 'Cargando el estado del equipo…'),
      h('p', { class: 'nota' }, 'Contexto: AP y STA comparten canal. El equipo fija el canal y la STA debe seguirlo; es un dato informativo, no un campo editable.')
    )
  );
}
