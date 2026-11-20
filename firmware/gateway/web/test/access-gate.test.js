import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { AccessGate } from '../src/components/AccessGate.js';

const html = render(h(AccessGate, { error: null, onSubmit: () => {} }));

test('la puerta pide la credencial con campo etiquetado y boton', () => {
  assert.ok(html.includes('<form'));
  assert.ok(/<label[^>]*for="credencial"/.test(html));
  assert.ok(
    /<input[^>]*id="credencial"[^>]*type="password"/.test(html) ||
    /<input[^>]*type="password"[^>]*id="credencial"/.test(html)
  );
  assert.ok(html.includes('<button'));
});

test('no muestra ninguna credencial en pantalla: apunta a la consola serie', () => {
  assert.ok(!html.includes('acceso-inicial'));
  assert.ok(html.includes('consola serie'));
  assert.ok(!/demostraci/i.test(html));
});

test('la credencial incorrecta se explica con texto y rol de alerta', () => {
  const conError = render(h(AccessGate, { error: 'credencial-incorrecta', onSubmit: () => {} }));
  assert.ok(conError.includes('Credencial incorrecta'));
  assert.ok(conError.includes('role="alert"'));
});

test('un fallo de conexion en la puerta se comunica, sin quedar mudo', () => {
  const sinRed = render(h(AccessGate, { error: 'sin-conexion', onSubmit: () => {} }));
  assert.ok(sinRed.includes('role="alert"'));
  assert.ok(sinRed.includes('No hay conexión'));
});

test('no queda ningun boton de restablecer de la demo', () => {
  assert.ok(!html.includes('Restablecer'));
  assert.ok(!/acceso-inicial|resetear/i.test(html));
});
