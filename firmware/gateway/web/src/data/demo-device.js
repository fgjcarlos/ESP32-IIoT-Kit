/**
 * Estado simulado del dispositivo para la demo (W01-v0.2).
 *
 * Todo vive en memoria: no hay persistencia, la credencial es un valor de
 * demostración, no un secreto. Las transiciones son puras para poder
 * probarlas sin DOM: devuelven un estado nuevo y no mutan el anterior.
 */
export const DEMO_INITIAL_CREDENTIAL = 'acceso-inicial';

/** Dispositivo recién encendido: cerrado, sin intentos. */
export function createDemoDevice() {
  return Object.freeze({ unlocked: false, attempts: 0 });
}

/**
 * Intenta desbloquear con una credencial.
 * @returns {{device: object, error: string|null}} nuevo estado y error o null.
 */
export function tryUnlock(device, credential) {
  if (device.unlocked || credential === DEMO_INITIAL_CREDENTIAL) {
    return { device: Object.freeze({ unlocked: true, attempts: device.attempts }), error: null };
  }
  return {
    device: Object.freeze({ unlocked: false, attempts: device.attempts + 1 }),
    error: 'credencial-incorrecta'
  };
}
