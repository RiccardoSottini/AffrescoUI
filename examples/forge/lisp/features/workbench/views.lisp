;;; views.lisp --- the secondary side bar with the Outline, the breadcrumbs, and the JSON pages of settings and keys

;;; The Outline

(defcomponent forgeSecondarySideBar "The side bar at the right: the Outline of the active file, a row a symbol."
  {}
  `(column#secondary-side-bar :role complementary
     :label ~(t "secondary-side-bar.label")
     :width 240
     :height "100%"
     :color @color.sideBar
     :borderWidth 1
     :borderColor @color.border
     ~(outline-head)
     (label#outline-empty :when ~(empty? state.outline)
       :width "100%"
       :padding 12
       :wrap true
       :textColor @color.muted
       :text ~(t "outline-empty"))
     (list#outline :label ~(t "outline.label")
       :when ~(not (empty? state.outline))
       :width "100%"
       :grow 1
       :items ~computed.outlineItems
       :rowHeight 22
       :color @color.sideBar
       :textColor @color.text
       :selectedBackground @color.selection
       :accentColor @color.accent
       :onChange outlinePicked)))

(defview outline-head "The Outline's title, and Refresh." []
  `(row#outline-head :width "100%" :height 32 :alignItems center
     (label#outline-title :text ~(t "outline-title") :textColor @color.muted :grow 1 :height 32 :padding 12)
     (label#outline-refresh :role button
       :label ~(t "outline-refresh.label")
       :icon refresh-cw
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "outline-refresh.tip")
       :hover {:color @color.hover}
       :onClick refreshOutline)
     (label#outline-close :role button
       :label ~(t "outline-close.label")
       :icon x
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "outline-close.tip")
       :hover {:color @color.hover}
       :onClick toggleSecondarySidebar)))

;;; Breadcrumbs

(defcomponent forgeBreadcrumbs "Over the editor: the file's folders, then the symbols around the caret."
  {:path ""}
  `(row#breadcrumbs :role navigation
     :label ~(t "breadcrumbs.label")
     :width "100%"
     :height 22
     :alignItems center
     :color @color.editor
     :gap 2
     (node#breadcrumbs-start :width 10)
     ~(repeat [segment (split path "/") :index si]
       `(label#breadcrumb :role button
          :label ~segment
          :text ~segment
          :icon ~(if (> si 0) "chevron-right" "")
          :fitText true
          :padding 2
          :cursor hand
          :textColor @color.muted
          :hover {:textColor @color.text}
          :onClick ~(handler breadcrumbClicked path si)))
     ~(repeat [symbol computed.symbolTrail]
       `(label#breadcrumb-symbol :role button
          :label ~(trim symbol.label)
          :text ~(trim symbol.label)
          :icon chevron-right
          :fitText true
          :padding 2
          :cursor hand
          :textColor @color.muted
          :hover {:textColor @color.text}
          :onClick goToSymbol))))

;;; The JSON pages

(defcomponent forgeSettingsJsonPage "settings.json: the settings changed, as JSON to edit and Apply."
  {}
  `(column#settings-json-page :width "100%" :grow 1 :color @color.editor
     ~(settings-json-head)
     (editor#settings-json :font code
       :width "100%"
       :grow 1
       :bind state.settingsJson
       :grammar json
       :lineNumbers true
       :color @color.editor
       :textColor @color.editorText
       :lineNumberColor @color.muted
       :selectionColor @color.selection
       :caretColor @color.text
       :keys {"mod+enter" applySettingsJson})))

(defview settings-json-head "settings.json's head: what it is, Apply, and why the last Apply failed." []
  `(row#settings-json-head :width "100%" :height 30 :alignItems center :gap 8
     (label#settings-json-hint :text ~(t "settings-json-hint") :textColor @color.muted :padding 8 :grow 1)
     (label#settings-json-error :when ~(not (empty? state.settingsJsonError))
       :text ~state.settingsJsonError
       :textColor @color.error
       :fitText true)
     (label#settings-json-apply :role button
       :label ~(t "settings-json-apply.label")
       :text ~(t "settings-json-apply")
       :icon check
       :padding 6
       :fitText true
       :radius 2
       :cursor hand
       :color @color.accent
       :textColor "#ffffff"
       :focusable true
       :onClick applySettingsJson)
     (node#settings-json-end :width 4)))

(defcomponent forgeKeybindingsJsonPage "keybindings.json: every key, as JSON to read."
  {}
  `(editor#keybindings-json :font code
     :width "100%"
     :grow 1
     :readOnly true
     :text ~computed.keybindingsJson
     :grammar json
     :lineNumbers true
     :color @color.editor
     :textColor @color.editorText
     :lineNumberColor @color.muted
     :selectionColor @color.selection))
