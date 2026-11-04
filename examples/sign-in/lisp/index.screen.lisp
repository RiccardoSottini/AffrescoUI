;;; index.screen.lisp --- Sign in: sign in, create an account, two-step codes, a password reset, and an account page
;;; whose session expires
;;;
;;; A fake server answers in the functions of sign-in.lisp and profile.lisp, slowly when Slow network is on
;;; (Shift+Cmd+N); Example ID is a made-up OAuth 2 provider whose answers the scenarios play. Light and dark (Cmd+J),
;;; English, Italian and Arabic, right to left (Cmd+L). F1 lists the commands and their keys, Escape goes back from
;;; the inner screens, Shift+Cmd+H shows the passwords, Shift+Cmd+Q signs out.

(include "locales/en.lisp"
         "locales/it.lisp"
         "locales/ar.lisp"
         "state.lisp"
         "computed.lisp"
         "look.lisp"
         "components.lisp"
         "chrome.lisp"
         "dialogs.lisp"
         "sign-in-form.lisp"
         "create-account.lisp"
         "codes.lisp"
         "account.lisp"
         "account-sections.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "sign-in.lisp"
         "profile.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Sign in's one window: the glows, the screen shown, the bars, the dialogs and the toast."
  :window {:title ~(t "windowTitle")
           :width 960
           :height 720
           :minWidth 480
           :minHeight 640
           :focusRing {:color @color.accent :width 2 :offset 2}}
  :strings {:default en :locale ~state.locale :keep ["EXAM PLEE XAMP LE23" "DEMO-1001" "Example ID"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons arabic]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [iconsSmall arabicSmall]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 26 :fallback [arabicTitle]}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 19 :fallback [icons arabicHeading]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 18}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 13}
          :logo {:file "../../shared/fonts/Lucide.ttf" :size 22}
          :mono {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 15}
          :arabic {:file "../../shared/fonts/NotoSansArabic-Subset.ttf" :size 15}
          :arabicSmall {:file "../../shared/fonts/NotoSansArabic-Subset.ttf" :size 13}
          :arabicTitle {:file "../../shared/fonts/NotoSansArabic-Subset.ttf" :size 26}
          :arabicHeading {:file "../../shared/fonts/NotoSansArabic-Subset.ttf" :size 19}}
  :persist [state.theme state.remember state.locale]
  :theme ~state.theme
  (node#panel-root :color @color.background
    ~@(glows)
    (outlet#screens :dir auto :anchors {:left 0 :right 0 :top 56 :bottom 44})
    ~(announcer)
    ~(top-bar)
    ~(footer)
    ~(forgot-dialog)
    ~(passkey-dialog)
    ~(setup-dialog)
    ~(change-dialog)
    ~(delete-dialog)
    ~(reauth-dialog)
    ~(shortcuts-dialog)
    ~(toast)))

;;; The screens the outlet shows, one at a time (navigate), in this order: the first is shown first

(defscreen signInForm "Signing in: the email, then the password, or a provider or a passkey." ~(sign-in-card))

(defscreen createAccount "Creating an account." ~(create-card))

(defscreen twoStep "The code a second step asks for." ~(two-step-card))

(defscreen reset "A new password, after a reset code." ~(reset-card))

(defscreen signedIn "The account page, once signed in." ~(account-screen))
