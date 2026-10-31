{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "The host's own view of a change, in a window of its own (apps/host/ChangesWindow.cpp): what a proposal or a commit changes, file by file. It is read from the host's assets and fed only by the host from the version control, so nothing in the change it shows can alter it. The action changes.show stands in for the host in scenarios."
  :window {:title Changes :width 1120 :height 740 :minWidth 760 :minHeight 480}
  :theme "{{window.colorScheme}}"
  :onLoad [{:run changes.refresh}]
  :fonts {:default {:file "../fonts/Roboto-Regular.ttf" :size 14 :fallback [icons]}
    :small {:file "../fonts/Roboto-Regular.ttf" :size 12}
    :heading {:file "../fonts/Roboto-Regular.ttf" :size 18 :bold true}
    :code {:file "../fonts/JetBrainsMono-Regular.ttf" :size 13}
    :icons {:file "../fonts/Lucide.ttf" :size 15}}
  :tokens {:c.background {:light "#f6f5fb" :dark "#111118"}
    :c.surface {:light "#ffffff" :dark "#1c1c26"}
    :c.raised {:light "#f0eff7" :dark "#262633"}
    :c.selected {:light "#e6e3fb" :dark "#30304a"}
    :c.border {:light "#e3e1ee" :dark "#2b2b39"}
    :c.text {:light "#1b1a28" :dark "#ececf4"}
    :c.muted {:light "#5d5b72" :dark "#a4a2b9"}
    :c.accent {:light "#5b4fe0" :dark "#8b80ff"}
    :c.onAccent {:light "#ffffff" :dark "#111118"}
    :c.warning {:light "#9a5b00" :dark "#f1b65a"}
    :c.deleted {:light "#fbd9d9" :dark "#4a2428"}
    :c.inserted {:light "#d5f2dc" :dark "#1f3f2a"}}
  :strings {:default en
    :en {:changes.title Changes
      :changes.proposals "Waiting for review"
      :changes.history Applied
      :changes.files Files
      :changes.none "No file changes"
      :changes.filesSummary "{n, plural, one {# file} other {# files}}  +{added} −{removed}"
      :changes.by "by {author}"
      :changes.split Split
      :changes.inline Inline
      :changes.preview Preview
      :changes.endPreview "End preview"
      :changes.accept Accept
      :changes.reject Reject
      :changes.undo "Undo this change"
      :changes.reason "Why (told to the author)"
      :changes.stale "This change was updated. Check it again before you accept."
      :changes.closed "This proposal has been decided."
      :changes.behaviour "Changes what the app does: {lines}"
      :changes.person "Only you can accept this: {lines}"
      :changes.conflicts "It conflicts with changes made since. Its author must update it first."
      :changes.tooLarge "Too large to show."
      :changes.canonical "Shown in a standard layout, so lines may differ from the file on disk."
      :changes.assets "Files it adds or removes"
      :changes.run "Runs once when accepted"
      :changes.removed "{name} (removed)"
      :changes.pick "Choose a change on the left."
      :changes.revision "Revision {n}"
      :changes.layers "View {view} · Logic {logic} · Data: {data}"
      :changes.original "The file as loaded"}
    :es {:changes.title Cambios
      :changes.proposals "Pendientes de revisión"
      :changes.history Aplicados
      :changes.files Archivos
      :changes.none "Ningún archivo cambia"
      :changes.filesSummary "{n, plural, one {# archivo} other {# archivos}}  +{added} −{removed}"
      :changes.by "de {author}"
      :changes.split "En paralelo"
      :changes.inline "En línea"
      :changes.preview "Vista previa"
      :changes.endPreview "Terminar vista previa"
      :changes.accept Aceptar
      :changes.reject Rechazar
      :changes.undo "Deshacer este cambio"
      :changes.reason "Por qué (se lo dirá al autor)"
      :changes.stale "Este cambio se ha actualizado. Revísalo otra vez antes de aceptar."
      :changes.closed "Ya se ha decidido sobre esta propuesta."
      :changes.behaviour "Cambia lo que hace la app: {lines}"
      :changes.person "Solo tú puedes aceptarlo: {lines}"
      :changes.conflicts "Entra en conflicto con cambios posteriores. Su autor debe actualizarlo antes."
      :changes.tooLarge "Demasiado grande para mostrarlo."
      :changes.canonical "Se muestra con un formato estándar, así que las líneas pueden no coincidir con el archivo en disco."
      :changes.assets "Archivos que añade o quita"
      :changes.run "Se ejecuta una vez al aceptar"
      :changes.removed "{name} (quitado)"
      :changes.pick "Elige un cambio a la izquierda."
      :changes.revision "Revisión {n}"
      :changes.layers "Vista {view} · Lógica {logic} · Datos: {data}"
      :changes.original "El archivo tal como se cargó"}
    :fr {:changes.title Modifications
      :changes.proposals "En attente de revue"
      :changes.history "Appliquées"
      :changes.files Fichiers
      :changes.none "Aucun fichier modifié"
      :changes.filesSummary "{n, plural, one {# fichier} other {# fichiers}}  +{added} −{removed}"
      :changes.by "par {author}"
      :changes.split "Côte à côte"
      :changes.inline "En ligne"
      :changes.preview "Aperçu"
      :changes.endPreview "Terminer l''aperçu"
      :changes.accept Accepter
      :changes.reject Refuser
      :changes.undo "Annuler cette modification"
      :changes.reason "Pourquoi (sera dit à l''auteur)"
      :changes.stale "Cette modification a été mise à jour. Vérifiez-la de nouveau avant de l''accepter."
      :changes.closed "Cette proposition a déjà été traitée."
      :changes.behaviour "Change ce que fait l''app : {lines}"
      :changes.person "Vous seul pouvez l''accepter : {lines}"
      :changes.conflicts "Elle entre en conflit avec des modifications récentes. Son auteur doit d''abord la mettre à jour."
      :changes.tooLarge "Trop volumineux pour être affiché."
      :changes.canonical "Affiché dans une mise en forme standard : les lignes peuvent différer du fichier sur le disque."
      :changes.assets "Fichiers ajoutés ou retirés"
      :changes.run "S''exécute une fois à l''acceptation"
      :changes.removed "{name} (retiré)"
      :changes.pick "Choisissez une modification à gauche."
      :changes.revision "Révision {n}"
      :changes.layers "Vue {view} · Logique {logic} · Données : {data}"
      :changes.original "Le fichier tel que chargé"}}
  :state {:list {:proposals [] :history [] :selectionCount 0}
    :selectionCount 0
    :selected nil
    :view nil
    :file ""
    :text nil
    :mode split
    :notation nil
    :reason ""
    :message ""
    :stale false
    :closed false}
  :computed {:added "{{sum(map(state.view.files ?? [], it.added ?? 0))}}"
    :removed "{{sum(map(state.view.files ?? [], it.removed ?? 0))}}"
    :isProposal "{{state.view != null && state.view.kind == 'proposal'}}"
    :previewing "{{computed.isProposal && state.list.previewing == state.view.proposal}}"
    :open "{{computed.isProposal && !state.closed}}"
    :extras "{{concat(map(state.view.assets ?? [], it.removed == true ? t('changes.removed', {name: it.name}) : it.name), map(state.view.run ?? [], json(it)))}}"}
  :actions {:changes.refresh [{:note "Every half second: what the host wants shown, and whether what is shown has moved on"}
      {:call changes.list :into state.list :onError []}
      {:if "{{state.list.selectionCount != state.selectionCount}}"
        :then [{:set state.selectionCount :to "{{state.list.selectionCount}}"}
          {:run changes.select :with {:what "{{state.list.selection}}"}}
          {:stop true}]}
      {:if "{{computed.open}}"
        :then [{:if "{{!some(state.list.proposals, it.id == state.view.proposal)}}"
            :then [{:set state.closed :to true} {:stop true}]}
          {:if "{{find(state.list.proposals, it.id == state.view.proposal).head != state.view.head}}"
            :then [{:set state.stale :to true} {:run changes.load}]}]}]
    :changes.select [{:note "with: what, {proposal} or {to}"}
      {:set state.selected :to "{{what}}"}
      {:set state.stale :to false}
      {:set state.closed :to false}
      {:set state.reason :to ""}
      {:set state.message :to ""}
      {:set state.file :to ""}
      {:set state.text :to nil}
      {:run changes.load}]
    :changes.load [{:if "{{state.selected == null}}" :then [{:stop true}]}
      {:call changes.load
        :with "{{merge(state.selected, state.notation == null ? {} : {notation: state.notation})}}"
        :into state.view
        :onError [{:set state.message :to "{{error}}"} {:stop true}]}
      {:run changes.open
        :with {:file "{{some(state.view.files, it.file == state.file) ? state.file : ((state.view.files[0] ?? {}).file ?? '')}}"}}]
    :changes.open [{:note "with: file. The texts of one file, asked for when it is shown: a change's others may be large"}
      {:set state.file :to "{{file}}"}
      {:if "{{file == ''}}" :then [{:set state.text :to nil} {:stop true}]}
      {:call changes.load
        :with "{{merge(state.selected, {file: file, notation: state.view.notation})}}"
        :into state.opened
        :onError [{:set state.message :to "{{error}}"} {:stop true}]}
      {:set state.text :to "{{find(state.opened.files, it.file == file)}}"}]
    :changes.setNotation [{:note "with: notation"} {:set state.notation :to "{{notation}}"} {:run changes.load}]
    :changes.show [{:note "For scenarios, in the host's place: event.list, event.view, event.text"}
      {:set state.selectionCount :to "{{event.list.selectionCount ?? 0}}"}
      {:set state.list :to "{{event.list}}"}
      {:set state.selected :to "{{event.selected}}"}
      {:set state.view :to "{{event.view}}"}
      {:set state.text :to "{{event.text}}"}
      {:set state.file :to "{{event.text.file}}"}]
    :changes.preview [{:call changes.preview :with {:proposal "{{state.view.proposal}}"} :into state.answer :onError []}
      {:set state.message :to "{{state.answer.error ?? ''}}"}
      {:run changes.refresh}]
    :changes.accept [{:call changes.accept
        :with {:proposal "{{state.view.proposal}}" :head "{{state.view.head}}"}
        :into state.answer
        :onError []}
      {:set state.message :to "{{state.answer.error ?? ''}}"}
      {:if "{{state.message == ''}}" :then [{:set state.closed :to true}]}
      {:run changes.refresh}]
    :changes.reject [{:call changes.reject
        :with {:proposal "{{state.view.proposal}}" :reason "{{state.reason}}"}
        :into state.answer
        :onError []}
      {:set state.closed :to true}
      {:run changes.refresh}]
    :changes.undo [{:call changes.undo :with {:commit "{{state.view.commit}}"} :into state.answer :onError []}
      {:set state.message :to "{{state.answer.error ?? ''}}"}
      {:run changes.refresh}]}
  :root (node #changesRoot :color @c.background :every {:ms 500 :do [{:run changes.refresh}]}
    (node #changesLayout :type row :fill 16 :gap 16
      (node #changesSide :type column :width 290 :gap 6 :alignItems stretch :anchors {:top 0 :bottom 0} :scroll vertical
        (node #changesProposalsTitle
          :when "{{length(state.list.proposals) > 0}}"
          :type label
          :text "{{t('changes.proposals')}}"
          :font small
          :textColor @c.muted
          :height 20)
        (node :repeat {:for p :in "{{state.list.proposals}}"}
          #"changesProposal{{p.id}}"
          :role button
          :focusable true
          :cursor hand
          :height 52
          :radius 8
          :anchors {:left 0 :right 0}
          :color "{{computed.isProposal && state.view.proposal == p.id ? '@c.selected' : 'transparent'}}"
          :selected "{{computed.isProposal && state.view.proposal == p.id}}"
          :label "{{p.message}}"
          :onClick [{:run changes.select :with {:what {:proposal "{{p.id}}"}}}]
          {:type label :text "{{p.message}}" :textColor @c.text :anchors {:left 10 :right 10 :top 8} :height 18}
          {:type label
            :text "{{t('changes.by', {author: p.author})}}"
            :textColor @c.muted
            :font small
            :anchors {:left 10 :right 10 :top 28}
            :height 16})
        (node #changesHistoryTitle
          :type label
          :text "{{t('changes.history')}}"
          :font small
          :textColor @c.muted
          :height 20)
        (node :repeat {:for c :in "{{state.list.history}}"}
          #"changesCommit{{c.number}}"
          :role button
          :focusable true
          :cursor hand
          :height 52
          :radius 8
          :anchors {:left 0 :right 0}
          :color "{{state.view != null && state.view.commit == c.id ? '@c.selected' : 'transparent'}}"
          :selected "{{state.view != null && state.view.commit == c.id}}"
          :label "{{c.number == 0 ? t('changes.original') : c.message}}"
          :onClick [{:run changes.select :with {:what {:to "{{c.id}}"}}}]
          {:type label
            :text "{{c.number == 0 ? t('changes.original') : c.message}}"
            :textColor @c.text
            :anchors {:left 10 :right 10 :top 8}
            :height 18}
          {:type label
            :text "{{t('changes.revision', {n: c.number}) + ' · ' + c.author}}"
            :textColor @c.muted
            :font small
            :anchors {:left 10 :right 10 :top 28}
            :height 16}))
      (node #changesMain :type column :grow 1 :gap 10 :alignItems stretch :anchors {:top 0 :bottom 0}
        (node #changesPick
          :when "{{state.view == null}}"
          :type label
          :text "{{t('changes.pick')}}"
          :textColor @c.muted
          :height 24)
        (node #changesTitle
          :when "{{state.view != null}}"
          :type label
          :font heading
          :text "{{state.view.message}}"
          :textColor @c.text
          :height 26
          :role heading)
        (node #changesSummary
          :when "{{state.view != null}}"
          :type label
          :text "{{t('changes.by', {author: state.view.author}) + '   ' + t('changes.filesSummary', {n: length(state.view.files), added: computed.added, removed: computed.removed}) + (state.view.impact == null ? '' : '   ' + t('changes.layers', {view: state.view.impact.view, logic: state.view.impact.logic, data: state.view.impact.data}))}}"
          :textColor @c.muted
          :height 20)
        (node #changesStale
          :when "{{state.stale && !state.closed}}"
          :type label
          :wrap true
          :text "{{t('changes.stale')}}"
          :textColor @c.warning
          :anchors {:left 0 :right 0}
          :height "{{36}}")
        (node #changesClosed
          :when "{{state.closed}}"
          :type label
          :wrap true
          :text "{{t('changes.closed')}}"
          :textColor @c.muted
          :anchors {:left 0 :right 0}
          :height "{{36}}")
        (node #changesConflicts
          :when "{{length(state.view.conflicts ?? []) > 0}}"
          :type label
          :wrap true
          :text "{{t('changes.conflicts')}}"
          :textColor @c.warning
          :anchors {:left 0 :right 0}
          :height "{{36}}")
        (node #changesPerson
          :when "{{length(state.view.person ?? []) > 0}}"
          :type label
          :wrap true
          :text "{{t('changes.person', {lines: join(state.view.person, '; ')})}}"
          :textColor @c.warning
          :anchors {:left 0 :right 0}
          :height "{{36}}")
        (node #changesBehaviour
          :when "{{length(state.view.behaviour ?? []) > 0}}"
          :type label
          :wrap true
          :text "{{t('changes.behaviour', {lines: join(slice(state.view.behaviour, 0, 3), '; ') + (length(state.view.behaviour) > 3 ? '; …' : '')})}}"
          :textColor @c.warning
          :anchors {:left 0 :right 0}
          :height "{{36}}")
        (node #changesBody :when "{{state.view != null}}" :type row :grow 1 :gap 12
          (node #changesFiles
            :type column
            :width 240
            :gap 2
            :alignItems stretch
            :anchors {:top 0 :bottom 0}
            :scroll vertical
            (node #changesFilesTitle
              :type label
              :text "{{t('changes.files')}}"
              :font small
              :textColor @c.muted
              :height 20)
            (node #changesNoFiles
              :when "{{length(state.view.files) == 0}}"
              :type label
              :text "{{t('changes.none')}}"
              :textColor @c.muted
              :height 20)
            (node :repeat {:for f :in "{{state.view.files}}"}
              #"changesFile{{f.file}}"
              :role button
              :focusable true
              :cursor hand
              :height 52
              :radius 8
              :anchors {:left 0 :right 0}
              :color "{{state.file == f.file ? '@c.selected' : 'transparent'}}"
              :selected "{{state.file == f.file}}"
              :label "{{f.file}}"
              :onClick [{:run changes.open :with {:file "{{f.file}}"}}]
              {:type label :text "{{f.file}}" :textColor @c.text :anchors {:left 10 :right 10 :top 8} :height 18}
              {:type label
                :text "{{f.tooLarge == true ? t('changes.tooLarge') : '+' + f.added + ' −' + f.removed}}"
                :textColor @c.muted
                :font small
                :anchors {:left 10 :right 10 :top 28}
                :height 16})
            (node #changesExtrasTitle
              :when "{{length(computed.extras) > 0}}"
              :type label
              :text "{{t('changes.assets') + ' · ' + t('changes.run')}}"
              :font small
              :textColor @c.muted
              :height 20)
            (node :repeat {:for x :in "{{computed.extras}}"}
              #"changesExtra{{index}}"
              :type label
              :text "{{x}}"
              :textColor @c.text
              :font small
              :height 18))
          (node #changesPane :type column :grow 1 :gap 6 :alignItems stretch :anchors {:top 0 :bottom 0}
            (node #changesTools :type row :gap 6 :height 32
              (node #changesFileName
                :type label
                :text "{{state.file}}"
                :font code
                :textColor @c.text
                :grow 1
                :height 32
                :align "start center")
              (node #changesSplit
                :when "{{true}}"
                :role button
                :focusable true
                :cursor hand
                :height 32
                :radius 8
                :color "{{state.mode == 'split' ? '@c.selected' : '@c.raised'}}"
                :borderWidth 1
                :borderColor @c.border
                :label "{{t('changes.split')}}"
                :focused {:borderColor @c.accent :borderWidth 1}
                :onClick [{:set state.mode :to split}]
                :children [{:type label
                    :text "{{t('changes.split')}}"
                    :textColor @c.text
                    :anchors {:left 10 :right 10 :top 7}
                    :height 18
                    :align center}]
                :width 96
                :selected "{{state.mode == 'split'}}")
              (node #changesInline
                :when "{{true}}"
                :role button
                :focusable true
                :cursor hand
                :height 32
                :radius 8
                :color "{{state.mode == 'inline' ? '@c.selected' : '@c.raised'}}"
                :borderWidth 1
                :borderColor @c.border
                :label "{{t('changes.inline')}}"
                :focused {:borderColor @c.accent :borderWidth 1}
                :onClick [{:set state.mode :to inline}]
                :children [{:type label
                    :text "{{t('changes.inline')}}"
                    :textColor @c.text
                    :anchors {:left 10 :right 10 :top 7}
                    :height 18
                    :align center}]
                :width 88
                :selected "{{state.mode == 'inline'}}")
              (node #changesJson
                :when "{{true}}"
                :role button
                :focusable true
                :cursor hand
                :height 32
                :radius 8
                :color "{{state.view.notation == 'json' ? '@c.selected' : '@c.raised'}}"
                :borderWidth 1
                :borderColor @c.border
                :label JSON
                :focused {:borderColor @c.accent :borderWidth 1}
                :onClick [{:run changes.setNotation :with {:notation json}}]
                :children [{:type label
                    :text JSON
                    :textColor @c.text
                    :anchors {:left 10 :right 10 :top 7}
                    :height 18
                    :align center}]
                :width 60
                :selected "{{state.view.notation == 'json'}}")
              (node #changesLisp
                :when "{{true}}"
                :role button
                :focusable true
                :cursor hand
                :height 32
                :radius 8
                :color "{{state.view.notation == 'lisp' ? '@c.selected' : '@c.raised'}}"
                :borderWidth 1
                :borderColor @c.border
                :label Lisp
                :focused {:borderColor @c.accent :borderWidth 1}
                :onClick [{:run changes.setNotation :with {:notation lisp}}]
                :children [{:type label
                    :text Lisp
                    :textColor @c.text
                    :anchors {:left 10 :right 10 :top 7}
                    :height 18
                    :align center}]
                :width 60
                :selected "{{state.view.notation == 'lisp'}}"))
            (node #changesDiff
              :when "{{state.text != null && state.text.tooLarge != true}}"
              :type diff
              :grow 1
              :font code
              :textColor @c.text
              :original "{{state.text.before ?? ''}}"
              :modified "{{state.text.after ?? ''}}"
              :mode "{{state.mode}}"
              :deletedColor @c.deleted
              :insertedColor @c.inserted)
            (node #changesTooLarge
              :when "{{state.text != null && state.text.tooLarge == true}}"
              :type label
              :text "{{t('changes.tooLarge')}}"
              :textColor @c.muted
              :height 24)
            (node #changesLayoutNote
              :when "{{state.text != null}}"
              :type label
              :text "{{t('changes.canonical')}}"
              :font small
              :textColor @c.muted
              :height 16)))
        (node #changesMessage
          :when "{{state.message != ''}}"
          :type label
          :wrap true
          :text "{{state.message}}"
          :textColor @c.warning
          :height 36)
        (node #changesActions
          :when "{{computed.open || (state.view != null && state.view.kind == 'commit' && state.view.revision > 0)}}"
          :type row
          :gap 8
          :height 32
          (node #changesReason
            :when "{{computed.open}}"
            :type textarea
            :bind state.reason
            :placeholder "{{t('changes.reason')}}"
            :grow 1
            :height 32
            :color @c.surface
            :textColor @c.text
            :borderWidth 1
            :borderColor @c.border
            :radius 8
            :label "{{t('changes.reason')}}")
          (node #changesSpacer :when "{{!computed.open}}" :grow 1 :height 32 :color transparent)
          (node #changesPreview
            :when "{{computed.open}}"
            :role button
            :focusable true
            :cursor hand
            :height 32
            :radius 8
            :color @c.raised
            :borderWidth 1
            :borderColor @c.border
            :label "{{computed.previewing ? t('changes.endPreview') : t('changes.preview')}}"
            :focused {:borderColor @c.accent :borderWidth 1}
            :onClick [{:run changes.preview}]
            :children [{:type label
                :text "{{computed.previewing ? t('changes.endPreview') : t('changes.preview')}}"
                :textColor @c.text
                :anchors {:left 10 :right 10 :top 7}
                :height 18
                :align center}]
            :width 130)
          (node #changesReject
            :when "{{computed.open}}"
            :role button
            :focusable true
            :cursor hand
            :height 32
            :radius 8
            :color @c.raised
            :borderWidth 1
            :borderColor @c.border
            :label "{{t('changes.reject')}}"
            :focused {:borderColor @c.accent :borderWidth 1}
            :onClick [{:run changes.reject}]
            :children [{:type label
                :text "{{t('changes.reject')}}"
                :textColor @c.text
                :anchors {:left 10 :right 10 :top 7}
                :height 18
                :align center}]
            :width 96)
          (node #changesAccept
            :when "{{computed.open && !state.stale && length(state.view.conflicts ?? []) == 0}}"
            :role button
            :focusable true
            :cursor hand
            :height 32
            :radius 8
            :color @c.accent
            :borderWidth 0
            :borderColor @c.border
            :label "{{t('changes.accept')}}"
            :focused {:borderColor @c.accent :borderWidth 1}
            :onClick [{:run changes.accept}]
            :children [{:type label
                :text "{{t('changes.accept')}}"
                :textColor @c.onAccent
                :anchors {:left 10 :right 10 :top 7}
                :height 18
                :align center}]
            :width 96)
          (node #changesUndo
            :when "{{!computed.open}}"
            :role button
            :focusable true
            :cursor hand
            :height 32
            :radius 8
            :color @c.raised
            :borderWidth 1
            :borderColor @c.border
            :label "{{t('changes.undo')}}"
            :focused {:borderColor @c.accent :borderWidth 1}
            :onClick [{:run changes.undo}]
            :children [{:type label
                :text "{{t('changes.undo')}}"
                :textColor @c.text
                :anchors {:left 10 :right 10 :top 7}
                :height 18
                :align center}]
            :width 170)))))}
