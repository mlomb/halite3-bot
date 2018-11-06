var Queue = require('bee-queue');
var spawn = require('child_process').spawn;
var fs = require('fs');

const queueMatches = new Queue('bot-execution', {
	redis: {
		host: '192.168.0.101',
	},
});

queueMatches.process(5, function (job, done) {
	console.log("Processing job " + job.id + " -- " + job.data.seed + " " + job.data.size);
	
	var json = JSON.stringify(job.data.features).replace(/"/g, '\\"');
	
	var args = [
		'--results-as-json',
		'--no-compression',
		'--width',
		job.data.size,
		'--height',
		job.data.size,
		'--seed',
		job.data.seed,
		'--no-logs',
		'--no-replay',
		'--no-timeout',
		'"MyBot.exe ' + json + '"',
		'"MyBotv38.exe"',
		//'"cd ../OLD && MyBot-DUMMY.exe"',
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
			var info = "Match failed! exit_code: " + match.exit_code;
			console.log(info);
			done(info);
			return;
		}
		
		//console.log(match.stderr);
		//console.log(match.stdout);

		var info = JSON.parse(match.stdout);
		
		var score = info.stats[0].score;
		var rank = info.stats[0].rank;
		var collected = score / job.data.halite;
		
		console.log("Match finalized -- Rank: " + rank + " Score: " + score + "  Collected: " + collected);
		
		done(null, {
			score: score,
			rank: rank,
			collected: collected
		});
	});
});