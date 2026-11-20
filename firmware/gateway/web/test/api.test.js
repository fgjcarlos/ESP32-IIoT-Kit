import { test } from 'node:test';
import assert from 'node:assert/strict';
import * as api from '../src/api.js';

// fetch simulado: cada prueba instala la respuesta que necesita y comprueba
// la petición pública (ruta, método, cookies, cuerpo JSON), no el interior.
const peticiones = [];
function instalarFetch(respuesta) {
  peticiones.length = 0;
  globalThis.fetch = (ruta, opciones) => {
    peticiones.push({ ruta, opciones });
    return Promise.resolve(respuesta);
  };
}
const json = (cuerpo, ok = true) => ({ ok, json: () => Promise.resolve(cuerpo) });

test('login con exito devuelve el JSON y envia credencial y cookies same-origin', async () => {
  instalarFetch(json({ ok: true }));
  assert.deepEqual(await api.login('la-credencial'), { ok: true });
  const { ruta, opciones } = peticiones[0];
  assert.equal(ruta, '/api/login');
  assert.equal(opciones.method, 'POST');
  assert.equal(opciones.credentials, 'same-origin');
  assert.deepEqual(JSON.parse(opciones.body), { credential: 'la-credencial' });
  assert.equal(opciones.headers['Content-Type'], 'application/json');
});

test('la clave error del servidor se propaga como {code}', async () => {
  instalarFetch(json({ error: 'credencial-incorrecta' }, false));
  await assert.rejects(() => api.login('mala'), (fallo) => {
    assert.deepEqual(fallo, { code: 'credencial-incorrecta' });
    return true;
  });
});

test('un fallo de red se reporta como sin-conexion', async () => {
  globalThis.fetch = () => Promise.reject(new Error('red caída'));
  await assert.rejects(() => api.status(), { code: 'sin-conexion' });
});

test('una respuesta que no es JSON se reporta como sin-conexion', async () => {
  instalarFetch({ ok: false, json: () => Promise.reject(new Error('html')) });
  await assert.rejects(() => api.status(), { code: 'sin-conexion' });
});

test('status pide GET /api/status con cookies same-origin', async () => {
  instalarFetch(json({ ap: {}, sta: {}, uptime_s: 1, portal: {} }));
  await api.status();
  const { ruta, opciones } = peticiones[0];
  assert.equal(ruta, '/api/status');
  assert.equal(opciones.method, 'GET');
  assert.equal(opciones.credentials, 'same-origin');
  assert.equal(opciones.body, undefined);
});

test('changeCredential envia current/new/repeat y propaga el error del servidor', async () => {
  instalarFetch(json({ error: 'repeticion-no-coincide' }, false));
  await assert.rejects(() => api.changeCredential('a', 'b', 'c'), { code: 'repeticion-no-coincide' });
  assert.deepEqual(JSON.parse(peticiones[0].opciones.body), { current: 'a', new: 'b', repeat: 'c' });
});

test('logout hace POST y devuelve el JSON', async () => {
  instalarFetch(json({ ok: true }));
  assert.deepEqual(await api.logout(), { ok: true });
  assert.equal(peticiones[0].ruta, '/api/logout');
  assert.equal(peticiones[0].opciones.method, 'POST');
});
