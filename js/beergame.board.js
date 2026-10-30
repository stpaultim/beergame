/**
 * @file
 * The player's board: shows their position and takes their weekly order.
 */
(function ($) {

  'use strict';

  Backdrop.behaviors.beergameBoard = {
    attach: function (context, settings) {
      if (!settings.beergame || !settings.beergame.stateUrl) {
        return;
      }
      $('#beergame-board', context).once('beergame-board').each(function () {
        new BeerGameBoard($(this), settings.beergame);
      });
    }
  };

  /**
   * Formats a cost as dollars.
   */
  function money(value) {
    return '$' + Number(value).toFixed(2);
  }

  /**
   * Controls one player's board.
   *
   * @param {jQuery} $board
   *   The board container.
   * @param {Object} options
   *   Settings from beergame_board_page().
   */
  function BeerGameBoard($board, options) {
    this.options = options;
    this.$board = $board;
    this.state = null;
    this.fingerprint = '';
    this.build();
    this.update(options.initial);
    this.schedule();
  }

  /**
   * Builds the static parts of the board once.
   */
  BeerGameBoard.prototype.build = function () {
    var self = this;
    var t = Backdrop.t;
    this.$board.empty();
    this.$header = $('<div class="beergame-board-header"></div>');
    this.$notice = $('<div class="beergame-notice" role="status" aria-live="polite"></div>');
    this.$week = $('<section class="beergame-panel beergame-week"></section>');
    this.$transit = $('<section class="beergame-panel beergame-transit"></section>');

    this.$form = $('<form class="beergame-order-form beergame-panel"></form>');
    this.$orderLabel = $('<label for="beergame-order-input"></label>');
    this.$input = $('<input type="number" id="beergame-order-input" class="form-number" min="0" step="1" inputmode="numeric" required>');
    this.$submit = $('<button type="submit" class="button button-primary"></button>').text(t('Place order'));
    this.$waiting = $('<p class="beergame-waiting"></p>');
    this.$error = $('<p class="beergame-error" role="alert" hidden></p>');
    this.$form.append(
      $('<h2></h2>').text(t('Your order')),
      this.$orderLabel,
      $('<div class="beergame-order-row"></div>').append(this.$input, this.$submit),
      this.$error,
      this.$waiting
    );
    this.$form.on('submit', function (event) {
      event.preventDefault();
      self.placeOrder();
    });

    this.$history = $('<section class="beergame-panel beergame-history"></section>');
    this.$board.append(this.$header, this.$notice, this.$week, this.$form, this.$transit, this.$history);
  };

  /**
   * Polls the server for changes made by teammates.
   */
  BeerGameBoard.prototype.schedule = function () {
    var self = this;
    window.setTimeout(function () {
      $.getJSON(self.options.stateUrl).done(function (state) {
        self.update(state);
      }).always(function () {
        if (self.state && self.state.teamStatus !== 'finished' && self.state.gameStatus !== 'finished') {
          self.schedule();
        }
      });
    }, this.options.pollInterval);
  };

  /**
   * Sends the player's order.
   */
  BeerGameBoard.prototype.placeOrder = function () {
    var self = this;
    var value = $.trim(this.$input.val());
    if (value === '' || !/^\d+$/.test(value)) {
      this.showError(Backdrop.t('Please enter a whole number of cases, zero or more.'));
      return;
    }
    this.$submit.prop('disabled', true);
    $.post(this.options.orderUrl, {
      order: value,
      week: this.state.week,
      token: this.options.token
    }, null, 'json').done(function (response) {
      if (!response.ok) {
        self.showError(response.message);
      }
      self.update(response.state);
    }).fail(function () {
      self.showError(Backdrop.t('Your order could not be sent. Check your connection and try again.'));
      self.$submit.prop('disabled', false);
    });
  };

  /**
   * Shows an error under the order field.
   */
  BeerGameBoard.prototype.showError = function (message) {
    this.$error.text(message).prop('hidden', !message);
  };

  /**
   * Redraws the board if the state has changed.
   *
   * @param {Object} state
   *   Board data from beergame_board_state().
   */
  BeerGameBoard.prototype.update = function (state) {
    var fingerprint = JSON.stringify(state);
    if (fingerprint === this.fingerprint) {
      return;
    }
    var previous = this.state;
    this.fingerprint = fingerprint;
    this.state = state;
    var t = Backdrop.t;

    if (previous && previous.week !== state.week) {
      this.$input.val('');
      this.showError('');
      this.$notice.text(t('Week @week has begun. Your teammates have all ordered.', {'@week': state.week})).addClass('is-new');
    }

    this.$header.empty().append(
      $('<p class="beergame-role"></p>').text(state.roleLabel),
      $('<p class="beergame-meta"></p>').text(t('@team · Week @week of @weeks', {
        '@team': state.teamName,
        '@week': state.week || 1,
        '@weeks': state.announcedWeeks
      }))
    );

    this.renderWeek(state);
    this.renderTransit(state);
    this.renderForm(state);
    this.renderHistory(state);
  };

  /**
   * Shows what happened this week.
   */
  BeerGameBoard.prototype.renderWeek = function (state) {
    var t = Backdrop.t;
    var p = state.position;
    this.$week.empty().append($('<h2></h2>').text(t('This week')));
    if (!p) {
      this.$week.append($('<p></p>').text(t('The game has not started yet. This page will update when your facilitator starts it.')));
      return;
    }
    var tiles = [
      {label: t('Beer received from @who', {'@who': state.upstreamLabel}), value: p.delivered},
      {label: state.role === 'retailer' ? t('Customers ordered') : t('Order from @who', {'@who': state.downstreamLabel}), value: p.incomingOrder},
      {label: t('Shipped to @who', {'@who': state.downstreamLabel}), value: p.shipped},
      {label: t('Inventory'), value: p.inventory, note: t('@cost per case', {'@cost': money(state.holdingCost)})},
      {label: t('Backlog'), value: p.backlog, note: t('@cost per case', {'@cost': money(state.backlogCost)}), alert: p.backlog > 0},
      {label: t('Cost this week'), value: money(p.costWeek), note: t('Total so far: @total', {'@total': money(p.costTotal)})}
    ];
    var $tiles = $('<div class="beergame-tiles"></div>');
    tiles.forEach(function (tile) {
      var $tile = $('<div class="beergame-tile"></div>').toggleClass('is-alert', !!tile.alert);
      $tile.append(
        $('<span class="beergame-tile-label"></span>').text(tile.label),
        $('<span class="beergame-tile-value"></span>').text(tile.value)
      );
      if (tile.note) {
        $tile.append($('<span class="beergame-tile-note"></span>').text(tile.note));
      }
      $tiles.append($tile);
    });
    this.$week.append($tiles);
  };

  /**
   * Shows the beer on its way, as a player can see on a physical board.
   */
  BeerGameBoard.prototype.renderTransit = function (state) {
    var t = Backdrop.t;
    var p = state.position;
    this.$transit.empty();
    if (!p) {
      return;
    }
    this.$transit.append($('<h2></h2>').text(state.role === 'factory' ? t('Beer being brewed') : t('Beer on the way to you')));
    var $list = $('<ol class="beergame-transit-list"></ol>');
    p.inTransit.forEach(function (cases, index) {
      $list.append($('<li></li>').append(
        $('<span class="beergame-transit-when"></span>').text(index === 0 ? t('Next week') : t('In @n weeks', {'@n': index + 1})),
        $('<span class="beergame-transit-cases"></span>').text(Backdrop.formatPlural(cases, '1 case', '@count cases'))
      ));
    });
    this.$transit.append($list, $('<p class="description"></p>').text(t('Beer you have ordered but that has not yet been shipped is not shown.')));
  };

  /**
   * Shows the order form, or why ordering is not possible right now.
   */
  BeerGameBoard.prototype.renderForm = function (state) {
    var t = Backdrop.t;
    var canOrder = state.position && state.gameStatus === 'running' && state.teamStatus === 'playing' && state.pendingOrder === null;
    this.$orderLabel.text(state.role === 'factory' ?
      t('How many cases should the brewery start brewing?') :
      t('How many cases will you order from @who?', {'@who': state.upstreamLabel}));
    this.$input.prop('disabled', !canOrder);
    this.$submit.prop('disabled', !canOrder);

    var message = '';
    if (state.teamStatus === 'finished' || state.gameStatus === 'finished') {
      message = t('The game is over. Thank you for playing!');
      if (state.resultsUrl) {
        this.$waiting.empty().append(
          $('<span></span>').text(message + ' '),
          $('<a></a>').attr('href', state.resultsUrl).text(t('See the results'))
        );
        return;
      }
      message += ' ' + t('Your facilitator will share the results.');
    }
    else if (state.gameStatus === 'paused') {
      message = t('Your facilitator has paused the game.');
    }
    else if (state.gameStatus === 'setup') {
      message = t('Waiting for your facilitator to start the game.');
    }
    else if (state.pendingOrder !== null) {
      this.$input.val(state.pendingOrder);
      message = t('You ordered @order. Waiting for teammates: @ordered of @people people have ordered.', {
        '@order': Backdrop.formatPlural(state.pendingOrder, '1 case', '@count cases'),
        '@ordered': state.ordered,
        '@people': state.people
      });
    }
    this.$waiting.text(message);
  };

  /**
   * Shows the player's own week-by-week record.
   */
  BeerGameBoard.prototype.renderHistory = function (state) {
    var t = Backdrop.t;
    this.$history.empty().append($('<h2></h2>').text(t('Your record')));
    if (!state.history.length) {
      this.$history.append($('<p></p>').text(t('Your past weeks will appear here.')));
      return;
    }
    var $table = $('<table class="beergame-history-table"></table>');
    var $head = $('<tr></tr>');
    [t('Week'), t('Received'), t('Incoming order'), t('Shipped'), t('Inventory'), t('Backlog'), t('You ordered'), t('Cost')].forEach(function (label) {
      $head.append($('<th></th>').text(label));
    });
    $table.append($('<thead></thead>').append($head));
    var $body = $('<tbody></tbody>');
    state.history.slice().reverse().forEach(function (row) {
      var $row = $('<tr></tr>');
      [row.week, row.delivered, row.incomingOrder, row.shipped, row.inventory, row.backlog, row.order, money(row.cost)].forEach(function (value) {
        $row.append($('<td></td>').text(value));
      });
      $body.append($row);
    });
    this.$history.append($('<div class="beergame-table-scroll"></div>').append($table.append($body)));
  };

})(jQuery);
