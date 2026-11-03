;;; state.lisp --- what Sign in keeps while it runs, and where each value starts
;;;
;;; Each defstate is one key of the state, with the value it starts from and, for some, its type. Only the theme, the
;;; language and "remember this device" persist (index.screen.lisp); never a password or a code.

;;; The look and the demo's switches

(defstate theme light "The colours: light or dark." :type {:enum [light dark]})

(defstate locale en "The language shown: English, Italian or Arabic (right to left)." :type {:enum [en it ar]})

(defstate slow false "Slow network (demo): the fake server answers after 800 ms." :type (boolean))

(defstate busy "" "What the fake server is doing, if anything: the busy button and the status line show it."
  :type (string))

(defstate shortcutsOpen false "The keyboard shortcuts dialog is open.")

(defstate toast {:open false :message "" :action ""} "The notice in the corner; its action, if any, is stay.")

;;; The sign-in form

(defstate step email "The identifier step, then the password step." :type {:enum [email password]})

(defstate email "" :type (string))

(defstate password "" :type (string))

(defstate confirm "" :type (string))

(defstate name "" :type (string))

(defstate submitted false "The form was sent once: its fields show their errors." :type (boolean))

(defstate focus "" "Which field has the focus, by its key." :type (string))

(defstate showPassword false :type (boolean))

(defstate remember true :type (boolean))

(defstate formError "" "Why the server refused, as a kind of serverError.")

(defstate providerMessage "" "What a provider said when it could not sign in.")

(defstate providerName exampleId "The provider being waited for.")

(defstate notice "" "expired once a session timed out.")

(defstate passkeyOpen false "The device's passkey picker, faked, is open.")

(defstate failures [] "The emails of the wrong passwords, one each: five in a row lock the account.")

;;; The demo's accounts and the session

(defstate accounts
  [{:email ada@example.com
    :name "Ada"
    :password ""
    :twoStep false
    :provider false
    :passkey true
    :username ada
    :demo true
    :passwordDays 30}
   {:email sam@example.com
    :name "Sam"
    :password ""
    :twoStep true
    :provider false
    :passkey false
    :username sam
    :demo true
    :passwordDays 90}]
  "The fake server's accounts; the demo ones take any password.")

(defstate user {:email "" :name "" :twoStep false :provider false :passkey false :username ""}
  "The account signed in.")

(defstate pending {:email "" :name "" :twoStep false :provider false :passkey false :username ""}
  "The account signing in, until its last step passes.")

(defstate session {:left 300 :warned false} "Seconds until the session ends, and whether the toast warned of it.")

(defstate confirmedLeft 0 "Seconds left in which a sensitive change needs no password again.")

(defstate reauth {:open false :password "" :error "" :then ""}
  "The password asked again before a sensitive change, and the function that change runs.")

;;; Codes: two-step, email codes and backup codes

(defstate demoCode "246810" "The one code the fake server sends.")

(defstate code "" :type (string))

(defstate codeError "")

(defstate codeMode twoStep "Which code the code screen asks for." :type {:enum [twoStep email verify sms backup]})

(defstate resendLeft 0 "Seconds before a new code can be sent." :type (number))

(defstate phone "555-0142")

(defstate backupCodes
  ["DEMO-1001" "DEMO-1002" "DEMO-1003" "DEMO-1004" "DEMO-1005" "DEMO-1006" "DEMO-1007" "DEMO-1008"])

(defstate backupCode "")

(defstate trust false "Remember this device for two-step.")

(defstate trusted [{:email sam@example.com :device phone :days 12}] "The devices two-step remembers, per account.")

(defstate setup {:open false :step scan :code "" :error "" :saved false}
  "The two-step setup: scan the code, then save the backup codes.")

;;; Creating an account, and resetting a password

(defstate username "" :type (string))

(defstate usernameChecked "" "The username the demo server last checked." :type (string))

(defstate spin 0 "The username check's spinner, in degrees." :type (number))

(defstate terms false :type (boolean))

(defstate invite {:open false :code ""})

(defstate forgot {:open false :email "" :error ""} "The forgot-password dialog.")

(defstate reset {:email "" :code "" :password "" :confirm "" :submitted false :error "" :left 0}
  "The new-password screen; its code lasts left seconds.")

;;; The account page

(defstate section profile "The account's section shown."
  :type {:enum [profile emails security devices connected activity data]})

(defstate clock 0 "Seconds since the session opened: the activity log's times." :type (number))

(defstate target "" "The email, passkey or provider a step-up change is about." :type (string))

(defstate profile {:name "" :email "" :submitted false})

(defstate photoOpen false)

(defstate emails [])

(defstate newEmail {:open false :address "" :submitted false})

(defstate myPasskeys [])

(defstate sessions [])

(defstate connected [])

(defstate log [] "The account's activity, newest first, at most 50.")

(defstate change {:open false :password "" :confirm "" :submitted false} "The change-password dialog.")

(defstate deleting {:open false :typed "" :error ""} "The delete-account dialog.")
