;;; search.lisp --- what the search box runs: the named commands, and every command of the ribbon's menus
;;;
;;; The search box lists the commands whose titles hold what is typed. Besides the named ones (parts/menus.lisp), it
;;; finds every item of the ribbon's drop-down menus (paste, layouts, sections, case, lists, spacing, anchor, select,
;;; arrange, colours, fonts, effects, icons, styled text, shapes, themes), run as the menu runs it.

(ns sinopia.parts.search)

(defcomputed searchMenus "The ribbon's menus whose items the search box finds too."
  ["menu-b-paste" "menu-b-copy" "menu-b-new-slide" "menu-b-layout" "menu-b-section" "menu-b-case" "menu-b-bullets"
   "menu-b-numbering" "menu-b-line-spacing" "menu-b-columns" "menu-b-direction" "menu-b-anchor" "menu-b-select"
   "menu-b-arrange" "menu-b-colors" "menu-b-fonts" "menu-b-effects" "menu-b-ins-icons" "menu-b-ins-art-text"
   "menu-b-ins-header" "menu-b-ins-shapes" "menu-gal-themes"])

(defcomputed commandTitles "The named commands' titles." (map :title computed.commands))

(defcomputed searchCommands "What the search box can run: the named commands, then the menus' items not among them."
  (let [items (flat (map #(or-else (get computed %) []) computed.searchMenus))
        runnable (filter #(and (some? (:title %)) (some? (:run %))) items)]
    (concat computed.commands (remove #(contains computed.commandTitles (:title %)) runnable))))
