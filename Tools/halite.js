
function copyMap(src, dst, size) {
	for(var x = 0; x < size; x++) {
		if(!(x in dst)) dst[x] = [];
		
		for(var y = 0; y < size; y++) {
			var h;
			if(src[x][y].hasOwnProperty('halite'))
				h = src[x][y].halite;
			else if(src[y][x].hasOwnProperty('energy'))
				h = src[y][x].energy; // y, x
		
			dst[x][y] = {
				halite: h,
				near_info: {}
			};
		}
	}
}



var Halite = {
	Normalize: function(p, size) {
		return {
			x: (p.x + size) % size,
			y: (p.y + size) % size
		}
	},
	ToroidalDistanceTo: function(a, b, size) {
		var dx = Math.abs(a.x - b.x);
		var dy = Math.abs(a.y - b.y);
		
		var t_dx = Math.min(dx, size - dx);
		var t_dy = Math.min(dy, size - dy);
		
		return t_dx + t_dy;
	},
	parse: function(game_engine) {
		var game_players = {};
		for(var p of game_engine.players) {
			game_players[p.name.split(' ')[0]] = p.player_id;
		}
		
		var game = {
			game_engine: game_engine,
			turns: game_engine.full_frames.length,
			width: game_engine.production_map.width,
			height: game_engine.production_map.height,
			frames: [],
			num_players: game_engine.players.length,
			players: game_players
		};
		game.frames[0] = {
			map: [],
			players: []
		};
		copyMap(game_engine.production_map.grid, game.frames[0].map, game.width);

		for(var player_id in game_engine.players) {
			game.frames[0].players[player_id] = {
				dropoffs: [game_engine.players[player_id].factory_location]
			};
		}
		
		for(var turn in game_engine.full_frames) {
			var frame = game_engine.full_frames[turn];
			var turn_i = parseInt(turn) + 1;
			//console.log(turn_i);
			
			game.frames[turn_i] = {
				map: [],
				players: [],
				events: frame.events || [],
				halite_remaining: 0,
			};
			
			// copy last turn map
			copyMap(game.frames[turn_i - 1].map, game.frames[turn_i].map, game.width);
			
			// update map
			for(var cell_i in frame.cells) {
				var cell = frame.cells[cell_i];
				game.frames[turn_i].map[cell.x][cell.y].halite = cell.production;
			}
			
			const MAX_CELL_NEAR_AREA_INFO = 5;
			
			for(var x = 0; x < game.width; x++) {
				for(var y = 0; y < game.height; y++) {
					var p = { x: x, y: y };
					game.frames[turn_i].halite_remaining += game.frames[turn_i].map[x][y].halite;
					
					for(var k = 0; k <= MAX_CELL_NEAR_AREA_INFO; k++) {
						game.frames[turn_i].map[x][y].near_info[k] = {
							cells: 0,
							halite: 0
						};
					}
					
					for(var xx = -MAX_CELL_NEAR_AREA_INFO; xx <= MAX_CELL_NEAR_AREA_INFO; xx++) {
						for(var yy = -MAX_CELL_NEAR_AREA_INFO; yy <= MAX_CELL_NEAR_AREA_INFO; yy++) {
							var pp = this.Normalize({ x: x + xx, y: y + yy }, game.width);
							var d = this.ToroidalDistanceTo(p, pp, game.width);
							d = Math.min(d, MAX_CELL_NEAR_AREA_INFO);
							
							for(var k = d; k <= MAX_CELL_NEAR_AREA_INFO; k++) {
								var info = game.frames[turn_i].map[x][y].near_info[k];
								
								info.cells++;
								info.halite += game.frames[turn_i].map[pp.x][pp.y].halite;
							}
						}
					}
					
					for(var k = 0; k <= MAX_CELL_NEAR_AREA_INFO; k++) {
						var info = game.frames[turn_i].map[x][y].near_info[k];
						info.avg_halite = info.halite / info.cells;
					}
				}
			}
			game.frames[turn_i].map_avg_halite = game.frames[turn_i].halite_remaining / (game.width * game.height);
			
			// copy last turn dropoffs
			for(var player_id in game_engine.players) {
				game.frames[turn_i].players[player_id] = {
					dropoffs: [].concat(game.frames[turn_i - 1].players[player_id].dropoffs),
					entities: game_engine.full_frames[turn_i - 1].entities[player_id] || [],
					moves: game_engine.full_frames[turn_i - 1].moves[player_id] || [],
				};
			}
			
			for(var event_i in frame.events) {
				var event = frame.events[event_i];
				if(event.type == 'construct') {
					game.frames[turn_i].players[event.owner_id].dropoffs.push(event.location);
				}
			}
		}
		
		return game;
	}
};

if (typeof module !== 'undefined' && typeof module.exports !== 'undefined')
	module.exports = Halite;
else
	window.Halite = Halite;