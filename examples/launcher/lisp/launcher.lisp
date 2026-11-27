;;; launcher.lisp --- what the Launcher does: its functions, its commands, and the services it asks the host for
;;;
;;; The views say what is shown; this file says what happens. The examples folder is read only, the apps folder read
;;; and written, and apps are opened by the host's program: each through a service whose answer comes later. The
;;; function its answer runs is given with what it needs, :onDone [mineRead i sub modified pass], and gets `result` or
;;; `error` after them; a closure's values would be gone by then. Where nothing is granted (the rendering check), every
;;; call is refused, and the Launcher shows the examples and an empty My apps.

(ns launcher.launcher)

;;; What the host gives, and runs by name

(defpart host "The services, the read of the apps folder on load, and the actions the host and scenarios run."
  :services {:examples {:provider fs :root examples :readOnly true}
             :apps {:provider fs :root apps :copyFrom [examples]}
             :launch {:provider process :process affresco}
             :packages {:provider packages}}
  :onLoad {:run launcher.refresh}
  :actions {:launcher.refresh [{:lisp refresh-mine}]
            :launcher.show [{:lisp show-mine-list}]
            :launcher.export [{:lisp show-export}]
            :launcher.review [{:lisp show-review}]
            :launcher.catalogue [{:lisp show-listing}]
            :launcher.reviewListed [{:lisp show-listed-review}]})

;;; Pages and notes

(defn go "Shows a page." [page] (set "state.page" page))

(defn show-kind "Shows the apps of a category on the Apps page: all, the person's own, or an example's category." [kind]
  (set "state.kind" kind))

(defn clear-toasts "Takes the notes away." [] (set "state.toasts" []))

(defn toast "A note at the bottom for a few seconds, in red when it is an error." [message error]
  (push "state.toasts" :value {:message message :color (if error "@c.danger" "@c.accent")})
  (debounce 4000 clear-toasts))

(defn show-error "A service's error, as a note." [error] (toast (:message error) true))

(defcommand showApps "Shows the Apps page."
  :title ~(t "nav.apps") :description ~(t "command.showApps.about") :category ~(t "category.view")
  :icon "layout-grid" []
  (go :apps))

(defcommand showMine "Shows My apps."
  :title ~(t "nav.mine") :description ~(t "command.showMine.about") :category ~(t "category.view")
  :icon "folder-heart" []
  (go :mine))

(defcommand showCreate "Shows the Create page, its name field focused."
  :title ~(t "nav.create") :description ~(t "command.showCreate.about") :category ~(t "category.view")
  :icon "circle-plus" []
  (go :create)
  (focus "#createName"))

(defcommand showSettings "Shows the settings."
  :title ~(t "nav.settings") :description ~(t "command.showSettings.about") :category ~(t "category.view")
  :icon "settings" []
  (go :settings))

(defcommand findApp "Shows the Apps page, its search focused."
  :title ~(t "apps.search") :description ~(t "command.findApp.about") :category ~(t "category.view") :icon "search" []
  (go :apps)
  (focus "#appSearch"))

;;; What an agent can do with the apps, as the person would

(defn app-named? "Whether an app has that name or folder, in lower case." [app wanted]
  (contains? #{(-> app :name str/lower-case) (-> app :id str str/lower-case)} wanted))

(defn named-app "The app of the cards with that name or folder, ignoring case, or nil." [name]
  (let [wanted (-> name str str/trim str/lower-case)]
    (find #(app-named? % wanted) computed.everyApp)))

(defn open-named "Opens the app of that name, or says there is none." [name]
  (if-some [app (named-app name)]
    (open-app app)
    (toast (t "toast.noSuchApp" {:name name}) true)))

(defcommand appOpen "Opens an app by its name: an example, or one of the person's own."
  :title ~(t "command.appOpen") :description ~(t "command.appOpen.about") :category ~(t "category.apps")
  :icon "app-window"
  :arguments {:name {:type string :description "The app's name, like Flow or Reading list"}}
  [name]
  (open-named name))

(defn create-named "Fills in the Create page and makes the app, as Create would: a name it refuses stays there, said."
  [name from describe]
  (let [example (named-app (or from ""))
        start (if (or (nil? example) (:mine example)) "blank" (:id example))]
    (set "state.create" {:name (str name)
                         :from start
                         :describe (str (or describe ""))
                         :features []
                         :tried false
                         :busy false
                         :error ""})
    (go :create)
    ; after the update, so the name's checks and folder are worked out from the name just set
    (after 1 create-app)))

(defcommand appCreate "Makes a new app and opens it: blank, or a copy of an example, its assistant asked first for a description."
  :title ~(t "command.appCreate") :description ~(t "command.appCreate.about") :category ~(t "category.apps")
  :icon "circle-plus"
  :arguments {:name {:type string :description "The new app's name, like My budget"}
              :from {:type [string null] :description "The example it starts from, like Flow; a blank app when left out"}
              :describe {:type [string null] :description "What its assistant is asked to build first, in the person's words"}}
  [name from describe]
  (create-named name from describe))

;;; Reading the apps folder

;; Each read is numbered (mineRun): one started later makes an earlier one, still reading, stop, so two never fill
;; the list together. The apps are read one after another (an fs service runs 16 calls at once): each folder's screen's
;; modified time and name, the last changed first. A folder holds its screen, or a json folder (a copy of an example),
;; or a lisp folder (a copy of an example written as a program); a folder with none of the three is left out.

(defn refresh-mine "Reads the apps folder again." []
  (let [pass (inc state.mineRun)]
    (set "state.mineRun" pass)
    (list-mine pass)
    (list-catalogue)))

(defn current? "Whether a read is the last one started." [pass] (= pass state.mineRun))

(defn list-mine "Lists the apps folder, for the read numbered pass." [pass]
  (invoke "apps.list" :with {:path "."} :onDone [listed pass] :onError [not-listed pass]))

(defn listed "The apps folder's folders, but shared and hidden ones; then the first is read." [pass result]
  (when (current? pass)
    (set "state.mineScan"
         (filter (fn [{:keys [kind name]}]
                   (and (= kind :folder) (not= name "shared") (not (str/starts-with? name "."))))
                 (:entries result)))
    (set "state.mineNext" [])
    (set "state.mineError" "")
    (load-mine 0 pass)))

(defn not-listed "The apps folder couldn't be read: My apps says so." [pass error]
  (when (current? pass)
    (set "state.mineError" (:message error))
    (set "state.mineLoaded" true)))

(defn folder-of "The folder of the i-th app of the read." [i] (:name (get state.mineScan i)))

(defn file? "Whether a service's stat found a file." [result] (and (true? (:exists result)) (= (:kind result) :file)))

(defn load-mine "Reads the i-th app of the read, or ends the read: the last changed first." [i pass]
  (when (current? pass)
    (if (>= i (count state.mineScan))
      (do (set "state.mine" (sort-by #(- (:modified %)) state.mineNext))
          (set "state.mineLoaded" true))
      (invoke "apps.stat" :with {:path (str (folder-of i) "/index.screen.json")} :onDone [screen-found i pass]
        :onError [load-mine-copy i pass]))))

(defn screen-found "The folder holds its screen: it is read; else it may be a copy." [i pass result]
  (if (file? result)
    (read-mine i "" (or (:modified result) 0) pass)
    (load-mine-copy i pass)))

(defn load-mine-copy "Looks for a copy of an example written in JSON: a json folder." [i pass]
  (invoke "apps.stat" :with {:path (str (folder-of i) "/json/index.screen.json")} :onDone [copy-found i pass]
    :onError [load-mine-program i pass]))

(defn copy-found "A json folder is read; else it may be a copy of a program." [i pass result]
  (if (file? result)
    (read-mine i "/json" (or (:modified result) 0) pass)
    (load-mine-program i pass)))

(defn load-mine-program "Looks for a copy of an example written as a program: a lisp folder, with its app.lisp." [i pass]
  (invoke "apps.stat" :with {:path (str (folder-of i) "/lisp/app.lisp")} :onDone [program-found i pass]
    :onError [skip-mine i pass]))

(defn program-found "A lisp folder is read; else the folder is left out." [i pass result]
  (if (file? result)
    (read-mine i "/lisp" (or (:modified result) 0) pass)
    (skip-mine i pass)))

(defn skip-mine "Goes on to the next app of the read." [i pass] (load-mine (inc i) pass))

(defn read-mine "Reads an app's name: a JSON screen's window title, or a program's name in its app.lisp."
  [i sub modified pass]
  (invoke "apps.read" :with {:path (str (folder-of i) sub (if (= sub "/lisp") "/app.lisp" "/index.screen.json"))}
          :onDone [mine-read i sub modified pass] :onError [skip-mine i pass]))

(defn mine-read "An app read joins the list; then the next is read." [i sub modified pass result]
  (when (current? pass)
    (let [text (:text result)
          named (if (= sub "/lisp") (formKeyword text "name") (some-> text parseJson (get-in [:window :title])))]
      (push "state.mineNext" :value {:id (folder-of i) :sub sub :name (or named (folder-of i)) :modified modified}))
    (skip-mine i pass)))

(defn show-mine-list "Shows the person's apps as the folder gave them (event.mine): scenarios play its answer with it." []
  (set "state.mine" (or event.mine []))
  (set "state.mineError" "")
  (set "state.mineLoaded" true))

(defn list-new "Shows an app just made at the top of the list at once, before the folder is read again." [id sub title]
  (set "state.mine" (concat [{:id id :sub sub :name title :modified (now)}] (remove #(= (:id %) id) state.mine))))

(defn sub-of "Where an app's files are in its folder: '', '/json' or '/lisp'." [id]
  (or (:sub (find #(= (:id %) id) state.mine)) ""))

;;; Opening an app, showing it, deleting it

(defn open-app "Opens an app of the cards or the rows: the person's own, or an example." [app]
  (if (:mine app)
    (open-mine (:id app) (:name app) "")
    (open-example app)))

(defn open-example "Opens an example in a window of its own, in its notation, through the host's program." [app]
  (invoke "launch.run"
          :with {:arguments (if (= (:program app) :host)
                              ["affrescoui_launcher"
                               (str "examples:" (:folder app) "/" (or (:notation app) state.notation))]
                              [(:program app)])}
          :onDone [example-opened app]
          :onError [example-not-opened app]))

(defn example-opened "Says the example is opening." [app] (toast (t "toast.opening" {:name (:name app)}) false))

(defn example-not-opened "Says the example couldn't be opened, and why." [app error]
  (toast (t "toast.openFailed" {:name (:name app) :why (:message error)}) true))

;; Its screen, or its lisp folder; ask, when it isn't empty, is what the assistant is asked once it opens.
(defn open-mine "Opens one of the person's apps in a window of its own." [id name ask]
  (let [sub (sub-of id)
        entry (if (= sub "/lisp") sub (str sub "/index.screen.json"))]
    (invoke "launch.run"
            :with {:arguments (concat ["affrescoui_launcher" (str "apps:" id entry)]
                                      (if (not= ask "") ["--ask" ask] []))}
            :onDone [mine-opened name]
            :onError [mine-not-opened name])))

(defn mine-opened "Says the app is opening." [name] (toast (t "toast.opening" {:name name}) false))

(defn mine-not-opened "Says the app couldn't be opened, and why." [name error]
  (toast (t "toast.openFailed" {:name name :why (:message error)}) true))

(defn reveal-mine "Shows an app's folder, or the apps folder itself for an empty id." [id]
  (invoke "launch.run" :with {:arguments ["reveal" (str "apps:" id)]} :onError show-error))

(defn ask-delete "Asks before an app is deleted." [id name]
  (set "state.confirm" {:id id :name name})
  (set "state.confirmOpen" true))

(defn delete-mine "Deletes the app the question was about, once confirmed." []
  (invoke "apps.delete" :with {:path state.confirm.id} :onDone deleted :onError show-error))

;; Its data is set aside (never erased), so a new app of its name starts empty; on a failure the next app made there
;; tries again.
(defn deleted "Says the app is deleted, and reads the folder again." []
  (invoke "packages.setAside" :with {:app state.confirm.id} :onError ignored)
  (toast (t "toast.deleted" {:name state.confirm.name}) false)
  (after 1 refresh-mine))

;;; Duplicating an app

(defn duplicate-mine "A copy of an app, in the first free folder of <id>-copy, <id>-copy-2..." [id name]
  (let [taken (into #{} computed.myIds)]
    (copy-mine id name (->> (range 50)
                           (map #(if (zero? %) (str id "-copy") (str id "-copy-" (inc %))))
                           (remove taken)
                           first))))

(defn copy-mine "Copies an app's folder." [id name to]
  (invoke "apps.copy" :with {:path id :to to} :onDone [folder-copied id name to] :onError show-error))

(defn folder-copied "The data an app of the copy's name left is set aside, then the copy is renamed." [id name to]
  (invoke "packages.setAside" :with {:app to} :onDone [rename-copy id name to]
          :onError [copy-data-not-set-aside id name to]))

;; A host without the packages service can't set it aside: the copy goes on, as before.
(defn copy-data-not-set-aside "Data that couldn't be moved stays: the copy, which would open with it, goes."
  [id name to error]
  (if (data-kept? error)
    (do (show-error error)
        (invoke "apps.delete" :with {:path to} :onError ignored))
    (rename-copy id name to)))

(defn rename-copy "The copy is renamed." [id name to]
  (retitle to (sub-of id) (t "mine.copyName" {:name name}) "" :copied))

(defn copied "The copy is at the top of the list, and a note says so." [id sub title]
  (list-new id sub title)
  (toast (t "toast.duplicated" {:name title}) false)
  (after 1 refresh-mine))

;;; Naming a copy

;; Each takes id, sub, title, ask, and then: what happens once the copy is named (:copied or :createShared, named()).
;; A JSON app's window title and its app.json are set as text, so its files keep their layout; a program's through its
;; keywords, every other character as it was.

(defn retitle "Sets the copy's window title, then its id and name." [id sub title ask then]
  (if (= sub "/lisp")
    (retitle-program id title ask then)
    (invoke "apps.read" :with {:path (str id sub "/index.screen.json")}
            :onDone [screen-read id sub title ask then] :onError [failed-to-create id])))

(defn retitled "The screen's text with its window title replaced, as text: nothing else moves." [text title]
  (if-some [old (some-> text parseJson (get-in [:window :title]))]
    (str/replace text (json old) (json title))
    text))

(defn screen-read "Writes the screen back with its new title." [id sub title ask then result]
  (invoke "apps.write" :with {:path (str id sub "/index.screen.json") :text (retitled (:text result) title)}
          :onDone [screen-written id sub title ask then] :onError [failed-to-create id]))

(defn screen-written "Goes on to the app's id and name." [id sub title ask then]
  (reidentify id sub title ask then))

(defn reidentify "Gives a copy's app.json (a starter has none) the new app's id and name, so it keeps its data apart."
  [id sub title ask then]
  (invoke "apps.read" :with {:path (str id sub "/app.json")} 
          :onDone [manifest-read id sub title ask then] :onError [named id sub title ask then]))

(defn reidentified "An app.json's text with its id and name replaced, as text." [text id title]
  (let [manifest (or (parseJson text) {})]
    (-> text
        (str/replace (str "\"id\": " (json (:id manifest))) (str "\"id\": " (json id)))
        (str/replace (str "\"name\": " (json (:name manifest))) (str "\"name\": " (json title))))))

(defn manifest-read "Writes the app.json back with its new id and name." [id sub title ask then result]
  (invoke "apps.write" :with {:path (str id sub "/app.json") :text (reidentified (:text result) id title)}
          :onDone [named id sub title ask then] :onError [failed-to-create id]))

(defn named "The copy is named: what was to happen next does, a copy listed or a new app's shared files brought."
  [id sub title ask then]
  (if (= then :copied)
    (copied id sub title)
    (create-shared id sub title ask)))

(defn retitle-program "A program copied into a lisp folder: the title of the entry its app.lisp names."
  [id title ask then]
  (invoke "apps.read" :with {:path (str id "/lisp/app.lisp")} 
          :onDone [program-manifest-read id title ask then] :onError [failed-to-create id]))

(defn program-manifest-read "Finds the entry the app.lisp names." [id title ask then result]
  (retitle-entry id (-> (:text result) (formKeyword "entry") (or "index.screen.lisp")) title ask then))

;; The entry an app.lisp names is a file of its folder: one going elsewhere (a path, a folder up) is taken as
;; index.screen.lisp, so a copy never writes outside itself.
(defn retitle-entry "Sets the entry's :window :title, by a keyword edit." [id entry title ask then]
  (let [entry (if (or (not (string? entry)) (empty? entry) (str/starts-with? entry ".")
                      (some #(str/includes? entry %) ["/" "\\" ":"]))
                "index.screen.lisp"
                entry)]
    (retitle-entry-file id entry title ask then)))

(defn retitle-entry-file "Reads the entry, to set its title." [id entry title ask then]
  (invoke "apps.read" :with {:path (str id "/lisp/" entry)} 
          :onDone [entry-read id entry title ask then] :onError [failed-to-create id]))

(defn entry-read "Writes the entry back with its new title." [id entry title ask then result]
  (invoke "apps.write"
          :with {:path (str id "/lisp/" entry)
                 :text (or (setFormKeyword (:text result) ["window" "title"] title) (:text result))}
          :onDone [entry-written id title ask then] :onError [failed-to-create id]))

(defn entry-written "Goes on to the app.lisp's id and name." [id title ask then]
  (reidentify-program id title ask then))

(defn reidentify-program "Gives the copy's app.lisp the new app's :id and :name, as reidentify does an app.json."
  [id title ask then]
  (invoke "apps.read" :with {:path (str id "/lisp/app.lisp")} 
          :onDone [program-manifest-named id title ask then] :onError [failed-to-create id]))

(defn program-manifest-named "Writes the app.lisp back with its new :id and :name." [id title ask then result]
  (let [text (:text result)
        withId (or (setFormKeyword text "id" id) text)]
    (invoke "apps.write" :with {:path (str id "/lisp/app.lisp") :text (or (setFormKeyword withId "name" title) text)}
            :onDone [program-named id title ask then] :onError [failed-to-create id])))

(defn program-named "The program is named: what was to happen next does." [id title ask then]
  (named id "/lisp" title ask then))

;;; Creating an app

(defn choose-start "What the new app starts from: blank, or an example's id." [id] (set "state.create.from" id))

(defn start-from "Starts a new app from an example: the Create page, its name field focused." [id]
  (choose-start id)
  (go :create)
  (focus "#createName"))

(defn toggle-feature "Asks for a feature of another app, or not." [id]
  (set "state.create.features"
       (if (contains state.create.features id)
         (remove #{id} state.create.features)
         (concat state.create.features [id]))))

(defn reset-create "Empties the form." []
  (set "state.create" {:name "" :from "blank" :describe "" :features [] :tried false :busy false :error ""}))

(defn create-app "Makes the new app, if its name is right and none is being made." []
  (set "state.create.tried" true)
  (set "state.create.error" "")
  (if (or (not= computed.createError "") state.create.busy)
    (focus "#createName")
    (let [{:keys [folder notation]} (find #(= (:id %) state.create.from) state.examples)
          title (str/trim state.create.name)]
      (set "state.create.busy" true)
      (if (= state.create.from :blank)
        (create-blank computed.slug title computed.askText)
        (create-copy computed.slug title computed.askText (or folder "")
                    (if (= (or notation state.notation) :lisp) "/lisp" "/json"))))))

(defn create-blank "A folder, and the blank starter in it." [id title ask]
  (invoke "apps.mkdir" :with {:path id} :onDone [blank-folder-made id title ask] :onError [folder-not-made id]))

;; A new app's folder starts with no data of an app of its name deleted before (in Finder too): it is set aside.
(defn blank-folder-made "Sets aside the data an app of that name left, then brings the blank starter." [id title ask]
  (invoke "packages.setAside" :with {:app id} :onDone [copy-blank id title ask]
          :onError [blank-data-not-set-aside id title ask]))

(defn blank-data-not-set-aside "Data that couldn't be moved stops the app; a host without the service goes on."
  [id title ask error]
  (if (data-kept? error) (failed-to-create id error) (copy-blank id title ask)))

(defn copy-blank "Copies the blank starter into the new folder." [id title ask]
  (invoke "apps.copy"
          :with {:from "examples" :path "launcher/json/starters/blank.screen.json" :to (str id "/index.screen.json")}
          :onDone [blank-copied id title ask] :onError [failed-to-create id]))

(defn blank-copied "The blank app is named, then the shared files brought." [id title ask]
  (retitle id "" title ask :createShared))

;; A copy of an example's folder in the notation Settings choose, or the one its entry pins (sub: '/json' or
;; '/lisp'), and its assets, without its C++: its paths lead to the shared folder from there.
(defn create-copy "A folder, and a copy of an example in it." [id title ask folder sub]
  (invoke "apps.mkdir" :with {:path id} :onDone [copy-folder-made id title ask folder sub] :onError [folder-not-made id]))

(defn copy-folder-made "Sets aside the data an app of that name left, then brings the example's files."
  [id title ask folder sub]
  (invoke "packages.setAside" :with {:app id} :onDone [copy-example id title ask folder sub]
          :onError [example-data-not-set-aside id title ask folder sub]))

(defn example-data-not-set-aside "Data that couldn't be moved stops the app; a host without the service goes on."
  [id title ask folder sub error]
  (if (data-kept? error) (failed-to-create id error) (copy-example id title ask folder sub)))

(defn copy-example "Copies the example's files into the new folder." [id title ask folder sub]
  (invoke "apps.copy" :with {:from "examples" :path (str folder sub) :to (str id sub) :exclude computed.excluded}
          :onDone [example-copied id title ask folder sub] :onError [failed-to-create id]))

(defn example-copied "Copies the example's assets beside them; with none, the copy goes on."
  [id title ask folder sub]
  (invoke "apps.copy" :with {:from "examples" :path (str folder "/assets") :to (str id "/assets")
                             :exclude computed.excluded}
          :onDone [copy-made id title ask sub] :onError [copy-made id title ask sub]))

(defn copy-made "The copy is named, then the shared files brought." [id title ask sub]
  (retitle id sub title ask :createShared))

(defn create-shared "The fonts, add-ons and components the apps name as ../shared, brought up to date beside them."
  [id sub title ask]
  (invoke "apps.copy" :with {:from "examples" :path "shared" :to "shared" :merge true :exclude computed.excluded}
          :onDone [created id sub title ask] :onError [failed-to-create id]))

(defn created "The new app is listed and opened, the form emptied." [id sub title ask]
  (list-new id sub title)
  (reset-create)
  (go :mine)
  (open-mine id title ask)
  (after 1 refresh-mine))

(defn folder-not-made "The new app's folder couldn't be made: it was there already, or something else." [id error]
  (create-failed id (if (= (:code error) :exists) (t "create.error.taken" {:folder id}) (:message error)) true))

(defn failed-to-create "A step of making the app failed: its folder goes."
  [id error] (create-failed id (:message error) false))

;; The packages service's setAside couldn't move some of it ("failed"): a new app there would open with an old one's
;; state, records and chat. Any other error is a host without the service: the app is made as before.
(defn data-kept? "Whether an old app's data couldn't be set aside." [error] (= (:code error) :failed))

;; keep: the folder isn't the new app's, as it was there already, or was never made.
(defn create-failed "Says why the app couldn't be made, and takes its folder away unless keep." [id why keep]
  (set "state.create.busy" false)
  (set "state.create.error" (t "create.failed" {:why why}))
  (when-not (or (true? keep) (empty? id))
    (invoke "apps.delete" :with {:path id} :onError ignored)))

(defn ignored "Nothing: a failure that changes nothing." [])
