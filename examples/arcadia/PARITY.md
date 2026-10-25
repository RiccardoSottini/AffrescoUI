# Arcadia: parity matrix

Arcadia is a game launcher modelled on the layout and workflows of popular desktop game-store clients. Every game,
studio, price, friend and picture in it is invented; the covers and screenshots are drawn by `art/make_art.py`.

Each row is a feature a daily user of such a client expects, with its state in Arcadia: `done` (works as expected,
looks the part), `partial` (there, but simpler or different) or `missing`. The **Start** column is the launcher as it
was at the start of this work; **Now** is the current state.

| Area | Feature | Start | Now |
|---|---|---|---|
| Chrome | Menu row: Arcadia, View, Friends, Games, Help, with their items | partial | partial |
| Chrome | Navigation pills: Discover, My games, Friends, profile name, with the active one lit | partial | done |
| Chrome | Back and forward through the pages visited | partial | done |
| Chrome | Account menu with the wallet balance; notifications bell with a count | missing | done |
| Chrome | Bottom bar: Add a Game, download progress (Manage), Friends & Chat | missing | done |
| Chrome | The look: its own aubergine and charcoal with an amber accent, rounded 2:3 cover tiles, hover glow, mint call to action | missing | done |
| Library | Left game list: home button, search, installed/ready-to-play filter | partial | done |
| Library | Left list grouped by collection (favourites, uncategorised) with counts | partial | done |
| Library | Play-state colours: installed white, not installed grey, running lime, updating amber | missing | done |
| Library | Home: Latest news shelf of event cards | missing | done |
| Library | Home: Jump back in shelf grouped by time (big tile for the latest) | missing | done |
| Library | Home: collections shown as shelves of cover tiles | partial | done |
| Library | Home: All Games grid with sort (alphabetical, hours played, last played) | partial | done |
| Library | Collections: manual and dynamic (filtered) ones, create a collection | partial | partial |
| Library | Right-click a game: Play/Install, Add to Favourites, Manage, Properties, Uninstall | partial | done |
| Game page | Hero art with the game's logo over it | missing | done |
| Game page | PLAY (mint) / INSTALL (amber) / UPDATE / RUNNING button with a drop-down | partial | done |
| Game page | Last played, play time, achievements progress, cloud status | partial | done |
| Game page | Gear (properties), info and favourite buttons | missing | done |
| Game page | Section tabs: Store Page, DLC, Player hub, Discussions, Guides, Support | missing | partial |
| Game page | Activity feed with a "say something" box and friend posts | partial | done |
| Game page | Friends who play, achievements panel, screenshots, notes, DLC, controller info | partial | partial |
| Game page | Properties dialog: general/launch options, compatibility, updates, installed files, verify, DLC | missing | partial |
| Game page | Uninstall with a confirmation | done | done |
| Store | Store sub-navigation (For you, Fresh, Categories, Rewards, News, Experiments) and search | missing | done |
| Store | Spotlight carousel: big tile, screenshots that change on hover, price, tags | partial | done |
| Store | Special offers with discount badges and old/new prices | missing | done |
| Store | Browse by category tiles | missing | done |
| Store | Game store page: media gallery with a thumbnail strip, header picture, description | missing | done |
| Store | Store page: reviews summary (Very Positive), release date, developer, publisher, tags | missing | done |
| Store | Store page: purchase boxes with Add to Cart, Add to Wishlist, Follow | missing | done |
| Store | Store page: system requirements with platform tabs (minimum / recommended) | missing | done |
| Store | Cart: items, total, remove, purchase for myself | missing | done |
| Store | Wishlist with sort (rank, name, price, discount) | missing | done |
| Downloads | Current download with banner, speed, progress and pause | partial | done |
| Downloads | Network and disk usage graph | missing | done |
| Downloads | Up next queue with reorder, unscheduled, completed lists | partial | done |
| Friends | Friends list: own status, in-game / online / offline groups with presence colours, search, add a friend | partial | done |
| Friends | Chat: message bubbles, timestamps, game invites, typing | missing | done |
| Settings | Settings window with a category list | partial | missing |
| Settings | Interface: language, start page, theme-like options | partial | missing |
| Settings | Downloads: region, bandwidth limit, schedule, clear cache | partial | missing |
| Settings | Storage manager: drives with usage bars by game, move and uninstall | missing | missing |
| Community | Community hub page: activity, screenshots, discussions | missing | missing |
| Profile | Profile page: avatar, level, showcase, recent activity | missing | done |
| TV mode | Big-screen mode entry and its home | missing | missing |
| Keyboard | Every action reachable by keyboard, focus visible, shortcuts | partial | partial |
| Locales | English, Spanish and French | done | done |
| Assistant | The shared assistant chat (bottom-right bubble) | missing | done |

Counts at the start: done 2, partial 21, missing 26 (49 features). Tokens at the start (merged, compact): 24,559.
Counts after round 1 (library and game page): done 16, partial 7, missing 26. Tokens (merged, compact): 19,469.
Counts after round 2 (store): done 26, partial 7, missing 16. Tokens (merged, compact): 36,493 (the store adds about 17,000).
Counts after round 3 (downloads, friends and chat, profile, a game's menu, properties and uninstall, the assistant):
done 37, partial 6, missing 6. Tokens (merged, compact): 66,961. Properties has no compatibility tab: the games are
invented and never run, so there is no compatibility layer to choose.
