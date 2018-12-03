var Queue = require('bee-queue');
const cTable = require('console.table');
const histogram = require('ascii-histogram');

var config = require('./config.json');
var healthCounts;

process.on('unhandledRejection', async (err) => {
	console.error("Error: ", err.code);
	process.exit(1);
});

const queueMatches = new Queue(config.queue_name, {
	redis: config.redis,
	isWorker: false
});

var games = [];

queueMatches.on('error', (err) => {
	console.error("Error: ", err.code);
	process.exit(1);
});

queueMatches.on('ready', () => {
	console.log('Connected sucessfully.');
	
	setInterval(function() {
		queueMatches.checkHealth(function(err, counts) {
			healthCounts = counts;
			delete healthCounts.newestJob;
			delete healthCounts.succeeded;
		});
	}, 500);
});

queueMatches.on('job succeeded' + ``, (jobId, result) => {
	games.push(result);
	while(games.length > 500)
		games.shift();
	printInfo();
});


function printInfo() {
	var data = {};
	var workers = {};
	var wins = 0;
	
	for(var game of games) {
		var size = game.map.width;
		if(!(size in data)) {
			data[size] = {
				size: size,
				wins: 0,
				losses: 0,
				total: 0
			}
		}
		
		
		data[size].total++;
		if(game.players[0].rank == 1) {
			data[size].wins++;
			wins++;
		} else
			data[size].losses++;
		
		if(!(game.worker in workers)) {
			workers[game.worker] = {
				worker: game.worker,
				games: 0,
				execution_time_sum: 0
			}
		}
		workers[game.worker].games++;
		workers[game.worker].execution_time_sum += game.execution_time;
	}
	
	var table = [];
	var table2 = [];
	var data_histogram = {};
	var workers_histogram = {};
	
	for(var size in data) {
		data[size].winrate = (data[size].wins / data[size].total * 100).toFixed(2);
		if(data[size].winrate != 0)
			data_histogram[size] = data[size].winrate;
		
		table.push(data[size]);
	}
	
	for(var worker in workers) {
		workers[worker].execution_time_avg = workers[worker].execution_time_sum / workers[worker].games;
		delete workers[worker].execution_time_sum;
		workers_histogram[worker] = workers[worker].games;
		
		table2.push(workers[worker]);
	}
	
	
	console.clear();
	console.log(`**********************************************************`);
	console.log(`Games: ${games.length}`);
	console.log(`Winrate: ${(wins / games.length*100).toFixed(2)}%`);
	
	console.log();
	console.log();
	console.table(table);
	console.log();
	console.table(table2);
	console.log();
	
	console.log("Win games:");
	try{
	console.log(histogram(data_histogram, { sort: true, width: 100 }));
	} catch(err) { console.log('Problem rendering histogram.') }
	console.log();
	console.log("Workers:");
	try{
	console.log(histogram(workers_histogram, { sort: true, width: 100 }));
	} catch(err) { console.log('Problem rendering histogram.') }
	console.log();
	console.log("Jobs:");
	if(healthCounts != null) {
		try{
		console.log(histogram(healthCounts, { sort: true, width: 100 }));
		} catch(err) { console.log('Problem rendering histogram.') }
		console.log();
	}
	
}