var Queue = require('bee-queue');
var spawn = require('child_process').spawn;

var config = require('./config.json');

process.on('unhandledRejection', async (err) => {
	console.error("Error:", err);
	try {
		await queueMatches.close(0);
	} catch (err) { }
	process.exit(1);
});

const queueMatches = new Queue(config.queue_name, {
	redis: config.redis,
	isWorker: true
});

queueMatches.on('ready', () => {
	console.log('Connected sucessfully.');
});

queueMatches.on('error', (err) => {
	console.error("Error: ", err.code);
	process.exit(1);
});

queueMatches.process(config.concurrency, function (job, done) {
	var args = [
		'--results-as-json',
		'--no-compression',
		'--no-logs'
	];
	
	if(!job.data.replay) {
		args.push('--no-replay');
	}
	
	if(job.data.size) {
		args = args.concat([
			'--width',
			job.data.size,
			'--height',
			job.data.size
		]);
	}
	
	if(job.data.seed) {
		args = args.concat([
			'--seed',
			job.data.seed
		]);
	}
	
	for(var bot of job.data.bots) {
		var command = '"cd bots/' + bot.bot + ' && MyBot.exe ' + (bot.arguments || '') + '"';
		args.push(command);
	}
	
	var prc = spawn("halite.exe", args, {
		cwd: __dirname
	});
	
	var match = {
		prc: prc,
		stdout: '',
		stderr: '',
		exit_code: -1
	};

	match.timeout = setTimeout(() => {
		console.log('Match timed out! PID: ' + match.prc.pid);
		try {
			match.prc.kill('SIGINT');
		} catch (e) {
			console.log(e);
			console.log('Cannot kill process!');
		}
	}, 10 * 60 * 1000);
	
	prc.stdout.on('data', (data) => {
		match.stdout += data.toString();
	});
	prc.stderr.on('data', (data) => {
		match.stderr += data.toString();
	});

	prc.on('exit', (code) => {
		clearTimeout(match.timeout);
		
		match.exit_code = code;
		
		if(match.exit_code != 0) {
			var info = "Match failed! exit_code: " + match.exit_code;
			console.log(info);
			done(info);
			return;
		}
		
		//console.log(match.stderr);
		//console.log(match.stdout);

		var info = JSON.parse(match.stdout);
		var crash = false;
		var players = [];
		
		for(var i = 0; i < job.data.bots.length; i++) {
			var terminated = info["terminated"][i] || false;
			
			crash = crash || terminated;
			
			players.push({
				index: i,
				bot: job.data.bots[i].bot,
				rank: info.stats[i].rank,
				score: info.stats[i].score,
				collected: +(info.stats[i].score / info.map_total_halite).toFixed(5),
				terminated: terminated
			});
		}
		
		players.sort(function(a, b) {
			return a.rank - b.rank;
		});

		var match_result = "";
		for(var r of players) {
			match_result += r.bot.padStart(10) + " " + (r.score+'').padStart(6) + "/" + r.collected.toFixed(2) + " >> ";
		}
		match_result = match_result.substr(0, match_result.length - 3);
		
		console.log("Match #" + job.id + " Seed " + (info.map_seed+'').padStart(6) + " Size " + info.map_width + "x" + info.map_height + " " + (info.execution_time+'').padStart(6) + "ms  -- " + match_result);
		
		players.sort(function(a, b) {
			return a.index - b.index;
		});
		
		done(null, {
			worker: config.worker_name,
			crash: crash,
			execution_time: info.execution_time,
			map: {
				seed: info.map_seed,
				width: info.map_width,
				height: info.map_height,
				total_halite: info.map_total_halite
			},
			players: players
		});
	});
});