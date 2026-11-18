/**
 * @file
 * Keeps the facilitator's dashboard up to date while a game is running.
 */
(function ($) {

  'use strict';

  var ROLES = ['retailer', 'wholesaler', 'distributor', 'factory'];
  var HIDE_KEY = 'beergame-dashboard-hide';

  Backdrop.behaviors.beergameDashboard = {
    attach: function (context, settings) {
      if (!settings.beergame || !settings.beergame.monitorUrl) {
        return;
      }
      $('#beergame-monitor', context).once('beergame-monitor').each(function () {
        var monitor = $(this);
        var status = settings.beergame.status;
        var $charts = $('#beergame-live-charts');

        buildHideToggle(monitor, settings.beergame);
        renderCharts($charts, settings.beergame, settings.beergame.charts);

        var refresh = function () {
          $.getJSON(settings.beergame.monitorUrl).done(function (data) {
            monitor.html(data.html);
            $('#beergame-status').text(data.statusLabel);
            renderCharts($charts, settings.beergame, data.charts);
            // Reload when the game changes state (e.g. every team finished),
            // so the control buttons match.
            if (data.status !== status) {
              window.location.reload();
            }
          }).always(function () {
            if (status === 'running' || status === 'paused') {
              window.setTimeout(refresh, settings.beergame.pollInterval);
            }
          });
        };
        if (status === 'running' || status === 'paused') {
          window.setTimeout(refresh, settings.beergame.pollInterval);
        }
      });
    }
  };

  /**
   * Adds a button that hides game details, for when players can see the
   * facilitator's screen. The choice is remembered in this browser.
   */
  function buildHideToggle(monitor, options) {
    var labels = options.labels;
    var icons = options.hideIcons || {};
    var hidden = false;
    try {
      hidden = window.localStorage.getItem(HIDE_KEY) === 'on';
    }
    catch (e) {
      // Storage may be blocked; details are shown.
    }
    var $toggle = $('<button type="button" class="beergame-hide-toggle"></button>');
    var refresh = function () {
      $('body').toggleClass('beergame-hiding-details', hidden);
      $toggle.attr('aria-pressed', hidden ? 'true' : 'false')
        .attr('title', labels.hideTitle)
        .html(icons[hidden ? 'show' : 'hide'] || '')
        .append(document.createTextNode(hidden ? labels.show : labels.hide));
    };
    $toggle.on('click', function () {
      hidden = !hidden;
      try {
        window.localStorage.setItem(HIDE_KEY, hidden ? 'on' : 'off');
      }
      catch (e) {
        // The choice lasts until the page is reloaded.
      }
      refresh();
    });
    refresh();
    monitor.before($('<div class="beergame-hide-row"></div>').append($toggle));
  }

  /**
   * Draws each team's orders and inventory so far.
   *
   * Each team's charts sit in a folded section and are only redrawn when the
   * team has played another week, so open sections and hovering are kept.
   */
  function renderCharts($charts, options, data) {
    var labels = options.labels;
    var lineChart = Backdrop.beergameChart && Backdrop.beergameChart.lineChart;
    if (!$charts.length || !lineChart || !data) {
      return;
    }
    if (!$charts.children('h2').length) {
      $charts.append($('<h2></h2>').text(labels.heading));
    }
    var played = data.teams.some(function (team) {
      return team.weeks > 0;
    });
    var $empty = $charts.children('.beergame-live-empty');
    if (!played) {
      if (!$empty.length) {
        $charts.append($('<p class="beergame-live-empty"></p>').text(labels.empty));
      }
      return;
    }
    $empty.remove();

    data.teams.forEach(function (team) {
      var $team = $charts.children('[data-tid="' + team.tid + '"]');
      if (!$team.length) {
        $team = $('<details class="beergame-live-team"></details>').attr('data-tid', team.tid)
          .append($('<summary></summary>'), $('<div class="beergame-live-team-charts"></div>'));
        $charts.append($team);
      }
      $team.children('summary').text(team.name + ': ' + Backdrop.formatPlural(team.weeks, '1 week played', '@count weeks played'));
      if ($team.data('weeks') === team.weeks) {
        return;
      }
      $team.data('weeks', team.weeks);
      var $body = $team.children('.beergame-live-team-charts').empty();
      lineChart($body[0], {
        title: labels.orders,
        weekLabel: labels.week,
        series: [{key: 'customers', label: labels.customers, values: data.demand.slice(0, team.weeks)}].concat(ROLES.map(function (role) {
          return {key: role, label: options.roleLabels[role], values: team.orders[role]};
        }))
      });
      lineChart($body[0], {
        title: labels.inventory,
        weekLabel: labels.week,
        series: ROLES.map(function (role) {
          return {key: role, label: options.roleLabels[role], values: team.inventory[role]};
        })
      });
    });
  }

})(jQuery);
