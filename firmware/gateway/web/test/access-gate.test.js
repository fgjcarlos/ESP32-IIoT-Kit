import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { App } from '../src/app.js';
import {
  DEMO_INITIAL_CREDENTIAL,
  createDemoDevice,
  tryUnlock
} from '../src/data/demo-device.js';

// El acceso protegido es la primera vista de W01-v0.2: la demo arranca
// cerrada y la vista de estado solo aparece tras desbloquear.
const html = render(h(App));

test('la demo arranca cerrada: puerta de acceso con campo etiquetado', () => {
  assert.ok(html.includes('<form'));
  assert.ok(/<label[^>]*for="credencial"/.test(html));
  assert.ok(/<input[^>]*id="credencial"[^>]*type="password"/.test(html));
  assert.ok(html.includes('<button'));
});

test('cerrada, la puerta no muestra la vista de estado', () => {
  assert.ok(!html.includes('Estado de red y acceso local'));
  assert.ok((html.match(/<article/g) ?? []).length === 0);
});

test('la puerta explica la credencial de demostracion sin simular un secreto', () => {
  assert.ok(html.includes('Credencial inicial de demostración'));
  assert.ok(html.includes(DEMO_INITIAL_CREDENTIAL));
  assert.ok(!/ssid|contraseña|clave/i.test(html));
});

test('la transicion de desbloqueo es pura y no muta el estado previo', () => {
  const device = createDemoDevice();
  const antes = JSON.stringify(device);
  const fallo = tryUnlock(device, 'no-es-la-credencial');
  assert.equal(fallo.device.unlocked, false);
  assert.equal(fallo.error, 'credencial-incorrecta');
  const exito = tryUnlock(device, DEMO_INITIAL_CREDENTIAL);
  assert.equal(exito.device.unlocked, true);
  assert.equal(exito.error, null);
  assert.equal(JSON.stringify(device), antes);
});

test('la credencial incorrecta acumula intentos y la correcta no', () => {
  const device = createDemoDevice();
  const conFallo = tryUnlock(device, 'no-es-la-credencial').device;
  assert.equal(conFallo.attempts, 1);
  const desbloqueado = tryUnlock(conFallo, DEMO_INITIAL_CREDENTIAL).device;
  assert.equal(desbloqueado.attempts, 1);
  assert.equal(desbloqueado.unlocked, true);
});

test('el estado vive en memoria: crear otro dispositivo vuelve a cerrar', () => {
  const device = tryUnlock(createDemoDevice(), DEMO_INITIAL_CREDENTIAL).device;
  assert.equal(device.unlocked, true);
  assert.equal(createDemoDevice().unlocked, false);
});
