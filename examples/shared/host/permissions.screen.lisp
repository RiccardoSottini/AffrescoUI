;;; permissions.screen.lisp --- the host's question before it opens an app
;;;
;;; What the app asks for, from its app.json (apps/host/Permissions.cpp), each capability with the app's reason (none
;;; for a screen without an app.json), all checked to start; a click anywhere on a card flips it. The host gives the app
;;; with the native permissions.app and takes the person's answer with permissions.decide; closing the window allows
;;; nothing, and the host asks again next time. It is read from the host's assets, never from the app.

(include "locales/permissions.en.lisp"
         "locales/permissions.es.lisp"
         "locales/permissions.fr.lisp")

;;; What the host asks about

(defstate app {:name "" :capabilities []}
  "The app's name and the capabilities it asks for: each {key kind name why allowed}, from permissions.app.")

(defn loadApp "Asks the host which app this is, and what it asks for." []
  (call "permissions.app" :into "state.app"))

(defn flip "Checks or unchecks the i-th capability." [i allowed]
  (set (str "state.app.capabilities[" i "].allowed") allowed))

(defn decide "Gives the host the person's answer: the capabilities checked, or none, and the window closes." [all]
  (call "permissions.decide"
        :with {:allowed (if all
                          (map (fn [capability] (:key capability))
                               (filter (fn [capability] (= (:allowed capability) true)) state.app.capabilities))
                          [])}))

;;; The look

(deftokens prompt "The window's colours, light and dark."
  {:prompt.background {:light "#f6f5fb" :dark "#111118"}
   :prompt.surface {:light "#ffffff" :dark "#1c1c26"}
   :prompt.border {:light "#e3e1ee" :dark "#2b2b39"}
   :prompt.text {:light "#1b1a28" :dark "#ececf4"}
   :prompt.muted {:light "#5d5b72" :dark "#a4a2b9"}
   :prompt.accent {:light "#5b4fe0" :dark "#8b80ff"}
   :prompt.onAccent "#ffffff"})

;;; The views

(defview capability-text "What a capability allows, and the app's reason in its own words, quoted as the app's."
  [capability]
  `(column#about :grow 1 :fitContent height :alignItems stretch
     (node#top :height 6 :color transparent)
     (label#what :padding 4
       :wrap true
       :fitText {:maxWidth ~(- window.width 116)}
       :textColor @prompt.text
       :text ~(t (str "permissions." capability.kind) {:name capability.name}))
     (label#why :when ~(not= capability.why "")
       :padding 4
       :wrap true
       :fitText {:maxWidth ~(- window.width 116)}
       :textColor @prompt.muted
       :text ~(t "permissions.says" {:name state.app.name :why capability.why}))
     (node#bottom :height 6 :color transparent)))

(defview capability-card "A capability as a card: its box and its text; a click anywhere on it flips the box."
  [capability i]
  `(row#capability :fitContent height
     :minHeight 64
     :gap 12
     :alignItems center
     :color @prompt.surface
     :radius 10
     :cursor hand
     :onClick ~(handler flip i (not capability.allowed))
     (node#inset :width 2)
     (checkbox#allowed :width 20
       :height 20
       :checked ~capability.allowed
       :activeColor @prompt.accent
       :onChange ~(handler flip i event.value))
     ~(capability-text capability)
     (node#end :width 4 :color transparent)))

(defview answer-button "One of the two answers." [id width color text-color key all]
  `(label :id ~id
     :width ~width
     :radius 8
     :color ~color
     :textColor ~text-color
     :align center
     :text ~(t key)
     :onClick ~(handler decide all)))

;;; The window

(defscreen main "The question: the app's name, a card per capability, a hint, and the two answers."
  :window {:title "Permissions" :width 560 :height 460 :minWidth 420 :minHeight 320}
  :theme ~window.colorScheme
  :onLoad loadApp
  :fonts {:default {:file "../fonts/Roboto-Regular.ttf" :size 15 :fallback [icons]}
          :title {:file "../fonts/Roboto-Regular.ttf" :size 22 :bold true}
          :icons {:file "../fonts/Lucide.ttf" :size 15}}
  :strings {:default en}
  (node#prompt :color @prompt.background
    (column#content :fill 24 :gap 12 :alignItems stretch
      (label#title :height 32 :font title :textColor @prompt.text :text ~(t "permissions.asks" {:name state.app.name}))
      (column#list :grow 1 :gap 8 :scroll vertical :alignItems stretch
        ~(repeat [capability state.app.capabilities :index i :key capability.key] (capability-card capability i)))
      (label#hint :height 40 :wrap true :textColor @prompt.muted :text ~(t "permissions.hint"))
      (row#buttons :height 40 :gap 12 :justify end :alignItems stretch
        ~(answer-button "none" 150 "@prompt.surface" "@prompt.text" "permissions.none" false)
        ~(answer-button "allow" 170 "@prompt.accent" "@prompt.onAccent" "permissions.allow" true)))))
