;;; views.lisp --- what lsp.lisp shows

;;; Views

(defcomponent forgeLsp "The forgeLsp component."
  {}
  `(column#lsp :width 0 :height 0
     (column#lsp-hover :width 420
       :color @color.widget
       :borderWidth 1
       :borderColor @color.border
       :x ~state.lsp.hover.x
       :y ~state.lsp.hover.y
       :overlay {:placement window :open state.lsp.hover.open :dismiss outside}
       (label#lsp-hover-text :width "100%"
         :padding 8
         :wrap true
         :fitText true
         :textColor @color.text
         :text ~state.lsp.hover.text))
     ~(lsp-signature)
     ~(lsp-completion)
     ~(lsp-peek)
     ~(lsp-rename-dialog-view)
     ~(lsp-review-dialog-view)
     ~(lsp-symbols-view)))

(defview lsp-signature "#lsp-signature." []
  `(column#lsp-signature :width 420
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :x ~state.lsp.at.x
     :y ~(max (- state.lsp.at.top 48) 0)
     :overlay {:placement window :open state.lsp.signature.open :dismiss outside}
     (label#lsp-signature-label :padding
                                6
                                :font
                                code
                                :fitText
                                true
                                :textColor
                                @color.text
                                :text
                                ~state.lsp.signature.label)
     (label#lsp-signature-doc :width "100%"
       :padding 6
       :wrap true
       :fitText true
       :when ~(not (empty? state.lsp.signature.documentation))
       :textColor @color.muted
       :text ~state.lsp.signature.documentation)))

(defview lsp-completion "#lsp-completion." []
  `(column#lsp-completion :width 360
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :x ~state.lsp.at.x
     :y ~state.lsp.at.y
     :overlay {:placement window :open state.lsp.completion.open :dismiss outside}
     ~(repeat [c (slice computed.lspCompletions (max (- state.lsp.completion.index 9) 0) (+ (max (- state.lsp.completion.index 9) 0) 10)) :index ci]
       `(row#lsp-completion-item :width "100%"
          :height 22
          :gap 8
          :color ~(if (= c.index state.lsp.completion.index) "@color.selection" "@color.widget")
          :cursor hand
          :onClick ~(handler lspComplete c.index)
          (label#lsp-completion-label :font code :fitText true :textColor @color.text :text ~c.item.label)
          (label#lsp-completion-detail :grow 1 :textColor @color.muted :text ~c.item.detail)))))

(defview lsp-peek "#lsp-peek." []
  `(column#lsp-peek :width 640
     :height 260
     :color @color.widget
     :borderWidth 1
     :borderColor @color.border
     :overlay {:placement center :open state.lsp.peek.open :dismiss outside}
     (label#lsp-peek-title :width "100%" :padding 6 :textColor @color.text :text ~state.lsp.peek.title)
     (table#lsp-peek-list :width "100%"
       :grow 1
       :items ~computed.lspPeekRows
       :rowHeight 22
       :color @color.widget
       :textColor @color.text
       :selectedBackground @color.selection
       :accentColor @color.accent
       :columns [{:id text :title "" :width 360}
                 {:id file :title ~(t "lsp-peek-list.file") :width 200}
                 {:id line :title ~(t "lsp-peek-list.line") :width 60}]
       :onOpen lspPeekOpen)))

(defview lsp-rename-dialog-view "#lsp-rename-dialog." []
  `(node#lsp-rename-dialog :use dialog
     :with {:open state.lsp.rename.open
            :height 150
            :title "Rename Symbol"
            :message ""
            :buttons [{:title ~(t "lsp-rename-dialog.rename") :run lspRenameSend} {:title ~(t "lsp-rename-dialog.cancel")}]}
     (textarea#lsp-rename-name :bind state.lsp.rename.name
       :autofocus true
       :anchors {:left 20 :right 20 :top 48}
       :height 28
       :padding 7
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "lsp-rename-name.placeholder")
       :onSubmit lspRenameSend)))

(defview lsp-review-dialog-view "#lsp-review-dialog." []
  `(node#lsp-review-dialog :use dialog
     :with {:open state.lsp.review.open
            :height 320
            :title ~(if (= state.lsp.review.kind "rename") "Rename Symbol" "Format Document")
            :message ~(str "Apply "
                           (plural (count state.lsp.review.edits) "edit")
                           " in "
                           (plural state.lsp.review.files "file")
                           "? Open files are changed in their editors and left unsaved; others are written.")
            :buttons [{:title ~(t "lsp-review-dialog.apply") :run lspApply} {:title ~(t "lsp-review-dialog.cancel")}]}
     (table#lsp-review-list :anchors {:left 20 :right 20 :top 80 :bottom 56}
       :items ~(map-indexed (fn [index it]
                              {:id index
                               :cells [it.path
                                       (+ it.line 1 ":" (+ it.column 1))
                                       (slice (join "⏎" (split it.text "\n")) 0 80)]})
                            state.lsp.review.edits)
       :rowHeight 20
       :color @color.widget
       :textColor @color.text
       :columns [{:id file :title ~(t "lsp-review-list.file") :width 200}
                 {:id at :title ~(t "lsp-review-list.at") :width 70}
                 {:id text :title ~(t "lsp-review-list.newText") :width 260}])))

(defview lsp-symbols-view "#lsp-symbols." []
  `(node#lsp-symbols :use quickPick
     :with {:name symbolPick
            :width 600
            :color @color.widget
            :textColor @color.text
            :dimColor @color.muted
            :accent @color.accent
            :highlight @color.selection
            :field @color.input
            :providers [{:prefix @
                         :items ~computed.lspSymbolItems
                         :key name
                         :run lspSymbolPicked
                         :placeholder ~(t "lsp-symbols.goToA")}
                        {:prefix "#"
                         :items ~computed.lspSymbolItems
                         :key name
                         :run lspSymbolPicked
                         :placeholder ~(t "lsp-symbols.typeToSearch")}]}))
