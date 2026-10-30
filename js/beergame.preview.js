/**
 * @file
 * Live preview of customer demand on the game settings forms.
 */
(function ($) {

  'use strict';

  var FIELDS = ['demand_pattern', 'demand_initial', 'demand_step', 'demand_step_week', 'demand_variation', 'demand_min', 'demand_max', 'end_week'];

  Backdrop.behaviors.beergameDemandPreview = {
    attach: function (context, settings) {
      if (!settings.beergamePreview) {
        return;
      }
      var options = settings.beergamePreview;
      $('.beergame-demand-preview', context).once('beergame-demand-preview').each(function () {
        var $preview = $(this);
        var $form = $preview.closest('form');
        var timer = null;
        var request = null;

        // Finds a settings field by the last part of its name, e.g.
        // settings[demand][demand_initial].
        var field = function (name) {
          return $form.find(':input[name$="[' + name + ']"]');
        };

        var refresh = function () {
          var params = {};
          FIELDS.forEach(function (name) {
            var $input = field(name);
            params[name] = $input.is(':radio') ? $input.filter(':checked').val() : $input.val();
          });
          if (request) {
            request.abort();
          }
          request = $.getJSON(options.url, params).done(function (data) {
            $preview.empty();
            if (data.surprise) {
              $preview.append($('<p class="description"></p>').text(options.labels.surprise));
              return;
            }
            Backdrop.beergameChart.lineChart($preview[0], {
              title: options.labels.title,
              weekLabel: options.labels.week,
              width: 420,
              height: 220,
              series: [{key: 'demand', label: options.labels.demand, values: data.demand}]
            });
            if (data.sample) {
              $preview.append($('<p class="description"></p>').text(options.labels.sample));
            }
          }).fail(function (xhr, status) {
            if (status !== 'abort') {
              $preview.empty().append($('<p class="description"></p>').text(options.labels.error));
            }
          });
        };

        FIELDS.forEach(function (name) {
          field(name).on('input change', function () {
            window.clearTimeout(timer);
            timer = window.setTimeout(refresh, 300);
          });
        });
        refresh();
      });
    }
  };

})(jQuery);
