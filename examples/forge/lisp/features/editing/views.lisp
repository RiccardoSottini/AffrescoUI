;;; views.lisp --- the find bar over the active editor: the query, its options and its buttons; the replacement

;;; The find bar

(defcomponent forgeFindBar
  "The find bar: Escape closes it, Shift+Enter finds the previous match, Alt+L finds in the selection."
  {}
  `(column#find-bar :width "100%"
     :height ~(if state.findBar.replacing 60 30)
     :justify center
     :gap 4
     :color @color.tabs
     :keys {:escape closeFind "shift+enter" findPrevious "alt+l" toggleFindInSelection}
     ~(find-row)
     ~(replace-row)))

(defview find-row "The query, its options, how many matches, and Previous, Next and Close." []
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
       :onChange ~(handler findStep "first")
       :onSubmit findNext)
     (node :use forgeFindOption :with {:option case :bind state.findBar.caseSensitive :text ~(t "findBar.aa")})
     (node :use forgeFindOption :with {:option word :bind state.findBar.wholeWord :text ~(t "findBar.word")})
     (node :use forgeFindOption :with {:option regex :bind state.findBar.regex :text ".*"})
     (node :use forgeFindOption :with {:option selection :bind state.findBar.inSelection :text ~(t "findBar.sel")})
     (label#find-count :text ~computed.findSummary :width 80 :textColor @color.muted)
     (node :use forgeFindButton
       :with {:name previous :icon arrow-up :label ~(t "findBar.previousMatch.label") :run findPrevious})
     (node :use forgeFindButton :with {:name next :icon arrow-down :label ~(t "findBar.nextMatch.label") :run findNext})
     (node :use forgeFindButton :with {:name close :icon x :label ~(t "findBar.close.label") :run closeFind})))

(defview replace-row "The replacement, and Replace and Replace All." []
  `(row#replace-row :when ~state.findBar.replacing :width "100%" :height 26 :gap 6 :alignItems center
     (textarea#replace-text :bind state.findBar.replace
       :grow 1
       :height 26
       :padding 5
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "replace-text.placeholder")
       :onSubmit replaceOne)
     (node :use forgeFindButton :with {:name replace :icon replace :label ~(t "findBar.replace.label") :run replaceOne})
     (node :use forgeFindButton
       :with {:name replace-all :icon replace-all :label ~(t "findBar.replaceAll.label") :run replaceAllInFile})))

(defcomponent forgeFindOption
  "An option of the query, a check box bound to `bind`; changing it finds again from the top."
  {:option "" :bind "" :text ""}
  `(row :id ~(str "find-" option) :height 26 :gap 4 :alignItems center
     (checkbox :id ~(str "find-" option "-box")
       :bind ~bind
       :width 16
       :height 16
       :onChange ~(handler findStep "first"))
     (label :text ~text :textColor @color.text :fitText true)))

(defcomponent forgeFindButton "A button of the bar, an icon that runs `run`."
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
