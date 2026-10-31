/**
 * @file
 * Folds away the player's story once read, and remembers that it was folded.
 */
(function ($) {

  'use strict';

  Backdrop.behaviors.beergameStory = {
    attach: function (context) {
      $('.beergame-story', context).once('beergame-story').each(function () {
        var details = this;
        var key = 'beergame-story-' + $(details).data('beergameStory');
        // Storage may be unavailable, e.g. in a private window. The story
        // then simply starts open each time.
        try {
          if (window.localStorage.getItem(key) === 'closed') {
            details.open = false;
          }
        }
        catch (e) {}

        $(details).on('toggle', function () {
          try {
            window.localStorage.setItem(key, details.open ? 'open' : 'closed');
          }
          catch (e) {}
        });
        $('.beergame-story-close', details).on('click', function () {
          details.open = false;
          $('summary', details)[0].focus();
        });
      });
    }
  };

})(jQuery);
