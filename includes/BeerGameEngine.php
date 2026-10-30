<?php
/**
 * @file
 * Simulation engine for the Beer Distribution Game.
 *
 * This class is deliberately free of Backdrop API calls so that it can be
 * tested in isolation. It works on a plain array describing the state of one
 * team (one supply chain of four positions).
 *
 * A week is played in five steps, following the MIT/Sterman protocol:
 *   1. Receive incoming deliveries (shipping delays advance).
 *   2. Receive incoming orders (order delays advance).
 *   3. Fill orders: orders to fill = new orders + backlog.
 *   4. Ship what you can; the rest is added to the backlog.
 *   5. Place an order upstream (the only decision).
 *
 * The state returned by initialState() and advance() has already run steps
 * 1 to 4 for the current week, so it is ready for the players to decide on
 * step 5.
 */
class BeerGameEngine {

  /**
   * Positions in the supply chain, from downstream to upstream.
   */
  const ROLES = array('retailer', 'wholesaler', 'distributor', 'factory');

  /**
   * Parameters of Sterman's (1989) anchoring and adjustment ordering rule,
   * used by computer players.
   */
  const BOT_THETA = 0.36;
  const BOT_ALPHA = 0.26;
  const BOT_BETA = 0.34;

  /**
   * Game settings.
   *
   * @var array
   */
  protected $settings;

  /**
   * Constructs a BeerGameEngine.
   *
   * @param array $settings
   *   Game settings. Missing keys fall back to the classic MIT values.
   */
  public function __construct(array $settings = array()) {
    $this->settings = $settings + self::defaultSettings();
  }

  /**
   * Returns the classic game settings.
   *
   * @return array
   *   Settings keyed by name.
   */
  public static function defaultSettings() {
    return array(
      'starting_inventory' => 12,
      'pipeline_initial' => 4,
      'order_delay' => 2,
      'shipping_delay' => 2,
      'production_request_delay' => 1,
      'production_delay' => 2,
      'demand_pattern' => 'step',
      'demand_initial' => 4,
      'demand_step' => 8,
      'demand_step_week' => 5,
      'demand_variation' => 2,
      'demand_min' => 1,
      'demand_max' => 15,
      'demand_schedule' => array(),
      'demand_scenario' => '',
      'holding_cost' => 0.5,
      'backlog_cost' => 1.0,
      'announced_weeks' => 50,
      'end_week' => 36,
    );
  }

  /**
   * Returns the game settings in use.
   *
   * @return array
   *   Settings keyed by name.
   */
  public function getSettings() {
    return $this->settings;
  }

  /**
   * Returns customer demand for a given week.
   *
   * Uses the schedule generated when the game started; games without one
   * fall back to the classic step.
   *
   * @param int $week
   *   The week number, starting at 1.
   *
   * @return int
   *   Cases ordered by customers from the retailer.
   */
  public function demand($week) {
    $s = $this->settings;
    if (isset($s['demand_schedule'][$week - 1])) {
      return (int) $s['demand_schedule'][$week - 1];
    }
    return $week < $s['demand_step_week'] ? (int) $s['demand_initial'] : (int) $s['demand_step'];
  }

  /**
   * Returns the demand scenarios a surprise game can pick from.
   *
   * @return string[]
   *   Scenario keys.
   */
  public static function surpriseScenarios() {
    return array('step', 'growth', 'seasonal', 'promotion', 'noisy');
  }

  /**
   * Generates the customer demand schedule for a game.
   *
   * Demand holds at the starting level until the week it starts to change,
   * then follows the scenario. Every week is kept within the demand limits.
   * The schedule is generated once per game so that every team faces the
   * same customers.
   *
   * Scenarios:
   * - step: jumps once to the step level (the classic game).
   * - growth: rises by one case every three weeks, up to 8 above the start.
   * - seasonal: a smooth wave between the start and 8 above it, every 12 weeks.
   * - promotion: a four-week burst 8 above the start, then back again.
   * - noisy: bounces randomly within 2 of a level 2 above the start.
   * - random: drifts up or down by up to the variation each week.
   *
   * @param string $scenario
   *   The scenario key. 'surprise' must be resolved by the caller first.
   * @param int $weeks
   *   (optional) Number of weeks to generate. Defaults to the announced length.
   *
   * @return int[]
   *   Demand for each week, starting with week 1.
   */
  public function demandSchedule($scenario, $weeks = NULL) {
    $s = $this->settings;
    $weeks = $weeks ? $weeks : max($s['announced_weeks'], $s['end_week']);
    $base = (int) $s['demand_initial'];
    $variation = max(0, (int) $s['demand_variation']);
    $min = (int) $s['demand_min'];
    $max = max($min, (int) $s['demand_max']);
    $demand = $base;
    $schedule = array();
    for ($week = 1; $week <= $weeks; $week++) {
      // Weeks since demand started to change, or -1 before then.
      $k = $week - $s['demand_step_week'];
      if ($k < 0) {
        $demand = $base;
      }
      else {
        switch ($scenario) {
          case 'growth':
            $demand = $base + min(8, 1 + intdiv($k, 3));
            break;

          case 'seasonal':
            $demand = (int) round($base + 4 - 4 * cos(2 * M_PI * $k / 12));
            break;

          case 'promotion':
            $demand = ($k >= 2 && $k <= 5) ? $base + 8 : $base;
            break;

          case 'noisy':
            $demand = $base + 2 + random_int(-2, 2);
            break;

          case 'random':
            $demand += random_int(-$variation, $variation);
            break;

          case 'step':
          default:
            $demand = (int) $s['demand_step'];
            break;
        }
      }
      $demand = max($min, min($max, $demand));
      $schedule[] = $demand;
    }
    return $schedule;
  }

  /**
   * Builds the starting state of a team, with week 1 ready for orders.
   *
   * @return array
   *   The team state.
   */
  public function initialState() {
    $s = $this->settings;
    $state = array('week' => 1, 'positions' => array());
    foreach (self::ROLES as $index => $role) {
      $is_factory = ($role == 'factory');
      $state['positions'][$role] = array(
        'inventory' => (int) $s['starting_inventory'],
        'backlog' => 0,
        // Order slips on their way to this position from downstream. The
        // retailer gets customer orders directly, so has none.
        'orders_in' => $index == 0 ? array() : $this->fill($s['order_delay'], $s['pipeline_initial']),
        // Beer on its way to this position (production for the factory).
        'ship_in' => $this->fill($is_factory ? $s['production_delay'] : $s['shipping_delay'], $s['pipeline_initial']),
        // Production requests waiting to start brewing (factory only).
        'requests' => $is_factory ? $this->fill($s['production_request_delay'], $s['pipeline_initial']) : array(),
        'incoming_order' => 0,
        'delivered' => 0,
        'shipped' => 0,
        'last_order' => (int) $s['pipeline_initial'],
        'expected_demand' => (float) $s['pipeline_initial'],
        'cost_week' => 0.0,
        'cost_total' => 0.0,
      );
    }
    $this->runWeek($state);
    return $state;
  }

  /**
   * Applies this week's orders and plays the next week up to ordering.
   *
   * @param array $state
   *   The team state, as returned by initialState() or advance().
   * @param array $orders
   *   Orders placed this week, keyed by role.
   *
   * @return array
   *   The team state for the following week.
   */
  public function advance(array $state, array $orders) {
    foreach (self::ROLES as $index => $role) {
      $order = isset($orders[$role]) ? max(0, (int) $orders[$role]) : 0;
      $position = &$state['positions'][$role];
      $position['last_order'] = $order;
      $position['expected_demand'] = self::BOT_THETA * $position['incoming_order'] + (1 - self::BOT_THETA) * $position['expected_demand'];
      if ($role == 'factory') {
        $position['requests'][] = $order;
      }
      else {
        $state['positions'][self::ROLES[$index + 1]]['orders_in'][] = $order;
      }
      unset($position);
    }
    $state['week']++;
    $this->runWeek($state);
    return $state;
  }

  /**
   * Calculates the order a computer player would place this week.
   *
   * Uses Sterman's anchoring and adjustment heuristic: forecast demand, then
   * correct part of the gap between the desired and actual stock and supply
   * line. Like most people, the computer gives the supply line (beer already
   * ordered) too little weight, which is what produces the bullwhip effect.
   * Targets are set so that a computer player orders steadily when the system
   * is in balance.
   *
   * @param array $state
   *   The team state.
   * @param string $role
   *   The role to order for.
   *
   * @return int
   *   The order quantity.
   */
  public function botOrder(array $state, $role) {
    $position = $state['positions'][$role];
    $forecast = self::BOT_THETA * $position['incoming_order'] + (1 - self::BOT_THETA) * $position['expected_demand'];
    $s = $this->settings;
    $effective_inventory = $position['inventory'] - $position['backlog'];
    // Slots in the supply line when an order is being decided: one of the
    // order (or production request) slots has already been emptied this week.
    $slots = $role == 'factory' ? $s['production_delay'] + $s['production_request_delay'] - 1 : $s['shipping_delay'] + $s['order_delay'] - 1;
    $stock_gap = $s['starting_inventory'] - $effective_inventory;
    $supply_line_gap = $forecast * $slots - $this->supplyLine($state, $role);
    $order = $forecast + self::BOT_ALPHA * ($stock_gap + self::BOT_BETA * $supply_line_gap);
    return max(0, (int) round($order));
  }

  /**
   * Counts cases a position has ordered but not yet received.
   *
   * @param array $state
   *   The team state.
   * @param string $role
   *   The role.
   *
   * @return int
   *   Cases in the supply line.
   */
  public function supplyLine(array $state, $role) {
    $position = $state['positions'][$role];
    $total = array_sum($position['ship_in']);
    if ($role == 'factory') {
      return $total + array_sum($position['requests']);
    }
    $upstream = $state['positions'][self::ROLES[array_search($role, self::ROLES) + 1]];
    // The upstream position has only one customer, so its whole backlog and
    // all its incoming order slips are owed to this position.
    return $total + array_sum($upstream['orders_in']) + $upstream['backlog'];
  }

  /**
   * Plays steps 1 to 4 of the current week.
   *
   * Positions are processed from downstream to upstream, so a position has
   * already taken this week's delivery before its supplier ships to it.
   *
   * @param array $state
   *   The team state, modified in place.
   */
  protected function runWeek(array &$state) {
    $s = $this->settings;
    foreach (self::ROLES as $index => $role) {
      $position = &$state['positions'][$role];

      // Step 1: receive deliveries.
      $position['delivered'] = (int) array_shift($position['ship_in']);
      $position['inventory'] += $position['delivered'];
      if ($role == 'factory') {
        // A production request reaching the brewery starts brewing.
        $position['ship_in'][] = (int) array_shift($position['requests']);
      }

      // Step 2: receive orders.
      $position['incoming_order'] = $index == 0 ? $this->demand($state['week']) : (int) array_shift($position['orders_in']);

      // Steps 3 and 4: fill orders and ship.
      $to_fill = $position['incoming_order'] + $position['backlog'];
      $position['shipped'] = min($position['inventory'], $to_fill);
      $position['inventory'] -= $position['shipped'];
      $position['backlog'] = $to_fill - $position['shipped'];
      if ($index > 0) {
        $state['positions'][self::ROLES[$index - 1]]['ship_in'][] = $position['shipped'];
      }

      $position['cost_week'] = $position['inventory'] * $s['holding_cost'] + $position['backlog'] * $s['backlog_cost'];
      $position['cost_total'] += $position['cost_week'];
      unset($position);
    }
  }

  /**
   * Builds a delay pipeline.
   *
   * @param int $length
   *   Number of weeks of delay.
   * @param int $value
   *   Cases (or order quantity) in each slot.
   *
   * @return array
   *   The pipeline, next arrival first.
   */
  protected function fill($length, $value) {
    return $length > 0 ? array_fill(0, (int) $length, (int) $value) : array();
  }

}
