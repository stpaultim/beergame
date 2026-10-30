<?php
/**
 * @file
 * Entity and controller classes for Beer Game sessions.
 */

/**
 * A Beer Game session, run by a facilitator.
 */
class BeerGame extends Entity {

  /**
   * The game ID.
   *
   * @var int
   */
  public $gid;

  /**
   * The game name.
   *
   * @var string
   */
  public $title = '';

  /**
   * The facilitator's user ID.
   *
   * @var int
   */
  public $uid = 0;

  /**
   * One of setup, running, paused or finished.
   *
   * @var string
   */
  public $status = 'setup';

  /**
   * Game settings, as used by BeerGameEngine.
   *
   * @var array
   */
  public $settings = array();

  /**
   * Created timestamp.
   *
   * @var int
   */
  public $created = 0;

  /**
   * Changed timestamp.
   *
   * @var int
   */
  public $changed = 0;

  /**
   * Implements EntityInterface::id().
   */
  public function id() {
    return isset($this->gid) ? $this->gid : NULL;
  }

  /**
   * Implements EntityInterface::entityType().
   */
  public function entityType() {
    return 'beergame_game';
  }

  /**
   * Implements EntityInterface::label().
   */
  public function label() {
    return $this->title;
  }

  /**
   * Implements EntityInterface::uri().
   */
  public function uri() {
    return array(
      'path' => 'beergame/' . $this->gid,
      'options' => array(),
    );
  }

  /**
   * Implements EntityInterface::access().
   */
  public function access($op, $account = NULL) {
    return beergame_game_access($op, $this, $account);
  }

  /**
   * Returns a simulation engine configured for this game.
   *
   * @return BeerGameEngine
   *   The engine.
   */
  public function engine() {
    return new BeerGameEngine((array) $this->settings);
  }

}

/**
 * Storage controller for Beer Game sessions.
 */
class BeerGameStorageController extends EntityDatabaseStorageController {

  /**
   * Overrides DefaultEntityController::attachLoad().
   *
   * Unserializes game settings.
   */
  protected function attachLoad(&$queried_entities, $revision_id = FALSE) {
    foreach ($queried_entities as $entity) {
      $entity->settings = empty($entity->settings) ? array() : unserialize($entity->settings, array('allowed_classes' => FALSE));
    }
    parent::attachLoad($queried_entities, $revision_id);
  }

  /**
   * Overrides EntityDatabaseStorageController::preSave().
   */
  protected function preSave(EntityInterface $entity) {
    if (empty($entity->created)) {
      $entity->created = REQUEST_TIME;
    }
    $entity->changed = REQUEST_TIME;
  }

  /**
   * Overrides EntityDatabaseStorageController::postDelete().
   *
   * Removes the teams, positions and history belonging to deleted games.
   */
  protected function postDelete($entities) {
    $tids = db_query('SELECT tid FROM {beergame_team} WHERE gid IN (:gids)', array(':gids' => array_keys($entities)))->fetchCol();
    if ($tids) {
      db_delete('beergame_log')->condition('tid', $tids, 'IN')->execute();
      db_delete('beergame_position')->condition('tid', $tids, 'IN')->execute();
      db_delete('beergame_team')->condition('tid', $tids, 'IN')->execute();
    }
  }

}
