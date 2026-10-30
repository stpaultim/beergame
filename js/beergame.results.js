/**
 * @file
 * Draws the debrief graphs on the Beer Game results page.
 */
(function ($) {

  'use strict';

  var ROLES = ['retailer', 'wholesaler', 'distributor', 'factory'];
  var lineChart = Backdrop.beergameChart.lineChart;

  Backdrop.behaviors.beergameResults = {
    attach: function (context, settings) {
      if (!settings.beergame || !settings.beergame.results) {
        return;
      }
      var results = settings.beergame.results;
      var labels = results.labels;

      $('#beergame-charts', context).once('beergame-charts').each(function () {
        var container = this;
        results.teams.forEach(function (team) {
          var section = $('<section class="beergame-team-charts"></section>');
          section.append($('<h2></h2>').text(team.name));
          $(container).append(section);
          ['orders', 'inventory'].forEach(function (measure) {
            lineChart(section[0], {
              title: labels[measure],
              weekLabel: labels.week,
              series: ROLES.map(function (role) {
                return {key: role, label: results.roleLabels[role], values: team.roles[role][measure]};
              })
            });
          });
        });
      });

      $('#beergame-demand-chart', context).once('beergame-demand').each(function () {
        lineChart(this, {
          title: labels.demand,
          weekLabel: labels.week,
          series: [{key: 'demand', label: labels.demand, values: results.demand}]
        });
      });
    }
  };

})(jQuery);
