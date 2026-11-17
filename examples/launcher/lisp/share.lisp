;;; share.lisp --- sharing a version: an app of the person's exported as one file, and a file someone gave installed
;;;
;;; Export on a row asks the host's packages service for a package of the app, then shows what it holds and what it
;;; never does; Save writes it where the person chooses. Install from a file reads one, asks the service what
;;; installing it would do, and shows that as a review; Install makes a new app of it, whose changes wait in it as a
;;; proposal only the person accepts.

;;; What it keeps

(defstate exporting {:open false :id "" :name "" :description "" :author "" :summary {} :leftOut {} :busy false}
  "The export under way: the app's folder, the package's name, description and author, and what the service said.")

(defstate install {:package {} :summary {} :error "" :name "" :tried false :busy false}
  "The file being reviewed: the package, what installing it would do or why it can't be, and the new app's name.")

(defcomputed installSlug "The installed app's folder: its name as Create makes one."
  (slice (join "-"
               (filter (fn [it] (not= it ""))
                       (split (join ""
                                    (map (fn [it] (if (contains "abcdefghijklmnopqrstuvwxyz0123456789" it) it " "))
                                         (split (lower-case (trim state.install.name)) "")))
                              " ")))
         0
         40))

(defcomputed installError "What is wrong with the installed app's name, as a key of the locales, or empty."
  (cond (= (trim state.install.name) "") "create.error.empty"
        (> (count (trim state.install.name)) 80) "create.error.long"
        (or (contains state.install.name (str "{" "{")) (contains state.install.name (str "}" "}")))
        "create.error.braces"
        (= computed.installSlug "") "create.error.letters"
        (contains computed.reserved computed.installSlug) "create.error.reserved"
        (contains computed.myIds computed.installSlug) "create.error.exists"
        :else ""))

(defcomputed installable "Whether the file reviewed can be installed: it read, and has no problem."
  (and (= state.install.error "") (= (count (or state.install.summary.problems [])) 0)))

(defcomputed exportLines "What the package holds, in plain lines." (exportLinesOf state.exporting))

(defcomputed reviewLines "What installing the file would do, a line each." (reviewOf state.install.summary))

(defcomputed signatureLine "Who signed the file reviewed, as the person reads it."
  (signatureOf (or-else state.install.summary.signature "none")))

(defcomputed signedTrusted "Whether a publisher the person trusts signed the file reviewed."
  (= (slice (or-else state.install.summary.signature "") 0 8) "trusted:"))

(defcomputed neverIncluded "What a package never holds, whatever the app."
  ["share.never.records" "share.never.history" "share.never.grants" "share.never.secrets" "share.never.paths"])

;;; The lines

(defn line "A line of a summary, in a tone: heading, text, muted, added or danger." [text tone] {:text text :tone tone})

(defn section "A heading and its lines, or nothing when there are none." [heading lines]
  (if (= (count lines) 0) [] (concat [(line (t heading) "heading")] lines)))

(defn exportLinesOf "An export's summary: what it is, its definitions and layers, and what it leaves out." [exporting]
  (let [summary (:summary exporting)
        definitions (or (:definitions summary) {})
        impact (or (:impact summary) {})
        leftOut (or (:leftOut exporting) {})]
    [(line (if (= (:kind summary) "changes")
             (t "share.kindChanges" {:base (get-in summary [:base :name])})
             (t "share.kindApp"))
           "text")
     (line (t "share.definitions" {:added (count (or (:added definitions) []))
                                   :changed (count (or (:changed definitions) []))
                                   :removed (count (or (:removed definitions) []))})
           "text")
     (line (t "share.layers" {:view (or (:view impact) 0) :logic (or (:logic impact) 0)}) "text")
     (line (t "share.leftOut" {:records (or (:records leftOut) 0) :commits (or (:commits leftOut) 0)}) "muted")]))

(defn reviewOf "A review, a section each: the base, what doesn't merge, the layers, definitions, hooks and advice, what
  it writes and uses, capabilities, who may change it, its state, pictures and records, and its problems." [summary]
  (concat (baseLines summary)
          (section "review.conflicts"
                   (map (fn [it] (line (:path it) "danger")) (or (:conflicts summary) [])))
          (section "review.layers" (layerLines (or (:impact summary) {})))
          (section "review.definitions" (definitionLines (or (:definitions summary) {})))
          (section "review.extensions" (extensionLines (or (:extensions summary) {})))
          (section "review.effects"
                   (concat (map (fn [it] (line (t "review.writes" {:what (or (:collection it) (:step it))}) "text"))
                                (or (:writes summary) []))
                           (map (fn [it] (line (t "review.uses" {:what (:use it)}) "text")) (or (:uses summary) []))))
          (section "review.capabilities" (capabilityLines (or (:capabilities summary) [])))
          (section "review.permissions" (map (fn [it] (line it "text")) (or (:permissions summary) [])))
          (section "review.contents" (contentLines summary))
          (section "review.problems" (map (fn [it] (line it "danger")) (or (:problems summary) [])))))

(defn baseLines "What the package was made on, and how that compares with what the person has." [summary]
  (let [base (or (:base summary) {})]
    [(line (t "review.base") "heading")
     (line (cond (= (:kind summary) "app") (t "review.baseApp")
                 (= (:exact base) true) (t "review.baseExact" {:name (:name base) :release (or (:release base) "")})
                 :else (t "review.baseOther" {:name (:name base) :release (or (:release base) "")}))
           (if (or (= (:kind summary) "app") (= (:exact base) true)) "text" "danger"))]))

(defn layerLines "How many changes each layer has: what it shows, what it does, its data." [impact]
  (concat (if (> (or (:view impact) 0) 0) [(line (t "review.view" {:n (:view impact)}) "text")] [])
          (if (> (or (:logic impact) 0) 0) [(line (t "review.logic" {:n (:logic impact)}) "text")] [])
          (if (and (:data impact) (not= (:data impact) "none")) [(line (t "review.data" {:effect (:data impact)}) "danger")] [])))

(defn definitionLines "Each definition added, changed or taken away, by name." [definitions]
  (concat (map (fn [it] (line (t "review.added" {:name it}) "added")) (or (:added definitions) []))
          (map (fn [it] (line (t "review.changed" {:name it}) "text")) (or (:changed definitions) []))
          (map (fn [it] (line (t "review.removed" {:name it}) "muted")) (or (:removed definitions) []))))

(defn extensionLines "Each hook and advice it adds or takes away, one per line." [extensions]
  (concat (map (fn [it] (line (t "review.extension" {:kind (:kind it) :name (:name it) :target (:target it)}) "added"))
               (or (:added extensions) []))
          (map (fn [it] (line (t "review.extensionRemoved" {:kind (:kind it) :name (:name it)}) "muted"))
               (or (:removed extensions) []))))

(defn capabilityLines "What it asks the person for, those they haven't given it yet marked new." [capabilities]
  (map (fn [it] (line (if (:added it)
                        (t "review.capabilityNew" {:what (:capability it) :why (:why it)})
                        (t "review.capability" {:what (:capability it) :why (:why it)}))
                      (if (:added it) "added" "text")))
       capabilities))

(defn contentLines "The state values it brings, its pictures and its first records." [summary]
  (concat (map (fn [it] (line (t "review.state" {:key (:key it) :value (json (:value it))}) "text"))
               (entries (or (:state summary) {})))
          (map (fn [it] (line (t "review.picture" {:name (:name it)}) "text")) (or (:assets summary) []))
          (map (fn [it] (line (t "review.seed" {:collection (:collection it) :n (or (:records it) 0)}) "text"))
               (or (:seed summary) []))))

;;; Exporting

(defn exportMine "Asks for a package of an app, to show what it holds before it is saved." [id name]
  (set "state.exporting" {:open false :id id :name name :description "" :author "" :summary {} :leftOut {} :busy true})
  (invoke "packages.export" :with {:root "apps" :app id :name name} :onDone [exportReady id] :onError exportFailed))

(defn exportFailed "The app couldn't be exported: a note says why." [error]
  (set "state.exporting.busy" false)
  (showError error))

(defn exportReady "The dialog opens on what the package holds." [id result]
  (when (= id state.exporting.id)
    (set "state.exporting.summary" (:summary result))
    (set "state.exporting.leftOut" (:leftOut result))
    (set "state.exporting.busy" false)
    (set "state.exporting.open" true)))

(defn showExport "Opens the dialog on an export's answer as event gives it ({id, name, summary, leftOut}): scenarios
  play the service's answer with it." []
  (set "state.exporting" {:open true :id event.id :name event.name :description "" :author "" :summary event.summary
                          :leftOut (or event.leftOut {}) :busy false}))

(defn exportSave "Makes the package again with the name, description and author given, to save it." []
  (invoke "packages.export"
          :with {:root "apps" :app state.exporting.id :name (trim state.exporting.name)
                 :description (trim state.exporting.description) :author (trim state.exporting.author)}
          :onDone [exportMade state.exporting.id] :onError showError))

(defn exportMade "Asks where to save the package." [id result]
  (saveFile (:package result) :name (str id ".affresco.json") :onDone exportSaved :onError showError))

(defn exportSaved "Says the file is saved, by its name only." [result]
  (toast (t "toast.exported" {:file (:name result)}) false))

;;; Installing

(defn installFromFile "Asks for a file someone gave, to review it." []
  (openFile "state.install.package" :extensions ["json"] :onDone fileChosen :onError showError))

(defn fileChosen "Asks the service what installing the file would do." []
  (invoke "packages.inspect" :with {:package state.install.package} :onDone inspected :onError notInspected))

(defn inspected "The review shows what the service said." [result] (review (:summary result) ""))

(defn notInspected "The review says why the file can't be installed." [error] (review {} (:message error)))

(defn review "Shows the review: a summary, or why the file can't be installed." [summary why]
  (set "state.install.summary" summary)
  (set "state.install.error" why)
  (set "state.install.name" (or (:name summary) ""))
  (set "state.install.tried" false)
  (set "state.install.busy" false)
  (go "install"))

(defn showReview "Shows the review of a file as event gives it ({summary, package?, error?}): scenarios play the
  service's answer with it." []
  (set "state.install.package" (or event.package {}))
  (review (or event.summary {}) (or event.error "")))

(defn installApp "Installs the file as a new app, if it can be and its name is right." []
  (set "state.install.tried" true)
  (when (and computed.installable (= computed.installError "") (not state.install.busy))
    (set "state.install.busy" true)
    (invoke "packages.install"
            :with {:package state.install.package :id computed.installSlug :name (trim state.install.name)}
            :onDone [installed (trim state.install.name)] :onError notInstalled)))

(defn notInstalled "Says why it wasn't installed: nothing was made." [error]
  (set "state.install.busy" false)
  (toast (t "toast.notInstalled" {:why (:message error)}) true))

(defn installed "The shared files are brought, as for a new app." [title result]
  (invoke "apps.copy" :with {:from "examples" :path "shared" :to "shared" :merge true :exclude computed.excluded}
          :onDone [installReady (:id result) (:sub result) title (:proposal result)]
          :onError [installReady (:id result) (:sub result) title (:proposal result)]))

(defn installReady "The new app is listed first and opened; a note says its changes wait for the person."
  [id sub title proposal]
  (set "state.install.busy" false)
  (listNew id sub title)
  (go "mine")
  (openMine id title "")
  (toast (t (if proposal "toast.installedProposal" "toast.installed") {:name title}) false)
  (after 1 refreshMine))

(defn cancelInstall "Leaves the review: nothing is installed." []
  (set "state.install" {:package {} :summary {} :error "" :name "" :tried false :busy false})
  (go "mine"))

;;; The export dialog

(defpart exportDialog "The dialog before an app is exported: over the window."
  :overlays {:exportDialog (node#exportDialog :use dialog
                             :with {:open state.exporting.open
                                    :title ~(t "share.title" {:name state.exporting.name})
                                    :message ""
                                    :width 560
                                    :height 560
                                    :highlight @c.accent
                                    :buttons [{:title ~(t "share.save") :run exportSave :default true}
                                              {:title ~(t "delete.cancel")}]}
                             ~(export-fields)
                             ~(export-summary))})

(defview export-fields "The package's name, its description and its author." []
  `(node#exportFields :anchors {:left 20 :right 20 :top 48} :height 200 :color transparent
     (node#exportNameLabel :style hint :text ~(t "share.name") :anchors {:left 0 :right 0 :top 0})
     (node#exportName :style field :bind state.exporting.name :maxLength 80 :label ~(t "share.name")
       :anchors {:left 0 :right 0 :top 20} :height 38)
     (node#exportDescriptionLabel :style hint :text ~(t "share.description") :anchors {:left 0 :right 0 :top 66})
     (node#exportDescription :style field :bind state.exporting.description :maxLength 2000
       :label ~(t "share.description") :placeholder ~(t "share.descriptionPlaceholder")
       :anchors {:left 0 :right 0 :top 86} :height 38)
     (node#exportAuthorLabel :style hint :text ~(t "share.author") :anchors {:left 0 :right 0 :top 132})
     (node#exportAuthor :style field :bind state.exporting.author :maxLength 200 :label ~(t "share.author")
       :placeholder ~(t "share.authorPlaceholder") :anchors {:left 0 :right 0 :top 152} :height 38)))

(defview export-summary "What the package holds, a line each, and what it never holds." []
  `(node#exportSummary :anchors {:left 20 :right 20 :top 256 :bottom 52} :color transparent
     (column#exportLines :anchors {:left 0 :right 0 :top 0} :height 100 :gap 4 :role list
       ~(repeat [it computed.exportLines :index i]
         `(label#exportLine :text ~it.text :font small :height 20 :anchors {:left 0 :right 0}
            :textColor ~(if (= it.tone "muted") "@c.muted" "@c.text"))))
     (node#exportNeverTitle :style sectionTitle :text ~(t "share.never") :anchors {:left 0 :right 0 :top 104})
     (column#exportNever :anchors {:left 0 :right 0 :top 130} :height 120 :gap 2 :role list
       ~(repeat [key computed.neverIncluded :index i]
         `(label#exportNeverLine :icon x :text ~(t key) :font small :textColor @c.muted :height 20
            :anchors {:left 0 :right 0})))))

;;; The review

(defview install-page "The review of a file: what it is, what installing it would do, and Install." []
  `(node#installPage :when ~(= state.page "install") :fill true :color transparent :role tabpanel
     :label ~(t "review.title")
     (node#installTitle :style pageTitle :text ~(or state.install.summary.name (t "review.title")) :role heading)
     ~(install-header)
     (column#reviewList :anchors {:left 36 :right 36 :top 176 :bottom 130} :gap 2 :scroll vertical :role list
       :label ~(t "review.title")
       ~(repeat [it computed.reviewLines :index i]
         `(label#reviewLine :text ~it.text :height ~(if (= it.tone "heading") 30 22) :anchors {:left 0 :right 0}
            :font ~(if (= it.tone "heading") "strong" "default")
            :textColor ~(cond (= it.tone "muted") "@c.muted" (= it.tone "added") "@c.accent"
                              (= it.tone "danger") "@c.danger" :else "@c.text")))
       (node#reviewListEnd :height 12 :color transparent :anchors {:left 0 :right 0}))
     ~(install-footer)))

(defview install-header "Its release and description, who it says it is by, and its signature." []
  `(node#installHeader :anchors {:left 36 :right 36 :top 72} :height 96 :color transparent
     (label#installAbout :text ~(if (= state.install.error "")
                                  (if (= (or state.install.package.description "") "")
                                    (t "review.release" {:release (or state.install.summary.release "")})
                                    (t "review.about" {:release (or state.install.summary.release "")
                                                       :description state.install.package.description}))
                                  (t "review.unreadable"))
       :textColor @c.muted :anchors {:left 0 :right 0 :top 0} :height 20)
     (label#installAuthor :when ~(= state.install.error "")
       :text ~(if (not= (or state.install.summary.author "") "")
                (t "review.author" {:author state.install.summary.author})
                (t "review.noAuthor"))
       :textColor @c.text :anchors {:left 0 :right 0 :top 26} :height 20)
     (label#installSignature :when ~(= state.install.error "")
       :text ~computed.signatureLine
       :textColor ~(if computed.signedTrusted "@c.accent" "@c.muted")
       :anchors {:left 0 :right 0 :top 50} :height 20)
     (label#installRefused :when ~(not= state.install.error "")
       :text ~state.install.error :wrap true :textColor @c.danger :anchors {:left 0 :right 0 :top 26} :height 44
       :role alert)))

(defview install-footer "The new app's name, what is wrong with it, Install and Cancel; no Install when it can't be."
  []
  `(node#installFooter :anchors {:left 36 :right 36 :bottom 0} :height 120 :color transparent
     (label#installCant :when ~(not computed.installable) :text ~(t "review.cant") :textColor @c.danger
       :anchors {:left 0 :right 160 :top 20} :height 22 :role status)
     (node#installName :when ~computed.installable :style field :bind state.install.name :maxLength 80
       :label ~(t "review.name") :anchors {:left 0 :right 320 :top 14} :height 42 :onSubmit installApp
       :borderColor ~(if (and state.install.tried (not= computed.installError "")) "@c.danger" "@c.border"))
     (label#installNameHint :when ~computed.installable :font small :anchors {:left 2 :right 320 :top 62}
       :height 18 :live polite
       :textColor ~(if (and state.install.tried (not= computed.installError "")) "@c.danger" "@c.muted")
       :text ~(cond (and state.install.tried (not= computed.installError "")) (t computed.installError)
                    (not= computed.installSlug "") (t "create.folder" {:folder computed.installSlug})
                    :else (t "create.nameHint")))
     (node#installCancel :style secondary :text ~(t "delete.cancel") :anchors {:right 160 :top 14} :width 140
       :height 42 :onClick cancelInstall)
     (node#installButton :when ~computed.installable :style primary :icon package-open :text ~(t "review.install")
       :anchors {:right 0 :top 14} :width 150 :height 42 :onClick installApp)))

(defn signatureOf "The signature line: a publisher the person trusts, one they don't know, or none." [signature]
  (cond (= (slice signature 0 8) "trusted:") (t "review.signedTrusted" {:name (subs signature 8)})
        (= (slice signature 0 8) "unknown:") (t "review.signedUnknown" {:key (subs signature 8)})
        :else (t "review.unsigned")))
