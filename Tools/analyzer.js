const fs = require('fs');
const _cliProgress = require('cli-progress');
const cTable = require('console.table');
const histogram = require('ascii-histogram');

const Halite = require("./halite.js");

var target_player_name = 'Rachol';
var replays = "../Analysis/games_" + target_player_name;
var files = fs.readdirSync(replays);

const INF = 99999999;

var analysis = {
	samples: 0,
	dropoffs: {
		min_dist: INF,
		max_dist: -INF,
		sum_dist: 0,
		sum_samples: 0,
		hist: {},
		hist_turns: {},
		hist_avgs: {
			'0': {},
			'1': {},
			'2': {},
			'3': {},
			'4': {},
			'5': {},
			'6': {},
			'7': {},
			'8': {},
			'9': {},
			'10': {}
		},
		nth: {},
	},
	spawns_hist: {},
	last_spawn_hist: {},
	max_ships_hist: {}
};

var loading_bar = new _cliProgress.Bar({
    format: ' {bar} {percentage}% | ETA: {eta}s | {value}/{total} | Samples: {samples}'
}, _cliProgress.Presets.shades_classic);
console.clear();
loading_bar.start(files.length, 0);

for(var file of files) {
	loading_bar.increment(1, { samples: analysis.samples });
	
	var replay = null;
	try {
		replay = JSON.parse(fs.readFileSync(replays + '/' + file, 'utf8'));
	} catch(e) { /* console.log(e); */ }
	if(replay == null) continue;
	
	/* FILTER */
	//if(replay.production_map.width != 32) continue;
	if(replay.number_of_players != 2) continue;
	/* FILTER */
	
	var game = Halite.parse(replay);
	
	analysis.samples++;
	
	var target_player_id = game.players[target_player_name];
	var last_frame = game.frames[game.frames.length - 1];
	
	// DROPOFFS
	var target_player_dropoffs = last_frame.players[target_player_id].dropoffs;
	for(var i = 0; i < target_player_dropoffs.length; i++) {
		for(var j = i + 1; j < target_player_dropoffs.length; j++) {
			var dist = Halite.ToroidalDistanceTo(target_player_dropoffs[i], target_player_dropoffs[j], game.width);
			
			if(!(dist in analysis.dropoffs.hist))
				analysis.dropoffs.hist[dist] = 0;
			
			analysis.dropoffs.min_dist = Math.min(analysis.dropoffs.min_dist, dist);
			analysis.dropoffs.max_dist = Math.max(analysis.dropoffs.max_dist, dist);
			analysis.dropoffs.sum_dist += dist;
			analysis.dropoffs.sum_samples++;
			analysis.dropoffs.hist[dist]++;
		}
	}
	
	var last_spawn = 0;
	var last_spawn_ship_diff = 0;
	var last_spawn_map_avg_halite = 0;
	var last_spawn_halite_diff = 0;
	var max_ships = 0;
	
	for(var turn_i in game.frames) {
		var frame = game.frames[turn_i];
		if(frame.players[target_player_id].entities == null) continue;
		var ships_alive = Object.keys(frame.players[target_player_id].entities).length;
		var ally_halite = game.game_engine.full_frames[turn_i-1].energy[target_player_id];
		var enemy_ships = 0;
		var enemy_halite = 0;
		var all_ships = ships_alive;
		for(var pi in frame.players) {
			if(frame.players[pi].entities == null) continue;
			if(pi != target_player_id) {
				var s = Object.keys(frame.players[pi].entities).length;
				enemy_ships = Math.max(enemy_ships, s);
				enemy_halite = Math.max(enemy_halite, game.game_engine.full_frames[turn_i-1].energy[pi]);
				all_ships += s;
			}
		}
		var ships_diff = ships_alive - enemy_ships;
		
		max_ships = Math.max(max_ships, ships_alive);
		
		for(var event_i in frame.events) {
			var event = frame.events[event_i];
			
			if(event.owner_id != target_player_id) continue;
			
			switch(event.type) {
				case 'spawn':
				
				last_spawn_halite_diff = Math.round((ally_halite - enemy_halite)/1000);
				last_spawn = turn_i;
				last_spawn_ship_diff = ships_alive - enemy_ships;
				last_spawn_map_avg_halite = Math.round(game.frames[turn_i-1].map_avg_halite);
				
				var k = Math.round(frame.halite_remaining/1000);
				k = Math.round((ships_alive / (game.frames.length - turn_i)) * 10);
				
				if(!(k in analysis.spawns_hist)) {
					analysis.spawns_hist[k] = 0;
				}
				analysis.spawns_hist[k]++;
				
				break;
				
				case 'shipwreck': break;
				
				case 'construct':
				var nth = frame.players[target_player_id].dropoffs.length - 1;
				//console.log(event.location);
				//console.log(frame.players[target_player_id].dropoffs.length + '/' + target_player_dropoffs.length);
				
				if(!(nth in analysis.dropoffs.nth)) {
					analysis.dropoffs.nth[nth] = {};
					analysis.dropoffs.hist_turns[nth] = {};
				}
				
				if(!(ships_alive in analysis.dropoffs.nth[nth]))
					analysis.dropoffs.nth[nth][ships_alive] = 0;
				if(!(turn_i in analysis.dropoffs.hist_turns[nth]))
					analysis.dropoffs.hist_turns[nth][turn_i] = 0;
				
				/*
				for(var k = 0; k <= 10; k++) {
					var avg_val = game.frames[turn_i-1].map[event.location.x][event.location.y].near_info[k].avg_halite / game.frames[turn_i-1].map_avg_halite;
					avg_val = Math.round(avg_val*10); 
					if(!(avg_val in analysis.dropoffs.hist_avgs[k]))
						analysis.dropoffs.hist_avgs[k][avg_val] = 0;
					
					analysis.dropoffs.hist_avgs[k][avg_val]++;
				}
				*/
				
				analysis.dropoffs.nth[nth][ships_alive]++;
				analysis.dropoffs.hist_turns[nth][turn_i]++;
				
				//console.log();
				//console.log(nth + ": " + analysis.dropoffs.nth[nth]);
				
				//console.log(game.frames[turn_i-1].map[event.location.x][event.location.y].near_info[4].avg_halite);
				break;
			}
		}
	}
	
	var key_to_display = last_spawn;
	
	if(!(key_to_display in analysis.last_spawn_hist))
		analysis.last_spawn_hist[key_to_display] = 0;
	analysis.last_spawn_hist[key_to_display]++;
	
	if(!(max_ships in analysis.max_ships_hist)) {
		analysis.max_ships_hist[max_ships] = 0;
	}
	analysis.max_ships_hist[max_ships]++;
	
	//if(analysis.samples > 3) break;
}
loading_bar.stop();

analysis.dropoffs.avg_dist = analysis.dropoffs.sum_dist / analysis.dropoffs.sum_samples;
delete analysis.dropoffs.sum_dist;
delete analysis.dropoffs.sum_samples;

console.log("***************************");
console.log("Samples: " + analysis.samples);
console.log();

console.log("Dropoffs:");
console.table(analysis.dropoffs);
console.log();

console.log(histogram(analysis.dropoffs.hist, { sort: false, width: 100 }));
console.log();

console.log("Dropoffs nths:");
for(var nth in analysis.dropoffs.nth) {
	console.log("NTH: " + nth);
	console.log("Number of ships:");
	console.log(histogram(analysis.dropoffs.nth[nth], { sort: false, width: 100 }));
	console.log("Turn number:");
	console.log(histogram(analysis.dropoffs.hist_turns[nth], { sort: false, width: 100 }));
}

console.log("Hist AVGS:");
for(var k = 0; k <= 10; k++) {
	console.log();
	console.log("K=",k);
	console.log(histogram(analysis.dropoffs.hist_avgs[k], { sort: false, width: 100 }));
}




console.log("Hist Spawns:");
console.log(histogram(analysis.spawns_hist, { sort: false, width: 100 }));
console.log("Last spawn hist:");
console.log(histogram(analysis.last_spawn_hist, { sort: false, width: 100 }));
////
console.log("Max ships:");
console.log(histogram(analysis.max_ships_hist, { sort: false, width: 100 }));

