var spawn = require('child_process').spawn;
var fs = require('fs');

function random(low, high) {
  return Math.random() * (high - low) + low
}

var bots_to_benchmark = [
	'"cd OLD && MyBotv9.exe"',
	'"cd Build/Release && MyBot.exe"'
];

var seeds_to_benckmark = [];

for(var i = 0; i < 300; i++) {
	seeds_to_benckmark.push(parseInt(random(1000000, 100000000)));
}

var bench_results = {};

var bot_i = -1;
var seed_i = 0;

function next_game() {
	var bench_results_human = "";
	
	for(var i in bench_results) {
		var br = bench_results[i];
		var ks = Object.keys(br);
		var vls = Object.values(br);
		if(ks.length != 2) {
			continue;
		}
		var dif = vls[0] - vls[1];
		var perc = "+" + ((Math.max(vls[0], vls[1]) / Math.min(vls[0], vls[1]) - 1) * 100).toFixed(2) + "%";
		bench_results_human += "SEED " + i + ": ";
		if(dif > 0)
			bench_results_human += ks[0] + " (" + vls[0] + ") > " + ks[1] + " (" + vls[1] + ")    " + perc + "\n";
		else if(dif < 0)
			bench_results_human += ks[1] + " (" + vls[1] + ") > " + ks[0] + " (" + vls[0] + ")    " + perc + "\n";
		else
			bench_results_human += " BOTS ARE EQUAL";
	}
	
	fs.writeFileSync("benchmark_results.json",JSON.stringify(bench_results),{encoding:'utf8',flag:'w'});
	fs.writeFileSync("benchmark_results.txt",JSON.stringify(bench_results),{encoding:'utf8',flag:'w'});
	console.log(bench_results_human);
	console.log("---------------------------------------");
	
	if(bot_i + 1 >= bots_to_benchmark.length) {
		bot_i = 0;
		seed_i++;
	} else bot_i++;
	if(seed_i >= seeds_to_benckmark.length) {
		console.log("DONE");
		return;
	}
	
	var bot_to_play = bots_to_benchmark[bot_i];
	var seed_to_play = seeds_to_benckmark[seed_i];
	
	console.log("Playing new match... " + seed_to_play + " bot '" + bot_to_play + "'");
	
	var args = [
		//'--replay-directory',
		//'"Build/replays/"',
		'-vvv',
		'--seed',
		seed_to_play,
		'--width',
		'56',
		'--height',
		'56',
		'--results-as-json',
		'--no-compression',
		'--no-logs',
		bot_to_play,
		'"cd OLD && MyBot-DUMMY.exe"'
	];

	var prc = spawn("halite.exe", args, {
		cwd: __dirname
	});

	var match = {
		prc: prc,
		stdout: '',
		stderr: '',
		exit_code: -1
	};

	prc.stdout.on('data', (data) => {
		match.stdout += data.toString();
	});
	prc.stderr.on('data', (data) => {
		match.stderr += data.toString();
	});

	prc.on('exit', (code) => {
		match.exit_code = code;
		
		if(match.exit_code != 0) {
			console.log("Match failed! exit_code: " + match.exit_code);
			return;
		} else {
			console.log("Match finalized!");
		}
		
		//console.log(match.stderr);
		//console.log(match.stdout);

		var info = JSON.parse(match.stdout);
		if(!(seed_to_play in bench_results))
			bench_results[seed_to_play] = {};
		
		var score = 0;
		for(var i in info["stats"]) {
			score = Math.max(score, info["stats"][i]["score"]);
		}
		
		bench_results[seed_to_play][bot_to_play] = score;
		
		next_game();
	});
}

next_game();