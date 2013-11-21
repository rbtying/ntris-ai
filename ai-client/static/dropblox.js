var dropblox = {
  ANIMATE_MOVE: 40,
  WAIT_FOR_MOVE: 1000,

  games: undefined,
  cur_game: undefined,
  history_board: undefined,

  initialize: function() {
    dropblox.cur_game = {};
    dropblox.cur_game.id = 0;
    dropblox.history_board = dropblox.create_board('history-boards', 'history_board', 'Current Board');
    setTimeout('dropblox.set_cur_game_state(0, 0)', 300);
    dropblox.load_game_from_json(saved_data);
  },


  create_board: function(target, id, header) {
    var html = (
      '<div class="container">' +
      '  <div class="header">' + header + '</div>' +
      '  <object id="' + id + '" data="Board.swf" type="application/x-shockwave-flash" width="280" height="416">' +
      '    <param name="movie" value="Board.swf" />' +
      '    <param name="flashVars" value="playable=false&squareWidth=16" />' +
      '  </object>' +
      '</div>'
    );
    $('#' + target).append(html);
    return board.initialize(id);
  },

  load_game_from_json: function(json) {
    var response = JSON.parse(json);
    if (response.code == 200) {
      // Variables used to animate the game's progress in real-time.
      var index = dropblox.cur_game.index;
      var catch_up = (dropblox.cur_game.states &&
                      index === dropblox.cur_game.states.length - 1);
      var was_active = dropblox.cur_game.active;

      dropblox.cur_game.states = [];
      for (var i = 0; i < response.states.length; i++) {
        var moves = JSON.parse(response.states[i].moves);
        for (var j = 0; j < moves.length + 1; j++) {
          var state = {
            state_index: i,
            move_index: j,
            board: response.states[i].state,
            moves: [],
          }
          for (var k = 0; k < j; k++) {
            state.moves.push(moves[k]);
          }
          dropblox.cur_game.states.push(state);
        }
      }
      dropblox.cur_game.active = response.active;
      $('#post-history-boards').html(
        '<table><tr>' +
        '<td id="select-a-move">Game progress:</td>' +
        '<td><div id="move-slider"></td>' +
        '<td><button id="animate" class="bloxbutton">Animate</button></td>' +
        '</tr></table>' +
        '<div id="cur-state-label"></div>'
      );
      $('#move-slider').slider({
        min: 0,
        max: dropblox.cur_game.states.length - 1,
        step: 1,
        slide: function(event, ui) {
          dropblox.set_cur_game_state(0, ui.value);
        },
      });
      $('#animate').click(function() {
        var index = dropblox.cur_game.index;
        if (index !== undefined && index < dropblox.cur_game.states.length - 1) {
          dropblox.set_cur_game_state(0, index + 1);
          setTimeout(function() {
            dropblox.animate_game(0, index + 1);
          }, dropblox.ANIMATE_MOVE);
        }
        window.event.preventDefault();
      });

      if (index === undefined) {
        var index = dropblox.cur_game.states.length - 1;
        dropblox.set_cur_game_state(0, index);
        if (dropblox.cur_game.active) {
          setTimeout(function() {
            dropblox.animate_game(game_id, index);
          }, dropblox.ANIMATE_MOVE);
        }
      } else if (dropblox.cur_game.active && catch_up) {
        dropblox.set_cur_game_state(game_0, index);
        setTimeout(function() {
          dropblox.animate_game(game_id, index);
        }, dropblox.ANIMATE_MOVE);
      }
    } else {
        alert('error')
    }
  },

  animate_game: function(game_id, index) {
    if (this.cur_game.id == game_id && this.cur_game.index == index) {
      if (index < this.cur_game.states.length - 1) {
        this.set_cur_game_state(game_id, index + 1);
        setTimeout(function() {
          dropblox.animate_game(game_id, index + 1);
        }, dropblox.ANIMATE_MOVE);
      } else if (dropblox.cur_game.active) {
        setTimeout(function() {
          if (dropblox.cur_game.id == game_id) {
            dropblox.load_game_history(game_id);
          }
        }, dropblox.WAIT_FOR_MOVE);
      }
    }
  },

  set_cur_game_state: function(game_id, index) {
    if (dropblox.cur_game.id == game_id) {
      $('#move-slider').slider('option', 'value', index);
      dropblox.cur_game.index = index;
      var state = dropblox.cur_game.states[index];
      $('#cur-state-label').html('Turn ' + state.state_index + ', moves: [' + state.moves.join(', ') + ']');
      dropblox.history_board.setBoardState(state.board, true);
      for (var i = 0; i < state.moves.length; i++) {
        dropblox.history_board.issueCommand(state.moves[i], true);
      }
      dropblox.history_board.draw();
    }
  },


  set_active_link: function(link) {
    $.cookie('active-link', link);
    if (link != 'log_in' && link != 'sign_up') {
      $.cookie('last-active-link', link);
    }
    $('#left-bar a, #top-bar a').removeClass('active');
    $('#' + link).addClass('active');
  },

  format_timestamp: function(ts) {
    var date = new Date(1000*ts);
    var hours = date.getHours();
    var am_pm;
    if (hours < 12) {
	am_pm = ' AM';
    } else {
	am_pm = ' PM';
	hours -= 12
    }
    if (hours === 0) {
	hours += 12
    }
    return (hours +
            (date.getMinutes() < 10 ? ':0' : ':') + date.getMinutes() +
            (date.getSeconds() < 10 ? ':0' : ':') + date.getSeconds() +
	    am_pm)
  },

  post: function(url, data, success, error) {
    $.ajax({
      type: 'POST',
      url: url,
      data: JSON.stringify(data),
      contentType: "application/json",
      dataType: "json",
      success: success,
      error: error,
    });
  },
};
