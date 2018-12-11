var Queue = require('bee-queue');
var seedrandom = require('seedrandom');
var shuffle = require('shuffle-array');

var SEED = process.argv[2] || 0;
var NUM_PLAYERS = process.argv[3] || 2;
var NUM_GAMES = process.argv[4] || 10;

console.log("Games seed: " + SEED);
console.log("Num. players: " + NUM_PLAYERS);
console.log("Num. games: " + NUM_GAMES);
console.log();

seedrandom(SEED, { global: true });

var config = require('./config.json');

const queueMatches = new Queue(config.queue_name, {
	redis: config.redis,
	isWorker: false
});

var results = [];

function printResults() {
	if(results.length == NUM_GAMES) {
		var fitness = 0;
		var fitness_wins = 0;
		var wins = 0;
		for(var r of results) {
			if(r.players[0].rank == 1) wins++;
			fitness += (r.players[0].collected);
			fitness_wins += (r.players[0].collected) * (r.players[0].rank == 1 ? 1 : 0.1);
		}
		console.log();
		console.log(" --  Fitness: " + fitness + "   Fitness Wins: " + fitness_wins + "   Winrate: " + ((wins / results.length) * 100).toFixed(2) + "%");
	}
}

for(var i = 0; i < NUM_GAMES; i++) {
	var bots = [
		{
			bot: '../../Build/Release'
		}
	];

	var possible_enemies = ['Aggresive', 'default', 'v58', 'v65', 'v68', 'v80', 'v85', 'v38', 'v92', 'v106'];
	while(bots.length < NUM_PLAYERS) {
		bots.push({
			bot: shuffle.pick(possible_enemies)
		});
	}
	
	const job = queueMatches.createJob({
		seed: Math.round(Math.random() * 100000),
		bots: bots,
		replay: true
	}).timeout(11 * 60 * 1000);

	job.on('succeeded', function(result) {
		result.players.sort(function(a, b) {
			return a.rank - b.rank;
		});
		
		var match_result = "";
		for(var r of result.players) {
			match_result += r.bot.padStart(10) + " " + (r.score+'').padStart(6) + "/" + r.collected.toFixed(2) + " >> ";
		}
		match_result = match_result.substr(0, match_result.length - 3);
		
		console.log("Match #" + job.id + " Seed " + (result.map.seed+'').padStart(6) + " Size " + result.map.width + "x" + result.map.height + " " + (result.execution_time+'').padStart(6) + "ms  -- " + match_result);
		
		result.players.sort(function(a, b) {
			return a.index - b.index;
		});
		
		results.push(result);
		printResults();
	});
	job.on('failed', function() {
		console.log("Job failed.");
		results.push(null);
		printResults();
	});
	job.save();
}