const fs = require('fs');
const mysql = require('mysql');

const pool = mysql.createPool({
	"host": "localhost",
	"user": "root",
	"password": "",
	"database": "halite"
});

fs.readdirSync('public/entries').forEach(file => {
	try {
		JSON.parse(fs.readFileSync('public/entries/' + file, {encoding:'utf8'}));
	} catch(e) {
		var id = file.split('.')[0];
		console.log("Broken: " + id + "    " + e);
		fs.unlinkSync('public/entries/' + file);
		pool.query("DELETE FROM entries WHERE id=?", [id], function (error, results) {
			if (error) throw error;
		});
	}
});