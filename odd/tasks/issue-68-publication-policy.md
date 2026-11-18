# Excepción de publicación de la demo local de #68

Esta enmienda permite revisar la demo Industrial Local W01-v0.2 ya validada sin dividir su comportamiento de sus pruebas. Solo entra en vigor al fusionarse la PR documental de política. No aprueba el frontend ni autoriza su fusión.

## Alcance y presupuesto

- Una única PR funcional sobre `feat/issue-68-status-tracker`, dentro de la cadena existente cuyo tracker es #110. No crear otra cadena ni mezclar estrategias.
- Máximo **5.000 líneas autorales cambiadas** y **7.500 líneas totales cambiadas**, contando siempre adiciones más eliminaciones; el lockfile generado se declara aparte, nunca se oculta del total.
- El tracker final contra `main` debe respetar ambos máximos. El presupuesto no se reutiliza para entregas futuras ni se acumula entre PR.
- La candidata funcional debe declarar base/head exactas, cifras reales por archivo, lockfile y diff acumulado del tracker antes de publicarse. Si supera un máximo, se detiene la entrega; esta política no permite nuevas rondas de compresión ni excepciones implícitas.

Medición previa contra la integración `9469c6bcea46d79f345f708d94899f905804cefd`: 4.054 adiciones y 250 eliminaciones autorales (4.304), más 996 adiciones y 12 eliminaciones del lockfile (1.008): **5.312 líneas totales solo de web**. El diseño y el seguimiento aún deben contarse al preparar la candidata. Estos números son evidencia previa, no sustituyen la medición de la PR final.

## Mapa de archivos permitido

| Superficie | Contenido que se puede publicar |
|---|---|
| `firmware/gateway/web/` | Demo Preact, componentes, estado simulado, estilos, pruebas, configuración y lockfile; excluir `node_modules`, `dist`, cachés y capturas |
| `design/gateway-industrial-v02/` | Artboard elegido y `design.json`; excluir `handoffs.json`, comentarios y otros metadatos de sesión |
| `design/system.md`, `design/system.html` | Tokens, extensión propuesta y tablero de referencia; no identidad de marca aprobada |
| `odd/tasks/issue-68-web-wireframes.md` | Requisitos, decisiones y evidencia pública de esta entrega; excluir rutas temporales y URLs privadas de sesión |

Fuera de alcance: TFT, firmware C/CMake, API, autenticación real, persistencia, secretos, flasheo, cambios de dependencias no necesarios para reproducir esta demo y modificaciones ajenas del worktree. La demo seguirá siendo local; esta enmienda no desbloquea la integración real ni cierra #68.

## Revisión por recorridos

La PR debe explicar su carga de revisión y ofrecer este orden, manteniendo código, pruebas y documentación en la misma entrega:

1. Leer el artboard, los tokens y el mapa exacto de componentes; contrastar escritorio y móvil.
2. Revisar navegación, sheet modal, foco, cierre, bloqueo del fondo y restauración al cambiar de ancho.
3. Recorrer acceso, credenciales, configuración, estados AP/STA y recuperación; comprobar que no hay red, persistencia ni eco de contraseñas.
4. Revisar pruebas, paquete/lockfile, instrucciones reproducibles y los límites de la evidencia.

## Lista obligatoria de la futura PR

- [ ] Enmienda de política fusionada antes de abrir la entrega funcional; enlace a esta política y a #110.
- [ ] `Refs #68`, exactamente una etiqueta `type:feature`, sin cierre automático de la issue.
- [ ] Base/head, mapa de archivos y recuentos por PR y acumulados dentro de los dos máximos; lockfile generado declarado.
- [ ] Código, pruebas y documentación juntos; ningún archivo ajeno, secreto, metadato privado ni artefacto generado de ejecución.
- [ ] Pruebas y build de la demo correctos sobre la candidata exacta; evidencia de escritorio, móvil y teclado con procedencia explícita.
- [ ] Políticas de diseño acordadas separadas de su viabilidad técnica; recuperación de propietario y prueba offline pendientes declaradas si siguen sin resolver.
- [ ] Comprobaciones requeridas por el repositorio correctas sobre el head exacto; no extrapolar la CI de #111.
- [ ] Revisión humana de la entrega funcional y una revisión aprobatoria antes de fusionar a `main`; no se omite ningún control de revisión vigente.

## Siguiente paso

Revisar esta PR de política. Si se fusiona, preparar y volver a medir la candidata funcional. La autorización de publicación no implica permiso de merge ni aprobación visual o de seguridad del producto.
