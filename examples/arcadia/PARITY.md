# Arcadia: parity matrix

Arcadia is a game launcher modelled on the layout and workflows of popular desktop game-store clients. Every game,
studio, price, friend and picture in it is invented; the covers and screenshots are drawn by `art/make_art.py`.

Each row is a feature a daily user of such a client expects, with its state in Arcadia: `done` (works as expected,
looks the part), `partial` (there, but simpler or different) or `missing`. The **Start** column is the launcher as it
was at the start of this work; **Now** is the current state.

| Area | Feature | Start | Now |
|---|---|---|---|
| Chrome | Menu row: Arcadia, View, Friends, Games, Help, with their items | partial | partial |
| Chrome | Big navigation: STORE, LIBRARY, COMMUNITY, profile name, with the active one lit | partial | done |
| Chrome | Back and forward through the pages visited | partial | done |
| Chrome | Account menu with the wallet balance; notifications bell with a count | missing | done |
| Chrome | Bottom bar: Add a Game, download progress (Manage), Friends & Chat | missing | partial |
| Chrome | The look: dark blue gradients, capsule proportions, hover glow, green call to action | missing | done |
| Library | Left game list: home button, search, installed/ready-to-play filter | partial | done |
| Library | Left list grouped by collection (favourites, uncategorised) with counts | partial | done |
| Library | Play-state colours: installed white, not installed grey, running green, updating blue | missing | done |
| Library | Home: What's New shelf of event cards | missing | done |
| Library | Home: Recent Games shelf grouped by time (big capsule for the latest) | missing | done |
| Library | Home: collections shown as shelves of capsule covers | partial | done |
| Library | Home: All Games grid with sort (alphabetical, hours played, last played) | partial | done |
| Library | Collections: manual and dynamic (filtered) ones, create a collection | partial | partial |
| Library | Right-click a game: Play/Install, Add to Favourites, Manage, Properties, Uninstall | partial | missing |
| Game page | Hero art with the game's logo over it | missing | done |
| Game page | PLAY (green) / INSTALL (blue) / UPDATE / RUNNING button with a drop-down | partial | done |
| Game page | Last played, play time, achievements progress, cloud status | partial | done |
| Game page | Gear (properties), info and favourite buttons | missing | partial |
| Game page | Section tabs: Store Page, DLC, Community Hub, Discussions, Guides, Support | missing | partial |
| Game page | Activity feed with a "say something" box and friend posts | partial | done |
| Game page | Friends who play, achievements panel, screenshots, notes, DLC, controller info | partial | partial |
| Game page | Properties dialog: general/launch options, compatibility, updates, installed files, verify, DLC | missing | missing |
| Game page | Uninstall with a confirmation | done | missing |
| Store | Store sub-navigation (Your Store, New & Noteworthy, Categories, Points Shop, News, Labs) and search | missing | missing |
| Store | Featured carousel: capsule, screenshots that change on hover, price, tags | partial | missing |
| Store | Special offers with discount badges and old/new prices | missing | missing |
| Store | Browse by category tiles | missing | missing |
| Store | Game store page: media gallery with a thumbnail strip, header capsule, description | missing | missing |
| Store | Store page: reviews summary (Very Positive), release date, developer, publisher, tags | missing | missing |
| Store | Store page: purchase boxes with Add to Cart, Add to Wishlist, Follow | missing | missing |
| Store | Store page: system requirements with platform tabs (minimum / recommended) | missing | missing |
| Store | Cart: items, total, remove, purchase for myself | missing | missing |
| Store | Wishlist with sort (rank, name, price, discount) | missing | missing |
| Downloads | Current download with banner, speed, progress and pause | partial | missing |
| Downloads | Network and disk usage graph | missing | missing |
| Downloads | Up next queue with reorder, unscheduled, completed lists | partial | missing |
| Friends | Friends list: own status, in-game / online / offline groups with presence colours | partial | missing |
| Friends | Chat: message bubbles, timestamps, game invites, typing | missing | missing |
| Settings | Settings window with a category list | partial | missing |
| Settings | Interface: language, start page, theme-like options | partial | missing |
| Settings | Downloads: region, bandwidth limit, schedule, clear cache | partial | missing |
| Settings | Storage manager: drives with usage bars by game, move and uninstall | missing | missing |
| Community | Community hub page: activity, screenshots, discussions | missing | missing |
| Profile | Profile page: avatar, level, showcase, recent activity | missing | missing |
| TV mode | Big-screen mode entry and its home | missing | missing |
| Keyboard | Every action reachable by keyboard, focus visible, shortcuts | partial | partial |
| Locales | English, Italian and Arabic (right to left) | done | done |
| Assistant | The shared assistant chat (bottom-right bubble) | missing | missing |

Counts at the start: done 2, partial 21, missing 26 (49 features). Tokens at the start (merged, compact): 24,559.
Counts after round 1 (library and game page): done 16, partial 7, missing 26. Tokens (merged, compact): 19,469.
