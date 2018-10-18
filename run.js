var spawn = require('child_process').spawn;
var fs = require('fs');

var args = [
	//'--replay-directory',
	//'"Build/replays/"',
	'-vvv',
	'--results-as-json',
	'--no-compression',
	'--width',
	'64',
	'--height',
	'64',
	'--no-logs',
	'"cd Build/Release && MyBot.exe"',
	'"cd Build/Release && MyBot.exe"'
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

var global_replay = null;

function applyDebug(i, cb) {
	fs.readFile(__dirname + '/Build/Release/bot-' + i + '.log', 'utf8', function (err, data) {
		var s = data.split('\n');
		for(var line of s) {
			var token = "FLUORINEDEBUG ";
			if(line.startsWith(token)) {
				line = line.substr(token.length);
				var debuginfo = JSON.parse(line);
				if(debuginfo.type == "ship") {
					global_replay["full_frames"][debuginfo.turn]["entities"][i+""][debuginfo.ship_id+""].data = debuginfo.data;
				}
			}
		}
		cb();
	});
}

prc.on('exit', (code) => {
	match.exit_code = code;
	
	if(match.exit_code != 0) {
		console.log("Match failed! exit_code: " + match.exit_code);
		return;
	} else {
		console.log("Match finalized!");
	}
	
	console.log(match.stderr);
	console.log(match.stdout);

	var info = JSON.parse(match.stdout);
	
	var replay_file = __dirname + '/' + info.replay;
	fs.readFile(replay_file, function (err, data) {
		if(err) throw err;
		global_replay = JSON.parse(data.toString());
		
		applyDebug(0, function() {
			applyDebug(1, function() {
				console.log("Apply and save again");
				fs.writeFileSync(replay_file,JSON.stringify(global_replay),{encoding:'utf8',flag:'w'})
			});
		});
	});
});