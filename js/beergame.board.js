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
    this.deadline = null;
    this.build();
    this.update(options.initial);
    this.schedule();
    var self = this;
    window.setInterval(function () {
      self.renderTimer();
    }, 1000);
  }

  /**
   * Formats a number of seconds as minutes and seconds.
   */
  function clock(seconds) {
    var rest = seconds % 60;
    return Math.floor(seconds / 60) + ':' + (rest < 10 ? '0' : '') + rest;
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
    this.$weekTitle = $('<h2 class="beergame-week-title"></h2>');
    this.$timer = $('<p class="beergame-timer" role="timer" hidden></p>');
    this.$week = $('<div class="beergame-week"></div>');
    this.$transit = $('<p class="beergame-transit"></p>');

    this.$form = $('<form class="beergame-order-form"></form>');
    this.$orderLabel = $('<label for="beergame-order-input"></label>');
    this.$input = $('<input type="number" id="beergame-order-input" class="form-number" min="0" step="1" inputmode="numeric" required disabled>');
    this.$submit = $('<button type="submit" class="button button-primary"></button>').text(t('Place order'));
    this.$waiting = $('<p class="beergame-waiting"></p>');
    this.$error = $('<p class="beergame-error" role="alert" hidden></p>');
    this.$form.append(
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
    // The current week, waiting for this player's order.
    var $main = $('<section class="beergame-panel beergame-board-main"></section>').append(this.$weekTitle, this.$timer, this.$notice, this.$week, this.$transit, this.$form);
    this.$board.append(
      this.$header,
      $('<div class="beergame-board-layout"></div>').append($main, this.$history)
    );
  };

  /**
   * Polls the server for changes made by teammates.
   */
  BeerGameBoard.prototype.schedule = function () {
    var self = this;
    window.setTimeout(function () {
      $.getJSON(self.options.stateUrl).done(function (state) {
        self.update(state);
      }).fail(function (xhr) {
        if (xhr.status === 403) {
          self.showError(Backdrop.t('You no longer have a place in this game. Please ask your facilitator.'));
        }
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
    // The countdown runs in the browser between updates.
    this.deadline = state.secondsLeft === null ? null : Date.now() + state.secondsLeft * 1000;
    this.secondsLeft = state.secondsLeft;
    this.timerPaused = state.gameStatus !== 'running';
    this.renderTimer();

    // The time left changes on every update, so it does not count as a
    // change worth redrawing the board for.
    var fingerprint = JSON.stringify($.extend({}, state, {secondsLeft: null}));
    if (fingerprint === this.fingerprint) {
      return;
    }
    var previous = this.state;
    this.fingerprint = fingerprint;
    this.state = state;
    var t = Backdrop.t;

    if (state.handedOver) {
      if (previous && !previous.handedOver) {
        this.$input.val('');
        this.showError('');
        this.$notice.text(t('Your facilitator has turned on autoplay for your position.')).addClass('is-new');
      }
    }
    else if (previous && previous.week !== state.week) {
      this.$input.val('');
      this.showError('');
      var last = state.history[state.history.length - 1];
      if (last && last.timedOut) {
        this.$notice.text(t('Time ran out, so your order from the week before was repeated: @order.', {
          '@order': Backdrop.formatPlural(last.order, '1 case', '@count cases')
        })).addClass('is-new');
      }
      else if (previous.secondsLeft !== null) {
        // With a timer, the week may have moved on because time ran out
        // for a teammate.
        this.$notice.text(t('A new week has started.')).addClass('is-new');
      }
      else {
        this.$notice.text(t('New week: your teammates have all ordered.')).addClass('is-new');
      }
    }

    this.$header.empty().append(
      $('<h2 class="beergame-role"></h2>').text(state.roleLabel),
      $('<p class="beergame-meta"></p>').text(state.teamName)
    );
    this.$weekTitle.empty().append(
      document.createTextNode(t('Week @week', {'@week': state.week || 1}) + ' '),
      $('<span class="beergame-week-of"></span>').text(t('of @weeks', {'@weeks': state.announcedWeeks}))
    );

    this.renderWeek(state);
    this.renderTransit(state);
    this.renderForm(state);
    this.renderHistory(state);
  };

  /**
   * Shows the time left this week, if the facilitator has started the timer.
   */
  BeerGameBoard.prototype.renderTimer = function () {
    var t = Backdrop.t;
    if (this.deadline === null) {
      this.$timer.prop('hidden', true);
      return;
    }
    var seconds = this.timerPaused ? this.secondsLeft : Math.max(0, Math.ceil((this.deadline - Date.now()) / 1000));
    var text = seconds > 0 ? t('Time left: @time', {'@time': clock(seconds)}) : t('Time is up');
    if (this.timerPaused) {
      text += ' ' + t('(paused)');
    }
    this.$timer.prop('hidden', false).text(text)
      .toggleClass('is-low', seconds <= 10);
  };

  /**
   * Shows what happened this week.
   */
  BeerGameBoard.prototype.renderWeek = function (state) {
    var t = Backdrop.t;
    var p = state.position;
    this.$week.empty();
    if (!p) {
      this.$week.append($('<p></p>').text(t('The game has not started yet. This page will update when your facilitator starts it.')));
      return;
    }
    var tiles = [
      {label: t('Received'), title: state.role === 'factory' ? t('Beer brewed this week') : t('Beer received from @who this week', {'@who': state.upstreamLabel}), value: p.delivered},
      {label: t('Order in'), title: state.role === 'retailer' ? t('Customers ordered this week') : t('Order received from @who this week', {'@who': state.downstreamLabel}), value: p.incomingOrder},
      {label: t('Shipped'), title: t('Beer shipped to @who this week', {'@who': state.downstreamLabel}), value: p.shipped},
      {label: t('Inventory'), title: t('Cases in stock, costing @cost each per week', {'@cost': money(state.holdingCost)}), value: p.inventory, note: t('@cost/case', {'@cost': money(state.holdingCost)})},
      {label: t('Backlog'), title: t('Cases owed to @who, costing @cost each per week', {'@who': state.downstreamLabel, '@cost': money(state.backlogCost)}), value: p.backlog, note: t('@cost/case', {'@cost': money(state.backlogCost)}), alert: p.backlog > 0},
      {label: t('Cost'), title: t('Cost this week'), value: money(p.costWeek), note: t('Total @total', {'@total': money(p.costTotal)})}
    ];
    var $tiles = $('<div class="beergame-tiles"></div>');
    tiles.forEach(function (tile) {
      var $tile = $('<div class="beergame-tile"></div>').attr('title', tile.title).toggleClass('is-alert', !!tile.alert);
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
    this.$transit.empty().prop('hidden', !p);
    if (!p) {
      return;
    }
    this.$transit.attr('title', state.role === 'factory' ?
      t('Beer being brewed, and when it will be ready.') :
      t('Beer on the way to you, and when it arrives. Beer you have ordered but that has not yet been shipped is not shown.'));
    p.inTransit.forEach(function (cases, index) {
      this.$transit.append($('<span class="beergame-transit-chip"></span>').append(
        document.createTextNode((index === 0 ? t('Next week:') : t('In @n weeks:', {'@n': index + 1})) + ' '),
        $('<strong></strong>').text(cases)
      ));
    }, this);
  };

  /**
   * Shows the order form, or why ordering is not possible right now.
   */
  BeerGameBoard.prototype.renderForm = function (state) {
    var t = Backdrop.t;
    var canOrder = state.position && !state.handedOver && state.gameStatus === 'running' && state.teamStatus === 'playing' && state.pendingOrder === null;
    this.$orderLabel.text(state.role === 'factory' ?
      t('Cases to brew') :
      t('Order from @who', {'@who': state.upstreamLabel}));
    var couldOrder = !this.$input.prop('disabled');
    this.$input.prop('disabled', !canOrder);
    this.$submit.prop('disabled', !canOrder);
    if (canOrder && !couldOrder) {
      this.$input[0].focus({preventScroll: true});
    }

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
    else if (state.handedOver) {
      message = t('Autoplay is now placing your orders. You can watch how it plays here, and see the results when the game ends.');
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
    this.$history.empty().append($('<h2></h2>').text(t('Past weeks')));
    if (!state.history.length) {
      this.$history.append($('<p></p>').text(t('Your past weeks will appear here.')));
      return;
    }
    var $table = $('<table class="beergame-history-table"></table>');
    var $head = $('<tr></tr>');
    [t('Week'), t('Received'), t('Order in'), t('Shipped'), t('Inventory'), t('Backlog'), t('Ordered'), t('Cost')].forEach(function (label) {
      $head.append($('<th></th>').text(label));
    });
    $table.append($('<thead></thead>').append($head));
    var $body = $('<tbody></tbody>');
    state.history.slice().reverse().forEach(function (row) {
      var $row = $('<tr></tr>');
      [row.week, row.delivered, row.incomingOrder, row.shipped, row.inventory, row.backlog, row.order, money(row.cost)].forEach(function (value) {
        $row.append($('<td></td>').text(value));
      });
      // Mark orders the person did not place themselves.
      var note = null;
      if (row.computer) {
        note = {text: t('(autoplay)'), title: t('Autoplay placed this order.')};
      }
      else if (row.timedOut) {
        note = {text: t('(time ran out)'), title: t('Time ran out, so the previous order was repeated.')};
      }
      if (note) {
        $row.children().eq(6).attr('title', note.title)
          .append($('<span class="beergame-order-note"></span>').text(' ' + note.text));
      }
      $body.append($row);
    });
    this.$history.append($('<div class="beergame-table-scroll"></div>').append($table.append($body)));
  };

})(jQuery);
