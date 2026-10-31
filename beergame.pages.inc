<?php
/**
 * @file
 * Page callbacks and forms for the Beer Game module.
 */

/**
 * JSON callback: previews the customer demand for unsaved settings.
 *
 * Reads the settings from the query string, so facilitators can see the
 * effect of the demand settings as they change them.
 */
function beergame_demand_preview_callback() {
  $settings = beergame_default_settings();
  $scenario = isset($_GET['demand_pattern']) && is_string($_GET['demand_pattern']) ? $_GET['demand_pattern'] : 'step';
  if (!isset(beergame_demand_scenarios()[$scenario])) {
    return MENU_NOT_FOUND;
  }
  if ($scenario == 'surprise') {
    return array('surprise' => TRUE, 'demand' => array());
  }
  $keys = array('demand_initial', 'demand_step', 'demand_step_week', 'demand_variation', 'demand_min', 'demand_max', 'end_week');
  foreach ($keys as $key) {
    if (isset($_GET[$key]) && is_numeric($_GET[$key])) {
      // Keep values in a sensible range so the preview stays cheap.
      $settings[$key] = max(0, min(200, (int) $_GET[$key]));
    }
  }
  $settings['end_week'] = max(1, $settings['end_week']);
  $engine = new BeerGameEngine($settings);
  return array(
    'surprise' => FALSE,
    'sample' => in_array($scenario, array('noisy', 'random')),
    'demand' => $engine->demandSchedule($scenario, $settings['end_week']),
  );
}

/**
 * Page callback: lists the games the current person plays or facilitates.
 */
function beergame_list_page() {
  global $user;
  $build = array();
  $status_labels = beergame_status_labels();
  $role_labels = beergame_role_labels();

  $query = 'SELECT g.gid, t.name AS team, t.week, p.role,'
    . ' (SELECT SUM(l.cost) FROM {beergame_log} l WHERE l.tid = p.tid AND l.role = p.role) AS position_cost,'
    . ' (SELECT SUM(l.cost) FROM {beergame_log} l WHERE l.tid = p.tid) AS team_cost,'
    . ' (SELECT MAX(l.week) FROM {beergame_log} l WHERE l.tid = p.tid) AS weeks_played'
    . ' FROM {beergame_position} p INNER JOIN {beergame_team} t ON t.tid = p.tid INNER JOIN {beergame_game} g ON g.gid = t.gid'
    . ' WHERE p.uid = :uid ORDER BY g.created DESC';
  $result = db_query($query, array(':uid' => $user->uid));
  $rows = array();
  foreach ($result as $record) {
    $game = beergame_game_load($record->gid);
    $links = array();
    if ($game->status != 'setup') {
      $links[] = l(t('Play'), 'beergame/' . $game->gid . '/play');
    }
    if (beergame_game_access('results', $game)) {
      $links[] = l(t('Results'), 'beergame/' . $game->gid . '/results');
    }
    $rows[] = array(
      check_plain($game->title),
      format_date($game->created, 'short'),
      check_plain($record->team),
      $role_labels[$record->role],
      $status_labels[$game->status],
      $record->week ? $record->week : '-',
      // Knowing the scenario would give the game away, so players only see
      // it once the game is over.
      $game->status == 'finished' ? beergame_demand_scenario_label($game) : t('Revealed at the end'),
      isset($record->position_cost) ? '$' . number_format($record->position_cost, 2) : '-',
      isset($record->team_cost) ? '$' . number_format($record->team_cost, 2) : '-',
      // Shown once the game is over, like the scenario.
      $game->status == 'finished' && $record->weeks_played ? '$' . number_format(array_sum(beergame_benchmark($game, (int) $record->weeks_played)), 2) : '-',
      implode(' · ', $links),
    );
  }
  if ($rows || !beergame_facilitator_access()) {
    $build['playing'] = array(
      '#theme' => 'table',
      '#caption' => t('Games you are playing'),
      '#header' => array(t('Game'), t('Played'), t('Team'), t('Position'), t('Status'), t('Week'), t('Scenario'), t('Your cost'), t('Team cost'), t('Computers alone'), t('Operations')),
      '#rows' => $rows,
      '#empty' => t('You have not been given a position in any game yet. Your facilitator will add you to a team.'),
    );
  }

  if (beergame_facilitator_access()) {
    $query = db_select('beergame_game', 'g')
      ->fields('g', array('gid'))
      ->orderBy('created', 'DESC');
    if (!user_access('administer beer games')) {
      $query->condition('uid', $user->uid);
    }
    $rows = array();
    foreach (entity_load('beergame_game', $query->execute()->fetchCol()) as $game) {
      $team_count = db_query('SELECT COUNT(*) FROM {beergame_team} WHERE gid = :gid', array(':gid' => $game->gid))->fetchField();
      $rows[] = array(
        l($game->title, 'beergame/' . $game->gid),
        $status_labels[$game->status],
        $team_count,
        format_date($game->created, 'short'),
        l(t('Edit'), 'beergame/' . $game->gid . '/edit'),
      );
    }
    $build['facilitating'] = array(
      '#theme' => 'table',
      '#caption' => t('Games you facilitate'),
      '#header' => array(t('Game'), t('Status'), t('Teams'), t('Created'), t('Operations')),
      '#rows' => $rows,
      '#empty' => t('No games yet. <a href="@url">Create a game</a>.', array('@url' => url('beergame/add'))),
    );
  }
  return $build;
}

/**
 * Page callback: creates a new game.
 */
function beergame_game_add_page() {
  global $user;
  $game = new BeerGame(array(
    'uid' => $user->uid,
    'settings' => beergame_default_settings(),
  ));
  backdrop_set_title(t('Create a game'));
  return backdrop_get_form('beergame_game_form', $game);
}

/**
 * Builds the form elements for game settings.
 *
 * Shared by the game form and the default settings form.
 *
 * @param array $settings
 *   Current settings values.
 *
 * @return array
 *   Form elements keyed by setting name.
 */
function beergame_settings_elements(array $settings) {
  $integer = array('#type' => 'number', '#min' => 0, '#step' => 1, '#size' => 6);
  $elements['demand'] = array(
    '#type' => 'fieldset',
    '#title' => t('Customer demand'),
    '#description' => t('Only the retailer ever sees customer demand.'),
  );
  // The scenario choice sits on the left and its settings on the right.
  // Markup wrappers are used rather than containers so the submitted values
  // keep their flat structure.
  $scenario_is = function (array $scenarios) {
    $values = array();
    foreach ($scenarios as $scenario) {
      $values[] = array('value' => $scenario);
    }
    return array(
      'visible' => array(':input[name$="[demand_pattern]"]' => $values),
    );
  };
  $elements['demand']['demand_pattern'] = array(
    '#type' => 'radios',
    '#title' => t('Demand scenario'),
    '#options' => array(),
    '#default_value' => $settings['demand_pattern'],
    '#prefix' => '<div class="beergame-demand-layout"><div class="beergame-demand-choice">',
    '#suffix' => '</div><div class="beergame-demand-options"><h3 class="beergame-demand-options-title">' . t('Scenario settings') . '</h3>',
  );
  foreach (beergame_demand_scenarios() as $key => $scenario) {
    $elements['demand']['demand_pattern']['#options'][$key] = $scenario['label'];
    $elements['demand']['demand_pattern'][$key]['#description'] = $scenario['description'];
  }
  $elements['demand']['demand_initial'] = array(
    '#title' => t('Cases per week at the start'),
    '#default_value' => $settings['demand_initial'],
  ) + $integer;
  $elements['demand']['demand_step_week'] = array(
    '#title' => t('Week demand starts to change'),
    '#default_value' => $settings['demand_step_week'],
    '#min' => 1,
  ) + $integer;
  $elements['demand']['demand_step'] = array(
    '#title' => t('Cases per week after the step'),
    '#description' => t('Used by Classic step, including when Surprise me picks it.'),
    '#default_value' => $settings['demand_step'],
    '#states' => $scenario_is(array('step', 'surprise')),
  ) + $integer;
  $elements['demand']['demand_variation'] = array(
    '#title' => t('Largest change per week (cases)'),
    '#description' => t('Used by Random drift, including when Surprise me picks it. Each week demand rises or falls by a random amount up to this many cases.'),
    '#default_value' => $settings['demand_variation'],
    '#states' => $scenario_is(array('random', 'surprise')),
  ) + $integer;
  $elements['demand']['demand_min'] = array(
    '#title' => t('Lowest demand (cases per week)'),
    '#description' => t('Demand always stays between the lowest and highest demand.'),
    '#default_value' => $settings['demand_min'],
  ) + $integer;
  $elements['demand']['demand_max'] = array(
    '#title' => t('Highest demand (cases per week)'),
    '#default_value' => $settings['demand_max'],
    '#min' => 1,
    '#suffix' => '<div class="beergame-demand-preview" aria-live="polite"></div></div></div>',
  ) + $integer;
  $path = backdrop_get_path('module', 'beergame');
  $elements['demand']['#attached'] = array(
    'library' => array(array('system', 'jquery.once')),
    'js' => array(
      $path . '/js/beergame.chart.js',
      $path . '/js/beergame.preview.js',
      array(
        'type' => 'setting',
        'data' => array(
          'beergamePreview' => array(
            'url' => url('beergame/demand-preview'),
            'labels' => array(
              'title' => t('Preview: customer demand'),
              'demand' => t('Customer orders'),
              'week' => t('Week'),
              'sample' => t('One possible run. The actual demand is drawn at random when the game starts.'),
              'surprise' => t('A scenario is picked at random when the game starts, so there is nothing to preview.'),
              'error' => t('The preview could not be loaded.'),
            ),
          ),
        ),
      ),
    ),
  );

  $elements['length'] = array(
    '#type' => 'fieldset',
    '#title' => t('Game length'),
  );
  $elements['length']['announced_weeks'] = array(
    '#title' => t('Weeks announced to players'),
    '#description' => t('Players are told the game lasts this long, so they do not run down their stock as the end approaches.'),
    '#default_value' => $settings['announced_weeks'],
    '#min' => 1,
  ) + $integer;
  $elements['length']['end_week'] = array(
    '#title' => t('Week the game actually ends'),
    '#default_value' => $settings['end_week'],
    '#min' => 1,
  ) + $integer;

  $elements['timer'] = array(
    '#type' => 'fieldset',
    '#title' => t('Timer'),
  );
  $elements['timer']['week_seconds'] = array(
    '#title' => t('Seconds per week'),
    '#description' => t('Leave at 0 for no timer. The timer only runs once you start it from the game dashboard. Anyone who has not ordered when time runs out repeats their order from the previous week.'),
    '#default_value' => $settings['week_seconds'],
  ) + $integer;

  $elements['starting'] = array(
    '#type' => 'fieldset',
    '#title' => t('Starting position'),
    '#description' => t('Every position starts in balance: the same inventory, and the same amount in every delivery, order and production slot.'),
  );
  $elements['starting']['starting_inventory'] = array(
    '#title' => t('Starting inventory (cases)'),
    '#default_value' => $settings['starting_inventory'],
  ) + $integer;
  $elements['starting']['pipeline_initial'] = array(
    '#title' => t('Cases in each delay slot'),
    '#default_value' => $settings['pipeline_initial'],
  ) + $integer;

  $elements['delays'] = array(
    '#type' => 'fieldset',
    '#title' => t('Delays (weeks)'),
  );
  $elements['delays']['order_delay'] = array(
    '#title' => t('Order delay'),
    '#description' => t('Time for an order to reach the supplier.'),
    '#default_value' => $settings['order_delay'],
    '#min' => 1,
  ) + $integer;
  $elements['delays']['shipping_delay'] = array(
    '#title' => t('Shipping delay'),
    '#description' => t('Time for beer to reach the customer.'),
    '#default_value' => $settings['shipping_delay'],
    '#min' => 1,
  ) + $integer;
  $elements['delays']['production_request_delay'] = array(
    '#title' => t('Production request delay'),
    '#description' => t('Time for the factory\'s order to reach the brewery.'),
    '#default_value' => $settings['production_request_delay'],
    '#min' => 1,
  ) + $integer;
  $elements['delays']['production_delay'] = array(
    '#title' => t('Production delay'),
    '#description' => t('Time to brew beer.'),
    '#default_value' => $settings['production_delay'],
    '#min' => 1,
  ) + $integer;

  $elements['costs'] = array(
    '#type' => 'fieldset',
    '#title' => t('Costs per case per week'),
  );
  $elements['costs']['holding_cost'] = array(
    '#type' => 'number',
    '#title' => t('Inventory'),
    '#default_value' => $settings['holding_cost'],
    '#min' => 0,
    '#step' => 0.01,
    '#size' => 6,
  );
  $elements['costs']['backlog_cost'] = array(
    '#type' => 'number',
    '#title' => t('Backlog'),
    '#default_value' => $settings['backlog_cost'],
    '#min' => 0,
    '#step' => 0.01,
    '#size' => 6,
  );
  return $elements;
}

/**
 * Flattens submitted settings values from beergame_settings_elements().
 *
 * @param array $values
 *   Values from the settings element tree.
 *
 * @return array
 *   Settings keyed by name.
 */
function beergame_settings_values(array $values) {
  $settings = array();
  foreach ($values as $group) {
    foreach ($group as $key => $value) {
      if ($key == 'demand_pattern') {
        $settings[$key] = $value;
      }
      else {
        $settings[$key] = in_array($key, array('holding_cost', 'backlog_cost')) ? (float) $value : (int) $value;
      }
    }
  }
  return $settings;
}

/**
 * Validates submitted game settings.
 *
 * @param array $settings
 *   Settings keyed by name.
 * @param string $parent
 *   The form element name holding the settings, used to flag errors.
 */
function beergame_settings_validate(array $settings, $parent) {
  if ($settings['demand_min'] > $settings['demand_max']) {
    form_set_error($parent . '][demand][demand_max', t('The highest demand cannot be lower than the lowest demand.'));
  }
  if ($settings['end_week'] > $settings['announced_weeks']) {
    form_set_error($parent . '][length][end_week', t('The game cannot end after the number of weeks announced to players.'));
  }
}

/**
 * Form constructor for creating and editing a game.
 *
 * @param BeerGame $game
 *   The game.
 *
 * @see beergame_game_form_validate()
 * @see beergame_game_form_submit()
 */
function beergame_game_form($form, &$form_state, BeerGame $game) {
  $form_state['game'] = $game;
  $setup = $game->status == 'setup';
  $teams = array_values(beergame_teams_load($game));

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Game name'),
    '#default_value' => $game->title ? $game->title : t('Beer Game, @date', array('@date' => format_date(REQUEST_TIME, 'custom', 'F j, Y'))),
    '#required' => TRUE,
    '#maxlength' => 255,
  );

  if (isset($form_state['values']['team_count'])) {
    $team_count = (int) $form_state['values']['team_count'];
  }
  else {
    $team_count = max(1, count($teams));
  }
  $form['team_count'] = array(
    '#type' => 'select',
    '#title' => t('Number of teams'),
    '#description' => $setup ? t('Each team is one supply chain of four positions. Teams compete for the lowest total cost.') : t('Teams cannot be added or removed once the game has started.'),
    '#options' => backdrop_map_assoc(range(1, 12)),
    '#default_value' => $team_count,
    '#disabled' => !$setup,
    '#ajax' => array(
      'callback' => 'beergame_game_form_teams_ajax',
      'wrapper' => 'beergame-teams',
    ),
  );

  $form['teams'] = array(
    '#type' => 'container',
    '#tree' => TRUE,
    '#prefix' => '<div id="beergame-teams">',
    '#suffix' => '</div>',
  );
  $form['teams']['help'] = array(
    '#type' => 'help',
    '#markup' => t('Enter the username of the person playing each position. Leave a position blank and the computer will play it. People need the "Play Beer Games" permission.'),
  );
  for ($i = 0; $i < $team_count; $i++) {
    $team = isset($teams[$i]) ? $teams[$i] : NULL;
    $form['teams'][$i] = array(
      '#type' => 'fieldset',
      '#title' => t('Team @number', array('@number' => $i + 1)),
      '#attributes' => array('class' => array('beergame-team-fieldset')),
    );
    $form['teams'][$i]['tid'] = array(
      '#type' => 'value',
      '#value' => $team ? $team->tid : NULL,
    );
    $form['teams'][$i]['name'] = array(
      '#type' => 'textfield',
      '#title' => t('Brewery name'),
      '#default_value' => $team ? $team->name : t('Team @number', array('@number' => $i + 1)),
      '#required' => TRUE,
      '#maxlength' => 128,
    );
    foreach (beergame_role_labels() as $role => $label) {
      $default = '';
      if ($team && $team->positions[$role]->uid) {
        $account = user_load($team->positions[$role]->uid);
        $default = $account ? $account->name : '';
      }
      $form['teams'][$i][$role] = array(
        '#type' => 'textfield',
        '#title' => $label,
        '#default_value' => $default,
        '#autocomplete_path' => 'beergame/autocomplete',
        '#maxlength' => 60,
        '#attributes' => array('placeholder' => t('Computer')),
      );
    }
  }

  $form['settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Game settings'),
    '#description' => $setup ? t('The defaults follow the classic game.') : t('Settings cannot be changed once the game has started.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#tree' => TRUE,
    '#disabled' => !$setup,
  );
  $form['settings'] += beergame_settings_elements($game->settings + beergame_default_settings());

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save game'),
  );
  if ($game->id()) {
    $form['actions']['cancel'] = array(
      '#type' => 'link',
      '#title' => t('Cancel'),
      '#href' => 'beergame/' . $game->id(),
    );
  }
  return $form;
}

/**
 * AJAX callback: returns the team fieldsets after the team count changes.
 */
function beergame_game_form_teams_ajax($form, &$form_state) {
  return $form['teams'];
}

/**
 * Form validation handler for beergame_game_form().
 *
 * @see beergame_game_form_submit()
 */
function beergame_game_form_validate($form, &$form_state) {
  $seen = array();
  $form_state['beergame_uids'] = array();
  foreach (element_children($form['teams']) as $i) {
    if (!is_numeric($i)) {
      continue;
    }
    foreach (array_keys(beergame_role_labels()) as $role) {
      $name = trim($form_state['values']['teams'][$i][$role]);
      $uid = 0;
      if ($name !== '') {
        $account = user_load_by_name($name);
        if (!$account) {
          form_set_error('teams][' . $i . '][' . $role, t('There is no account named %name.', array('%name' => $name)));
          continue;
        }
        if (isset($seen[$account->uid])) {
          form_set_error('teams][' . $i . '][' . $role, t('%name already has a position in this game. Each person can play one position.', array('%name' => $name)));
          continue;
        }
        $seen[$account->uid] = TRUE;
        $uid = $account->uid;
      }
      $form_state['beergame_uids'][$i][$role] = $uid;
    }
  }
  if ($form_state['game']->status == 'setup') {
    beergame_settings_validate(beergame_settings_values($form_state['values']['settings']), 'settings');
  }
}

/**
 * Form submission handler for beergame_game_form().
 *
 * @see beergame_game_form_validate()
 */
function beergame_game_form_submit($form, &$form_state) {
  $game = $form_state['game'];
  $setup = $game->status == 'setup';
  $is_new = $game->isNew();
  $game->title = $form_state['values']['title'];
  if ($setup) {
    $game->settings = beergame_settings_values($form_state['values']['settings']);
  }
  $game->save();

  $kept = array();
  foreach (element_children($form['teams']) as $i) {
    if (!is_numeric($i)) {
      continue;
    }
    $values = $form_state['values']['teams'][$i];
    $transaction = db_transaction();
    if ($values['tid']) {
      // Lock and reload, so a running game's state is not overwritten.
      db_query('SELECT tid FROM {beergame_team} WHERE tid = :tid FOR UPDATE', array(':tid' => $values['tid']));
      $team = beergame_team_load($values['tid']);
    }
    else {
      $team = (object) array('gid' => $game->id(), 'week' => 0, 'status' => 'ready', 'state' => array(), 'positions' => array());
      foreach (array_keys(beergame_role_labels()) as $role) {
        $team->positions[$role] = (object) array('uid' => 0, 'pending_order' => NULL);
      }
    }
    $team->name = $values['name'];
    foreach ($form_state['beergame_uids'][$i] as $role => $uid) {
      if ($team->positions[$role]->uid != $uid) {
        $team->positions[$role]->uid = $uid;
        $team->positions[$role]->pending_order = NULL;
      }
    }
    beergame_team_save($team);
    $kept[] = $team->tid;
    if ($team->status == 'playing') {
      // A position may have been handed to the computer.
      beergame_team_advance($game, $team->tid);
    }
    unset($transaction);
  }

  if ($setup) {
    foreach (beergame_teams_load($game) as $tid => $team) {
      if (!in_array($tid, $kept)) {
        beergame_team_delete($tid);
      }
    }
  }
  backdrop_static_reset('beergame_person_position');

  backdrop_set_message($is_new ? t('The game %title has been created.', array('%title' => $game->title)) : t('The game %title has been saved.', array('%title' => $game->title)));
  $form_state['redirect'] = 'beergame/' . $game->id();
}

/**
 * Form constructor for deleting a game.
 *
 * @param BeerGame $game
 *   The game.
 */
function beergame_game_delete_form($form, &$form_state, BeerGame $game) {
  $form_state['game'] = $game;
  return confirm_form(
    $form,
    t('Are you sure you want to delete the game %title?', array('%title' => $game->title)),
    'beergame/' . $game->id(),
    t('All teams and their week-by-week results will be deleted. This cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Form submission handler for beergame_game_delete_form().
 */
function beergame_game_delete_form_submit($form, &$form_state) {
  $game = $form_state['game'];
  $game->delete();
  backdrop_set_message(t('The game %title has been deleted.', array('%title' => $game->title)));
  $form_state['redirect'] = 'beergame';
}

/**
 * Page callback: the game page.
 *
 * Facilitators see the dashboard; players are sent to their board.
 *
 * @param BeerGame $game
 *   The game.
 */
function beergame_game_page(BeerGame $game) {
  if (!beergame_game_access('update', $game)) {
    backdrop_goto('beergame/' . $game->id() . '/play');
  }
  beergame_timer_check($game);
  $status_labels = beergame_status_labels();
  $settings = $game->engine()->getSettings();

  $items = array(
    t('Status: <strong id="beergame-status">@status</strong>', array('@status' => $status_labels[$game->status])),
    t('Players are told the game lasts @announced weeks; it ends after week @end.', array('@announced' => $settings['announced_weeks'], '@end' => $settings['end_week'])),
    beergame_demand_summary($game),
  );
  if ($settings['week_seconds']) {
    $items[] = empty($game->settings['timer_running'])
      ? t('Timer: @seconds seconds a week, not running.', array('@seconds' => $settings['week_seconds']))
      : t('Timer: @seconds seconds a week, running.', array('@seconds' => $settings['week_seconds']));
  }
  $build['summary'] = array(
    '#theme' => 'item_list',
    '#items' => $items,
    '#attributes' => array('class' => array('beergame-summary')),
  );
  $build['controls'] = backdrop_get_form('beergame_control_form', $game);
  $build['teams'] = array(
    '#type' => 'container',
    '#attributes' => array('id' => 'beergame-monitor'),
    'table' => beergame_monitor_table($game),
  );
  $build['#attached']['js'][] = backdrop_get_path('module', 'beergame') . '/js/beergame.dashboard.js';
  $build['#attached']['library'][] = array('system', 'jquery.once');
  $build['#attached']['js'][] = array(
    'type' => 'setting',
    'data' => array(
      'beergame' => array(
        'monitorUrl' => url('beergame/' . $game->id() . '/monitor'),
        'status' => $game->status,
        'pollInterval' => (int) config_get('beergame.settings', 'poll_interval') * 1000,
      ),
    ),
  );
  return $build;
}

/**
 * Builds the facilitator's table of team progress.
 *
 * @param BeerGame $game
 *   The game.
 *
 * @return array
 *   A render array.
 */
function beergame_monitor_table(BeerGame $game) {
  $settings = $game->engine()->getSettings();
  $role_labels = beergame_role_labels();
  $header = array(t('Team'), t('Week'));
  foreach ($role_labels as $label) {
    $header[] = $label;
  }
  $header[] = t('Team cost');
  if ($settings['week_seconds']) {
    $header[] = t('Time left');
  }

  $rows = array();
  foreach (beergame_teams_load($game) as $team) {
    $row = array(
      check_plain($team->name),
      $team->week ? t('@week of @end', array('@week' => $team->week, '@end' => $settings['end_week'])) : '-',
    );
    $cost = 0;
    foreach ($team->positions as $role => $position) {
      if (!empty($team->state['positions'][$role])) {
        $cost += $team->state['positions'][$role]['cost_total'];
      }
      $cell = '<span class="beergame-player">' . check_plain(beergame_position_player_name($position)) . '</span>';
      if ($team->status == 'playing' && $position->uid) {
        if (isset($position->pending_order)) {
          $cell .= ' <span class="beergame-badge beergame-badge-ready">' . icon('check-circle', array('attributes' => array('width' => 16, 'height' => 16))) . t('Ordered') . '</span>';
        }
        else {
          $cell .= ' <span class="beergame-badge beergame-badge-waiting">' . icon('hourglass', array('attributes' => array('width' => 16, 'height' => 16))) . t('Deciding') . '</span>';
        }
        $cell .= '<br>' . l(t('Hand to computer'), 'beergame/' . $game->id() . '/computer/' . $team->tid . '/' . $role, array(
          'query' => array('token' => backdrop_get_token('beergame-computer-' . $team->tid . '-' . $role)),
          'attributes' => array('class' => array('beergame-takeover')),
        ));
      }
      $row[] = $cell;
    }
    $row[] = $team->state ? '$' . number_format($cost, 2) : '-';
    if ($settings['week_seconds']) {
      $seconds_left = beergame_timer_seconds_left($game, $team);
      if ($seconds_left !== NULL) {
        $row[] = format_string('@minutes:@seconds', array('@minutes' => floor($seconds_left / 60), '@seconds' => sprintf('%02d', $seconds_left % 60)));
      }
      else {
        $row[] = ($team->status == 'playing' && $game->status != 'finished') ? t('Not started') : '-';
      }
    }
    $rows[] = array(
      'data' => $row,
      'class' => array('beergame-team-' . $team->status),
    );
  }
  return array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No teams yet. <a href="@url">Edit the game</a> to add teams.', array('@url' => url('beergame/' . $game->id() . '/edit'))),
    '#attributes' => array('class' => array('beergame-monitor-table')),
  );
}

/**
 * Form constructor for the facilitator's game controls.
 *
 * @param BeerGame $game
 *   The game.
 */
function beergame_control_form($form, &$form_state, BeerGame $game) {
  $form_state['game'] = $game;
  $form['actions'] = array('#type' => 'actions');
  switch ($game->status) {
    case 'setup':
      $form['actions']['start'] = array(
        '#type' => 'submit',
        '#value' => t('Start game'),
        '#submit' => array('beergame_control_form_start'),
      );
      break;

    case 'running':
      $form['actions']['pause'] = array(
        '#type' => 'submit',
        '#value' => t('Pause'),
        '#submit' => array('beergame_control_form_pause'),
      );
      break;

    case 'paused':
      $form['actions']['resume'] = array(
        '#type' => 'submit',
        '#value' => t('Resume'),
        '#submit' => array('beergame_control_form_resume'),
      );
      break;
  }
  $week_seconds = $game->engine()->getSettings()['week_seconds'];
  if ($week_seconds && in_array($game->status, array('running', 'paused'))) {
    if (empty($game->settings['timer_running'])) {
      $form['actions']['timer_start'] = array(
        '#type' => 'submit',
        '#value' => t('Start timer'),
        '#submit' => array('beergame_control_form_timer_start'),
        '#access' => $game->status == 'running',
      );
    }
    else {
      $form['actions']['timer_stop'] = array(
        '#type' => 'submit',
        '#value' => t('Stop timer'),
        '#submit' => array('beergame_control_form_timer_stop'),
        '#attributes' => array('class' => array('button-secondary')),
      );
    }
  }
  if (in_array($game->status, array('running', 'paused'))) {
    $form['actions']['end'] = array(
      '#type' => 'submit',
      '#value' => t('End game now'),
      '#submit' => array('beergame_control_form_end'),
      '#attributes' => array('class' => array('button-secondary')),
    );
  }
  if ($game->status == 'finished') {
    $form['actions']['results'] = array(
      '#type' => 'link',
      '#title' => t('Show results'),
      '#href' => 'beergame/' . $game->id() . '/results',
      '#attributes' => array('class' => array('button')),
    );
  }
  return $form;
}

/**
 * Submit handler: starts the game.
 */
function beergame_control_form_start($form, &$form_state) {
  $game = $form_state['game'];
  if (!beergame_teams_load($game)) {
    backdrop_set_message(t('Add at least one team before starting the game.'), 'error');
    return;
  }
  beergame_game_start($game);
  backdrop_set_message(t('The game has started. Players can now place their orders for week 1.'));
}

/**
 * Submit handler: pauses the game.
 */
function beergame_control_form_pause($form, &$form_state) {
  $game = $form_state['game'];
  $game->status = 'paused';
  // Remembered so the timer can carry on from the same point on resume.
  $game->settings['paused_at'] = REQUEST_TIME;
  $game->save();
  backdrop_set_message(t('The game is paused. Players cannot order until you resume.'));
}

/**
 * Submit handler: resumes the game.
 */
function beergame_control_form_resume($form, &$form_state) {
  $game = $form_state['game'];
  if (!empty($game->settings['paused_at'])) {
    // Give back the time the game spent paused.
    db_update('beergame_team')
      ->expression('deadline', 'deadline + :paused', array(':paused' => REQUEST_TIME - $game->settings['paused_at']))
      ->condition('gid', $game->id())
      ->condition('deadline', 0, '>')
      ->execute();
    unset($game->settings['paused_at']);
  }
  $game->status = 'running';
  $game->save();
  foreach (beergame_teams_load($game) as $team) {
    beergame_team_advance($game, $team->tid);
  }
  backdrop_set_message(t('The game has resumed.'));
}

/**
 * Submit handler: starts the week timer.
 */
function beergame_control_form_timer_start($form, &$form_state) {
  $game = $form_state['game'];
  beergame_timer_start($game);
  $seconds = $game->engine()->getSettings()['week_seconds'];
  backdrop_set_message(format_plural($seconds, 'The timer has started. Each team now has 1 second for every week.', 'The timer has started. Each team now has @count seconds for every week.'));
}

/**
 * Submit handler: stops the week timer.
 */
function beergame_control_form_timer_stop($form, &$form_state) {
  beergame_timer_stop($form_state['game']);
  backdrop_set_message(t('The timer has stopped. Teams can take as long as they like.'));
}

/**
 * Submit handler: ends the game early.
 */
function beergame_control_form_end($form, &$form_state) {
  $game = $form_state['game'];
  db_update('beergame_team')
    ->fields(array('status' => 'finished'))
    ->condition('gid', $game->id())
    ->execute();
  $game->status = 'finished';
  $game->save();
  backdrop_set_message(t('The game has ended. Players can now see the results.'));
}

/**
 * JSON callback: the facilitator's live view of all teams.
 *
 * @param BeerGame $game
 *   The game.
 */
function beergame_monitor_callback(BeerGame $game) {
  beergame_timer_check($game);
  $status_labels = beergame_status_labels();
  $table = beergame_monitor_table($game);
  return array(
    'status' => $game->status,
    'statusLabel' => $status_labels[$game->status],
    'html' => backdrop_render($table),
  );
}

/**
 * Page callback: hands a position over to the computer.
 *
 * @param BeerGame $game
 *   The game.
 * @param int $tid
 *   The team ID.
 * @param string $role
 *   The role.
 */
function beergame_computer_takeover_callback(BeerGame $game, $tid, $role) {
  if (!isset($_GET['token']) || !backdrop_valid_token($_GET['token'], 'beergame-computer-' . $tid . '-' . $role)) {
    return MENU_ACCESS_DENIED;
  }
  $labels = beergame_role_labels();
  if (!isset($labels[$role])) {
    return MENU_NOT_FOUND;
  }
  beergame_position_to_computer($game, $tid, $role);
  backdrop_set_message(t('The computer is now playing @role.', array('@role' => $labels[$role])));
  backdrop_goto('beergame/' . $game->id());
}

/**
 * Page callback: the player's board.
 *
 * @param BeerGame $game
 *   The game.
 */
function beergame_board_page(BeerGame $game) {
  global $user;
  beergame_timer_check($game, beergame_person_position($game, $user->uid)->tid);
  $path = backdrop_get_path('module', 'beergame');
  $build['board'] = array(
    '#type' => 'container',
    '#attributes' => array('id' => 'beergame-board', 'class' => array('beergame-board')),
    'loading' => array('#markup' => '<p>' . t('Loading your position…') . '</p>'),
  );
  $build['#attached']['js'][] = $path . '/js/beergame.board.js';
  $build['#attached']['library'][] = array('system', 'jquery.once');
  $build['#attached']['js'][] = array(
    'type' => 'setting',
    'data' => array(
      'beergame' => array(
        'stateUrl' => url('beergame/' . $game->id() . '/board-state'),
        'orderUrl' => url('beergame/' . $game->id() . '/order'),
        'token' => backdrop_get_token('beergame-order-' . $game->id()),
        'pollInterval' => (int) config_get('beergame.settings', 'poll_interval') * 1000,
        'initial' => beergame_board_state($game, $user->uid),
      ),
    ),
  );
  return $build;
}

/**
 * Builds the data a player's board shows.
 *
 * Only information the player would see at a physical board is included:
 * their own stock, backlog, incoming order, and the beer on its way to them.
 *
 * @param BeerGame $game
 *   The game.
 * @param int $uid
 *   The player's user ID.
 *
 * @return array
 *   Board data for beergame.board.js.
 */
function beergame_board_state(BeerGame $game, $uid) {
  $position = beergame_person_position($game, $uid);
  $team = beergame_team_load($position->tid);
  $role = $position->role;
  $settings = $game->engine()->getSettings();
  $role_labels = beergame_role_labels();
  $status_labels = beergame_status_labels();

  $people = 0;
  $ordered = 0;
  foreach ($team->positions as $team_position) {
    if ($team_position->uid) {
      $people++;
      if (isset($team_position->pending_order)) {
        $ordered++;
      }
    }
  }

  $data = array(
    'gameStatus' => $game->status,
    'gameStatusLabel' => $status_labels[$game->status],
    'teamName' => $team->name,
    'teamStatus' => $team->status,
    'week' => (int) $team->week,
    'announcedWeeks' => (int) $settings['announced_weeks'],
    'role' => $role,
    'roleLabel' => $role_labels[$role],
    'upstreamLabel' => $role == 'factory' ? t('the brewery') : $role_labels[BeerGameEngine::ROLES[array_search($role, BeerGameEngine::ROLES) + 1]],
    'downstreamLabel' => $role == 'retailer' ? t('customers') : $role_labels[BeerGameEngine::ROLES[array_search($role, BeerGameEngine::ROLES) - 1]],
    'holdingCost' => (float) $settings['holding_cost'],
    'backlogCost' => (float) $settings['backlog_cost'],
    'people' => $people,
    'ordered' => $ordered,
    'pendingOrder' => isset($team->positions[$role]->pending_order) ? (int) $team->positions[$role]->pending_order : NULL,
    'secondsLeft' => beergame_timer_seconds_left($game, $team),
    'resultsUrl' => beergame_game_access('results', $game) ? url('beergame/' . $game->id() . '/results') : NULL,
    'position' => NULL,
    'history' => array(),
  );

  if (!empty($team->state['positions'][$role])) {
    $values = $team->state['positions'][$role];
    $data['position'] = array(
      'inventory' => $values['inventory'],
      'backlog' => $values['backlog'],
      'incomingOrder' => $values['incoming_order'],
      'delivered' => $values['delivered'],
      'shipped' => $values['shipped'],
      'costWeek' => $values['cost_week'],
      'costTotal' => $values['cost_total'],
      'inTransit' => array_values($values['ship_in']),
    );
  }

  $history = beergame_team_history($team->tid, $role);
  if (!empty($history[$role])) {
    foreach ($history[$role] as $row) {
      $data['history'][] = array(
        'week' => (int) $row->week,
        'incomingOrder' => (int) $row->incoming_order,
        'delivered' => (int) $row->delivered,
        'shipped' => (int) $row->shipped,
        'inventory' => (int) $row->inventory,
        'backlog' => (int) $row->backlog,
        'order' => (int) $row->order_placed,
        'cost' => (float) $row->cost,
        'timedOut' => (bool) $row->timed_out,
      );
    }
  }
  return $data;
}

/**
 * JSON callback: the player's current board.
 *
 * @param BeerGame $game
 *   The game.
 */
function beergame_board_state_callback(BeerGame $game) {
  backdrop_page_is_cacheable(FALSE);
  $position = beergame_person_position($game, $GLOBALS['user']->uid);
  beergame_timer_check($game, $position->tid);
  return beergame_board_state($game, $GLOBALS['user']->uid);
}

/**
 * JSON callback: places the player's order for the week.
 *
 * Expects a POST with order, week and token.
 *
 * @param BeerGame $game
 *   The game.
 */
function beergame_order_callback(BeerGame $game) {
  global $user;
  backdrop_page_is_cacheable(FALSE);
  if ($_SERVER['REQUEST_METHOD'] != 'POST' || !isset($_POST['token']) || !backdrop_valid_token($_POST['token'], 'beergame-order-' . $game->id())) {
    return MENU_ACCESS_DENIED;
  }
  $order = isset($_POST['order']) ? $_POST['order'] : '';
  if (!is_numeric($order) || (int) $order != $order || $order < 0) {
    $result = t('Please enter a whole number of cases, zero or more.');
  }
  else {
    $week = isset($_POST['week']) ? (int) $_POST['week'] : 0;
    $result = beergame_place_order($game, $user->uid, (int) $order, $week);
  }
  return array(
    'ok' => $result === TRUE,
    'message' => $result === TRUE ? '' : $result,
    'state' => beergame_board_state(beergame_game_load($game->id()), $user->uid),
  );
}

/**
 * Page callback: the debrief, with costs and graphs for every team.
 *
 * @param BeerGame $game
 *   The game.
 */
function beergame_results_page(BeerGame $game) {
  $engine = $game->engine();
  $role_labels = beergame_role_labels();
  $teams = beergame_teams_load($game);

  $last_week = 0;
  $chart_teams = array();
  $cost_rows = array();
  foreach ($teams as $team) {
    $history = beergame_team_history($team->tid);
    $chart_team = array('name' => $team->name, 'roles' => array());
    $row = array('team' => check_plain($team->name));
    $total = 0;
    $team_weeks = 0;
    foreach ($role_labels as $role => $label) {
      $orders = array();
      $inventory = array();
      $cost = 0;
      if (!empty($history[$role])) {
        foreach ($history[$role] as $week => $log) {
          $orders[] = (int) $log->order_placed;
          $inventory[] = (int) $log->inventory - (int) $log->backlog;
          $cost += $log->cost;
          $last_week = max($last_week, $week);
          $team_weeks = max($team_weeks, $week);
        }
      }
      $chart_team['roles'][$role] = array('orders' => $orders, 'inventory' => $inventory);
      $row[$role] = '$' . number_format($cost, 2);
      $total += $cost;
    }
    $row['total'] = '<strong>$' . number_format($total, 2) . '</strong>';
    // Compare over the weeks this team played, in case the game ended early.
    $row['benchmark'] = $team_weeks ? beergame_benchmark_difference($total, array_sum(beergame_benchmark($game, $team_weeks))) : '-';
    $cost_rows[] = array('total' => $total, 'row' => $row);
    $chart_teams[] = $chart_team;
  }

  if (!$last_week) {
    return array('#markup' => '<p>' . t('No weeks have been played yet.') . '</p>');
  }

  usort($cost_rows, function ($a, $b) {
    return $a['total'] < $b['total'] ? -1 : ($a['total'] > $b['total'] ? 1 : 0);
  });
  $rows = array();
  foreach ($cost_rows as $rank => $cost_row) {
    $rows[] = array_merge(array($rank + 1), array_values($cost_row['row']));
  }
  $benchmark = beergame_benchmark($game, $last_week);
  $benchmark_row = array('-', t('Computers alone'));
  foreach (array_keys($role_labels) as $role) {
    $benchmark_row[] = '$' . number_format($benchmark[$role], 2);
  }
  $benchmark_row[] = '$' . number_format(array_sum($benchmark), 2);
  $benchmark_row[] = '';
  $rows[] = array('data' => $benchmark_row, 'class' => array('beergame-benchmark-row'));
  $header = array(t('Rank'), t('Team'));
  foreach ($role_labels as $label) {
    $header[] = $label;
  }
  $header[] = t('Total cost');
  $header[] = t('vs computers');

  $demand = array();
  for ($week = 1; $week <= $last_week; $week++) {
    $demand[] = $engine->demand($week);
  }

  $build['intro'] = array(
    '#markup' => '<p>' . t('Costs are $@holding per case of inventory and $@backlog per case of backlog, every week. The team with the lowest total cost wins, but the real lesson is in the graphs: look for the pattern every team shares.', array(
      '@holding' => number_format($game->settings['holding_cost'], 2),
      '@backlog' => number_format($game->settings['backlog_cost'], 2),
    )) . '</p>',
  );
  $build['costs'] = array(
    '#theme' => 'table',
    '#caption' => t('Total costs'),
    '#header' => $header,
    '#rows' => $rows,
    '#suffix' => '<p class="description">' . t('"Computers alone" is a team of four computer players facing the same customer demand. The computers order like a typical first-time player.') . '</p>',
  );
  $build['charts'] = array(
    '#type' => 'container',
    '#attributes' => array('id' => 'beergame-charts', 'class' => array('beergame-charts')),
  );
  $build['demand'] = array(
    '#type' => 'details',
    '#summary' => t('Reveal what customers actually ordered'),
    '#details' => '<p>' . t('Before opening this, ask everyone except the retailers to sketch what they think customer demand looked like.') . '</p><p><strong>' . beergame_demand_summary($game) . '</strong></p><div id="beergame-demand-chart" class="beergame-chart"></div>',
  );
  $build['#attached']['js'][] = backdrop_get_path('module', 'beergame') . '/js/beergame.chart.js';
  $build['#attached']['js'][] = backdrop_get_path('module', 'beergame') . '/js/beergame.results.js';
  $build['#attached']['library'][] = array('system', 'jquery.once');
  $build['#attached']['js'][] = array(
    'type' => 'setting',
    'data' => array(
      'beergame' => array(
        'results' => array(
          'teams' => $chart_teams,
          'demand' => $demand,
          'roleLabels' => $role_labels,
          'labels' => array(
            'orders' => t('Orders placed'),
            'inventory' => t('Effective inventory (backlog shown below zero)'),
            'demand' => t('Customer orders'),
            'week' => t('Week'),
            'cases' => t('Cases'),
          ),
        ),
      ),
    ),
  );
  return $build;
}

/**
 * JSON callback: autocompletes account names for team assignment.
 *
 * @param string $string
 *   The partial name typed so far.
 */
function beergame_people_autocomplete($string = '') {
  $matches = array();
  if ($string !== '') {
    $result = db_select('users', 'u')
      ->fields('u', array('name'))
      ->condition('name', db_like($string) . '%', 'LIKE')
      ->condition('status', 1)
      ->range(0, 10)
      ->execute();
    foreach ($result as $account) {
      $matches[$account->name] = check_plain($account->name);
    }
  }
  return $matches;
}
