<?php
/**
 * @file
 * Hooks provided by the Beer Game module.
 */

/**
 * @addtogroup hooks
 * @{
 */

/**
 * Respond to a team completing a week.
 *
 * Invoked after every order for the week is in, the week has been logged and
 * the team has moved on to the next week (or finished).
 *
 * @param BeerGame $game
 *   The game.
 * @param object $team
 *   The team, with its updated state, week and status.
 */
function hook_beergame_week_completed(BeerGame $game, $team) {
  if ($team->status == 'finished') {
    watchdog('mymodule', 'Team %name has finished.', array('%name' => $team->name));
  }
}

/**
 * @} End of "addtogroup hooks".
 */
