const fs = require('fs');
const _cliProgress = require('cli-progress');
const cTable = require('console.table');
const histogram = require('ascii-histogram');

const Halite = require("./halite.js");

var target_player_name = 'teccles';
var replays = "../Analysis/games_" + target_player_name;
replays = "C:/games_teccles";
var files = fs.readdirSync(replays);

const INF = 99999999;

var db = {};

var samples = 0;
var loading_bar = new _cliProgress.Bar({
    format: ' {bar} {percentage}% | ETA: {eta}s | {value}/{total} | Samples: {samples}'
}, _cliProgress.Presets.shades_classic);
console.clear();
loading_bar.start(files.length, 0);

for(var file of files) {
	loading_bar.increment(1, { samples: samples });
	
	var replay = null;
	try {
		replay = JSON.parse(fs.readFileSync(replays + '/' + file, 'utf8'));
	} catch(e) { /* console.log(e); */ }
	if(replay == null) continue;
	
	/* FILTER */
	//if(replay.production_map.width != 64) continue;
	//if(replay.number_of_players != 2) continue;
	/* FILTER */
	
	var game = Halite.parse(replay);
	
	samples++;
	
	var target_player_id = game.players[target_player_name];
	var target_player_rank = replay.game_statistics.player_statistics[target_player_id].rank;
	var last_frame = game.frames[game.frames.length - 1];
	
	if(target_player_rank != 1) continue;
	
	for(var turn_i in game.frames) {
		var frame = game.frames[turn_i];
		if(frame.players[target_player_id].entities == null) continue;
		
		for(var entity_id in frame.players[target_player_id].entities) {
			var e = frame.players[target_player_id].entities[entity_id];
			var p = { x: e.x, y: e.y };
			
			var pic_size = 3;
			var key = "";
			var num_enemies = 0;
			for(var xx = -pic_size; xx <= pic_size; xx++) {
				for(var yy = -pic_size; yy <= pic_size; yy++) {
					var pp = Halite.Normalize({ x: p.x + xx, y: p.y + yy }, game.width);
					var d = Halite.ToroidalDistanceTo(p, pp, game.width);
					
					if(d > 3) {
						key += 'x';
					} else {
						var ship_type = '*';
						for(var pi in frame.players) {
							if(frame.players[pi].entities == null) continue;
							var ally = pi == target_player_id;
							for(var eid_other in frame.players[pi].entities) {
								var e2 = frame.players[pi].entities[eid_other];
								if(e2.x == pp.x && e2.y == pp.y) {
									ship_type = ally ? '+' : '-';
								}
							}
						}
						
						if(ship_type == '-')
							num_enemies++;
						key += ship_type;
					}
				}
				//key += '\n';
			}
			if(num_enemies > 0) {
				var move_found = 'x';
				for(var m of frame.players[target_player_id].moves) {
					if(entity_id == m.id && m.type == 'm') {
						move_found = m.direction;
					}
				}
				if(!(key in db)) {
					db[key] = {
						total: 0,
						moves: {}
					};
				}
				db[key].total++;
				if(!(move_found in db[key].moves)) {
					db[key].moves[move_found] = 0;
				}
				db[key].moves[move_found]++;
			}
		}
	}
}
loading_bar.stop();

console.log("Writing file...");
fs.writeFileSync('counts_db.json',JSON.stringify(db, null, 4),{encoding:'utf8',flag:'w'});
console.log("Done.");