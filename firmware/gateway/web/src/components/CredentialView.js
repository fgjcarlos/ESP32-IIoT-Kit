import { h } from 'preact';
import { useState } from 'preact/hooks';

/** Mensajes en español de cada error que /api/credential puede devolver. */
export const MENSAJES_ERROR = Object.freeze({
  'credencial-incorrecta': 'La credencial actual no coincide: no se cambió nada.',
  'repeticion-no-coincide': 'La credencial nueva y su repetición no coinciden: no se cambió nada.',
  'longitud-fuera-de-rango': 'La credencial nueva no está dentro de la longitud permitida: no se cambió nada.',
  'sin-conexion': 'No hay conexión con el equipo: no se cambió nada.'
});

/**
 * Cambio de la credencial única (S5: propuesta de cambio en el primer uso).
 * Las reglas las decide el servidor: aquí solo se envían los tres valores y
 * se pinta el mensaje que corresponde al código recibido.
 */
export function CredentialView({ inicial, resultado, onChange }) {
  const [actual, setActual] = useState('');
  const [nueva, setNueva] = useState('');
  const [repetir, setRepetir] = useState('');

  const campo = (id, etiqueta, valor, poner) => h('div', { class: 'campo' },
    h('label', { for: id }, etiqueta),
    h('input', { id, type: 'password', value: valor, onInput: (e) => poner(e.target.value), autocomplete: 'current-password' })
  );

  return h('main', { class: 'contenido', id: 'contenido' },
    h('section', { class: 'credenciales', 'aria-labelledby': 'titulo-credencial' },
      h('h2', { id: 'titulo-credencial' }, 'Cambiar credencial'),
      h('p', { class: 'meta' }, 'Credencial única del equipo: la misma que abre este portal.'),
      inicial === true
        ? h('p', { class: 'nota' }, 'Primer uso: cambia la credencial inicial por la tuya antes de seguir usando el equipo.')
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
      resultado?.kind === 'error'
        ? h('p', { class: 'fallo', role: 'alert' }, MENSAJES_ERROR[resultado.code] ?? MENSAJES_ERROR['sin-conexion'])
        : null,
      resultado?.kind === 'exito'
        ? h('p', { class: 'exito', role: 'status' }, 'Credencial cambiada: usa la nueva la próxima vez.')
        : null
      )
    )
  );
}
