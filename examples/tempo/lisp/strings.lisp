{:note "Tempo's text in English, Spanish and French, included by index.screen.json. Song, artist, album, playlist and lyric names are data, not text to translate."
  :strings {:default en
    :locale "{{settings.ui.language}}"
    :en {:app.tagline "an AffrescoUI template"
      :menu.label Browse
      :menu.library Library
      :menu.albums Albums
      :menu.artists Artists
      :menu.search Search
      :menu.nowPlaying "Now playing"
      :menu.sound Sound
      :menu.liked "Liked songs"
      :tip.liked "The songs you like (Cmd+4)"
      :liked.caption PLAYLIST
      :liked.title "Liked songs"
      :liked.empty "Songs you like show here: the heart in the player, or a right click on a song."
      :player.like "Like this song"
      :song.play Play
      :song.playNext "Play next"
      :song.addToQueue "Add to the queue"
      :song.like Like
      :song.unlike "Remove from liked songs"
      :song.addToPlaylist "Add to playlist"
      :song.remove "Remove from this playlist"
      :playlist.new "New playlist"
      :now.show "Show {name}"
      :playlist.newName "My playlist {n}"
      :playlist.create "Create a playlist"
      :playlist.more "More for {name}"
      :playlist.rename Rename
      :playlist.delete Delete
      :playlist.nameLabel "The playlist's name: Enter keeps it, Escape leaves it"
      :playlist.deleteTitle "Delete this playlist?"
      :playlist.deleteMessage "{name} and its list of songs leave Tempo. The songs stay in the library."
      :playlist.empty "No songs yet: right click a song anywhere, then Add to playlist."
      :playlist.minutes "{n} min"
      :dialog.cancel Cancel
      :tip.library "Every song (Cmd+L)"
      :tip.albums "Albums (Cmd+2)"
      :tip.artists "Artists (Cmd+3)"
      :tip.search "Search the library (Cmd+F)"
      :tip.nowPlaying "Now playing (Cmd+5)"
      :tip.sound "Equaliser and crossfade (Cmd+6)"
      :theme.toggle "Switch light and dark (Cmd+T)"
      :language.toggle "Change the language (Cmd+Shift+L)"
      :playlists.caption MIXES
      :playlists.label Playlists
      :songs "{n, plural, one {{n} song} other {{n} songs}}"
      :playlist.caption PLAYLIST
      :playlist.play "Double-click to play"
      :playlist.tracks "Songs of {name}"
      :play "  Play"
      :play.label "Play {name}"
      :column.number "#"
      :column.title Title
      :column.artist Artist
      :column.album Album
      :column.time Time
      :library.caption LIBRARY
      :library.all "All songs"
      :library.count "{n} songs  ·  find one with Cmd+P"
      :library.label "Every song in the library"
      :queue.now "ON AIR"
      :queue.from "from {name}"
      :queue.upNext "COMING UP"
      :queue.empty "Nothing after this song"
      :queue.label "Coming up: Enter plays a song, Alt+Up and Alt+Down move it"
      :queue.moved "{title} moved to place {n}"
      :player.previous "Previous (←)"
      :player.playPause "Play or pause (Space)"
      :player.next "Next (→)"
      :player.seek "Position in the song"
      :player.queue "Queue (Cmd+U)"
      :player.volume "Volume (↑ ↓)"
      :player.level Level
      :player.mini "Mini player (Cmd+M)"
      :player.full "Back to the full player (Cmd+M)"
      :player.shuffle "Shuffle (Cmd+S)"
      :player.repeat.off "Repeat: off (Cmd+R)"
      :player.repeat.all "Repeat: all (Cmd+R)"
      :player.repeat.one "Repeat: this song (Cmd+R)"
      :player.mute "Mute (Cmd+Alt+↓)"
      :nav.back "Back (Cmd+[)"
      :nav.forward "Forward (Cmd+])"
      :now.seekLyric "Go to this line"
      :browse.albums "{n} albums  ·  open one to see its songs"
      :browse.artists "{n} artists  ·  open one to see their songs"
      :browse.label "{name}, {songs}"
      :collection.album ALBUM
      :collection.artist ARTIST
      :collection.back Back
      :search.placeholder "Title or artist"
      :search.label "Search the library"
      :search.found "{n, plural, =0 {No song matches} one {{n} song found} other {{n} songs found}}"
      :search.hint "Type part of a title or an artist: the letters in order, not side by side"
      :now.lyrics LYRICS
      :now.visualiser Visualiser
      :now.crossfading "Crossfading into {title}"
      :now.paused Paused
      :sound.equaliser EQUALISER
      :sound.equaliserOn "Equaliser on"
      :sound.equaliserNote "Shapes the visualiser; the sound itself plays flat for now"
      :sound.curve "Equaliser curve: Left and Right pick a band, Up and Down set it"
      :sound.presets Presets
      :preset.flat Flat
      :preset.bass "Bass boost"
      :preset.vocal Vocal
      :preset.treble Treble
      :sound.crossfade CROSSFADE
      :sound.crossfadeLabel "Crossfade between songs"
      :sound.crossfadeValue "{n, plural, =0 {Off} one {{n} second} other {{n} seconds}}"
      :sound.appearance APPEARANCE
      :theme.dark Dark
      :theme.light Light
      :language.en English
      :language.es "Español"
      :language.fr "Français"
      :pick.songs "Find a song by its title"
      :pick.commands "Run a command"}
    :es {:app.tagline "una plantilla de AffrescoUI"
      :menu.label Explorar
      :menu.library Biblioteca
      :menu.albums "Álbumes"
      :menu.artists Artistas
      :menu.search Buscar
      :menu.nowPlaying Reproduciendo
      :menu.sound Sonido
      :menu.liked Favoritas
      :tip.liked "Las canciones que te gustan (Cmd+4)"
      :liked.caption LISTA
      :liked.title Favoritas
      :liked.empty "Aquí aparecen las canciones que te gustan: el corazón del reproductor, o un clic derecho en una canción."
      :player.like "Me gusta esta canción"
      :song.play Reproducir
      :song.playNext "Reproducir a continuación"
      :song.addToQueue "Añadir a la cola"
      :song.like "Me gusta"
      :song.unlike "Quitar de favoritas"
      :song.addToPlaylist "Añadir a una lista"
      :song.remove "Quitar de esta lista"
      :playlist.new "Nueva lista"
      :now.show "Mostrar {name}"
      :playlist.newName "Mi lista {n}"
      :playlist.create "Crear una lista"
      :playlist.more "Más de {name}"
      :playlist.rename "Cambiar el nombre"
      :playlist.delete Eliminar
      :playlist.nameLabel "El nombre de la lista: Intro lo guarda, Esc lo deja"
      :playlist.deleteTitle "¿Eliminar esta lista?"
      :playlist.deleteMessage "{name} y su lista de canciones salen de Tempo. Las canciones siguen en la biblioteca."
      :playlist.empty "Aún no hay canciones: clic derecho en una canción y Añadir a una lista."
      :playlist.minutes "{n} min"
      :dialog.cancel Cancelar
      :tip.library "Todas las canciones (Cmd+L)"
      :tip.albums "Álbumes (Cmd+2)"
      :tip.artists "Artistas (Cmd+3)"
      :tip.search "Buscar en la biblioteca (Cmd+F)"
      :tip.nowPlaying "Reproduciendo (Cmd+5)"
      :tip.sound "Ecualizador y fundido cruzado (Cmd+6)"
      :theme.toggle "Cambiar entre claro y oscuro (Cmd+T)"
      :language.toggle "Cambiar el idioma (Cmd+Shift+L)"
      :playlists.caption MEZCLAS
      :playlists.label "Listas de reproducción"
      :songs "{n, plural, one {{n} canción} other {{n} canciones}}"
      :playlist.caption LISTA
      :playlist.play "Doble clic para reproducir"
      :playlist.tracks "Canciones de {name}"
      :play "  Reproducir"
      :play.label "Reproducir {name}"
      :column.number "#"
      :column.title "Título"
      :column.artist Artista
      :column.album "Álbum"
      :column.time "Duración"
      :library.caption BIBLIOTECA
      :library.all "Todas las canciones"
      :library.count "{n} canciones  ·  encuentra una con Cmd+P"
      :library.label "Todas las canciones de la biblioteca"
      :queue.now SONANDO
      :queue.from "de {name}"
      :queue.upNext "DESPUÉS"
      :queue.empty "Nada después de esta canción"
      :queue.label "Después: Intro reproduce una canción, Alt+Arriba y Alt+Abajo la mueven"
      :queue.moved "{title} movida al puesto {n}"
      :player.previous "Anterior (←)"
      :player.playPause "Reproducir o pausar (Espacio)"
      :player.next "Siguiente (→)"
      :player.seek "Posición en la canción"
      :player.queue "Cola (Cmd+U)"
      :player.volume "Volumen (↑ ↓)"
      :player.level Nivel
      :player.mini "Minirreproductor (Cmd+M)"
      :player.full "Volver al reproductor completo (Cmd+M)"
      :player.shuffle "Aleatorio (Cmd+S)"
      :player.repeat.off "Repetir: no (Cmd+R)"
      :player.repeat.all "Repetir: todo (Cmd+R)"
      :player.repeat.one "Repetir: esta canción (Cmd+R)"
      :player.mute "Silenciar (Cmd+Alt+↓)"
      :nav.back "Atrás (Cmd+[)"
      :nav.forward "Adelante (Cmd+])"
      :now.seekLyric "Ir a esta línea"
      :browse.albums "{n} álbumes  ·  abre uno para ver sus canciones"
      :browse.artists "{n} artistas  ·  abre uno para ver sus canciones"
      :browse.label "{name}, {songs}"
      :collection.album "ÁLBUM"
      :collection.artist ARTISTA
      :collection.back "Atrás"
      :search.placeholder "Título o artista"
      :search.label "Buscar en la biblioteca"
      :search.found "{n, plural, =0 {Ninguna canción coincide} one {{n} canción encontrada} other {{n} canciones encontradas}}"
      :search.hint "Escribe parte de un título o de un artista: las letras en orden, no necesariamente juntas"
      :now.lyrics LETRA
      :now.visualiser Visualizador
      :now.crossfading "Fundido cruzado hacia {title}"
      :now.paused "En pausa"
      :sound.equaliser ECUALIZADOR
      :sound.equaliserOn "Ecualizador activado"
      :sound.equaliserNote "Da forma al visualizador; por ahora el sonido suena plano"
      :sound.curve "Curva del ecualizador: Izquierda y Derecha eligen una banda, Arriba y Abajo la ajustan"
      :sound.presets Preajustes
      :preset.flat Plano
      :preset.bass "Refuerzo de graves"
      :preset.vocal Voz
      :preset.treble Agudos
      :sound.crossfade "FUNDIDO CRUZADO"
      :sound.crossfadeLabel "Fundido cruzado entre canciones"
      :sound.crossfadeValue "{n, plural, =0 {Desactivado} one {{n} segundo} other {{n} segundos}}"
      :sound.appearance APARIENCIA
      :theme.dark Oscuro
      :theme.light Claro
      :language.en English
      :language.es "Español"
      :language.fr "Français"
      :pick.songs "Buscar una canción por su título"
      :pick.commands "Ejecutar un comando"}
    :fr {:app.tagline "un modèle AffrescoUI"
      :menu.label Parcourir
      :menu.library "Bibliothèque"
      :menu.albums Albums
      :menu.artists Artistes
      :menu.search Rechercher
      :menu.nowPlaying "En lecture"
      :menu.sound Son
      :menu.liked "Titres aimés"
      :tip.liked "Les titres que vous aimez (Cmd+4)"
      :liked.caption PLAYLIST
      :liked.title "Titres aimés"
      :liked.empty "Les titres que vous aimez s'affichent ici : le cœur du lecteur, ou un clic droit sur un titre."
      :player.like "J'aime ce titre"
      :song.play Lire
      :song.playNext "Lire ensuite"
      :song.addToQueue "Ajouter à la file"
      :song.like "J'aime"
      :song.unlike "Retirer des titres aimés"
      :song.addToPlaylist "Ajouter à une playlist"
      :song.remove "Retirer de cette playlist"
      :playlist.new "Nouvelle playlist"
      :now.show "Afficher {name}"
      :playlist.newName "Ma playlist {n}"
      :playlist.create "Créer une playlist"
      :playlist.more "Plus pour {name}"
      :playlist.rename Renommer
      :playlist.delete Supprimer
      :playlist.nameLabel "Le nom de la playlist : Entrée le garde, Échap le laisse"
      :playlist.deleteTitle "Supprimer cette playlist ?"
      :playlist.deleteMessage "{name} et sa liste de titres quittent Tempo. Les titres restent dans la bibliothèque."
      :playlist.empty "Pas encore de titres : clic droit sur un titre, puis Ajouter à une playlist."
      :playlist.minutes "{n} min"
      :dialog.cancel Annuler
      :tip.library "Tous les titres (Cmd+L)"
      :tip.albums "Albums (Cmd+2)"
      :tip.artists "Artistes (Cmd+3)"
      :tip.search "Rechercher dans la bibliothèque (Cmd+F)"
      :tip.nowPlaying "En lecture (Cmd+5)"
      :tip.sound "Égaliseur et fondu enchaîné (Cmd+6)"
      :theme.toggle "Basculer entre clair et sombre (Cmd+T)"
      :language.toggle "Changer la langue (Cmd+Shift+L)"
      :playlists.caption MIXES
      :playlists.label Playlists
      :songs "{n, plural, one {{n} titre} other {{n} titres}}"
      :playlist.caption PLAYLIST
      :playlist.play "Double-cliquez pour lire"
      :playlist.tracks "Titres de {name}"
      :play "  Lire"
      :play.label "Lire {name}"
      :column.number "#"
      :column.title Titre
      :column.artist Artiste
      :column.album Album
      :column.time "Durée"
      :library.caption "BIBLIOTHÈQUE"
      :library.all "Tous les titres"
      :library.count "{n} titres  ·  trouvez-en un avec Cmd+P"
      :library.label "Tous les titres de la bibliothèque"
      :queue.now "À L'ANTENNE"
      :queue.from "depuis {name}"
      :queue.upNext ENSUITE
      :queue.empty "Rien après ce titre"
      :queue.label "Ensuite : Entrée lit un titre, Alt+Haut et Alt+Bas le déplacent"
      :queue.moved "{title} déplacé à la place {n}"
      :player.previous "Précédent (←)"
      :player.playPause "Lecture ou pause (Espace)"
      :player.next "Suivant (→)"
      :player.seek "Position dans le titre"
      :player.queue "File d’attente (Cmd+U)"
      :player.volume "Volume (↑ ↓)"
      :player.level Niveau
      :player.mini "Mini-lecteur (Cmd+M)"
      :player.full "Retour au lecteur complet (Cmd+M)"
      :player.shuffle "Lecture aléatoire (Cmd+S)"
      :player.repeat.off "Répéter : non (Cmd+R)"
      :player.repeat.all "Répéter : tout (Cmd+R)"
      :player.repeat.one "Répéter : ce titre (Cmd+R)"
      :player.mute "Couper le son (Cmd+Alt+↓)"
      :nav.back "Retour (Cmd+[)"
      :nav.forward "Avancer (Cmd+])"
      :now.seekLyric "Aller à cette ligne"
      :browse.albums "{n} albums  ·  ouvrez-en un pour voir ses titres"
      :browse.artists "{n} artistes  ·  ouvrez-en un pour voir ses titres"
      :browse.label "{name}, {songs}"
      :collection.album ALBUM
      :collection.artist ARTISTE
      :collection.back Retour
      :search.placeholder "Titre ou artiste"
      :search.label "Rechercher dans la bibliothèque"
      :search.found "{n, plural, =0 {Aucun titre ne correspond} one {{n} titre trouvé} other {{n} titres trouvés}}"
      :search.hint "Saisissez une partie d’un titre ou d’un artiste : les lettres dans l’ordre, pas forcément côte à côte"
      :now.lyrics PAROLES
      :now.visualiser Visualiseur
      :now.crossfading "Fondu enchaîné vers {title}"
      :now.paused "En pause"
      :sound.equaliser "ÉGALISEUR"
      :sound.equaliserOn "Égaliseur activé"
      :sound.equaliserNote "Façonne le visualiseur ; pour l’instant le son reste neutre"
      :sound.curve "Courbe de l’égaliseur : Gauche et Droite choisissent une bande, Haut et Bas la règlent"
      :sound.presets "Préréglages"
      :preset.flat Neutre
      :preset.bass "Graves renforcés"
      :preset.vocal Voix
      :preset.treble Aigus
      :sound.crossfade "FONDU ENCHAÎNÉ"
      :sound.crossfadeLabel "Fondu enchaîné entre les titres"
      :sound.crossfadeValue "{n, plural, =0 {Désactivé} one {{n} seconde} other {{n} secondes}}"
      :sound.appearance APPARENCE
      :theme.dark Sombre
      :theme.light Clair
      :language.en English
      :language.es "Español"
      :language.fr "Français"
      :pick.songs "Trouver un titre par son nom"
      :pick.commands "Exécuter une commande"}}}
