;;; views.lisp --- what editing.lisp shows

;;; Views

(defcomponent forgeFindBar "The forgeFindBar component."
  {}
  `(column#find-bar :width "100%"
     :height ~(if state.findBar.replacing 60 30)
     :justify center
     :gap 4
     :color @color.tabs
     :keys {:escape closeFind "shift+enter" findPrevious "alt+l" toggleFindInSelection}
     ~(find-row)
     ~(replace-row)))

(defcomponent forgeFindOption "The forgeFindOption component."
  {:option "" :bind "" :text ""}
  `(row :id ~(str "find-" option) :height 26 :gap 4 :alignItems center
     (checkbox :id ~(str "find-" option "-box")
       :bind ~bind
       :width 16
       :height 16
       :onChange {:run findStep :with {:step first}})
     (label :text ~text :textColor @color.text :fitText true)))

(defcomponent forgeFindButton "The forgeFindButton component."
  {:name "" :icon "" :label "" :run ""}
  `(label :id ~(str "find-" name)
     :role button
     :label ~label
     :tooltip ~label
     :icon ~icon
     :width 24
     :height 24
     :radius 4
     :cursor hand
     :focusable true
     :textColor @color.text
     :hover {:color @color.hover}
     :onClick {:run ~run}))

(defview find-row "#find-row." []
  `(row#find-row :width "100%" :height 26 :gap 6 :alignItems center
     (textarea#find-text :bind state.findBar.text
       :grow 1
       :height 26
       :padding 5
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "find-text.placeholder")
       :autofocus true
       :onChange {:run findStep :with {:step first}}
       :onSubmit {:run findNext})
     (node :use forgeFindOption :with {:option case :bind state.findBar.caseSensitive :text ~(t "findBar.aa")})
     (node :use forgeFindOption :with {:option word :bind state.findBar.wholeWord :text ~(t "findBar.word")})
     (node :use forgeFindOption :with {:option regex :bind state.findBar.regex :text ".*"})
     (node :use forgeFindOption :with {:option selection :bind state.findBar.inSelection :text ~(t "findBar.sel")})
     (label#find-count :text ~computed.findSummary :width 80 :textColor @color.muted)
     (node :use forgeFindButton
       :with {:name previous :icon arrow-up :label ~(t "findBar.previousMatch.label") :run findPrevious})
     (node :use
           forgeFindButton
           :with
           {:name next :icon arrow-down :label ~(t "findBar.nextMatch.label") :run findNext})
     (node :use forgeFindButton :with {:name close :icon x :label ~(t "findBar.close.label") :run closeFind})))

(defview replace-row "#replace-row." []
  `(row#replace-row :when ~state.findBar.replacing :width "100%" :height 26 :gap 6 :alignItems center
     (textarea#replace-text :bind state.findBar.replace
       :grow 1
       :height 26
       :padding 5
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "replace-text.placeholder")
       :onSubmit {:run replaceOne})
     (node :use
           forgeFindButton
           :with
           {:name replace :icon replace :label ~(t "findBar.replace.label") :run replaceOne})
     (node :use forgeFindButton
       :with {:name replace-all :icon replace-all :label ~(t "findBar.replaceAll.label") :run replaceAllInFile})))
