;;; state.lisp --- what Quaderno keeps while it runs, and where each value starts

;;; Settings

(defcustom appearance.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "settings.language") :description ~(t "settings.language.about"))

;;; The state

(defstate dir auto "The text's direction: auto, ltr or rtl.")

(defstate project weather-station "The project's name.")

(defstate theme dark "The theme: dark, light or system.")

(defstate zen false "Whether only the code is shown.")

(defstate minimap true "Whether the minimap is shown.")

(defstate tabs [0 1 3] "The open files, by index, in the order of their tabs.")

(defstate active 0 "The file shown, by index; -1 for none.")

(defstate caret {:line 1 :column 1 :selected 0} "Where the caret is, and how much is selected.")

(defstate command "")

(defstate expanded ["d:root" "d:src" "d:screens"] "The folders open in the tree.")

(defstate sidebarOpen true "Whether the side bar is shown.")

(defstate panelOpen false "Whether the bottom panel is shown.")

(defstate panelTab problems "The bottom panel's tab: problems or terminal.")

(defstate menuBar {} "The menu bar's state.")

(defstate quickPick {:open false :query ""} "The Quick Switcher: open, and its text.")

(defstate palette {:open false :query ""} "The command palette: open, and its text.")

(defstate symbolPick {:open false :query ""} "Find Symbol in Project: open, and its text.")

(defstate themePick {:open false :query ""} "The theme picker: open, its text and the theme it shows.")

(defstate syntaxPick {:open false :query ""} "The syntax picker: open, and its text.")

(defstate tabMenu {:open false :x 0 :y 0 :file -1} "A tab's menu: open, where, and for which file.")

(defstate editorMenu {:open false :x 0 :y 0} "The editor's menu: open, and where.")

(defstate symbolIndex [] "Each file's symbols, found when it is saved.")

(defstate place nil "Where the editor goes when it shows a file, or nil.")

(defstate results {:query "" :at 0} "Find in Files' last search: its query, counts, text and the result shown.")

(defstate wrap false "Whether long lines wrap.")

(defstate ruler false "Whether the ruler at column 80 is shown.")

(defstate findBar
  {:open false
   :replacing false
   :inFiles false
   :where ""
   :scope []
   :text ""
   :replace ""
   :regex false
   :wholeWord false
   :caseSensitive false
   :inSelection false
   :step first
   :request 0})

(defstate closing {:open false :file -1} "The file whose tab is closing, while the dialog asks.")

(defstate closed [] "The files closed, the last latest, for Reopen Closed File.")

(defstate aboutOpen false "Whether About is shown.")

(defstate problems [] "The TODO and FIXME lines of the saved files.")

(defstate problemPick "")

(defstate scan [] "A scan's lines, while it runs.")

(defstate files
  [{:name main.cpp
    :folder src
    :language "C++"
    :dirty false
    :text "#include <affresco/affresco.hpp>\n#include \"Station.hpp\"\n\n// A weather station: the UI is a live document, the readings come from C++\nint main() {\n    affresco::Application app;\n    affresco::Screen screen = affresco::Screen::fromFile(\"screens/station.screen.json\");\n    screen.createWindow(app);\n\n    Station station(screen);\n    // TODO: read the sensor's name from the settings\n    station.connect(\"sensor-0\");\n    return app.run();\n}\n"}
   {:name "Station.hpp"
    :folder src
    :language "C++"
    :dirty false
    :text "#pragma once\n\n#include <affresco/Screen.hpp>\n#include <string>\n\n/// @brief Reads the sensors and puts their values in the screen's state.\nclass Station {\npublic:\n    explicit Station(affresco::Screen& screen);\n\n    /// @brief Starts reading the sensor with this name.\n    void connect(const std::string& sensor);\n\nprivate:\n    affresco::Screen& screen_; ///< The station's live document.\n    double lastCelsius_ = 0.0; ///< The last temperature read.\n};\n"}
   {:name "Station.cpp"
    :folder src
    :language "C++"
    :dirty false
    :text "#include \"Station.hpp\"\n\nStation::Station(affresco::Screen& screen) : screen_(screen) {}\n\nvoid Station::connect(const std::string& sensor) {\n    // FIXME: a sensor that never answers blocks the UI thread\n    lastCelsius_ = sensor0(readRaw(sensor));\n    screen_.applyRevision(R\"({\"message\": \"New reading\",\n        \"patch\": [{\"op\": \"replace\", \"path\": \"/state/celsius\", \"value\": 21.5}]})\");\n}\n"}
   {:name "Calibration.cpp" :folder src :language "C++" :dirty false :text ""}
   {:name station.screen.json
    :folder screens
    :language "JSON"
    :dirty false
    :text "{\n    \"format\": \"affrescoui-screen\",\n    \"version\": 1,\n    \"permissions\": { \"authors\": { \"agent\": \"review\" } },\n    \"state\": { \"celsius\": 21.5, \"humidity\": 48 },\n    \"root\": { \"children\": [ ... ] }\n}\n"}
   {:name "CMakeLists.txt"
    :folder ""
    :language "CMake"
    :dirty false
    :text "cmake_minimum_required(VERSION 3.25)\nproject(weather-station CXX)\n\nadd_subdirectory(external/AffrescoUI)\nadd_executable(station src/main.cpp src/Station.cpp src/Calibration.cpp)\ntarget_link_libraries(station PRIVATE affrescoui::affrescoui)\n"}
   {:name "README.md"
    :folder ""
    :language "Markdown"
    :dirty false
    :text "# weather-station\n\nA native weather station built on AffrescoUI.\n\n## Build\n    cmake -S . -B build && cmake --build build\n\nTODO: describe how the sensors are wired\n"}])

(defstate terminal
  [{:t "$ cmake --build build" :c prompt}
   {:t "[ 50%] Building CXX object src/Calibration.cpp.o" :c plain}
   {:t "[100%] Linking CXX executable station" :c plain}
   {:t "Build finished: 0 warnings" :c comment}])

(defstate outputs
  {:help [{:t "commands: build, run, help, clear" :c plain}]
   :build [{:t "[100%] Built target station" :c plain} {:t "[Done in 0.8s]" :c comment}]
   :run [{:t "station: reading sensor-0 every second" :c plain}]})
