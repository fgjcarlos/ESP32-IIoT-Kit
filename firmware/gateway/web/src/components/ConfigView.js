import { h } from 'preact';
import { useState } from 'preact/hooks';

function Campo({ id, etiqueta, tipo, valor, soloLectura, alCambiar, ayuda }) {
  return h('div', { class: 'campo' },
    h('label', { for: id }, etiqueta),
    h('input', {
      id,
      type: tipo,
      value: valor,
      readonly: soloLectura || null,
      onInput: alCambiar ? (e) => alCambiar(e.target.value) : null
    }),
    ayuda ? h('p', { class: 'ayuda' }, ayuda) : null
  );
}

/**
 * Formulario AP/STA de la demo (alcance aceptado en L27): SSID y claves
 * editables; canal e IP solo lectura porque solo el equipo los fija.
 * La aplicación es simulada, con control explícito de fallo para poder
 * recorrer éxito y error; el fallo preserva la configuración anterior.
 */
export function ConfigView({ device, onApply }) {
  const [borrador, setBorrador] = useState(() => ({ ...device.config }));
  const [simularFallo, setSimularFallo] = useState(false);

  function poner(campo) {
    return (valor) => setBorrador({ ...borrador, [campo]: valor });
  }

  return h('main', { class: 'contenido', id: 'contenido' },
    h('section', { class: 'configuracion', 'aria-labelledby': 'titulo-config' },
      h('h2', { id: 'titulo-config' }, 'Configuración AP y STA'),
      h('p', { class: 'meta' }, 'Formulario con datos de demostración: no hay dispositivo, la aplicación es simulada y nada se escribe en el firmware.'),
      h('form', {
        class: 'formulario',
        onSubmit: (e) => {
          e.preventDefault();
          onApply(borrador, simularFallo);
        }
      },
      h('fieldset', null,
        h('legend', null, 'AP del equipo'),
        h(Campo, { id: 'ap-ssid', etiqueta: 'SSID del AP', tipo: 'text', valor: borrador.apSsid, alCambiar: poner('apSsid') }),
        h(Campo, { id: 'ap-clave', etiqueta: 'Clave del AP', tipo: 'password', valor: borrador.apClave, alCambiar: poner('apClave') })
      ),
      h('fieldset', null,
        h('legend', null, 'STA de la red del usuario (opcional)'),
        h(Campo, { id: 'sta-ssid', etiqueta: 'SSID de la red', tipo: 'text', valor: borrador.staSsid, alCambiar: poner('staSsid') }),
        h(Campo, { id: 'sta-clave', etiqueta: 'Clave de la red', tipo: 'password', valor: borrador.staClave, alCambiar: poner('staClave') })
      ),
      h('fieldset', null,
        h('legend', null, 'Fijados por el equipo (solo el equipo los fija)'),
        h(Campo, {
          id: 'canal', etiqueta: 'Canal (solo lectura)', tipo: 'text', valor: borrador.canal, soloLectura: true,
          ayuda: 'AP y STA comparten canal: el equipo lo fija y no se edita aquí.'
        }),
        h(Campo, {
          id: 'ip', etiqueta: 'Dirección IP (solo lectura)', tipo: 'text', valor: borrador.ip, soloLectura: true,
          ayuda: 'La dirección la indica el equipo; aquí es un valor de demostración.'
        })
      ),
      h('div', { class: 'accion-demo' },
        h('input', {
          id: 'simular-fallo', type: 'checkbox',
          checked: simularFallo || null,
          onChange: (e) => setSimularFallo(e.target.checked)
        }),
        h('label', { for: 'simular-fallo' }, 'Simular fallo de aplicación (demo)')
      ),
      h('button', { type: 'submit' }, 'Aplicar configuración'),
      device.lastApply && device.lastApply.status === 'success'
        ? h('p', { class: 'exito', role: 'status' }, 'Configuración aplicada (simulado): los valores editados pasan a estar activos en la demo.')
        : null,
      device.lastApply && device.lastApply.status === 'error'
        ? h('p', { class: 'fallo', role: 'alert' }, 'No se aplicó: fallo simulado. La configuración anterior sigue activa; puedes corregir y volver a aplicar.')
        : null
      )
    )
  );
}
