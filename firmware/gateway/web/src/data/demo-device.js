/**
 * Estado simulado del dispositivo para la demo (W01-v0.2).
 *
 * Todo vive en memoria: no hay persistencia, la credencial es un valor de
 * demostración, no un secreto. Las transiciones son puras para poder
 * probarlas sin DOM: devuelven un estado nuevo y no mutan el anterior.
 */
export const DEMO_INITIAL_CREDENTIAL = 'acceso-inicial';

/** Configuración AP/STA de demostración: ningún valor es real ni aprobado. */
export function createDemoConfig() {
  return Object.freeze({
    apSsid: 'Gateway-Demo',
    apClave: 'demo-ap',
    staSsid: '',
    staClave: '',
    canal: 6,
    ip: '192.168.4.1'
  });
}

/** Dispositivo recién encendido: cerrado, sin intentos, configuración inicial. */
export function createDemoDevice() {
  return Object.freeze({
    unlocked: false,
    attempts: 0,
    credential: DEMO_INITIAL_CREDENTIAL,
    config: createDemoConfig(),
    lastApply: null
  });
}

/**
 * Intenta desbloquear con una credencial.
 * @returns {{device: object, error: string|null}} nuevo estado y error o null.
 */
export function tryUnlock(device, credential) {
  if (device.unlocked || credential === device.credential) {
    return { device: Object.freeze({ ...device, unlocked: true }), error: null };
  }
  return {
    device: Object.freeze({ ...device, unlocked: false, attempts: device.attempts + 1 }),
    error: 'credencial-incorrecta'
  };
}

/**
 * Aplica un borrador de configuración de forma simulada.
 * Con fallo simulado, la configuración anterior queda intacta: esa es la
 * vía de recuperación (el equipo sigue con los valores previos).
 * @returns {{device: object, error: string|null}}
 */
export function applyConfig(device, draft, simulateFailure) {
  if (simulateFailure) {
    return {
      device: Object.freeze({
        ...device,
        lastApply: Object.freeze({ status: 'error', reason: 'fallo-simulado' })
      }),
      error: 'fallo-aplicacion'
    };
  }
  const config = Object.freeze({ ...device.config, ...draft });
  return {
    device: Object.freeze({ ...device, config, lastApply: Object.freeze({ status: 'success' }) }),
    error: null
  };
}
