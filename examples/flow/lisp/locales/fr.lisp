{:strings {:fr {:issue.back Retour
      :issue.close "Fermer le ticket"
      :issue.addSubs "Ajouter des sous-tickets"
      :display.grouping Regroupement
      :filter.placeholder "Filtrer…"
      :prop.status Statut
      :display.ordering Tri
      :display.properties "Propriétés affichées"
      :order.manual Manuel
      :order.priority "Priorité"
      :order.due "Échéance"
      :prop.id ID
      :prop.priority "Priorité"
      :prop.label "Étiquettes"
      :prop.due "Échéance"
      :prop.estimate Estimation
      :prop.assignee "Assigné à"
      :issue.previous "Ticket précédent"
      :issue.next "Ticket suivant"
      :issue.project Projet
      :issue.cycle Sprint
      :issue.noCycle "Aucun sprint"
      :issue.properties "Propriétés"
      :issue.status Statut
      :issue.priority "Priorité"
      :issue.assignee "Assigné à"
      :issue.labels "Étiquettes"
      :issue.due "Échéance"
      :issue.estimate Estimation
      :issue.activity "Activité"
      :issue.created "{who} a créé le ticket"
      :issue.subIssues "Sous-tickets  {done}/{total}"
      :issue.noAssignee "Non assigné"
      :issue.addSub "Ajouter un sous-ticket…"
      :issue.comment "Écrire un commentaire…"
      :bulk.count "{n} sélectionnés"
      :bulk.clear "Effacer la sélection (Échap)"
      :bulk.delete "Supprimer les tickets sélectionnés"
      :bulk.changed "{n} tickets modifiés"
      :bulk.deleted "{n} tickets supprimés"
      :filter.button Filtrer
      :filter.status Statut
      :filter.assignee "Assigné à"
      :filter.priority "Priorité"
      :filter.label "Étiquette"
      :filter.project Projet
      :filter.is est
      :filter.clear "Effacer le filtre"
      :display.button Vue
      :display.layout Disposition
      :composer.title "Nouveau ticket"
      :composer.notes "Ajouter une description…"
      :composer.more "En créer un autre"
      :composer.placeholder "Titre du ticket"
      :composer.create "Créer le ticket"
      :composer.in "Nouveau ticket dans {column}"
      :theme.menu "Basculer entre clair et sombre"
      :locale.menu "Langue : {code}"
      :toast.undo Annuler
      :toast.close Fermer
      :issue.addLabel "Ajouter une étiquette"
      :issue.changeTag "Changer le type"
      :issue.projectFollows "Le projet suit le type"
      :issue.removeLabel "Retirer l’étiquette"
      :scope.all "Tous les tickets"
      :scope.active Actifs
      :scope.backlog Backlog
      :nav.inbox "Boîte de réception"
      :nav.cycles Sprints
      :nav.projects Projets
      :projects.all "Tous les projets"
      :projects.active Actifs
      :projects.completed "Terminés"
      :projects.open "Voir les tickets du projet"
      :projects.new "Nouveau projet"
      :projects.untitled "Projet {n}"
      :inbox.assigned "{who} vous a assigné le ticket"
      :inbox.commented "{who} a commenté"
      :inbox.mentioned "{who} vous a mentionné"
      :inbox.status "{who} l’a déplacé vers {to}"
      :inbox.due "Le ticket arrive bientôt à échéance"
      :inbox.open "Ouvrir le ticket"
      :inbox.snooze Reporter
      :inbox.snoozed "Notification reportée"
      :inbox.markRead "Marquer comme lu"
      :inbox.markUnread "Marquer comme non lu"
      :inbox.allRead "Tout marquer comme lu"
      :inbox.empty "Aucune notification"
      :cycle.name "Sprint {n}"
      :cycle.current "En cours"
      :cycle.left "{n} jours restants"
      :cycle.scope "Périmètre"
      :cycle.started "Démarrés"
      :cycle.completed "Terminés"
      :cycle.others "À venir et passés"
      :cycle.issues "Tickets · {n}"
      :cycle.upcoming "À venir"
      :cycle.past "Terminés"
      :project.name Nom
      :project.health "Santé"
      :project.lead Responsable
      :project.target "Date cible"
      :project.progress Progression
      :health.onTrack "Dans les temps"
      :health.atRisk "À risque"
      :health.offTrack "En retard"
      :health.completed "Terminé"
      :workspace.name "Lumen Labs"
      :nav.newTooltip "Nouveau ticket (Cmd+N)"
      :nav.mine "Assignés à moi"
      :list.collapse "Réduire ou développer {name}"
      :nav.teams "Équipes"
      :nav.issues Tickets
      :nav.boards "Épinglés"
      :nav.commands "Commandes (Cmd+Shift+P)"
      :search.placeholder "Rechercher des tickets"
      :search.label "Rechercher parmi les tickets de ce tableau"
      :goTo.tooltip "Aller à un ticket par son titre (Cmd+P)"
      :theme.tooltip "Basculer entre clair et sombre (Cmd+J)"
      :locale.tooltip "Changer de langue (Cmd+L)"
      :members.label "Membres de l’équipe : afficher les tickets d’une personne"
      :members.title "Afficher les tickets de"
      :presence.here "{n, plural, =0 {Vous êtes seul ici} one {# autre personne ici} other {# autres personnes ici}}"
      :filter.all "Tous les tickets"
      :tag.bug bug
      :tag.feature "fonctionnalité"
      :tag.design design
      :tag.chore "tâche"
      :label.urgent urgent
      :label.frontend frontend
      :label.backend backend
      :label.docs docs
      :view.board " Tableau"
      :view.list " Liste"
      :priority.0 "Aucune priorité"
      :priority.1 Urgente
      :priority.2 Haute
      :priority.3 Moyenne
      :priority.4 Basse
      :view.table " Table"
      :view.timeline " Chronologie"
      :view.lanes "Couloirs par membre"
      :newCard.placeholder "Nouveau ticket pour le Backlog, puis Entrée"
      :column.limit "max. {limit}"
      :column.more "Actions de la colonne"
      :column.over "{name} compte {count} tickets, au-delà de sa limite de {limit}"
      :card.points "{n} pt"
      :card.label "{title}, {tag}, {who}, {column}"
      :lane.cards "{n, plural, one {# ticket} other {# tickets}}"
      :chart.title " Tickets par colonne"
      :burndown.title " Burndown du sprint : {n} points restants"
      :activity.title " Activité"
      :activity.added "{who} a ajouté « {title} »"
      :activity.moved "{who} a déplacé « {title} » vers {column}"
      :activity.deleted "{who} a supprimé « {title} »"
      :activity.renamed "{who} a renommé un ticket en « {title} »"
      :activity.commented "{who} a commenté « {title} »"
      :activity.undone "{who} a annulé une modification"
      :activity.joined "{who} a ouvert le tableau"
      :activity.revised "{who} a modifié le tableau : {title}"
      :presence.viewing "{who} est sur « {title} »"
      :noCard.select "Sélectionnez un ticket pour le voir ici. "
      :noCard.rightClick "Faites un clic droit"
      :noCard.menu " sur un ticket pour son menu, ou appuyez sur "
      :noCard.goTo " pour aller à un ticket."
      :detail.noNotes "Pas encore de description."
      :detail.edit " Modifier"
      :detail.left "Déplacer à gauche"
      :detail.right " Déplacer à droite"
      :detail.delete " Supprimer"
      :detail.more "Plus (ou clic droit sur le ticket)"
      :detail.editNotes "Modifier la description (*gras* entre astérisques)"
      :detail.save Enregistrer
      :detail.cancel Annuler
      :drawer.due "Échéance"
      :drawer.noDue "Aucune échéance"
      :drawer.clearDue "Effacer l’échéance"
      :drawer.dueIn "{n, plural, =0 {aujourd’hui} one {demain} other {dans # jours}}"
      :drawer.late "{n, plural, one {un jour de retard} other {# jours de retard}}"
      :drawer.labels "Étiquettes"
      :drawer.assignees "Assignés"
      :drawer.checklist "Liste de contrôle {done} sur {total}"
      :drawer.addItem "Ajoutez un élément, puis Entrée"
      :drawer.comments "{n, plural, =0 {Aucun commentaire} one {# commentaire} other {# commentaires}}"
      :drawer.addComment "Écrivez un commentaire, puis Entrée"
      :drawer.attachments "Pièces jointes"
      :drawer.addAttachment "Un nom de fichier, puis Entrée"
      :drawer.remove "Retirer {name}"
      :calendar.months "janvier,février,mars,avril,mai,juin,juillet,août,septembre,octobre,novembre,décembre"
      :calendar.days "Di,Lu,Ma,Me,Je,Ve,Sa"
      :timeline.title "Sprint à partir du {from} : les tickets datés, par responsable"
      :timeline.sprintEnd "Fin du sprint"
      :toast.added "« {title} » ajouté à {column}"
      :toast.renamed "Ticket renommé"
      :toast.deleted "« {title} » supprimé"
      :toast.linkCopied "{link} copié dans le presse-papiers"
      :toast.undone "Annulé"
      :toast.nothingToUndo "Rien à annuler"
      :toast.wip "{column} dépasse sa limite de {limit}"
      :toast.saved "Enregistré"
      :edit.title "Modifier le ticket"
      :edit.message "Le titre affiché sur le tableau."
      :quick.cards "Aller à un ticket par son titre"
      :quick.commands "Saisissez une commande ou recherchez…"
      :quick.status "Changer le statut…"
      :quick.priority "Définir la priorité…"
      :quick.assignee "Assigner à…"
      :status.revision "Révision {number}  ·  {author} : {message}"
      :table.id ID
      :table.priority "Priorité"
      :table.card Ticket
      :table.column Statut
      :table.label "Étiquette"
      :table.owner "Assigné à"
      :table.points Estimation
      :settings.title "Paramètres"
      :settings.close Fermer}}}
