import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { CredentialView } from '../src/components/CredentialView.js';
import { AccessGate } from '../src/components/AccessGate.js';
import {
  DEMO_INITIAL_CREDENTIAL,
  createDemoDevice,
  changeCredential,
  resetCredential,
  tryUnlock
} from '../src/data/demo-device.js';

const html = render(h(CredentialView, { device: createDemoDevice(), onChange: () => {} }));
const puerta = render(h(AccessGate, { error: null, onSubmit: () => {}, onReset: () => {} }));

test('el formulario de credencial pide actual, nueva y repeticion', () => {
  for (const id of ['credencial-actual', 'credencial-nueva', 'credencial-repetir']) {
    assert.ok(html.includes(`id="${id}"`), `falta el campo «${id}»`);
  }
  assert.ok(html.includes('Cambiar credencial'));
});

test('propone el cambio en el primer uso mientras dura la inicial (S5)', () => {
  assert.ok(html.includes('primer uso'));
  const cambiada = render(h(CredentialView, {
    device: { ...createDemoDevice(), credential: 'otra' },
    onChange: () => {}
  }));
  assert.ok(!cambiada.includes('primer uso'));
});

test('cambiar con exito actualiza la credencial y la anterior deja de abrir', () => {
  const trasCambio = changeCredential(createDemoDevice(), DEMO_INITIAL_CREDENTIAL, 'nueva-demo', 'nueva-demo').device;
  assert.equal(trasCambio.credential, 'nueva-demo');
  const sesionNueva = { ...trasCambio, unlocked: false };
  assert.equal(tryUnlock(sesionNueva, 'nueva-demo').device.unlocked, true);
  assert.equal(tryUnlock(sesionNueva, DEMO_INITIAL_CREDENTIAL).device.unlocked, false);
  assert.equal(tryUnlock(sesionNueva, DEMO_INITIAL_CREDENTIAL).error, 'credencial-incorrecta');
});

test('la credencial actual incorrecta no cambia nada', () => {
  const device = createDemoDevice();
  const { device: nuevo, error } = changeCredential(device, 'equivocada', 'nueva', 'nueva');
  assert.equal(error, 'credencial-actual-incorrecta');
  assert.equal(JSON.stringify(nuevo), JSON.stringify(device));
});

test('la repeticion que no coincide no cambia nada', () => {
  const device = createDemoDevice();
  const { device: nuevo, error } = changeCredential(device, DEMO_INITIAL_CREDENTIAL, 'nueva', 'otra');
  assert.equal(error, 'no-coinciden');
  assert.equal(JSON.stringify(nuevo), JSON.stringify(device));
});

test('la puerta cerrada ofrece recuperacion via AP del equipo', () => {
  assert.ok(puerta.includes('Recuperar acceso'));
  assert.ok(puerta.includes('AP del equipo'));
});

test('resetear restaura la credencial inicial y los intentos', () => {
  const conCambio = changeCredential(createDemoDevice(), DEMO_INITIAL_CREDENTIAL, 'nueva', 'nueva').device;
  const fallidos = tryUnlock(conCambio, 'incorrecta').device;
  const reseteado = resetCredential(fallidos);
  assert.equal(reseteado.credential, DEMO_INITIAL_CREDENTIAL);
  assert.equal(reseteado.attempts, 0);
  assert.equal(JSON.stringify(fallidos.config), JSON.stringify(reseteado.config));
});
