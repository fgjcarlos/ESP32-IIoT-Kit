import { h } from 'preact';
import { useState } from 'preact/hooks';
import { DEMO_INITIAL_CREDENTIAL } from '../data/demo-device.js';

/**
 * Cambio de credencial del equipo (S5: credencial única, con propuesta de
 * cambio en el primer uso). Todo simulado; los errores llevan texto.
 */
export function CredentialView({ device, onChange }) {
  const [actual, setActual] = useState('');
  const [nueva, setNueva] = useState('');
  const [repetir, setRepetir] = useState('');
  const primeraVez = device.credential === DEMO_INITIAL_CREDENTIAL;

  const campo = (id, etiqueta, valor, poner) => h('div', { class: 'campo' },
    h('label', { for: id }, etiqueta),
    h('input', { id, type: 'password', value: valor, onInput: (e) => poner(e.target.value), autocomplete: 'current-password' })
  );

  return h('main', { class: 'contenido', id: 'contenido' },
    h('section', { class: 'credenciales', 'aria-labelledby': 'titulo-credencial' },
      h('h2', { id: 'titulo-credencial' }, 'Cambiar credencial'),
      h('p', { class: 'meta' }, 'Credencial única del equipo, simulada: no hay dispositivo y nada se escribe en el firmware.'),
      primeraVez
        ? h('p', { class: 'nota' }, 'Propuesta para el primer uso: cambia la credencial inicial por la tuya antes de usar el equipo de verdad.')
        : null,
      h('form', {
        class: 'formulario',
        onSubmit: (e) => {
          e.preventDefault();
          onChange(actual, nueva, repetir);
        }
      },
      campo('credencial-actual', 'Credencial actual', actual, setActual),
      campo('credencial-nueva', 'Credencial nueva', nueva, setNueva),
      campo('credencial-repetir', 'Repite la credencial nueva', repetir, setRepetir),
      h('button', { type: 'submit' }, 'Guardar credencial'),
      device.credentialError === 'credencial-actual-incorrecta'
        ? h('p', { class: 'fallo', role: 'alert' }, 'La credencial actual no coincide: no se cambió nada.')
        : null,
      device.credentialError === 'no-coinciden'
        ? h('p', { class: 'fallo', role: 'alert' }, 'La credencial nueva y su repetición no coinciden: no se cambió nada.')
        : null,
      device.credentialError === null && device.credentialChanged
        ? h('p', { class: 'exito', role: 'status' }, 'Credencial cambiada (simulado): usa la nueva la próxima vez.')
        : null
      )
    )
  );
}
