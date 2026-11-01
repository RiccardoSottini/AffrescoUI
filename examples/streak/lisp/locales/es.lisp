{:strings {:es {:app Streak
      :add "Añadir hábito"
      :filter Filtrar
      :more "Más opciones"
      :back "Atrás"
      :edit Editar
      :kind.yesNo "Sí o no"
      :kind.measurable Medible
      :sortBy Ordenar
      :sort.manual Manualmente
      :sort.name "Por nombre"
      :sort.color "Por color"
      :sort.score "Por puntuación"
      :sort.status "Por estado"
      :hideCompleted "Ocultar completados"
      :showArchived "Mostrar archivados"
      :darkMode "Modo oscuro"
      :settings Ajustes
      :exportAll "Exportar todo como CSV"
      :syncNow "Sincronizar ahora"
      :archive Archivar
      :unarchive Desarchivar
      :delete Eliminar
      :exportCsv "Exportar como CSV"
      :empty "Aún no hay hábitos"
      :emptyHint "Añade uno con el botón + o cambia el filtro."
      :noSelection "Elige un hábito para ver cómo va."
      :freq.daily "Cada día"
      :freq.everyN "{n, plural, one {Cada día} other {Cada # días}}"
      :freq.week "{n, plural, one {Una vez a la semana} other {# veces a la semana}}"
      :freq.month "{n, plural, one {Una vez al mes} other {# veces al mes}}"
      :target "{type, select, atMost {Como máximo} other {Al menos}} {value} {unit} {per, select, day {al día} week {a la semana} other {al mes}}"
      :reminderOff "Sin recordatorio"
      :archivedTag Archivado
      :card.overview "De un vistazo"
      :card.target Objetivo
      :card.score Fuerza
      :card.history Progreso
      :card.calendar Calendario
      :card.streaks "Rachas más largas"
      :card.frequency "Por día de la semana"
      :card.notes Notas
      :stat.score Fuerza
      :stat.month Mes
      :stat.year "Año"
      :stat.total Total
      :period Periodo
      :period.day "Día"
      :period.week Semana
      :period.month Mes
      :period.quarter Trimestre
      :period.year "Año"
      :quarter "T{n}"
      :goal.day Hoy
      :goal.week "Esta semana"
      :goal.month "Este mes"
      :goal.quarter "Este trimestre"
      :goal.year "Este año"
      :goalOf "{value} / {goal}"
      :noNotes "Aún no hay notas. Haz clic derecho en un día para escribir una."
      :noStreaks "Aún no hay rachas."
      :days "{n, plural, one {# día} other {# días}}"
      :popup.yes "Sí"
      :popup.no No
      :popup.skip Omitir
      :popup.save Guardar
      :popup.note Nota
      :popup.value Valor
      :cell.yes "{date}: hecho"
      :cell.no "{date}: no hecho"
      :cell.skip "{date}: omitido"
      :cell.auto "{date}: en el objetivo"
      :cell.value "{date}: {value} {unit}"
      :cell.none "{date}"
      :edit.new "Crear hábito"
      :edit.edit "Editar hábito"
      :edit.name Nombre
      :edit.namePh "p. ej. Estirar"
      :edit.question Pregunta
      :edit.questionPh "p. ej. ¿Has estirado hoy?"
      :edit.unit Unidad
      :edit.unitPh "p. ej. páginas"
      :edit.target Objetivo
      :edit.frequency Frecuencia
      :edit.reminder Recordatorio
      :edit.notes Notas
      :edit.notesPh "Por qué es importante, o cómo lo mantendrás"
      :edit.color Color
      :edit.nameNeeded "Un hábito necesita un nombre."
      :chip.daily "Cada día"
      :chip.everyN "Cada n días"
      :chip.week "Por semana"
      :chip.month "Por mes"
      :chip.atLeast "Al menos"
      :chip.atMost "Como máximo"
      :chip.perDay "Por día"
      :save Guardar
      :cancel Cancelar
      :picked "{n} seleccionados"
      :clearPick "Borrar selección"
      :delete.titleN "¿Eliminar estos hábitos?"
      :delete.messageN "Se eliminarán {n} hábitos y todo su historial. Esta acción no se puede deshacer."
      :toast.archivedN "{n, plural, one {# hábito archivado} other {# hábitos archivados}}"
      :toast.unarchivedN "{n, plural, one {# hábito restaurado} other {# hábitos restaurados}}"
      :toast.deletedN "{n, plural, one {# hábito eliminado} other {# hábitos eliminados}}"
      :delete.title "¿Eliminar este hábito?"
      :delete.message "Se eliminarán {name} y todo su historial. Esta acción no se puede deshacer."
      :toast.archived "{name} archivado"
      :toast.unarchived "{name} restaurado"
      :toast.deleted "{name} eliminado"
      :toast.copied "{n, plural, one {# hábito copiado como CSV} other {# hábitos copiados como CSV}}"
      :toast.reminder "Es hora de {name}"
      :toast.done "Marcar como hecho"
      :undo Deshacer
      :set.interface Interfaz
      :set.theme Tema
      :theme.light Claro
      :theme.dark Oscuro
      :theme.system "Predeterminado del sistema"
      :set.pureBlack "Negro total"
      :set.pureBlackHint "Un fondo negro en modo oscuro"
      :set.reverse "Invertir el orden de los días"
      :set.reverseHint "Mostrar primero el día más antiguo"
      :set.skip "Activar días omitidos"
      :set.skipHint "Haz clic en una casilla una tercera vez para marcar el día como omitido"
      :set.firstDay "Primer día de la semana"
      :set.language Idioma
      :set.notifications Notificaciones
      :set.reminders Recordatorios
      :set.remindersHint "Una notificación a la hora de cada hábito"
      :set.data Datos
      :set.export "Exportar todo como CSV"
      :set.exportHint "Copia el historial de cada hábito"
      :set.backup "Copia de seguridad en un archivo"
      :set.backupHint "Guarda cada hábito y su historial en un archivo JSON"
      :set.restore "Restaurar desde un archivo"
      :set.restoreHint "Añade o sustituye hábitos desde una copia"
      :toast.backedUp "{n, plural, one {# hábito guardado} other {# hábitos guardados}} en {name}"
      :toast.restored "{n, plural, one {# hábito restaurado} other {# hábitos restaurados}}"
      :toast.restoredSkipped "{n, plural, one {# hábito restaurado} other {# hábitos restaurados}}, {skipped, plural, one {# entrada omitida} other {# entradas omitidas}}"
      :toast.notRestored "No restaurado: {message}"
      :toast.notBackedUp "No guardado: {message}"
      :toast.badBackup "Ese archivo no es una copia de Streak"
      :set.sync "Sincronización"
      :set.account Cuenta
      :lang.en English
      :lang.es "Español"
      :lang.fr "Français"
      :sync.signedOut "Sin sesión iniciada"
      :sync.offline "Sin conexión"
      :sync.online Sincronizado
      :sync.error "Error de sincronización: {message}"
      :sync.received "Hábitos sincronizados desde el servidor"
      :sync.commits "{state}  ·  {n, plural, one {# cambio enviado} other {# cambios enviados}}"
      :signIn "Iniciar sesión"
      :signOut "Cerrar sesión"
      :signedIn "Sesión iniciada como {account}"
      :notSignedIn "Sin sesión iniciada: {message}"
      :you "tú"
      :palettePlaceholder "Escribe un comando"
      :cmd.addYesNo "Nuevo hábito de sí o no"
      :cmd.addMeasurable "Nuevo hábito medible"
      :cmd.editHabit "Editar hábito"
      :cmd.archiveHabit "Archivar o restaurar hábito"
      :cmd.confirmDelete "Eliminar hábito"
      :cmd.exportHabit "Exportar hábito como CSV"
      :cmd.exportAll "Exportar todo como CSV"
      :cmd.openSettings Ajustes
      :cmd.toggleTheme "Alternar modo oscuro"
      :cmd.toggleHideCompleted "Ocultar completados"
      :cmd.toggleShowArchived "Mostrar archivados"
      :cmd.nextHabit "Hábito siguiente"
      :cmd.prevHabit "Hábito anterior"
      :cmd.checkToday "Marcar hoy"
      :cmd.syncNow "Sincronizar ahora"
      :cmd.signIn "Iniciar sesión"
      :cmd.signOut "Cerrar sesión"
      :cmd.commandPalette "Paleta de comandos"
      :action.addYesNo.title "Nuevo hábito de sí o no"
      :action.addYesNo.category "Hábitos"
      :action.addMeasurable.title "Nuevo hábito medible"
      :action.addMeasurable.category "Hábitos"
      :action.editHabit.title "Editar hábito"
      :action.editHabit.category "Hábitos"
      :action.archiveHabit.title "Archivar o restaurar hábito"
      :action.archiveHabit.category "Hábitos"
      :action.confirmDelete.title "Eliminar hábito"
      :action.confirmDelete.category "Hábitos"
      :action.exportHabit.title "Exportar hábito como CSV"
      :action.exportHabit.category "Hábitos"
      :action.exportAll.title "Exportar todo como CSV"
      :action.exportAll.category "Hábitos"
      :action.backupAll.title "Hacer una copia en un archivo"
      :action.backupAll.category "Hábitos"
      :action.restoreAll.title "Restaurar desde un archivo"
      :action.restoreAll.category "Hábitos"
      :action.openSettings.title Ajustes
      :action.toggleTheme.title "Cambiar modo oscuro"
      :action.toggleHideCompleted.title "Ocultar completados"
      :action.toggleShowArchived.title "Mostrar archivados"
      :action.nextHabit.title "Hábito siguiente"
      :action.nextHabit.category "Hábitos"
      :action.prevHabit.title "Hábito anterior"
      :action.prevHabit.category "Hábitos"
      :action.checkToday.title "Marcar hoy"
      :action.checkToday.category "Hábitos"
      :action.syncNow.title "Sincronizar ahora"
      :action.syncNow.category "Sincronización"
      :action.signIn.title "Iniciar sesión"
      :action.signIn.category "Sincronización"
      :action.signOut.title "Cerrar sesión"
      :action.signOut.category "Sincronización"
      :action.commandPalette.title "Paleta de comandos"}}}
