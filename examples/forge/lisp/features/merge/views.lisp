;;; views.lisp --- what merge.lisp shows

;;; Views

(defcomponent forgeConflictButton "The forgeConflictButton component."
  {:run "" :text ""}
  `(label :role button
     :text ~text
     :fitText true
     :padding 6
     :height 22
     :radius 3
     :cursor hand
     :textColor @color.accent
     :hover {:color @color.hover}
     :onClick {:run ~run}))

(defcomponent forgeConflictBar "The forgeConflictBar component."
  {}
  `(row#conflict-bar :width "100%" :height 28 :gap 2 :alignItems center :color @color.tabs
     (label#conflict-summary :text ~computed.conflictSummary :textColor @color.muted :grow 1 :height 22 :padding 8)
     (node#conflict-accept-current :use forgeConflictButton
       :with {:run acceptCurrentChange :text ~(t "conflict-accept-current")})
     (node#conflict-accept-incoming :use forgeConflictButton
       :with {:run acceptIncomingChange :text ~(t "conflict-accept-incoming")})
     (node#conflict-accept-both :use
                                forgeConflictButton
                                :with
                                {:run acceptBothChanges :text ~(t "conflict-accept-both")})
     (node#conflict-next :use forgeConflictButton :with {:run nextConflict :text ~(t "conflict-next")})))
