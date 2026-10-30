/**
 * @file
 * Draws the debrief graphs on the Beer Game results page.
 */
(function ($) {

  'use strict';

  var SVG_NS = 'http://www.w3.org/2000/svg';
  var ROLES = ['retailer', 'wholesaler', 'distributor', 'factory'];

  /**
   * Creates an SVG element with attributes.
   */
  function svgEl(name, attributes) {
    var el = document.createElementNS(SVG_NS, name);
    $.each(attributes || {}, function (key, value) {
      el.setAttribute(key, value);
    });
    return el;
  }

  /**
   * Returns a "nice" step for an axis covering the given range.
   */
  function niceStep(range, ticks) {
    var raw = range / ticks;
    var magnitude = Math.pow(10, Math.floor(Math.log10(raw)));
    var residual = raw / magnitude;
    var nice = residual > 5 ? 10 : residual > 2 ? 5 : residual > 1 ? 2 : 1;
    return Math.max(1, nice * magnitude);
  }

  /**
   * Draws a line chart with a legend, end labels and a hover tooltip.
   *
   * @param {HTMLElement} container
   *   The element to draw into.
   * @param {Object} options
   *   - title: chart title.
   *   - series: array of {key, label, values}.
   *   - weekLabel: label for the x axis.
   */
  function lineChart(container, options) {
    var width = 640;
    var height = 260;
    var margin = {top: 12, right: 92, bottom: 32, left: 44};
    var plotWidth = width - margin.left - margin.right;
    var plotHeight = height - margin.top - margin.bottom;
    var series = options.series;
    var weeks = 0;
    var min = 0;
    var max = 0;
    series.forEach(function (s) {
      weeks = Math.max(weeks, s.values.length);
      s.values.forEach(function (v) {
        min = Math.min(min, v);
        max = Math.max(max, v);
      });
    });
    if (max === min) {
      max = min + 1;
    }
    var step = niceStep(max - min, 5);
    min = Math.floor(min / step) * step;
    max = Math.ceil(max / step) * step;

    var x = function (index) {
      return margin.left + (weeks > 1 ? index * plotWidth / (weeks - 1) : 0);
    };
    var y = function (value) {
      return margin.top + plotHeight - (value - min) * plotHeight / (max - min);
    };

    var figure = $('<figure class="beergame-chart"></figure>');
    figure.append($('<figcaption class="beergame-chart-title"></figcaption>').text(options.title));

    if (series.length > 1) {
      var legend = $('<ul class="beergame-legend"></ul>');
      series.forEach(function (s) {
        legend.append($('<li></li>').append(
          $('<span class="beergame-swatch"></span>').addClass('beergame-series-' + s.key),
          $('<span></span>').text(s.label)
        ));
      });
      figure.append(legend);
    }

    var svg = svgEl('svg', {
      viewBox: '0 0 ' + width + ' ' + height,
      role: 'img',
      'aria-label': options.title,
      'class': 'beergame-chart-svg'
    });

    // Grid lines and y axis labels.
    for (var tick = min; tick <= max; tick += step) {
      svg.appendChild(svgEl('line', {
        x1: margin.left, x2: margin.left + plotWidth, y1: y(tick), y2: y(tick),
        'class': tick === 0 ? 'beergame-zero' : 'beergame-grid'
      }));
      var label = svgEl('text', {x: margin.left - 8, y: y(tick) + 4, 'class': 'beergame-axis-label', 'text-anchor': 'end'});
      label.textContent = tick;
      svg.appendChild(label);
    }
    // X axis labels every five weeks.
    for (var week = 1; week <= weeks; week++) {
      if (week === 1 || week % 5 === 0) {
        var xLabel = svgEl('text', {x: x(week - 1), y: height - 10, 'class': 'beergame-axis-label', 'text-anchor': 'middle'});
        xLabel.textContent = week;
        svg.appendChild(xLabel);
      }
    }

    // Lines, with labels at the end. End labels are nudged apart so they
    // do not overlap.
    var ends = [];
    series.forEach(function (s) {
      var points = s.values.map(function (v, i) {
        return x(i) + ',' + y(v);
      }).join(' ');
      svg.appendChild(svgEl('polyline', {points: points, 'class': 'beergame-line beergame-series-' + s.key}));
      if (s.values.length && series.length > 1) {
        ends.push({key: s.key, label: s.label, y: y(s.values[s.values.length - 1])});
      }
    });
    ends.sort(function (a, b) {
      return a.y - b.y;
    });
    for (var i = 1; i < ends.length; i++) {
      ends[i].y = Math.max(ends[i].y, ends[i - 1].y + 13);
    }
    ends.forEach(function (end) {
      var text = svgEl('text', {x: margin.left + plotWidth + 6, y: end.y + 4, 'class': 'beergame-end-label'});
      text.textContent = end.label;
      svg.appendChild(text);
    });

    // Hover layer: a crosshair and a tooltip listing every series.
    var crosshair = svgEl('line', {y1: margin.top, y2: margin.top + plotHeight, 'class': 'beergame-crosshair', visibility: 'hidden'});
    svg.appendChild(crosshair);
    var hit = svgEl('rect', {x: margin.left, y: margin.top, width: plotWidth, height: plotHeight, fill: 'transparent'});
    svg.appendChild(hit);

    var wrapper = $('<div class="beergame-chart-plot"></div>').append(svg);
    var tooltip = $('<div class="beergame-tooltip" hidden></div>');
    wrapper.append(tooltip);
    figure.append(wrapper);

    $(hit).on('mousemove', function (event) {
      var box = svg.getBoundingClientRect();
      var svgX = (event.clientX - box.left) * width / box.width;
      var index = Math.round((svgX - margin.left) * (weeks - 1) / plotWidth);
      index = Math.max(0, Math.min(weeks - 1, index));
      crosshair.setAttribute('x1', x(index));
      crosshair.setAttribute('x2', x(index));
      crosshair.setAttribute('visibility', 'visible');
      tooltip.empty().append($('<strong></strong>').text(options.weekLabel + ' ' + (index + 1)));
      series.forEach(function (s) {
        if (index < s.values.length) {
          tooltip.append($('<div></div>').append(
            $('<span class="beergame-swatch"></span>').addClass('beergame-series-' + s.key),
            $('<span></span>').text(s.label + ': ' + s.values[index])
          ));
        }
      });
      var left = x(index) * box.width / width;
      tooltip.css({left: left + 12 > box.width - 160 ? left - 172 : left + 12}).prop('hidden', false);
    }).on('mouseleave', function () {
      crosshair.setAttribute('visibility', 'hidden');
      tooltip.prop('hidden', true);
    });

    $(container).append(figure);
  }

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
