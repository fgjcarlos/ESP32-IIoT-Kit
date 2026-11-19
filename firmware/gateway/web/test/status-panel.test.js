import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { StatusView, formatearUptime, tarjetasDeEstado } from '../src/components/StatusView.js';
import { Header } from '../src/components/Header.js';

// Documento de ejemplo con la forma exacta de GET /api/status (W02).
const ESTADO = Object.freeze({
  ap: Object.freeze({ up: true, ssid: 'Gateway-9f2a', ip: '192.168.4.1', channel: 6, mac: '24:6f:28:aa:bb:cc' }),
  sta: Object.freeze({ state: 'idle', ssid: '', ip: '' }),
  uptime_s: 4980,
  portal: Object.freeze({ initial_credential: true })
});

const html = render(h(StatusView, { status: ESTADO, error: null }));

test('muestra las cuatro tarjetas de estado con su titulo', () => {
  assert.equal((html.match(/<article/g) ?? []).length, 4);
  for (const titulo of ['AP del equipo', 'STA de la red del usuario', 'Acceso local', 'Internet']) {
    assert.ok(html.includes(titulo), `falta la tarjeta «${titulo}»`);
  }
});

test('la tarjeta AP muestra el ssid, la ip y el canal reales del equipo', () => {
  assert.ok(html.includes('Gateway-9f2a'), 'falta el SSID real');
  assert.ok(html.includes('192.168.4.1'), 'falta la IP real');
  assert.ok(html.includes('canal 6'), 'falta el canal real');
});

test('cada estado de la STA tiene su etiqueta en espanol', () => {
  const esperadas = {
    idle: 'No configurada · opcional',
    connecting: 'Conectando…',
    connected: 'Conectada',
    disconnected: 'Desconectada · reintento'
  };
  for (const [estado, etiqueta] of Object.entries(esperadas)) {
    const vista = render(h(StatusView, {
      status: { ...ESTADO, sta: { state: estado, ssid: 'MiRed', ip: '10.0.0.5' } },
      error: null
    }));
    assert.ok(vista.includes(etiqueta), `falta la etiqueta «${etiqueta}» para ${estado}`);
  }
});

test('la STA conectada lleva tono ok; el resto, tono neutro', () => {
  const estadoConectado = { ...ESTADO, sta: { state: 'connected', ssid: 'MiRed', ip: '10.0.0.5' } };
  const conectada = tarjetasDeEstado(estadoConectado);
  assert.equal(conectada.find((t) => t.id === 'sta').tone, 'ok');
  assert.ok(render(h(StatusView, { status: estadoConectado, error: null })).includes('MiRed'));
  const inactiva = tarjetasDeEstado(ESTADO);
  assert.equal(inactiva.find((t) => t.id === 'sta').tone, 'neutro');
});

test('el tiempo activo se formatea en horas y minutos', () => {
  assert.equal(formatearUptime(4980), '1 h 23 min');
  assert.equal(formatearUptime(60), '1 min');
  assert.equal(formatearUptime(45), '45 s');
  assert.equal(formatearUptime(0), '0 s');
});

test('el tiempo activo del equipo aparece formateado en la vista', () => {
  assert.ok(html.includes('1 h 23 min'));
});

test('la vista de estado es de solo lectura: sin controles', () => {
  assert.ok(!/<(input|button|select|textarea|form)\b/.test(html));
});

test('un error del estado se comunica con texto, no solo con color', () => {
  const conError = render(h(StatusView, { status: null, error: 'sin-conexion' }));
  assert.ok(conError.includes('role="alert"'));
  assert.ok(conError.includes('estado del equipo'));
});

test('sin datos todavia muestra carga, no tarjetas vacias', () => {
  const cargando = render(h(StatusView, { status: null, error: null }));
  assert.ok(!cargando.includes('<article'));
  assert.ok(cargando.includes('Cargando'));
});

test('la cabecera identifica el portal W02, sin relato de demostracion', () => {
  const cabecera = render(h(Header));
  assert.ok(cabecera.includes('W02'));
  assert.ok(!/demostraci/i.test(cabecera));
  assert.ok(cabecera.includes('<header'));
});

test('el canal compartido sigue explicandose como contexto, no como campo', () => {
  assert.ok(html.includes('comparten canal'));
});
