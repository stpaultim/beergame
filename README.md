Beer Game
=========

A multiplayer online version of the Beer Distribution Game for Backdrop CMS,
for teaching supply chain management and system dynamics.

Four positions (retailer, wholesaler, distributor and factory) form one supply
chain. Each week every player sees only their own stock, backlog and incoming
orders, and decides one thing: how much to order from the position upstream.
Orders and deliveries take weeks to arrive. Even when customer demand barely
changes, orders swing wildly as they move up the chain (the "bullwhip
effect"). The debrief afterwards shows players how the structure of the system,
not the people in it, caused the trouble.

The module is built for groups playing remotely, with a facilitator running
the game.

Features
--------

- **A story for every role.** Each player's board opens with a short story
  that explains their part of the supply chain, the delays and the costs,
  using the game's own settings. Players can fold it away once read and
  reopen it at any time.
- **Several teams per game.** Each team is its own supply chain with its own
  brewery name, and teams compete on total cost.
- **People play with their site accounts.** The facilitator assigns people to
  positions by username; no join codes are needed.
- **Self-paced weeks.** A team moves on to the next week as soon as every
  person on it has placed an order, so fast teams are not held up by slow ones.
- **Autoplay.** Empty positions are played automatically, and the facilitator
  can turn on autoplay for a position if someone drops out. That person can
  still watch their position and see the results. Autoplay follows Sterman's
  anchoring and adjustment rule, which imitates how people typically play.
- **Demand scenarios.** Classic step, steady growth, seasonal wave, promotion
  spike, noisy steady, random drift, or "Surprise me", which picks one at
  random and keeps it secret until the results. The settings forms show a live
  preview of the demand pattern.
- **Facilitator dashboard.** Watch every team week by week, then start, pause,
  resume or end the game.
- **Optional week timer.** Set a number of seconds per week, then start the
  timer from the dashboard whenever the group is ready (for example, after a
  few untimed practice weeks). Players see a countdown. Anyone who has not
  ordered when time runs out repeats their previous order, and their history
  notes that time ran out. The timer can be stopped and started again at any
  time, for example to answer questions; a facilitator who is also playing can
  do this from their own board.
- **Results and debrief.** Charts of orders, inventory and costs for every
  position, a cost ranking of teams, and a comparison with an all-computer
  team playing the same demand.
- **Hidden game end.** Players are told the game lasts longer than it really
  does, so they don't run down their stock as the end approaches.

Installation
------------

- Install this module using the official Backdrop CMS instructions at
  https://docs.backdropcms.org/documentation/extend-with-modules.
- Give roles the permissions they need at
  Administration > Configuration > User accounts > Permissions:
  - **Play Beer Games**: for everyone who will play.
  - **Facilitate Beer Games**: for people who create and run their own games.
  - **Administer Beer Games**: for people who manage all games and the default
    settings.
- Optionally, adjust the default settings for new games at
  Administration > Configuration > Content authoring > Beer Game
  (`admin/config/content/beergame`).

Running a game
--------------

1. Make sure every player has an account on the site with the
   **Play Beer Games** permission.
2. Go to **Beer Game** (`/beergame`) and click **Create a game**.
3. Choose the number of teams, name each team's brewery, and enter a username
   for each position. Leave a position empty to have autoplay play it.
4. Adjust the game settings if you like: demand scenario, game length,
   starting stock, delays and costs. The defaults follow the classic game.
5. Open the game's **Dashboard** and click **Start game**. Players go to
   `/beergame` and open their game to see their board.
6. When the game ends, click **Show results** and talk through the charts with
   the group. Players can see the results once the game is finished.

Settings
--------

| Setting                     | Default     |
| --------------------------- | ----------- |
| Demand scenario             | Classic step: 4 cases a week, rising to 8 in week 5 |
| Weeks announced to players  | 50          |
| Week the game actually ends | 40          |
| Starting inventory          | 12 cases    |
| Cases in each delay slot    | 4           |
| Order delay                 | 2 weeks     |
| Shipping delay              | 2 weeks     |
| Production request delay    | 1 week      |
| Production delay            | 2 weeks     |
| Inventory cost              | 0.50 per case per week |
| Backlog cost                | 1.00 per case per week |
| Seconds per week (timer)    | 0 (no timer) |

Facilitators can change these for each game until it starts.

Developers
----------

The module uses Backdrop for accounts, permissions, storage, pages and forms.
The game rules themselves are kept in `includes/BeerGameEngine.php`, which
works on plain PHP arrays with no Backdrop calls, so the rules can be read on
their own and unit-tested quickly. Tests are in the `tests` directory and run
with Backdrop's testing module.

Credits
-------

The Beer Distribution Game was created in the 1960s by Jay Forrester's System
Dynamics Group at the MIT Sloan School of Management. This module is an
independent implementation and is not affiliated with or endorsed by MIT.

The computer players and the classic game settings follow John D. Sterman,
"Modeling Managerial Behavior: Misperceptions of Feedback in a Dynamic
Decision Making Experiment", *Management Science* 35(3), 1989.

License
-------

This project is GPL v2 software. See the LICENSE.txt file in this directory
for complete text.

Maintainers
-----------

- [Tim Erickson](https://github.com/stpaultim)
