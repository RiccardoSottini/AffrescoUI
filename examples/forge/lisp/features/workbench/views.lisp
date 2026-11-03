;;; views.lisp --- what workbench.lisp shows

;;; Views

(defcomponent forgeSecondarySideBar "The forgeSecondarySideBar component."
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
       :onChange {:run outlinePicked})))

(defcomponent forgeBreadcrumbs "The forgeBreadcrumbs component."
  {:path ""}
  `(row#breadcrumbs :role navigation
     :label ~(t "breadcrumbs.label")
     :width "100%"
     :height 22
     :alignItems center
     :color @color.editor
     :gap 2
     {:id breadcrumbs-start :width 10}
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
          :onClick [{:if ~(< si (- (count (split path "/")) 1))
                     :then [{:run revealFolder :with {:path ~(join "/" (slice (split path "/") 0 (+ si 1)))}}]
                     :else [{:run goToSymbol}]}]))
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
          :onClick {:run goToSymbol}))))

(defcomponent forgeSettingsJsonPage "The forgeSettingsJsonPage component."
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

(defcomponent forgeKeybindingsJsonPage "The forgeKeybindingsJsonPage component."
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

(defview outline-head "#outline-head." []
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
       :onClick {:run refreshOutline})
     (label#outline-close :role button
       :label ~(t "outline-close.label")
       :icon x
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "outline-close.tip")
       :hover {:color @color.hover}
       :onClick {:run toggleSecondarySidebar})))

(defview settings-json-head "#settings-json-head." []
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
       :onClick {:run applySettingsJson})
     {:id settings-json-end :width 4}))
