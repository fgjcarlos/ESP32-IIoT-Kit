import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { App } from '../src/app.js';

// Las pruebas consultan solo la salida publica de render del panel.
const html = render(h(App));

test('muestra las cuatro tarjetas de estado con su titulo', () => {
  assert.equal((html.match(/<article/g) ?? []).length, 4);
  for (const titulo of ['AP del equipo', 'STA de la red del usuario', 'Acceso local', 'Internet']) {
    assert.ok(html.includes(titulo), `falta la tarjeta «${titulo}»`);
  }
});

test('cada tarjeta declara su estado actual', () => {
  for (const estado of ['Disponible', 'Desconectada · opcional', 'Disponible por AP', 'No necesario']) {
    assert.ok(html.includes(estado), `falta el estado «${estado}»`);
  }
});

test('la cabecera identifica el diseno, la demostracion y la aprobacion pendiente', () => {
  assert.ok(html.includes('W01-v0.1'));
  assert.ok(html.includes('Demostración local'));
  assert.ok(html.includes('Aprobación pendiente'));
  assert.ok(html.includes('<header'));
  assert.ok(html.includes('<main'));
});

test('el panel es de solo lectura: no hay controles de configuracion', () => {
  assert.ok(!/<(input|button|select|textarea|form)\b/.test(html));
});

test('no fija valores de red ni credenciales', () => {
  assert.ok(!/\b\d{1,3}(\.\d{1,3}){3}\b/.test(html));
  assert.ok(!/ssid|password|contraseña|clave/i.test(html));
});

test('el canal compartido se explica como contexto, no como campo editable', () => {
  assert.ok(html.includes('comparten canal'));
  assert.ok(!/<(input|button)\b/.test(html));
});
