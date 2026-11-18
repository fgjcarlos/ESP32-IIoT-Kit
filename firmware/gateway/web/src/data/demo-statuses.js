/**
 * Estados de demostración del panel de red (W01-v0.1).
 *
 * Lista inmutable: el panel es de solo lectura, no hay simulador ni motor de
 * escenarios. Ningún valor identifica una red real: no hay SSID, dirección IP,
 * canal ni credenciales, porque todavía no hay valores aprobados.
 */
export const DEMO_STATUSES = Object.freeze([
  Object.freeze({
    id: 'ap',
    title: 'AP del equipo',
    state: 'Disponible',
    tone: 'ok',
    detail: 'Red propia del equipo para el primer acceso y para la recuperación.'
  }),
  Object.freeze({
    id: 'sta',
    title: 'STA de la red del usuario',
    state: 'Desconectada · opcional',
    tone: 'neutro',
    detail: 'Es opcional: el equipo sigue siendo accesible por el AP local aunque no se una a ninguna red.'
  }),
  Object.freeze({
    id: 'acceso',
    title: 'Acceso local',
    state: 'Disponible por AP',
    tone: 'ok',
    detail: 'El acceso no necesita Internet. La dirección la indica el equipo: aquí no se fija ningún valor.'
  }),
  Object.freeze({
    id: 'internet',
    title: 'Internet',
    state: 'No necesario',
    tone: 'neutro',
    detail: 'Todo el recorrido es local: no hay cuentas, servicios remotos ni actualizaciones en la nube.'
  })
]);
