const fs = require('fs');
const path = require('path');
const mysql = require('mysql');
const express = require('express');
const app = express();
const server = require('http').Server(app);
const io = require('socket.io')(server);

const pool = mysql.createPool({
	"host": "localhost",
	"user": "root",
	"password": "",
	"database": "halite"
});

var editing_right_now = null;
var code = '';
try {
	code = fs.readFileSync('code.js', {encoding:'utf8',flag:'r'});
} catch(e) {}

var entries_cache = {};
var test_info = 'Waiting...';
var test_code = '';
var test_results = {};
var testing = false;

function updateTestInfo(info) {
	console.log(info);
	test_info = info;
	io.to('code').emit('test_info', test_info);
}

function loadEntry(entry_id) {
	if(!(entry_id in entries_cache)) {
		entries_cache[entry_id] = JSON.parse(fs.readFileSync('public/entries/' + entry_id + '.json', {encoding:'utf8'}))
	}
	return entries_cache[entry_id];
}

function test(code, entry, callback) {
	try {
		eval(code + `
			var ___result = test(entry);
		`);
		callback(null, ___result+'');
	} catch (e) {
		callback(`${e.name}: <span style="color:red">${e.message}</span><br>${e.stack.split("\n")[1]}`, null);
	}
}

function doTest() {
	if(testing)
		return;
	if(code == test_code)
		return;
	
	testing = true;
	test_code = code;
	test_results = {
		"TOTAL": {
			right: 0,
			wrong: 0
		}
	};
	updateTestInfo('Testing...');

	pool.query("UPDATE entries SET evaluated_tag=NULL", function (error) {
		if (error) throw error;

		pool.query("SELECT * FROM entries WHERE tag != 'none'", function (error, results) {
			if (error) throw error;

			var index = 0;
			function next() {
				var should_cancel = test_code != code;
		
				if(should_cancel) {
					testing = false;
					updateTestInfo('Test cancelled.');
					doTest();
				} else if(index < results.length) {
					var entry = results[index];
					entry.data = loadEntry(entry.id);
					updateTestInfo('Test in progress... ' + (index+1) + '/' + results.length);
					index++;
					test(test_code, entry, function(error, evaluated_tag) {
						if(error) {
							testing = false;
							updateTestInfo(error);
							doTest();
							return;
						}
						if(!(entry.tag in test_results)) {
							test_results[entry.tag] = {
								right: 0,
								wrong: 0
							}
						}
						test_results[entry.tag].right += entry.tag == evaluated_tag;
						test_results[entry.tag].wrong += entry.tag != evaluated_tag;
						
						test_results['TOTAL'].right += entry.tag == evaluated_tag;
						test_results['TOTAL'].wrong += entry.tag != evaluated_tag;
						
						pool.query("UPDATE entries SET evaluated_tag=? WHERE id=?", [evaluated_tag, entry.id], function (error) {
							if (error) throw error;
							next();
						});
					});
				} else {
					testing = false;
					var summary = '';
					for(var tag in test_results) {
						var r = test_results[tag];
						summary += `${tag}: ${(r.right / (r.right + r.wrong) * 100.0).toFixed(2)}% (${r.right}/${r.right + r.wrong})<br>`;
					}
					updateTestInfo(summary);
					io.to('entries').emit('reload');
					doTest();
				}
			}
			next();
		});
	});
}

io.on('connection', function(socket) {
	console.log("Client connected!");

	/*
	pool.query("SELECT * FROM entries ORDER BY RAND() LIMIT 30", function (error, results) {
		if (error) throw error;
		socket.emit('entries', results);
	});
	*/
	socket.on('init', function(room) {
		socket.join(room);

		if(room == 'code') {
			socket.emit('code', code);
			socket.emit('test_info', test_info);
			if(editing_right_now == null) {
				editing_right_now = socket.id;
			} else {
				socket.emit('alert', "You can't modify the code.");
			}
		} else if(room == 'entries') {
			var type = socket.handshake.query.type || 'all';
			var query = '';
			switch(type) {
				case 'all':
				query = "SELECT * FROM entries ORDER BY RAND() LIMIT 100";
				break;
				case 'evaluated':
				query = "SELECT * FROM entries WHERE tag != 'none' ORDER BY RAND() LIMIT 30";
				break;
				case 'evaluated_wrong':
				query = "SELECT * FROM entries WHERE tag != 'none' AND tag != evaluated_tag ORDER BY RAND() LIMIT 30";
				break;
			}

			pool.query(query, function (error, results) {
				if (error) throw error;
				socket.emit('entries', results);
			});
		}
	});

	socket.on('update_tag', function(data) {
		pool.query("UPDATE entries SET tag=? WHERE id=?", [data.new_tag, data.id], function (error, results) {
			if (error) throw error;
			console.log(`Updated tag of #${data.id} to '${data.new_tag}'`);
		});
		if(!testing) {
			test_code = null;
		}
		doTest();
	});
	socket.on('update_code', function(new_code) {
		if(editing_right_now != socket.id || code === new_code)
			return;
		console.log("Code updated");
		code = new_code;
		socket.broadcast.emit('code', code);
		fs.writeFileSync('code.js', code, {encoding:'utf8',flag:'w'});
		doTest();
	});
	socket.on('disconnect', function(new_code) {
		if(editing_right_now == socket.id)
			editing_right_now = null;
	});
});
app.use(express.static(path.join(__dirname, 'public')));

server.listen(3000);
doTest();