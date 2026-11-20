/**
 * Cliente HTTP del portal (W02) contra la API real del gateway.
 * Toda petición es same-origin con la cookie de sesión HttpOnly.
 * Devuelve el JSON parseado o lanza { code }: la clave `error` del
 * servidor, o `sin-conexion` si falla la red o la respuesta no es JSON.
 */
async function llamar(ruta, cuerpo) {
  let respuesta;
  let datos;
  try {
    respuesta = await fetch(ruta, {
      method: cuerpo === undefined ? 'GET' : 'POST',
      credentials: 'same-origin',
      headers: cuerpo === undefined ? undefined : { 'Content-Type': 'application/json' },
      body: cuerpo === undefined ? undefined : JSON.stringify(cuerpo)
    });
    datos = await respuesta.json();
  } catch {
    throw { code: 'sin-conexion' };
  }
  if (!respuesta.ok) {
    throw { code: datos.error ?? 'sin-conexion' };
  }
  return datos;
}

/** POST /api/login: abre sesión (cookie portal_session) o 401 credencial-incorrecta. */
export function login(credential) {
  return llamar('/api/login', { credential });
}

/** GET /api/status: AP, STA y tiempo activo; 401 no-autorizado, 503 wifi-no-listo. */
export function status() {
  return llamar('/api/status');
}

/** POST /api/credential: cambia la credencial única; las reglas las valida el servidor. */
export function changeCredential(current, nueva, repetir) {
  return llamar('/api/credential', { current, new: nueva, repeat: repetir });
}

/** POST /api/logout: cierra la sesión del lado del equipo. */
export function logout() {
  return llamar('/api/logout', {});
}
