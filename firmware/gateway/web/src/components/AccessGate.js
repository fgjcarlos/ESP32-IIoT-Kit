import { h } from 'preact';
import { useState } from 'preact/hooks';

/**
 * Puerta de acceso protegido: una credencial única por equipo. La credencial
 * inicial nunca se pinta aquí: se revela una vez por la consola serie del
 * gateway (ADR-006). El error siempre se acompaña de texto, nunca solo color.
 */
export function AccessGate({ error, onSubmit }) {
  const [credential, setCredential] = useState('');
  return h('main', { class: 'contenido', id: 'contenido' },
    h('section', { class: 'acceso', 'aria-labelledby': 'titulo-acceso' },
      h('h2', { id: 'titulo-acceso' }, 'Primer acceso protegido'),
      h('p', { class: 'meta' }, 'Credencial única del equipo para entrar en el portal.'),
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
        : null,
      error && error !== 'credencial-incorrecta'
        ? h('p', { class: 'error', role: 'alert' }, 'No hay conexión con el equipo: inténtalo de nuevo.')
        : null
      ),
      h('p', { class: 'nota' }, 'Credencial inicial: se muestra una única vez en la consola serie del gateway al arrancar por primera vez. Si se pierde, la recuperación requiere acceso físico al equipo.')
    )
  );
}
