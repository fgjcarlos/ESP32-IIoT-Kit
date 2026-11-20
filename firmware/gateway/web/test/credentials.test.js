import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { CredentialView } from '../src/components/CredentialView.js';

const base = { inicial: false, resultado: null, onChange: () => {} };
const html = render(h(CredentialView, base));

test('el formulario pide actual, nueva y repeticion', () => {
  for (const id of ['credencial-actual', 'credencial-nueva', 'credencial-repetir']) {
    assert.ok(html.includes(`id="${id}"`), `falta el campo «${id}»`);
  }
  assert.ok(html.includes('Cambiar credencial'));
});

test('la propuesta de primer uso depende del indicador del servidor', () => {
  const primera = render(h(CredentialView, { ...base, inicial: true }));
  assert.ok(primera.includes('Primer uso'));
  assert.ok(!html.includes('Primer uso'));
});

test('cada error del servidor se explica con su mensaje en espanol', () => {
  const mensajes = {
    'credencial-incorrecta': 'La credencial actual no coincide',
    'repeticion-no-coincide': 'no coinciden',
    'longitud-fuera-de-rango': 'longitud',
    'sin-conexion': 'No hay conexión'
  };
  for (const [code, fragmento] of Object.entries(mensajes)) {
    const conError = render(h(CredentialView, { ...base, resultado: { kind: 'error', code } }));
    assert.ok(conError.includes(fragmento), `falta el mensaje para «${code}»`);
    assert.ok(conError.includes('role="alert"'));
  }
});

test('el exito confirma el cambio y no muestra error', () => {
  const exito = render(h(CredentialView, { ...base, resultado: { kind: 'exito' } }));
  assert.ok(exito.includes('role="status"'));
  assert.ok(exito.includes('Credencial cambiada'));
  assert.ok(!exito.includes('role="alert"'));
});

test('la copia no promete simulacion: las reglas las decide el servidor', () => {
  assert.ok(!/simulad/i.test(html));
});
