;;; places.lisp --- where you are in Arcadia: the pages, back and forward, and the keys that go about
;;;
;;; The navigation is a list of places, { tab, page, id }, and where in it you are, as a browser's history is: go
;;; (in arcadia.lisp) drops the places ahead and adds one; back and forward move along it.

;;; Where you are

(defstate nav [{:tab library :page home :id nil}])

(defstate at 0)

(defcomputed here "The place shown: its tab, its page and the id of what it shows." (get state.nav state.at))

(defcomputed game "The game shown, or the one chosen in the library."
  (find (fn [it] (= it.id (or-else computed.here.id state.selected))) data.games))

(defcomputed owned "The games in the library." (filter (fn [it] it.owned) data.games))

(defcomputed rtl "Whether the language is written right to left." (= (slice locale 0 2) "ar"))

(defpart start "Arcadia opens on the page Settings choose." :onLoad openStartPage)

;;; The pages

(defview pages "The page of the tab shown; the store and friends need to be online." []
  `(node#body :grow 1 :anchors {:left 0 :right 0}
     (node#library :when ~(= computed.here.tab "library") :use libraryPage)
     (node :when ~(and (= computed.here.tab "store") (not state.session.offline)) :use storePage)
     (node :when ~(= computed.here.tab "downloads") :use downloadsPage)
     (node :when ~(and (= computed.here.tab "community") (not state.session.offline)) :use friendsPage)
     (node :when ~(and (contains ["store" "community"] computed.here.tab) state.session.offline) :use offlinePage)
     (node :when ~(= computed.here.tab "profile") :use profilePage)
     (node#elsewhere :when ~(not (contains ["library" "store" "downloads" "community" "profile"] computed.here.tab))
       :use comingSoon)))

;;; Keys

(defkey "alt+left" back)

(defkey "alt+right" forward)

(defkey "mod+1" showStore)

(defkey "mod+2" showLibrary)

(defkey "mod+3" showCommunity)

(defkey "mod+k mod+t" toggleTheme)

(defkey "mod+k mod+l" nextLanguage)

(defkey "mod+," {:do ~(handler openSettings "interface")})
