import { h } from 'preact';
import { useState } from 'preact/hooks';
import { DEMO_INITIAL_CREDENTIAL } from '../data/demo-device.js';

/**
 * Puerta de primer acceso: una credencial única por dispositivo (S5),
 * simulada. El error siempre se acompaña de texto, nunca solo de color.
 * Ofrece la recuperación de acceso simulada (reentrar por el AP del equipo).
 */
export function AccessGate({ error, onSubmit, onReset }) {
  const [credential, setCredential] = useState('');
  return h('main', { class: 'contenido', id: 'contenido' },
    h('section', { class: 'acceso', 'aria-labelledby': 'titulo-acceso' },
      h('h2', { id: 'titulo-acceso' }, 'Primer acceso protegido'),
      h('p', { class: 'meta' }, 'Credencial única del equipo, como en el dispositivo real; aquí es una simulación sin dispositivo ni credencial real.'),
      h('form', {
        class: 'puerta',
        onSubmit: (e) => {
          e.preventDefault();
          onSubmit(credential);
        }
      },
      h('label', { for: 'credencial' }, 'Credencial del equipo'),
      h('input', {
        id: 'credencial',
        type: 'password',
        value: credential,
        onInput: (e) => setCredential(e.target.value),
        autocomplete: 'current-password'
      }),
      h('button', { type: 'submit' }, 'Entrar'),
      error === 'credencial-incorrecta'
        ? h('p', { class: 'error', role: 'alert' }, 'Credencial incorrecta: no coincide con la del equipo.')
        : null
      ),
      h('p', { class: 'nota' }, `Credencial inicial de demostración: ${DEMO_INITIAL_CREDENTIAL}. En el equipo real viene entregada con él y no se muestra en pantalla.`),
      h('details', { class: 'recuperacion' },
        h('summary', null, 'Recuperar acceso'),
        h('p', { class: 'ayuda' }, 'Sin credencial se reentra por el AP del equipo, como en el dispositivo real. En esta demo el reinicio restaura la credencial inicial y no toca la configuración.'),
        h('button', { type: 'button', onClick: onReset }, 'Restablecer credencial de demostración')
      )
    )
  );
}
