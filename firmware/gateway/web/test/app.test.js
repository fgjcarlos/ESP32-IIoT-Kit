import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { App, Portal, manejarErrorDeEstado } from '../src/app.js';

// La sesión vive en RAM: la app arranca cerrada y solo login la abre.
const cerrada = render(h(App));

test('la app arranca cerrada: puerta de acceso sin pestanas ni tarjetas', () => {
  assert.ok(/<label[^>]*for="credencial"/.test(cerrada));
  assert.ok(!cerrada.includes('aria-label="Vistas'));
  assert.ok(!cerrada.includes('<article'));
});

const abiertaProps = {
  sesion: { unlocked: true, error: null },
  view: 'estado',
  status: null,
  errorDeEstado: null,
  resultadoCredencial: null,
  onSubmit: () => {},
  onView: () => {},
  onCredentialChange: () => {}
};
const abierta = render(h(Portal, abiertaProps));

test('desbloqueada muestra solo las pestanas Estado y Credencial (W02)', () => {
  assert.ok(abierta.includes('Estado de red'));
  assert.ok(abierta.includes('Credencial'));
  assert.ok(!abierta.includes('Configuración'), 'la configuracion se activa en W03');
  assert.ok(/<button[\s\S]{0,200}aria-current="page"/.test(abierta));
});

test('la vista de credencial se alcanza desde las pestanas', () => {
  const conCredencial = render(h(Portal, { ...abiertaProps, view: 'credencial' }));
  assert.ok(conCredencial.includes('Cambiar credencial'));
});

test('un 401 del estado cierra la sesion; el resto se muestra como error', () => {
  assert.deepEqual(manejarErrorDeEstado('no-autorizado'), { cerrarSesion: true });
  assert.deepEqual(manejarErrorDeEstado('sin-conexion'), { cerrarSesion: false, errorDeEstado: 'sin-conexion' });
});
