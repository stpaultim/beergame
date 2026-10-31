<?php
/**
 * @file
 * Administration forms for the Beer Game module.
 */

/**
 * Form constructor for the default game settings.
 *
 * @see beergame_admin_settings_form_validate()
 * @see beergame_admin_settings_form_submit()
 */
function beergame_admin_settings_form($form, &$form_state) {
  module_load_include('inc', 'beergame', 'beergame.pages');
  $form['help'] = array(
    '#type' => 'help',
    '#markup' => t('These settings are used for new games. Facilitators can change them for each game before it starts. The defaults follow the classic Beer Game.'),
  );
  $form['settings'] = array('#tree' => TRUE) + beergame_settings_elements(beergame_default_settings());
  $form['poll_interval'] = array(
    '#type' => 'number',
    '#title' => t('Refresh interval (seconds)'),
    '#description' => t('How often player boards and the facilitator dashboard check for updates.'),
    '#default_value' => config_get('beergame.settings', 'poll_interval'),
    '#min' => 1,
    '#max' => 60,
    '#step' => 1,
    '#size' => 6,
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );
  return $form;
}

/**
 * Form validation handler for beergame_admin_settings_form().
 */
function beergame_admin_settings_form_validate($form, &$form_state) {
  beergame_settings_validate(beergame_settings_values($form_state['values']['settings']), 'settings');
}

/**
 * Form submission handler for beergame_admin_settings_form().
 */
function beergame_admin_settings_form_submit($form, &$form_state) {
  $config = config('beergame.settings');
  foreach (beergame_settings_values($form_state['values']['settings']) as $key => $value) {
    $config->set($key, $value);
  }
  $config->set('poll_interval', (int) $form_state['values']['poll_interval']);
  $config->save();
  backdrop_set_message(t('The configuration options have been saved.'));
}
