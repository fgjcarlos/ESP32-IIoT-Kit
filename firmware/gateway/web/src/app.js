import { h } from 'preact';
import { useState, useEffect } from 'preact/hooks';
import { Header } from './components/Header.js';
import { StatusView } from './components/StatusView.js';
import { AccessGate } from './components/AccessGate.js';
import { NavTabs } from './components/NavTabs.js';
import { CredentialView } from './components/CredentialView.js';
import * as api from './api.js';

/** Decisión ante un error de /api/status (pura): el 401 cierra la sesión, el resto se muestra. */
export function manejarErrorDeEstado(codigo) {
  return codigo === 'no-autorizado'
    ? { cerrarSesion: true }
    : { cerrarSesion: false, errorDeEstado: codigo };
}

/** Render completo según la sesión (pura): App posee el estado y llama aquí. */
export function Portal({ sesion, view, status, errorDeEstado, resultadoCredencial, onSubmit, onView, onCredentialChange }) {
  return h('div', { class: 'app' },
    h(Header, null),
    sesion.unlocked
      ? h('div', null,
        h(NavTabs, { view, onChange: onView }),
        view === 'credencial'
          ? h(CredentialView, {
            inicial: status?.portal?.initial_credential,
            resultado: resultadoCredencial,
            onChange: onCredentialChange
          })
          : h(StatusView, { status, error: errorDeEstado })
      )
      : h(AccessGate, { error: sesion.error, onSubmit })
  );
}

/**
 * Portal local del gateway (W02). La sesión vive en RAM: recargar vuelve a
 * cerrar la puerta. El estado real del equipo llega de la API; si /api/status
 * responde 401 (el servidor ya no reconoce la cookie), se vuelve a cerrar.
 */
export function App() {
  const [sesion, setSesion] = useState({ unlocked: false, error: null });
  const [view, setView] = useState('estado');
  const [status, setStatus] = useState(null);
  const [errorDeEstado, setErrorDeEstado] = useState(null);
  const [resultadoCredencial, setResultadoCredencial] = useState(null);

  useEffect(() => {
    if (!sesion.unlocked) return undefined;
    let vigente = true;
    api.status().then(
      (datos) => {
        if (!vigente) return;
        setStatus(datos);
        setErrorDeEstado(null);
      },
      (fallo) => {
        if (!vigente) return;
        const decision = manejarErrorDeEstado(fallo.code);
        if (decision.cerrarSesion) setSesion({ unlocked: false, error: null });
        else setErrorDeEstado(decision.errorDeEstado);
      }
    );
    return () => { vigente = false; };
  }, [sesion.unlocked]);

  async function handleSubmit(credential) {
    try {
      await api.login(credential);
      setSesion({ unlocked: true, error: null });
    } catch (fallo) {
      setSesion({ unlocked: false, error: fallo.code });
    }
  }

  async function handleCredentialChange(actual, nueva, repetir) {
    try {
      await api.changeCredential(actual, nueva, repetir);
      setResultadoCredencial({ kind: 'exito' });
      // El cambio ya surtió efecto en el equipo: baja el aviso de primer uso.
      setStatus((previo) => previo && { ...previo, portal: { ...previo.portal, initial_credential: false } });
    } catch (fallo) {
      setResultadoCredencial({ kind: 'error', code: fallo.code });
    }
  }

  return h(Portal, {
    sesion,
    view,
    status,
    errorDeEstado,
    resultadoCredencial,
    onSubmit: handleSubmit,
    onView: setView,
    onCredentialChange: handleCredentialChange
  });
}
