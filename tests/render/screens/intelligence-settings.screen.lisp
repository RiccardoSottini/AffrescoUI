;;; intelligence-settings.screen.lisp --- The intelligence settings add-on's Lisp version, for the rendering check
;;;
;;; The page of intelligence-settings.screen.json, on the add-on's settings.lisp, compared with the same pictures
;;; (lisp/intelligence.json). The check's runner has none of the host's intelligence.* natives, so it shows the
;;; add-on's stand-in providers; test actions stand in for a saved provider, a test's answer and a local server's
;;; models list.

(include "../../../examples/shared/addons/intelligence/settings.lisp")

;;; Definitions

(deftokens tokens "The page's colour." {:page {:light "#eef1f4" :dark "#0d1117"}})

(defstate theme light "Light or dark.")

(defstate locale en "The language.")

(defpart test "What stands in for the host: a provider in use, a local server's answers, a theme and a language."
  :actions {:test.connected [{:set state.intelligence.data.current :to {:provider "claude-code" :model "sonnet"}}
                             {:set state.intelligence.test
                              :to {:ok true :message "Connected: Claude Code answered with sonnet."}}]
            :test.noServer [{:set state.intelligence.modelsError :to "Can't reach Local LLM (connection refused)."}]
            :test.noModel [{:set state.intelligence.modelsError :to ""}
                           {:set state.intelligence.modelsLoaded :to true}]
            :test.dark [{:set state.theme :to dark}]
            :test.french [{:set state.locale :to fr}]})

;;; The screen

(defscreen main "A page with the container the AffrescoUI Launcher gives the settings."
  :window {:title "Intelligence" :width 800 :height 900}
  :fonts {:default {:file "../../../examples/shared/fonts/Roboto-Regular.ttf" :size 14}}
  :strings {:locale ~state.locale}
  :theme ~state.theme
  (node :color @page
    (node#settingsIntelligence :fill true :color transparent
      (node#intelligenceSettingsUse :use intelligenceSettings))))
