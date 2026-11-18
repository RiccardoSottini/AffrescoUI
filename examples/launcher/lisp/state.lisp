;;; state.lisp --- what the Launcher keeps while it runs, and what it works out from it
;;;
;;; Each defstate is one key of the state, with the value it starts from; the examples are state.examples, in
;;; apps.lisp. A defcomputed is a value the views read as computed.<name>.

(ns launcher.state)

;;; The page and the person's choices

(defstate page apps "The page shown." :type {:enum [apps mine create settings install catalogue]})

(defstate query "" "The search over the apps.")

(defstate theme system "Light, dark, or the system's." :type {:enum [system light dark]})

(defstate locale system "The language: English, Spanish, French, or the system's." :type {:enum [system en es fr]})

(defstate notation lisp "The notation each example opens in: Lisp, the standard, or JSON." :type {:enum [json lisp]})

(defstate toasts [] "The notes at the bottom, the newest last.")

;;; The person's apps

(defstate mine [] "The person's apps, the last changed first: each one's folder, where its files are, name and date.")

(defstate mineScan [] "The folders of the apps folder, as the last read listed them.")

(defstate mineNext [] "The apps read so far by the read under way.")

(defstate mineLoaded false "Whether the apps folder has been read.")

(defstate mineError "" "Why the apps folder couldn't be read, or empty.")

(defstate mineRun 0 "The number of the last read: an earlier read still under way stops when it sees a later one.")

(defstate confirmOpen false "Whether the question before a delete is open.")

(defstate confirm {:id "" :name ""} "The app the question is about.")

(defstate tileColors ["#5b4fe0" "#e0569a" "#1f9d62" "#e08a1e" "#2a86d6" "#9b4fd6"]
  "The colours of the apps' initials, in turn.")

(defcomputed myIds "The folders of the person's apps." (map :id state.mine))

;;; Creating an app

(defstate create {:name "" :from blank :describe "" :features [] :tried false :busy false :error ""}
  "The new app: its name, what it starts from, its description, the features asked for, and how it is going.")

(defstate featureChoices
  [{:id board :from "Flow" :ask "a Kanban board with columns, cards and drag and drop, like Flow's (examples/flow)"}
   {:id palette :from "Flow" :ask "a command palette (Cmd+K) listing the app's actions, like Flow's"}
   {:id charts :from "Haven" :ask "charts from ../../shared/components/charts.json, like Haven's energy page"}
   {:id heatmap :from "Streak" :ask "a year heatmap of daily entries, like Streak's"}
   {:id settings :from "Arcadia" :ask "a settings page with declared settings, like Arcadia's"}
   {:id signIn :from "Sign in" :ask "sign-in and create-account screens with checked fields, like the Sign in app's"}
   {:id slideSorter
    :from "Sinopia"
    :ask "Sinopia's slide sorter (the installable part slideSorter of app:sinopia), bound with an adapter"}]
  "The features of other apps a new app can ask for: what the assistant is told for each.")

(defcomputed starters "The examples a new app can start from: those the host runs, but those that say no."
  (filter #(and (= (:program %) :host) (not= (:starter %) false)) state.examples))

(defcomputed reserved "The folder names an app can't take."
  (concat (map :folder state.examples) ["shared" "launcher"]))

(defcomputed slug "The new app's folder: its name in lower case, letters and digits, words joined by dashes."
  (slice (join "-"
               (filter #(not= % "")
                       (split (->> (split (lower-case (trim state.create.name)) "")
                                   (map #(if (contains "abcdefghijklmnopqrstuvwxyz0123456789" %) % " "))
                                   (join ""))
                              " ")))
         0
         40))

(defcomputed askText "What the assistant is asked: the description, and the features from other apps."
  (trim (+ (trim state.create.describe)
           (if (pos? (count state.create.features))
             (str "\n\nAlso add these features from other apps: "
                  (->> state.featureChoices (filter #(contains state.create.features (:id %))) (map :ask) (join "; "))
                  ".")
             ""))))

(defcomputed createError "What is wrong with the new app, as a key of the locales, or empty."
  (cond (= (trim state.create.name) "") "create.error.empty"
        (> (count (trim state.create.name)) 80) "create.error.long"
        (or (contains state.create.name (str "{" "{")) (contains state.create.name (str "}" "}"))) "create.error.braces"
        (= computed.slug "") "create.error.letters"
        (contains computed.reserved computed.slug) "create.error.reserved"
        (contains computed.myIds computed.slug) "create.error.exists"
        (> (count computed.askText) 4000) "create.error.describe"
        :else ""))

(defcomputed fromName "The name of what the new app starts from."
  (if (= state.create.from :blank)
    (t "create.blank")
    (or-else (:name (find #(= (:id %) state.create.from) state.examples)) "")))

(defcomputed excluded "What a copy of an example leaves out: its C++, its build files, its hidden files."
  ["*.cpp" "*.hpp" "*.h" "*.c" "*.cc" "*.mm" "*.cmake" "CMakeLists.txt" "*.py" ".*"])

;;; The window

(defcomputed wide "Whether the window is wide enough for the sidebar's names." (>= window.width 960))

(defcomputed sideWidth "The sidebar's width." (if computed.wide 232 72))

(defcomputed columns "How many cards the Apps page fits in a row."
  (-> (- window.width computed.sideWidth 52) (/ 290) floor (max 1)))

(defcomputed everyApp "The person's apps, then the examples, as cards."
  ; style: keep, #(...) can't give a map, and a binding's function names its item it
  (concat (map (fn [it] {:id it.id :name it.name :modified it.modified :mine true}) state.mine) state.examples))

(defcomputed shown "The apps whose name, or tagline, holds the search."
  (filter #(or (= (trim state.query) "")
               (contains (lower-case (str (:name %) " " (if (:mine %) (:id %) (t (str "tagline." (:id %))))))
                         (lower-case (trim state.query))))
          computed.everyApp))
