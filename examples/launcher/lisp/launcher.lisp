;;; launcher.lisp --- what the Launcher does: its functions, its commands, and the services it asks the host for
;;;
;;; The views say what is shown; this file says what happens. The examples folder is read only, the apps folder read
;;; and written, and apps are opened by the host's program: each through a service whose answer comes later. A
;;; function that calls one is run by name with its variables, (run "readMine" :with {:i i ...}), so that the function
;;; its answer runs (:onDone readDone) finds them by name, beside `result` or `error`; a closure's values would be gone
;;; by then. Where nothing is granted (the rendering check), every call is refused, and the Launcher shows the examples
;;; and an empty My apps.

;;; What the host gives, and runs by name

(defpart host "The services, the read of the apps folder on load, and the actions the host and scenarios run."
  :services {:examples {:provider fs :root examples :readOnly true}
             :apps {:provider fs :root apps :copyFrom [examples]}
             :launch {:provider process :process affresco}}
  :onLoad {:run launcher.refresh}
  :actions {:launcher.refresh [{:lisp refreshMine}]
            :launcher.show [{:lisp showMineList}]})

;;; Pages and notes

(defn go "Shows a page." [page] (set "state.page" page))

(defn clearToasts "Takes the notes away." [] (set "state.toasts" []))

(defn toast "A note at the bottom for a few seconds, in red when it is an error." [message error]
  (push "state.toasts" :value {:message message :color (if error "@c.danger" "@c.accent")})
  (debounce 4000 clearToasts))

(defn showError "A service's error, as a note." [error] (toast (:message error) true))

(defcommand showApps "Shows the Apps page."
  :title ~(t "nav.apps") :description ~(t "command.showApps.about") :category ~(t "category.view")
  :icon "layout-grid" []
  (go "apps"))

(defcommand showMine "Shows My apps."
  :title ~(t "nav.mine") :description ~(t "command.showMine.about") :category ~(t "category.view")
  :icon "folder-heart" []
  (go "mine"))

(defcommand showCreate "Shows the Create page, its name field focused."
  :title ~(t "nav.create") :description ~(t "command.showCreate.about") :category ~(t "category.view")
  :icon "circle-plus" []
  (go "create")
  (focus "#createName"))

(defcommand showSettings "Shows the settings."
  :title ~(t "nav.settings") :description ~(t "command.showSettings.about") :category ~(t "category.view")
  :icon "settings" []
  (go "settings"))

(defcommand findApp "Shows the Apps page, its search focused."
  :title ~(t "apps.search") :description ~(t "command.findApp.about") :category ~(t "category.view") :icon "search" []
  (go "apps")
  (focus "#appSearch"))

;;; Reading the apps folder

;; Each read is numbered (mineRun): one started later makes an earlier one, still reading, stop, so two never fill
;; the list together. The apps are read one after another (an fs service runs 16 calls at once): each folder's screen's
;; modified time and name, the last changed first. A folder holds its screen, or a json folder (a copy of an example),
;; or a lisp folder (a copy of an example written as a program); a folder with none of the three is left out.

(defn refreshMine "Reads the apps folder again." []
  (increment "state.mineRun")
  (run "listMine" :with {:pass state.mineRun}))

(defn current? "Whether a read is the last one started." [pass] (= pass state.mineRun))

(defn listMine "Lists the apps folder, for the read numbered pass." [pass]
  (invoke "apps.list" :with {:path "."} :onDone listed :onError notListed))

(defn listed "The apps folder's folders, but shared and hidden ones; then the first is read." [pass result]
  (when (current? pass)
    (set "state.mineScan"
         (filter (fn [entry]
                   (and (= (:kind entry) "folder") (not= (:name entry) "shared") (not= (subs (:name entry) 0 1) ".")))
                 (:entries result)))
    (set "state.mineNext" [])
    (set "state.mineError" "")
    (run "loadMine" :with {:i 0 :pass pass})))

(defn notListed "The apps folder couldn't be read: My apps says so." [pass error]
  (when (current? pass)
    (set "state.mineError" (:message error))
    (set "state.mineLoaded" true)))

(defn folderOf "The folder of the i-th app of the read." [i] (:name (get state.mineScan i)))

(defn file? "Whether a service's stat found a file." [result] (and (= (:exists result) true) (= (:kind result) "file")))

(defn loadMine "Reads the i-th app of the read, or ends the read: the last changed first." [i pass]
  (when (current? pass)
    (if (>= i (count state.mineScan))
      (do (set "state.mine" (sort-by (fn [app] (- (:modified app))) state.mineNext))
          (set "state.mineLoaded" true))
      (invoke "apps.stat" :with {:path (str (folderOf i) "/index.screen.json")} :onDone screenFound
        :onError loadMineCopy))))

(defn screenFound "The folder holds its screen: it is read; else it may be a copy." [i pass result]
  (if (file? result)
    (run "readMine" :with {:i i :sub "" :modified (or (:modified result) 0) :pass pass})
    (run "loadMineCopy" :with {:i i :pass pass})))

(defn loadMineCopy "Looks for a copy of an example written in JSON: a json folder." [i pass]
  (invoke "apps.stat" :with {:path (str (folderOf i) "/json/index.screen.json")} :onDone copyFound
    :onError loadMineProgram))

(defn copyFound "A json folder is read; else it may be a copy of a program." [i pass result]
  (if (file? result)
    (run "readMine" :with {:i i :sub "/json" :modified (or (:modified result) 0) :pass pass})
    (run "loadMineProgram" :with {:i i :pass pass})))

(defn loadMineProgram "Looks for a copy of an example written as a program: a lisp folder, with its app.lisp." [i pass]
  (invoke "apps.stat" :with {:path (str (folderOf i) "/lisp/app.lisp")} :onDone programFound :onError skipMine))

(defn programFound "A lisp folder is read; else the folder is left out." [i pass result]
  (if (file? result)
    (run "readMine" :with {:i i :sub "/lisp" :modified (or (:modified result) 0) :pass pass})
    (skipMine i pass)))

(defn skipMine "Goes on to the next app of the read." [i pass] (run "loadMine" :with {:i (inc i) :pass pass}))

(defn readMine "Reads an app's name: a JSON screen's window title, or a program's name in its app.lisp."
  [i sub modified pass]
  (invoke "apps.read" :with {:path (str (folderOf i) sub (if (= sub "/lisp") "/app.lisp" "/index.screen.json"))}
          :onDone mineRead :onError skipMine))

(defn mineRead "An app read joins the list; then the next is read." [i sub modified pass result]
  (when (current? pass)
    (let [text (:text result)
          named (if (= sub "/lisp") (formKeyword text "name") (get-in (or (parseJson text) {}) [:window :title]))]
      (push "state.mineNext" :value {:id (folderOf i) :sub sub :name (or named (folderOf i)) :modified modified}))
    (skipMine i pass)))

(defn showMineList "Shows the person's apps as the folder gave them (event.mine): scenarios play its answer with it." []
  (set "state.mine" (or event.mine []))
  (set "state.mineError" "")
  (set "state.mineLoaded" true))

(defn listNew "Shows an app just made at the top of the list at once, before the folder is read again." [id sub title]
  (set "state.mine" (concat [{:id id :sub sub :name title
                              :modified (now)}] (remove (fn [app] (= (:id app) id)) state.mine))))

(defn subOf "Where an app's files are in its folder: '', '/json' or '/lisp'." [id]
  (or (:sub (find (fn [app] (= (:id app) id)) state.mine)) ""))

;;; Opening an app, showing it, deleting it

(defn openApp "Opens an app of the cards or the rows: the person's own, or an example." [app]
  (if (:mine app)
    (run "openMine" :with {:id (:id app) :name (:name app) :ask ""})
    (run "openExample" :with {:app app})))

(defn openExample "Opens an example in a window of its own, in its notation, through the host's program." [app]
  (invoke "launch.run"
          :with {:arguments (if (= (:program app) "host")
                              ["affrescoui_launcher"
                               (str "examples:" (:folder app) "/" (or (:notation app) state.notation))]
                              [(:program app)])}
          :onDone exampleOpened
          :onError exampleNotOpened))

(defn exampleOpened "Says the example is opening." [app] (toast (t "toast.opening" {:name (:name app)}) false))

(defn exampleNotOpened "Says the example couldn't be opened, and why." [app error]
  (toast (t "toast.openFailed" {:name (:name app) :why (:message error)}) true))

;; Its screen, or its lisp folder; ask, when it isn't empty, is what the assistant is asked once it opens.
(defn openMine "Opens one of the person's apps in a window of its own." [id name ask]
  (let [sub (subOf id)]
    (invoke "launch.run"
            :with {:arguments (concat ["affrescoui_launcher"
                                       (str "apps:" id (if (= sub "/lisp") sub (str sub "/index.screen.json")))]
                                      (if (not= ask "") ["--ask" ask] []))}
            :onDone mineOpened
            :onError mineNotOpened)))

(defn mineOpened "Says the app is opening." [name] (toast (t "toast.opening" {:name name}) false))

(defn mineNotOpened "Says the app couldn't be opened, and why." [name error]
  (toast (t "toast.openFailed" {:name name :why (:message error)}) true))

(defn revealMine "Shows an app's folder, or the apps folder itself for an empty id." [id]
  (invoke "launch.run" :with {:arguments ["reveal" (str "apps:" id)]} :onError showError))

(defn askDelete "Asks before an app is deleted." [id name]
  (set "state.confirm" {:id id :name name})
  (set "state.confirmOpen" true))

(defn deleteMine "Deletes the app the question was about, once confirmed." []
  (invoke "apps.delete" :with {:path state.confirm.id} :onDone deleted :onError showError))

(defn deleted "Says the app is deleted, and reads the folder again." []
  (toast (t "toast.deleted" {:name state.confirm.name}) false)
  (after 1 refreshMine))

;;; Duplicating an app

(defn duplicateMine "A copy of an app, in the first free folder of <id>-copy, <id>-copy-2..." [id name]
  (run "copyMine"
       :with {:id id
              :name name
              :to (find (fn [folder] (not (contains computed.myIds folder)))
                        (map (fn [n] (if (= n 0) (str id "-copy") (str id "-copy-" (inc n)))) (range 50)))}))

(defn copyMine "Copies an app's folder." [id name to]
  (invoke "apps.copy" :with {:path id :to to} :onDone folderCopied :onError showError))

(defn folderCopied "The copy is renamed." [id name to]
  (run "retitle" :with {:id to :sub (subOf id) :title (t "mine.copyName" {:name name}) :ask "" :then "copied"}))

(defn copied "The copy is at the top of the list, and a note says so." [id sub title]
  (listNew id sub title)
  (toast (t "toast.duplicated" {:name title}) false)
  (after 1 refreshMine))

;;; Naming a copy

;; Each takes id, sub, title, ask, and then: the action run at the end with the same variables. A JSON app's window
;; title and its app.json are set as text, so its files keep their layout; a program's through its keywords, every
;; other character as it was.

(defn retitle "Sets the copy's window title, then its id and name." [id sub title ask then]
  (if (= sub "/lisp")
    (run "retitleProgram" :with {:id id :title title :ask ask :then then})
    (invoke "apps.read" :with {:path (str id sub "/index.screen.json")} :onDone screenRead :onError failedToCreate)))

(defn retitled "The screen's text with its window title replaced, as text: nothing else moves." [text title]
  (let [old (get-in (or (parseJson text) {}) [:window :title])]
    (if (nil? old) text (join (json title) (split text (json old))))))

(defn screenRead "Writes the screen back with its new title." [id sub title ask then result]
  (invoke "apps.write" :with {:path (str id sub "/index.screen.json") :text (retitled (:text result) title)}
          :onDone screenWritten :onError failedToCreate))

(defn screenWritten "Goes on to the app's id and name." [id sub title ask then]
  (run "reidentify" :with {:id id :sub sub :title title :ask ask :then then}))

(defn reidentify "Gives a copy's app.json (a starter has none) the new app's id and name, so it keeps its data apart."
  [id sub title ask then]
  (invoke "apps.read" :with {:path (str id sub "/app.json")} :onDone manifestRead :onError named))

(defn reidentified "An app.json's text with its id and name replaced, as text." [text id title]
  (let [manifest (or (parseJson text) {})]
    (join (str "\"name\": " (json title))
          (split (join (str "\"id\": " (json id)) (split text (str "\"id\": " (json (:id manifest)))))
                 (str "\"name\": " (json (:name manifest)))))))

(defn manifestRead "Writes the app.json back with its new id and name." [id sub title ask then result]
  (invoke "apps.write" :with {:path (str id sub "/app.json") :text (reidentified (:text result) id title)}
          :onDone named :onError failedToCreate))

(defn named "The copy is named: what was to happen next does." [id sub title ask then]
  (run then :with {:id id :sub sub :title title :ask ask}))

(defn retitleProgram "A program copied into a lisp folder: the title of the entry its app.lisp names."
  [id title ask then]
  (invoke "apps.read" :with {:path (str id "/lisp/app.lisp")} :onDone programManifestRead :onError failedToCreate))

(defn programManifestRead "Finds the entry the app.lisp names." [id title ask then result]
  (run "retitleEntry" :with {:id id :entry (or (formKeyword (:text result) "entry") "index.screen.lisp") :title title
                             :ask ask
                             :then then}))

;; The entry an app.lisp names is a file of its folder: one going elsewhere (a path, a folder up) is taken as
;; index.screen.lisp, so a copy never writes outside itself.
(defn retitleEntry "Sets the entry's :window :title, by a keyword edit." [id entry title ask then]
  (let [entry (if (or (not (string? entry)) (= entry "") (= (subs entry 0 1) ".")
                      (contains entry "/") (contains entry "\\") (contains entry ":"))
                "index.screen.lisp"
                entry)]
    (run "retitleEntryFile" :with {:id id :entry entry :title title :ask ask :then then})))

(defn retitleEntryFile "Reads the entry, to set its title." [id entry title ask then]
  (invoke "apps.read" :with {:path (str id "/lisp/" entry)} :onDone entryRead :onError failedToCreate))

(defn entryRead "Writes the entry back with its new title." [id entry title ask then result]
  (invoke "apps.write"
          :with {:path (str id "/lisp/" entry)
                 :text (or (setFormKeyword (:text result) ["window" "title"] title) (:text result))}
          :onDone entryWritten :onError failedToCreate))

(defn entryWritten "Goes on to the app.lisp's id and name." [id title ask then]
  (run "reidentifyProgram" :with {:id id :title title :ask ask :then then}))

(defn reidentifyProgram "Gives the copy's app.lisp the new app's :id and :name, as reidentify does an app.json."
  [id title ask then]
  (invoke "apps.read" :with {:path (str id "/lisp/app.lisp")} :onDone programManifestNamed :onError failedToCreate))

(defn programManifestNamed "Writes the app.lisp back with its new :id and :name." [id title ask then result]
  (let [text (:text result)
        withId (or (setFormKeyword text "id" id) text)]
    (invoke "apps.write" :with {:path (str id "/lisp/app.lisp") :text (or (setFormKeyword withId "name" title) text)}
            :onDone programNamed :onError failedToCreate)))

(defn programNamed "The program is named: what was to happen next does." [id title ask then]
  (run then :with {:id id :sub "/lisp" :title title :ask ask}))

;;; Creating an app

(defn chooseStart "What the new app starts from: blank, or an example's id." [id] (set "state.create.from" id))

(defn startFrom "Starts a new app from an example: the Create page, its name field focused." [id]
  (chooseStart id)
  (go "create")
  (focus "#createName"))

(defn toggleFeature "Asks for a feature of another app, or not." [id]
  (set "state.create.features"
       (if (contains state.create.features id)
         (remove (fn [feature] (= feature id)) state.create.features)
         (concat state.create.features [id]))))

(defn resetCreate "Empties the form." []
  (set "state.create" {:name "" :from "blank" :describe "" :features [] :tried false :busy false :error ""}))

(defn createApp "Makes the new app, if its name is right and none is being made." []
  (set "state.create.tried" true)
  (set "state.create.error" "")
  (if (or (not= computed.createError "") state.create.busy)
    (focus "#createName")
    (let [example (or (find (fn [app] (= (:id app) state.create.from)) state.examples) {})]
      (set "state.create.busy" true)
      (run (if (= state.create.from "blank") "createBlank" "createCopy")
           :with {:id computed.slug
                  :title (trim state.create.name)
                  :ask computed.askText
                  :folder (or (:folder example) "")
                  :sub (if (= (or (:notation example) state.notation) "lisp") "/lisp" "/json")
                  :then "createShared"}))))

(defn createBlank "A folder, and the blank starter in it." [id title ask then]
  (invoke "apps.mkdir" :with {:path id} :onDone blankFolderMade :onError folderNotMade))

(defn blankFolderMade "Copies the blank starter into the new folder." [id title ask then]
  (invoke "apps.copy"
          :with {:from "examples" :path "launcher/json/starters/blank.screen.json" :to (str id "/index.screen.json")}
          :onDone blankCopied :onError failedToCreate))

(defn blankCopied "The blank app is named." [id title ask then]
  (run "retitle" :with {:id id :sub "" :title title :ask ask :then then}))

;; A copy of an example's folder in the notation Settings choose, or the one its entry pins (sub: '/json' or
;; '/lisp'), and its assets, without its C++: its paths lead to the shared folder from there.
(defn createCopy "A folder, and a copy of an example in it." [id title ask folder sub then]
  (invoke "apps.mkdir" :with {:path id} :onDone copyFolderMade :onError folderNotMade))

(defn copyFolderMade "Copies the example's files into the new folder." [id title ask folder sub then]
  (invoke "apps.copy" :with {:from "examples" :path (str folder sub) :to (str id sub) :exclude computed.excluded}
          :onDone exampleCopied :onError failedToCreate))

(defn exampleCopied "Copies the example's assets beside them; with none, the copy goes on."
  [id title ask folder sub then]
  (invoke "apps.copy" :with {:from "examples" :path (str folder "/assets") :to (str id "/assets")
                             :exclude computed.excluded}
          :onDone copyMade :onError copyMade))

(defn copyMade "The copy is named." [id title ask sub then]
  (run "retitle" :with {:id id :sub sub :title title :ask ask :then then}))

(defn createShared "The fonts, add-ons and components the apps name as ../shared, brought up to date beside them."
  [id sub title ask]
  (invoke "apps.copy" :with {:from "examples" :path "shared" :to "shared" :merge true :exclude computed.excluded}
          :onDone created :onError failedToCreate))

(defn created "The new app is listed and opened, the form emptied." [id sub title ask]
  (listNew id sub title)
  (resetCreate)
  (go "mine")
  (run "openMine" :with {:id id :name title :ask ask})
  (after 1 refreshMine))

(defn folderNotMade "The new app's folder couldn't be made: it was there already, or something else." [id error]
  (createFailed id (if (= (:code error) "exists") (t "create.error.taken" {:folder id}) (:message error)) true))

(defn failedToCreate "A step of making the app failed: its folder goes."
  [id error] (createFailed id (:message error) false))

;; keep: the folder isn't the new app's, as it was there already, or was never made.
(defn createFailed "Says why the app couldn't be made, and takes its folder away unless keep." [id why keep]
  (set "state.create.busy" false)
  (set "state.create.error" (t "create.failed" {:why why}))
  (when (and (not= keep true) (not= id ""))
    (invoke "apps.delete" :with {:path id} :onError ignored)))

(defn ignored "Nothing: a failure that changes nothing." [])
