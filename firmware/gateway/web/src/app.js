import { h } from 'preact';
import { useState } from 'preact/hooks';
import { Header } from './components/Header.js';
import { StatusView } from './components/StatusView.js';
import { AccessGate } from './components/AccessGate.js';
import { NavTabs } from './components/NavTabs.js';
import { ConfigView } from './components/ConfigView.js';
import { CredentialView } from './components/CredentialView.js';
import { createDemoDevice, tryUnlock, applyConfig, changeCredential, resetCredential } from './data/demo-device.js';

/**
 * Demo de acceso y estado del gateway (W01-v0.2). El estado del dispositivo
 * vive en memoria: cerrada al arrancar, la vista de estado solo aparece tras
 * desbloquear. Recargar vuelve a cerrar: no hay persistencia.
 */
export function App() {
  const [device, setDevice] = useState(createDemoDevice());
  const [error, setError] = useState(null);
  const [view, setView] = useState('estado');

  function handleSubmit(credential) {
    const result = tryUnlock(device, credential);
    setDevice(result.device);
    setError(result.error);
  }

  function handleApply(borrador, simularFallo) {
    const result = applyConfig(device, borrador, simularFallo);
    setDevice(result.device);
  }

  function handleCredentialChange(actual, nueva, repetir) {
    const result = changeCredential(device, actual, nueva, repetir);
    setDevice(Object.freeze({ ...result.device, credentialError: result.error }));
  }

  return h('div', { class: 'app' },
    h(Header, null),
    device.unlocked
      ? h('div', null,
        h(NavTabs, { view, onChange: setView }),
        view === 'configuracion'
          ? h(ConfigView, { device, onApply: handleApply })
          : view === 'credencial'
            ? h(CredentialView, { device, onChange: handleCredentialChange })
            : h(StatusView, null)
      )
      : h(AccessGate, { error, onSubmit: handleSubmit, onReset: () => setDevice(resetCredential(device)) })
  );
}
