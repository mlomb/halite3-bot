var Queue = require('bee-queue');
var util = require('util');
var async = require('async');
var fs = require('fs');

const queueMatches = new Queue('bot-execution');
queueMatches.destroy();

const settings = JSON.parse(fs.readFileSync('settings.json', 'utf8'));

var training_name = '2p_32';

console.log('settings', settings);


const maps_to_train = [
	{
		// 2P 32 LOW
		seed: 5474827,
		size: 32,
		halite: 58652,
	},
	{
		// 2P 32 HIGH
		seed: 33857932,
		size: 32,
		halite: 419010
	},
	{
		// 2P 32 TEST
		seed: 1541386366,
		size: 32,
		halite: 187538
	},
	/*
	{
		// 2P 48 LOW
		seed: 22792317,
		size: 48,
		halite: 132486
	},
	{
		// 2P 48 HIGH
		seed: 14459742,
		size: 48,
		halite: 735758
	},
	{
		// 2P 64 LOW
		seed: 31407558,
		size: 64,
		halite: 232444
	},
	{
		// 2P 64 HIGH
		seed: 33296431,
		size: 64,
		halite: 1581922
	},
	*/
];

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
		var feature = settings.features[feature_name];
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
		if(Math.random() < 0.15) {
			if(Math.random() < 0.5) {
				// totally new
				solution[feature_name] = feature.min + Math.random() * (feature.max - feature.min);
			} else {
				// adjustment
				solution[feature_name] += (Math.random() > 0.5 ? -1 : 1) * (feature.max - feature.min) * Math.random() * 0.3;
			}
			solution[feature_name] = Math.max(Math.min(solution[feature_name], feature.max), feature.min);
			// mutate feature
			//solution[feature_name] = feature.min + Math.random() * (feature.max - feature.min);
			//solution[feature_name] += (Math.random() > 0.5 ? -1 : 1) * (feature.max - feature.min) * Math.random() * 0.15;
			//solution[feature_name] += Math.random(-0.1, 0.1) * Math.random(0, feature.max - feature.min);
			//solution[feature_name] = Math.max(Math.min(solution[feature_name], feature.max), feature.min);
			
		}
	}

	return solution;
}

function fitness(features, callback) {
	//callback(features.a + Math.sqrt(features.b) + Math.pow(features.c, 2));
	//return;
	
	var results = [];
	
	for(var map of maps_to_train) {
		const job = queueMatches.createJob({
			seed: map.seed,
			size: map.size,
			halite: map.halite,
			features: features,
		});//.timeout(2 * 60 * 1000);
		
		function next() {
			if(results.length == maps_to_train.length) {
				// all maps done!
				var fitness = 0;
				var wins = 0;
				for(var r of results) {
					if(r.rank == 1) wins++;
					fitness += r.collected * (r.rank == 1 ? 1 : 0.1);
				}
				console.log(" -- Fitness: " + fitness + "   Winrate: " + (wins / results.length));
				callback(fitness);
			}
		}
		job.on('succeeded', function(result) {
			//console.log("Match finalized! " + JSON.stringify(result));
			results.push(result);
			next();
		});
		job.on('failed', function() {
			results.push({
				rank: 2,
				score: 0,
				collected: 0
			});
			console.log("Job failed! Seed: " + map.seed + "  Size: " + map.size);
			next();
		});
		job.save();
	}
}

var population = [];
var generation = 1;
var best = null;

try {
	var save = JSON.parse(fs.readFileSync('save_' + training_name + '.json', 'utf8'));
	population = save.population;
	generation = save.generation;
	best = save.best;
} catch(e) {}

// init random population
for(var i = population.length; i < settings.population; i++) {
	population.push({
		features: random()
	});
}

function calcFitness(cb) {
	async.forEach(population, function (item, callback) {
		fitness(item.features, function(val) {
			item.fitness = val;
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
	console.log("============= GENERATION " + generation + " ============= (" + population.length + ")");
	
	// 1. Calculate fitness
	calcFitness(function() {
		var old_population = population;
		
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
		
		console.log(" MIN: " + min + "  MAX: " + max + "  AVG: " + avg + "  AVG COLLECTED: " + (avg / maps_to_train.length));
		
		if(best == null || max > best.fitness) {
			best = old_population[0];
			fs.writeFileSync('best_' + training_name + '.json', JSON.stringify(best));
			console.log(" ------------- NEW BEST!");
		}
		
		console.log(" BEST: " + best.fitness + "  AVG COLLECTED: " + (best.fitness / maps_to_train.length));
		
		// 2. Crossover first half
		var cross_amount = 4;//settings.population/2;
		population = [];
		for(var i = 0; i < cross_amount; i++) {
			for(var j = i + 1; j < cross_amount; j++) {
				population.push({
					features: crossover(old_population[i].features, old_population[j].features)
				});
			}
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
		population[population.length-1] = random();
		
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



