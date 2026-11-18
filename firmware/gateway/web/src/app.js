import { h } from 'preact';
import { useState } from 'preact/hooks';
import { Header } from './components/Header.js';
import { StatusView } from './components/StatusView.js';
import { AccessGate } from './components/AccessGate.js';
import { createDemoDevice, tryUnlock } from './data/demo-device.js';

/**
 * Demo de acceso y estado del gateway (W01-v0.2). El estado del dispositivo
 * vive en memoria: cerrada al arrancar, la vista de estado solo aparece tras
 * desbloquear. Recargar vuelve a cerrar: no hay persistencia.
 */
export function App() {
  const [device, setDevice] = useState(createDemoDevice());
  const [error, setError] = useState(null);

  function handleSubmit(credential) {
    const result = tryUnlock(device, credential);
    setDevice(result.device);
    setError(result.error);
  }

  return h('div', { class: 'app' },
    h(Header, null),
    device.unlocked
      ? h(StatusView, null)
      : h(AccessGate, { error, onSubmit: handleSubmit })
  );
}
