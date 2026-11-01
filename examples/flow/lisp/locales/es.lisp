{:strings {:es {:issue.back "Atrás"
      :issue.close "Cerrar incidencia"
      :issue.addSubs "Añadir subincidencias"
      :display.grouping "Agrupación"
      :filter.placeholder "Filtrar…"
      :prop.status Estado
      :display.ordering Orden
      :display.properties "Propiedades visibles"
      :order.manual Manual
      :order.priority Prioridad
      :order.due "Fecha límite"
      :prop.id ID
      :prop.priority Prioridad
      :prop.label Etiquetas
      :prop.due "Fecha límite"
      :prop.estimate "Estimación"
      :prop.assignee Responsable
      :issue.previous "Incidencia anterior"
      :issue.next "Incidencia siguiente"
      :issue.project Proyecto
      :issue.cycle Sprint
      :issue.noCycle "Sin sprint"
      :issue.properties Propiedades
      :issue.status Estado
      :issue.priority Prioridad
      :issue.assignee Responsable
      :issue.labels Etiquetas
      :issue.due "Fecha límite"
      :issue.estimate "Estimación"
      :issue.activity Actividad
      :issue.created "{who} creó la incidencia"
      :issue.subIssues "Subincidencias  {done}/{total}"
      :issue.noAssignee "Sin asignar"
      :issue.addSub "Añadir subincidencia…"
      :issue.comment "Escribe un comentario…"
      :bulk.count "{n} seleccionadas"
      :bulk.clear "Borrar la selección (Esc)"
      :bulk.delete "Eliminar las incidencias seleccionadas"
      :bulk.changed "{n} incidencias modificadas"
      :bulk.deleted "{n} incidencias eliminadas"
      :filter.button Filtrar
      :filter.status Estado
      :filter.assignee Responsable
      :filter.priority Prioridad
      :filter.label Etiqueta
      :filter.project Proyecto
      :filter.is es
      :filter.clear "Quitar el filtro"
      :display.button Vista
      :display.layout "Diseño"
      :composer.title "Nueva incidencia"
      :composer.notes "Añadir descripción…"
      :composer.more "Crear otra"
      :composer.placeholder "Título de la incidencia"
      :composer.create "Crear incidencia"
      :composer.in "Nueva incidencia en {column}"
      :theme.menu "Cambiar entre claro y oscuro"
      :locale.menu "Idioma: {code}"
      :toast.undo Deshacer
      :toast.close Cerrar
      :issue.addLabel "Añadir una etiqueta"
      :issue.changeTag "Cambiar el tipo"
      :issue.projectFollows "El proyecto sigue al tipo"
      :issue.removeLabel "Quitar la etiqueta"
      :scope.all "Todas las incidencias"
      :scope.active Activas
      :scope.backlog Backlog
      :nav.inbox "Bandeja de entrada"
      :nav.cycles Sprints
      :nav.projects Proyectos
      :projects.all "Todos los proyectos"
      :projects.active Activos
      :projects.completed Completados
      :projects.open "Ver las incidencias del proyecto"
      :projects.new "Nuevo proyecto"
      :projects.untitled "Proyecto {n}"
      :inbox.assigned "{who} te asignó la incidencia"
      :inbox.commented "{who} comentó"
      :inbox.mentioned "{who} te mencionó"
      :inbox.status "{who} la movió a {to}"
      :inbox.due "La incidencia vence pronto"
      :inbox.open "Abrir incidencia"
      :inbox.snooze Posponer
      :inbox.snoozed "Notificación pospuesta"
      :inbox.markRead "Marcar como leída"
      :inbox.markUnread "Marcar como no leída"
      :inbox.allRead "Marcar todo como leído"
      :inbox.empty "Sin notificaciones"
      :cycle.name "Sprint {n}"
      :cycle.current Actual
      :cycle.left "Quedan {n} días"
      :cycle.scope Alcance
      :cycle.started Iniciadas
      :cycle.completed Completadas
      :cycle.others "Próximos y anteriores"
      :cycle.issues "Incidencias · {n}"
      :cycle.upcoming "Próximos"
      :cycle.past Completados
      :project.name Nombre
      :project.health "Estado de salud"
      :project.lead Responsable
      :project.target "Fecha objetivo"
      :project.progress Progreso
      :health.onTrack "En camino"
      :health.atRisk "En riesgo"
      :health.offTrack Desviado
      :health.completed Completado
      :workspace.name "Lumen Labs"
      :nav.newTooltip "Nueva incidencia (Cmd+N)"
      :nav.mine "Asignadas a mí"
      :list.collapse "Contraer o expandir {name}"
      :nav.teams Equipos
      :nav.issues Incidencias
      :nav.boards Fijados
      :nav.commands "Comandos (Cmd+Shift+P)"
      :search.placeholder "Buscar incidencias"
      :search.label "Buscar entre las incidencias de este tablero"
      :goTo.tooltip "Ir a una incidencia por su título (Cmd+P)"
      :theme.tooltip "Cambiar entre claro y oscuro (Cmd+J)"
      :locale.tooltip "Cambiar el idioma (Cmd+L)"
      :members.label "Miembros del equipo: ver las incidencias de una persona"
      :members.title "Ver las incidencias de"
      :presence.here "{n, plural, =0 {Solo tú estás aquí} one {# persona más está aquí} other {# personas más están aquí}}"
      :filter.all "Todas las incidencias"
      :tag.bug error
      :tag.feature "función"
      :tag.design "diseño"
      :tag.chore tarea
      :label.urgent urgente
      :label.frontend frontend
      :label.backend backend
      :label.docs docs
      :view.board " Tablero"
      :view.list " Lista"
      :priority.0 "Sin prioridad"
      :priority.1 Urgente
      :priority.2 Alta
      :priority.3 Media
      :priority.4 Baja
      :view.table " Tabla"
      :view.timeline " Cronograma"
      :view.lanes "Carriles por miembro"
      :newCard.placeholder "Nueva incidencia para el Backlog, luego Intro"
      :column.limit "máx. {limit}"
      :column.more "Acciones de la columna"
      :column.over "{name} tiene {count} incidencias, por encima de su límite de {limit}"
      :card.points "{n} pt"
      :card.label "{title}, {tag}, {who}, {column}"
      :lane.cards "{n, plural, one {# incidencia} other {# incidencias}}"
      :chart.title " Incidencias por columna"
      :burndown.title " Burndown del sprint: quedan {n} puntos"
      :activity.title " Actividad"
      :activity.added "{who} añadió “{title}”"
      :activity.moved "{who} movió “{title}” a {column}"
      :activity.deleted "{who} eliminó “{title}”"
      :activity.renamed "{who} renombró una incidencia como “{title}”"
      :activity.commented "{who} comentó en “{title}”"
      :activity.undone "{who} deshizo un cambio"
      :activity.joined "{who} abrió el tablero"
      :activity.revised "{who} cambió el tablero: {title}"
      :presence.viewing "{who} está en “{title}”"
      :noCard.select "Selecciona una incidencia para verla aquí. "
      :noCard.rightClick "Haz clic derecho"
      :noCard.menu " en una incidencia para ver su menú, o pulsa "
      :noCard.goTo " para ir a una."
      :detail.noNotes "Aún no hay descripción."
      :detail.edit " Editar"
      :detail.left "Mover a la izquierda"
      :detail.right " Mover a la derecha"
      :detail.delete " Eliminar"
      :detail.more "Más (o clic derecho en la incidencia)"
      :detail.editNotes "Editar la descripción (*negrita* entre asteriscos)"
      :detail.save Guardar
      :detail.cancel Cancelar
      :drawer.due Vence
      :drawer.noDue "Sin fecha límite"
      :drawer.clearDue "Quitar la fecha límite"
      :drawer.dueIn "{n, plural, =0 {hoy} one {mañana} other {en # días}}"
      :drawer.late "{n, plural, one {con un día de retraso} other {con # días de retraso}}"
      :drawer.labels Etiquetas
      :drawer.assignees Responsables
      :drawer.checklist "Lista de tareas {done} de {total}"
      :drawer.addItem "Añade un elemento y pulsa Intro"
      :drawer.comments "{n, plural, =0 {Sin comentarios} one {# comentario} other {# comentarios}}"
      :drawer.addComment "Escribe un comentario y pulsa Intro"
      :drawer.attachments "Archivos adjuntos"
      :drawer.addAttachment "Un nombre de archivo y pulsa Intro"
      :drawer.remove "Quitar {name}"
      :calendar.months "enero,febrero,marzo,abril,mayo,junio,julio,agosto,septiembre,octubre,noviembre,diciembre"
      :calendar.days "Do,Lu,Ma,Mi,Ju,Vi,Sá"
      :timeline.title "Sprint desde {from}: las incidencias con fecha, por responsable"
      :timeline.sprintEnd "Fin del sprint"
      :toast.added "Se añadió “{title}” a {column}"
      :toast.renamed "Incidencia renombrada"
      :toast.deleted "Se eliminó “{title}”"
      :toast.linkCopied "{link} copiado al portapapeles"
      :toast.undone Deshecho
      :toast.nothingToUndo "Nada que deshacer"
      :toast.wip "{column} supera su límite de {limit}"
      :toast.saved Guardado
      :edit.title "Editar incidencia"
      :edit.message "El título que se muestra en el tablero."
      :quick.cards "Ir a una incidencia por su título"
      :quick.commands "Escribe un comando o busca…"
      :quick.status "Cambiar estado…"
      :quick.priority "Establecer prioridad…"
      :quick.assignee "Asignar a…"
      :status.revision "Revisión {number}  ·  {author}: {message}"
      :table.id ID
      :table.priority Prioridad
      :table.card Incidencia
      :table.column Estado
      :table.label Etiqueta
      :table.owner Responsable
      :table.points "Estimación"
      :settings.title Ajustes
      :settings.close Cerrar}}}
