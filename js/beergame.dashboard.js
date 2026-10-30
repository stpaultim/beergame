/**
 * @file
 * Keeps the facilitator's dashboard up to date while a game is running.
 */
(function ($) {

  'use strict';

  Backdrop.behaviors.beergameDashboard = {
    attach: function (context, settings) {
      if (!settings.beergame || !settings.beergame.monitorUrl) {
        return;
      }
      $('#beergame-monitor', context).once('beergame-monitor').each(function () {
        var monitor = $(this);
        var status = settings.beergame.status;

        var refresh = function () {
          $.getJSON(settings.beergame.monitorUrl).done(function (data) {
            monitor.html(data.html);
            $('#beergame-status').text(data.statusLabel);
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

})(jQuery);
