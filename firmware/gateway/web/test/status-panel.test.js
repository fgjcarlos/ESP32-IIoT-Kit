import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { App } from '../src/app.js';
import { Header } from '../src/components/Header.js';
import { StatusView } from '../src/components/StatusView.js';

// La vista de estado (desbloqueada) conserva las garantias de W01-v0.1.
const html = render(h(StatusView));
const cabecera = render(h(App));

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
  assert.ok(cabecera.includes('W01-v0.2'));
  assert.ok(cabecera.includes('Demostración local'));
  assert.ok(cabecera.includes('Aprobación pendiente'));
  assert.ok(cabecera.includes('<header'));
  assert.ok(html.includes('<main'));
});

test('la vista de estado es de solo lectura: no hay controles de configuracion', () => {
  assert.ok(!/<(input|button|select|textarea|form)\b/.test(html));
});

test('la vista de estado no fija valores de red ni credenciales', () => {
  assert.ok(!/\b\d{1,3}(\.\d{1,3}){3}\b/.test(html));
  assert.ok(!/ssid|password|contraseña|clave/i.test(html));
});

test('el canal compartido se explica como contexto, no como campo editable', () => {
  assert.ok(html.includes('comparten canal'));
  assert.ok(!/<(input|button)\b/.test(html));
});

test('la cabecera conserva la exclusion de red y escritura', () => {
  assert.ok(cabecera.includes('no hay dispositivo conectado'));
});
