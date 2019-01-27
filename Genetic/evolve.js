var Queue = require('bee-queue');
var util = require('util');
var async = require('async');
var fs = require('fs');

const queueMatches = new Queue('bot-execution');
queueMatches.destroy();

const settings = JSON.parse(fs.readFileSync('settings_master.json', 'utf8'));

var training_name = '2p_32_master';

console.log('settings', settings);

const maps2p_32 = [
	{
		// 2P 32 test
		seed: 1542583743,
		size: 32,
		halite: 153614,
		enemy_bots: ['v65']
	},
	{
		// 2P 32 test
		seed: 1541557220,
		size: 32,
		halite: 127446,
		enemy_bots: ['v65']
	},
	{
		// 4P 32 TEST1
		seed: 1543096196,
		size: 32,
		halite: 205168,
		enemy_bots: ['v65']
	},
	{
		// 4P 32 TEST2
		seed: 1543086909,
		size: 32,
		halite: 130580,
		enemy_bots: ['v80']
	},
	{
		// 2P 32 TEST3
		seed: 1541466074,
		size: 32,
		halite: 183862,
		enemy_bots: ['v58']
	},
	{
		// 2P 32 TEST4
		seed: 1541521696,
		size: 32,
		halite: 198234,
		enemy_bots: ['v68']
	},
	{
		// 2P 32 TEST5
		seed: 1541519242,
		size: 32,
		halite: 196598,
		enemy_bots: ['v80']
	},
	{
		// 2P 32 TEST6
		seed: 1541512582,
		size: 32,
		halite: 191646,
		enemy_bots: ['v80']
	},
	{
		// 2P 32 TEST7
		seed: 1541510394,
		size: 32,
		halite: 143716,
		enemy_bots: ['Aggresive']
	},
	{
		// 2P 32 TEST8
		seed: 1541509736,
		size: 32,
		halite: 175068,
		enemy_bots: ['Aggresive']
	},
	{
		// 2P 32 TEST8
		seed: 1543447128,
		size: 32,
		halite: 156464,
		enemy_bots: ['v85']
	},
	{
		// 2P 32 TEST8
		seed: 1543444241,
		size: 32,
		halite: 159810,
		enemy_bots: ['v85']
	}
];

const maps2p_64 = [
	{
		seed: 1543382637,
		size: 64,
		halite: 651642,
		enemy_bots: ['v65']
	},
	{
		seed: 1543382516,
		size: 64,
		halite: 625016,
		enemy_bots: ['v65']
	},
	{
		seed: 1543382610,
		size: 64,
		halite: 803608,
		enemy_bots: ['v85']
	},
	{
		seed: 1543382610,
		size: 64,
		halite: 803608,
		enemy_bots: ['v65']
	},
	{
		seed: 1543382360,
		size: 64,
		halite: 862946,
		enemy_bots: ['v58']
	},
	{
		seed: 1543382282,
		size: 64,
		halite: 563780,
		enemy_bots: ['v68']
	},
	{
		seed: 1543382217,
		size: 64,
		halite: 767456,
		enemy_bots: ['v80']
	},
	{
		seed: 1543382239,
		size: 64,
		halite: 765152,
		enemy_bots: ['v80']
	},
	{
		seed: 1543382200,
		size: 64,
		halite: 778726,
		enemy_bots: ['Aggresive']
	},
	{
		seed: 1543382137,
		size: 64,
		halite: 579508,
		enemy_bots: ['Aggresive']
	}
];

const maps4p = [
	{
		seed: 1543096196,
		size: 32,
		halite: 205168,
		enemy_bots: ['v65', 'v65', 'v65']
	},
	{
		seed: 1543272914,
		size: 32,
		halite: 186344,
		enemy_bots: ['v58', 'v80', 'v58']
	},
	{
		seed: 1543273607,
		size: 40,
		halite: 305908,
		enemy_bots: ['Aggresive', 'v65', 'Aggresive']
	},
	{
		seed: 1543273176,
		size: 48,
		halite: 316900,
		enemy_bots: ['v80', 'v80', 'v80']
	},
	{
		seed: 1543273594,
		size: 32,
		halite: 203196,
		enemy_bots: ['v68', 'v68', 'v65']
	},
	{
		seed: 1543273607,
		size: 40,
		halite: 305908,
		enemy_bots: ['v68', 'v68', 'v68']
	},
	{
		seed: 1543272914,
		size: 32,
		halite: 186344,
		enemy_bots: ['v65', 'v65', 'v65']
	},
	{
		seed: 1543272607,
		size: 32,
		halite: 227036,
		enemy_bots: ['v65', 'v65', 'Aggresive']
	},
	{
		seed: 1543272738,
		size: 48,
		halite: 399504,
		enemy_bots: ['Aggresive', 'v65', 'v65']
	},
	{
		seed: 1543271775,
		size: 32,
		halite: 213288,
		enemy_bots: ['v65', 'v65', 'v65']
	},
];

var maps_to_train = maps2p_32;


function randomNum(low, high) {
	return Math.random() * (high - low) + low;
}

function random() {
	var solution = {};
	
	for(var feature_name in settings.features) {
		var feature = settings.features[feature_name];
		solution[feature_name] = feature.min + Math.random() * (feature.max - feature.min);
	}
	
	return solution;
}

function crossover(parent1, parent2) {
	var child = {};

	for(var feature_name in settings.features) {
		if(Math.random() > 0.5) {
			child[feature_name] = parent1[feature_name];
		} else {
			child[feature_name] = parent2[feature_name];
		}
	}
	
	return child;
}

function mutate(solution) {
	for(var feature_name in settings.features) {
		var feature = settings.features[feature_name];
		
		var r = Math.random();
		if(r < 0.15) {
			solution[feature_name] = feature.min + Math.random() * (feature.max - feature.min);
		} else if(r < 0.45) {
			solution[feature_name] += (Math.random() > 0.5 ? -1 : 1) * (feature.max - feature.min) * Math.random() * 0.15;
		}
		solution[feature_name] = Math.max(Math.min(solution[feature_name], feature.max), feature.min);
		// mutate feature
		//solution[feature_name] = feature.min + Math.random() * (feature.max - feature.min);
		//solution[feature_name] += (Math.random() > 0.5 ? -1 : 1) * (feature.max - feature.min) * Math.random() * 0.15;
		//solution[feature_name] += Math.random(-0.1, 0.1) * Math.random(0, feature.max - feature.min);
		//solution[feature_name] = Math.max(Math.min(solution[feature_name], feature.max), feature.min);
	}

	return solution;
}

var globa_i = 0;

function fitness(bot, callback) {
	var results = [];
	
	for(var map of maps_to_train) {
		var json = JSON.stringify(bot.features).replace(/"/g, '\\"');
		
		var b = [{
			index: -1,
			command: '"cd Latest && MyBot.exe ' + json + '"',
		}];
		for(var k in map.enemy_bots) {
			b.push({
				index: -1,
				command: '"cd "' + map.enemy_bots[k] + '" && MyBot.exe"',
			});
		}
		const job = queueMatches.createJob({
			seed: map.seed,
			size: map.size,
			halite: map.halite,
			bots: b
		});
		
		function next() {
			if(results.length == maps_to_train.length) {
				// all maps done!
				var fitness = 0;
				var wins = 0;
				var hal = 0;
				for(var r of results) {
					if(r.rank == 1) wins++;
					fitness += (r.collected) + (r.rank == 1 ? 10 : 0);
					hal += r.score;
				}
				globa_i++;
				console.log(" -- #" + (globa_i < 10 ? '0' + globa_i : globa_i) + "/" + settings.population + "   Fitness: " + fitness.toFixed(3) + "   Winrate: " + ((wins / results.length) * 100).toFixed(2) + "   HAL: " + hal.toLocaleString());
				
				callback(fitness);
			}
		}
		job.on('succeeded', function(result) {
			results.push(result[0]);
			next();
		});
		job.on('failed', function() {
			results.push({
				rank: 2,
				score: 0,
				index: solution_index
			});
			console.log("Job failed!");
			next();
		});
		job.save();
	}
}

var population = [];
var generation = 1;
var best = null;

try {
	best = JSON.parse(fs.readFileSync('best_' + training_name + '.json', 'utf8'));
	var save = JSON.parse(fs.readFileSync('save_' + training_name + '.json', 'utf8'));
	population = save.population;
	generation = save.generation;
} catch(e) {}

// init random population
for(var i = population.length; i < settings.population; i++) {
	population.push({
		features: random()
	});
}

function calcFitness(cb) {
	/*
	calcFitnessTournament(function() {
		cb();
	});
	*/
	globa_i = 0;
	async.forEach(population, function (item, callback) {
		fitness(item, function(val) {
			item.fitness = val;
							
			if(best == null || item.fitness > best.fitness) {
				best = item;
				fs.writeFileSync('best_' + training_name + '.json', JSON.stringify(best));
				console.log(" -------------------------------------");
				console.log(" ------------- NEW BEST! -------------");
				console.log(" -------------------------------------");
			}
			
			callback();
		});
	}, function(err) {
		if(err) throw err;
		cb();
	});
}

function areEqual(features1, features2) {
	for(var feature_name in settings.features) {
		var feature = settings.features[feature_name];
		if(Math.abs(features1[feature_name] - features2[feature_name]) > 0.01 * (feature.max - feature.min)) {
			return false;
		}
	}
	return true;
}


function iteration(cb) {
	if(generation % 1000 == 0) {
		console.log("Resetting generations...");
		for(var i = 0; i < settings.population; i++) {
			population[i] = {
				features: random()
			};
		}
		generation = 1;
	}
	
	var L = settings.features.length;
	var P = settings.population;
	var diversity = 0;
	for(var feature_name in settings.features) {
		for(var j = 0; j < P - 1; j++) {
			for(var j2 = j + 1; j2 < P; j2++) {
				diversity += Math.abs(population[j].features[feature_name] - population[j2].features[feature_name]);
			}
		}
	}
	
	
	console.log("============= GENERATION " + generation + " ============= (" + population.length + ")    DIVERSITY: " + diversity);
	
	// 1. Calculate fitness
	calcFitness(function() {
		var old_population = population;
		population = [];
		
		old_population.sort(function(a, b) {
			return b.fitness - a.fitness;
		});
		
		var min = 99999999999;
		var max = -min;
		var sum = 0;
		for(var i = 0; i < old_population.length; i++) {
			var p = old_population[i];
			min = Math.min(min, p.fitness);
			max = Math.max(max, p.fitness);
			sum += p.fitness;
		}
		var avg = sum / old_population.length;
		
		console.log(" MIN: " + min + "  MAX: " + max + "  AVG: " + avg);
		console.log(" BEST: " + best.fitness);
		
		// 2. Crossover
		var mx_pick = Math.ceil(settings.population * 0.3);
		for(var i = 0; i < Math.ceil(settings.population * 0.5); i++) {
			var a = Math.round(randomNum(0, mx_pick));
			var b = Math.round(randomNum(0, mx_pick));
			if(a == b) a = Math.round(randomNum(0, mx_pick));
			
			//console.log(a + ", " + b);
			
			population.push({
				features: crossover(old_population[a].features, old_population[b].features)
			});
		}
		
		// 3. Mutate
		for(var i = 0; i < population.length; i++) {
			population[i].features = mutate(population[i].features);
		}
		
		// Add missing individuals
		var i = 0;
		while(population.length < settings.population) {
			population.push({
				features: mutate(old_population[i++].features)
			});
		}
		
		// 4. Check for duplicates and mutate
		for(var i = 0; i < population.length; i++) {
			do {
				var shouldMutate = false;
				for(var j = 0; j < i; j++) {
					if(areEqual(population[i].features, population[j].features)) {
						shouldMutate = true;
						break;
					}
				}
				if(shouldMutate) {
					population[i].features = mutate(population[i].features);
				}
			} while(shouldMutate);
		}
		
		// last one always fresh
		population[population.length-1] = {
			features: random()
		};
		
		generation++;
		
		// save
		fs.writeFileSync('save_' + training_name + '.json', JSON.stringify({
			generation: generation,
			population: population,
			best: best
		}));
		cb();
	});
}

async.until(function () {
	return generation > settings.generations
}, function (cb) {
	iteration(function() {
		cb();
	});
}, function (err) {
	if(err) throw err;
	console.log("Done.");
	process.exit(0);
});



