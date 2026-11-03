;;; index.screen.lisp --- TODO

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "state.lisp"
         "computed.lisp"
         "look.lisp"
         "widgets.lisp"
         "list.lisp"
         "detail.lisp"
         "dialogs.lisp"
         "settings.lisp"
         "sync.lisp"
         "habits.lisp")

(defview body "#body." []
  `(node#body :fill 0 :color transparent :dir auto :every {:ms 60000 :do [{:run tick}]}
     ~(list-pane)
     (node#paneDivider
     :when ~computed.wide
     :anchors {:left ~computed.listWidth :top 0 :bottom 0}
     :width 1
     :color @c.divider)
     (node#detailPane :anchors {:right 0 :top 0 :bottom 0}
       :width ~computed.detailWidth
       :visible ~(or computed.wide (= state.page "detail"))
       :color @c.bg
       ~(detail-cards)
       (label#label-t-no-selection :text ~(t "noSelection")
         :when ~(< computed.sel 0)
         :textColor @c.muted
         :anchors {:left 0 :right 0 :top 200}
         :height 20
         :align center)
       ~(detail-bar))
     ~(entry-popup)
     (node#editDialog :use dialog
       :with {:open state.edit.open
              :title ~(if (= state.edit.id "") (t "edit.new") (t "edit.edit"))
              :message ""
              :width 600
              :height ~(if (= state.edit.kind "measurable") 500 436)
              :highlight ~computed.accent
              :buttons ~[{:title (t "save") :run "saveHabit"} {:title (t "cancel")}]}
       ~(edit-fields))
     (node#deleteDialog :use dialog
       :with {:open state.confirmDelete
              :title ~(t (if computed.picking "delete.titleN" "delete.title"))
              :message ~(if computed.picking
                            (t "delete.messageN" {:n (count state.picked)})
                            (t "delete.message" {:name (if (>= computed.sel 0) computed.h.name "")}))
              :highlight @c.danger
              :buttons ~[{:title (t "delete") :run "deleteHabit"} {:title (t "cancel")}]})
     ~(settings-page)
     (node#palette :use quickPick
       :with {:providers ~[{:placeholder (t "palettePlaceholder") :items computed.palette :key "title"}]
              :color @c.card
              :textColor @c.text
              :dimColor @c.muted
              :accent @c.primary
              :highlight @c.selected
              :field @c.bg})
     (node#toasts :use
                  toasts
                  :with
                  {:name toasts :width 344 :color @c.snack :textColor @c.snackText :accent @c.snackAction})))

(defscreen main
  :window {:title "Streak" :width 1280 :height 800 :minWidth 400 :minHeight 560}
  :strings {:default en :locale ~state.locale :required true :keep ["Streak"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [icons]}
          :tiny {:file "../../shared/fonts/Roboto-Regular.ttf" :size 11 :fallback [icons]}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 17 :fallback [icons]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 20 :fallback [icons]}
          :big {:file "../../shared/fonts/Roboto-Regular.ttf" :size 26 :fallback [icons]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 20}
          :mark {:file "../../shared/fonts/Lucide.ttf" :size 22}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 14}}
  :persist [state.nextId
            state.selected
            state.sort
            state.hideCompleted
            state.showArchived
            state.theme
            state.pureBlack
            state.firstWeekday
            state.reverseDays
            state.skipDays
            state.reminders
            state.locale
            state.scorePeriod
            state.historyPeriod]
  :permissions {:allow [studio]
                :ask ["agent(/actions)" "agent(/program)"]
                :deny ["*(#appBar)"]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  :theme ~computed.theme
  (node#root :color @c.bg
    ~(body)))
