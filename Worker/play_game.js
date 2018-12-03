var Queue = require('bee-queue');
var shuffle = require('shuffle-array');

var config = require('./config.json');
const NUM_PLAYERS = 2;

const queueMatches = new Queue(config.queue_name, {
	redis: config.redis,
	isWorker: false
});

var machine = process.argv[2];
var seed = process.argv[3];

var parameters = {};

for(var i = 4; i < process.argv.length; i += 2) {
	var name = process.argv[i];
	var value = process.argv[i + 1];
	
	parameters[name] = parseFloat(value);
}

var bots = [
	{
		bot: 'Latest',
		arguments: JSON.stringify(parameters).replace(/"/g, '\\"')
}];

var possible_enemies = ['Aggresive', 'default', 'v58', 'v65', 'v68', 'v80', 'v85', 'v38', 'v92'];
while(bots.length < NUM_PLAYERS) {
	bots.push({
		bot: shuffle.pick(possible_enemies)
	});
}

const job = queueMatches.createJob({
	seed: seed,
	bots: bots,
	replay: false
}).timeout(11 * 60 * 1000);

job.on('succeeded', function(result) {
	if(!result.crash && result.players[0].rank == 1 && result.players[0].score > 0) {
		console.log("W");
	} else {
		console.log("L");
	}
	process.exit(0);
});
job.on('failed', function() {
	console.log("L");
	process.exit(0);
});
job.save();