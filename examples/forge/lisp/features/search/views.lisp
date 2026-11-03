;;; views.lisp --- what search.lisp shows

;;; Views

(defcomponent forgeSearchEditor "The forgeSearchEditor component."
  {}
  `(editor#search-editor :label ~(t "search-editor.label")
     :font code
     :width "100%"
     :grow 1
     :text ~computed.searchEditorText
     :readOnly true
     :lineNumbers false
     :caret state.searchEditor.caret
     :color @color.editor
     :textColor @color.editorText
     :selectionColor @color.selection
     :lineColor @color.hover
     :caretColor @color.text
     :scrollMarks true
     :find ~(if (or (empty? state.search.query) (nil? state.search.result))
                nil
                {:text state.search.query
                 :regex state.search.regex
                 :wholeWord state.search.wholeWord
                 :caseSensitive state.search.caseSensitive})
     :keys {:enter openSearchEditorLine}
     :onDoubleClick {:run openSearchEditorLine}))

(defcomponent forgeSearchPage "The forgeSearchPage component."
  {}
  `(column#search-page :width "100%" :grow 1 :gap 4
     (node :use forgeViewTitle :with {:text ~(t "searchPage.search")})
     (node#search-query-row :use forgeInset :height 26
       (row#search-query-box :grow 1 :height 26 :color @color.input :alignItems center
         (node :use forgeSearchField :with {:field query :hint ~(t "searchPage.search.hint")} :onSubmit {:run runSearch})
         (node :use forgeSearchOption
           :with {:option case :bind caseSensitive :icon case-sensitive :tip ~(t "searchPage.matchCase.tip")})
         (node :use forgeSearchOption
           :with {:option word :bind wholeWord :icon whole-word :tip ~(t "searchPage.matchWholeWord.tip")})
         (node :use forgeSearchOption
           :with {:option regex :bind regex :icon regex :tip ~(t "searchPage.useRegularExpression.tip")})))
     (node#search-replace-row :use forgeInset :height 26 :alignItems center
       (node :use forgeSearchField :with {:field replace :hint ~(t "searchPage.replace.hint")})
       (label#search-replace-all :role button
         :label ~(t "search-replace-all.label")
         :icon replace-all
         :font icons
         :width 26
         :height 26
         :align center
         :cursor hand
         :tooltip ~(t "search-replace-all.tip")
         :textColor @color.text
         :hover {:color @color.hover}
         :onClick {:run askReplace}))
     (node :use forgeSearchFilter
       :with {:title ~(t "searchPage.filesToInclude") :filter include :example ~(t "searchPage.eGSrc.example")})
     (node :use forgeSearchFilter
       :with {:title ~(t "searchPage.filesToExclude") :filter exclude :example ~(t "searchPage.eGBuild.example")})
     ~(search-page-node)
     ~(search-results)
     (node#search-menu :use contextMenu
       :with {:open state.search.menu :items ~menus.searchResult :x ~state.search.x :y ~state.search.y})
     (node :use dialog
       :with {:open state.search.confirm
              :title "Replace All"
              :message ~(if state.search.result
                            (str "Replace "
                                 (plural state.search.result.matches "match" "matches")
                                 " in "
                                 (plural state.search.result.files "file")
                                 " with \""
                                 state.search.replace
                                 "\"? Files with unsaved edits are skipped.")
                            "")
              :buttons [{:title ~(t "searchPage.replace") :run replaceAll} {:title ~(t "searchPage.cancel")}]})))

(defcomponent forgeInset "The forgeInset component."
  {}
  `(row :width "100%"
     {:width 12}
     {:slot true}
     {:width 12}))

(defcomponent forgeSearchFilter "The forgeSearchFilter component."
  {:title "" :filter "" :example ""}
  `(node :use forgeInset :height 44
     (column :grow 1 :height 44
       (label :text ~title :textColor @color.muted :width "100%" :height 18)
       (node :use
             forgeSearchField
             :with
             {:field ~filter :hint ~example}
             :width
             "100%"
             :grow
             0
             :onSubmit
             {:run runSearch}))))

(defcomponent forgeSearchField "The forgeSearchField component."
  {:field "" :hint ""}
  `(textarea :id ~(str "search-" field)
     :bind ~(str "state.search." field)
     :grow 1
     :height 26
     :padding 5
     :color @color.input
     :textColor @color.text
     :placeholderColor @color.muted
     :placeholder ~hint))

(defcomponent forgeSearchOption "The forgeSearchOption component."
  {:option "" :bind "" :icon "" :tip ""}
  `(label :id ~(str "search-" option)
     :role checkbox
     :label ~tip
     :checked ~(get state.search bind)
     :icon ~icon
     :font icons
     :width 22
     :height 22
     :align center
     :cursor hand
     :tooltip ~tip
     :radius 3
     :textColor @color.text
     :borderWidth 1
     :borderColor ~(if (get state.search bind) "@color.accent" "@color.input")
     :color ~(if (get state.search bind) "@color.selection" "@color.input")
     :hover {:color @color.hover}
     :onClick {:toggle ~(str "state.search." bind)}))

(defview search-page-node "#search-page-node." []
  `(node :use forgeInset :height 22
     (label#search-summary :text ~(if state.search.busy
                                      (str "Searching... " computed.searchSummary)
                                      computed.searchSummary)
       :textColor @color.muted
       :grow 1
       :height 22)
     (label#search-open-editor :when ~(some? state.search.result)
       :role button
       :label ~(t "search-open-editor.label")
       :icon file-text
       :font icons
       :width 22
       :height 22
       :align center
       :cursor hand
       :radius 3
       :tooltip ~(t "search-open-editor.tip")
       :textColor @color.text
       :hover {:color @color.hover}
       :onClick {:run openSearchEditor})))

(defview search-results "#search-results." []
  `(tree#search-results :when ~(some? state.search.result)
     :width "100%"
     :grow 1
     :items ~state.search.result.tree
     :expanded ~computed.searchExpanded
     :bind state.search.selected
     :rowHeight 22
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onExpand [{:set state.search.collapsed
                 :to ~(if event.expanded
                          (filter (fn [it] (not= it event.id)) state.search.collapsed)
                          (concat state.search.collapsed [event.id]))}]
     :onOpen {:run openSearchHit}
     :keys {:delete searchDismiss :backspace searchDismiss}
     :onContextMenu [{:set state.search.x :to ~event.x}
                     {:set state.search.y :to ~event.y}
                     {:set state.search.menu :to true}]))
