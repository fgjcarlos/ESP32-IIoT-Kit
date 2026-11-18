import { test } from 'node:test';
import assert from 'node:assert/strict';
import { h } from 'preact';
import { render } from 'preact-render-to-string';
import { ConfigView } from '../src/components/ConfigView.js';
import { NavTabs } from '../src/components/NavTabs.js';
import {
  createDemoDevice,
  createDemoConfig,
  applyConfig
} from '../src/data/demo-device.js';

const html = render(h(ConfigView, { device: createDemoDevice(), onApply: () => {} }));

test('el formulario edita SSID y claves AP/STA (alcance aceptado en L27)', () => {
  for (const id of ['ap-ssid', 'ap-clave', 'sta-ssid', 'sta-clave']) {
    assert.ok(html.includes(`id="${id}"`), `falta el campo «${id}»`);
  }
  assert.ok(/<input[^>]*id="ap-ssid"[^>]*type="text"/.test(html) || /<input[^>]*type="text"[^>]*id="ap-ssid"/.test(html));
  assert.ok(/id="ap-clave"[\s\S]{0,80}type="password"|type="password"[\s\S]{0,80}id="ap-clave"/.test(html));
});

test('canal e IP son solo lectura: los fija el equipo', () => {
  assert.ok(/id="canal"[^>]*readonly|readonly[^>]*id="canal"/.test(html));
  assert.ok(/id="ip"[^>]*readonly|readonly[^>]*id="ip"/.test(html));
  assert.ok(html.includes('solo el equipo los fija'));
});

test('la demo ofrece control explicito de fallo y aplicacion', () => {
  assert.ok(/id="simular-fallo"/.test(html));
  assert.ok(html.includes('Simular fallo'));
  assert.ok(html.includes('Aplicar configuración'));
});

test('la aplicacion con exito actualiza la configuracion', () => {
  const device = createDemoDevice();
  const { device: nuevo, error } = applyConfig(device, { staSsid: 'Red-Demo' }, false);
  assert.equal(error, null);
  assert.equal(nuevo.config.staSsid, 'Red-Demo');
  assert.equal(nuevo.lastApply.status, 'success');
  assert.equal(JSON.stringify(device), JSON.stringify(createDemoDevice()));
});

test('el fallo simulado preserva la configuracion anterior (recuperacion)', () => {
  const device = createDemoDevice();
  const { device: nuevo, error } = applyConfig(device, { apSsid: 'Cambio' }, true);
  assert.equal(error, 'fallo-aplicacion');
  assert.equal(nuevo.lastApply.status, 'error');
  assert.equal(JSON.stringify(nuevo.config), JSON.stringify(createDemoConfig()));
});

test('la navegacion ofrece estado y configuracion sin perder el foco visible', () => {
  const nav = render(h(NavTabs, { view: 'estado' }));
  assert.ok(nav.includes('Estado de red'));
  assert.ok(nav.includes('Configuración'));
  assert.ok(/aria-current="page"/.test(nav));
  assert.ok(/<button[\s\S]{0,120}aria-current/.test(nav));
});

test('el formulario marca su caracter de demostracion', () => {
  assert.ok(html.includes('datos de demostración'));
});
