const fs = require('fs');
const net = require('net');
const path = require('path');
const http = require('http');
const https = require('https');
const crypto = require('crypto');
const express = require('express');
const bodyParser = require('body-parser');
const { spawn, exec } = require('child_process');

const app = express();

const credentials = {
	key: fs.readFileSync('server/certificados/key.pem'),
	cert: fs.readFileSync('server/certificados/cert.pem')
};

const secret = fs.readFileSync('server/seguridad/secret-jwt.data');

app.use(bodyParser.urlencoded({ extended: true }));
app.use(express.json());
app.use(express.static(path.join(__dirname, 'dist/servidor')));

//API HTTP ->

// VALIDACION ASINCRONA
app.post('/app00/check_username' , function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});
	
	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
	
	if (req.body.username === undefined) {
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}
	
	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err, data) {
		if (err) {
			console.log(err);
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let success = false;
		for (let i in json.users) {
			if (json.users[i].username === Buffer.from(req.body.username).toString()) {
				success = true;
				break;
			}
		}
		
		if(!success){
			res.status(401).send('{\"error\":\"username not registered\"}');
			return;
		}
		
		res.status(200).end();

		return;
	});
	return;
});

//REGISTRO
app.post( '/app00/registration', function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});

	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
	
	if (req.body.username === undefined){
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}
	
	if (req.body.password === undefined){
		res.status(400).send('{\"error\":\"no password property\"}');
		return;
	}
	
	if (req.body.email === undefined){
		res.status(400).send('{\"error\":\"no \"email\" property\"}');
		return;
	}
	
	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err1, data) {
		if (err1) {
			console.log(err1);
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
		
		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
		
		let success = true;
		for (let user of json.users) {
			if (user.username === req.body.username) {
				success = false;
				break;
			}
		}
		if(!success){
			res.status(403).send('{\"error\":\"the username is already registered\"}');
			return;
		}
		
		let hash = crypto.createHash('sha256').update(req.body.password);
		
		let user = {
			username: req.body.username,
			password: hash.digest('hex'),
			email: req.body.email,
			systems: [],
			harvests: [
				{
					name: 'Patata',
					ph: 7.00,
					threshold: 0.02
				},
				{
					name: 'Tomate',
					ph: 7.01,
					threshold: 0.03
				},
				{
					name: 'Lechuga',
					ph: 7.02,
					threshold: 0.04
				}
			]
		};
		
		json.users.push(user);
		
		fs.writeFile('server/apps/app00/usuarios/users.json', JSON.stringify(json), function (err2) {
			if (err2) {
				console.log(err2);
				res.status(500).send('{\"error\":\"there was an internal error\"}');
				return;
			}
			
			let header = {
				algorithm: 'sha256',
				type: 'jwt'
			};
						
			crypto.randomBytes(48, function (err3, bytes) {
				if (err3) {
					console.log(err3);
					res.status(500).send('{\"error\":\"there was an internal error\"}');
					return;
				}
							
				let payload = {
					username: Buffer.from(req.body.username).toString('hex'),
					expiration_time: Date.now() + (1000 * 60 * 10),
					refresh_time: Date.now() + (1000 * 60 * 60 * 24 * 3),
					token: Buffer.from(bytes).toString('base64')
				};
					
				let base64_header = Buffer.from(JSON.stringify(header)).toString('base64');
				let base64_payload = Buffer.from(JSON.stringify(payload)).toString('base64');
				let signature = crypto.createHmac(header.algorithm, secret).update(base64_header + '.' + base64_payload).digest('base64');

				res.set('Token-Auth-App00', base64_header+'.'+base64_payload+'.'+signature);

				res.status(200).end();
				return;
			});
			return;
		});
		return;
	});
	return;
});

//INCIO SESION
app.get( '/app00/authentication', function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});	
	
	if (req.headers['authorization'] === null) {
		res.status(403).send('{\"error\":\"no authorization header\"}');
		return;
	}
	
	if (!req.headers['authorization'].startsWith('Basic ')) {
		res.status(403).send('{\"error\":\"no basic authorization\"}');
		return;
	}
	
	let userEncoded = req.headers['authorization'].substring(req.headers['authorization'].indexOf(' ')+1);
	let userDecoded = Buffer.from(userEncoded, 'base64').toString();
	
	let username = userDecoded.substring(0, userDecoded.indexOf(':'));
	let hash = crypto.createHash('sha256').update(userDecoded.substring(userDecoded.indexOf(':') + 1));
	let password = hash.digest('hex');
	
	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err1, data) {
		if (err1) {
			console.log(err1)
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
		
		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let success = false;
		let credentials = false;
		for (let user of json.users) {
			if (user.username === username) {
				success = true;
				if (user.password === password) {
					credentials = true;
				}
				break;
			}
		}
		if(!success){
			res.status(403).send('{\"error\":\"the username is already registered\"}');
			return;
		}

		if (!credentials) {
			res.status(403).send('{\"error\":\"wrong credentials\"}');
			return;
		}
		
		let header = {
			algorithm: 'sha256',
			type: 'jwt'
		};
		
		crypto.randomBytes(48, function (err2, bytes) {
			if (err2) {
				res.status(500).send('{\"error\":\"there was an internal error\"}');
				return;
			}
			
			let payload = {
				username: Buffer.from(Buffer.from(userEncoded, 'base64').toString().substring(0, Buffer.from(userEncoded, 'base64').toString().indexOf(':'))).toString('hex'),
				expiration_time: Date.now() + (1000 * 60 * 10),
				refresh_time: Date.now() + (1000 * 60 * 60 * 24 * 3),
				token: Buffer.from(bytes).toString('base64')
			};
			
			let base64_header = Buffer.from(JSON.stringify(header)).toString('base64');
			let base64_payload = Buffer.from(JSON.stringify(payload)).toString('base64');
			
			let signature = crypto.createHmac(header.algorithm, secret).update(base64_header + '.' + base64_payload).digest('base64');
			
			res.set('Token-Auth-App00', base64_header+'.'+base64_payload+'.'+signature);
			
			res.status(200).end();
			return;
		});
		return;
	});
	return;
});

// CHEQUEO TOKEN
app.get( '/app00/authorization', function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});	

	if (req.headers['authorization'] === null) {
		res.status(403).send('{\"error\":\"no authorization header\"}');
		return;
	}
	
	if (!req.headers['authorization'].startsWith('Bearer ')) {
		res.status(403).send('{\"error\":\"no bearer authorization\"}');
		return;
	}
	
	let token = req.headers['authorization'].substring(req.headers['authorization'].indexOf(' ')+1);
	
	if ((token.match(/\./gi) === null) || (token.match(/\./gi).length !== 2)) {
		res.status(403).send('{\"error\":\"wrong token\"}');
		return;
	}
	
	let header = JSON.parse(Buffer.from(token.substring(0,token.indexOf('.')),'base64'));
	let payload = JSON.parse(Buffer.from(token.substring(token.indexOf('.')+1, token.lastIndexOf('.')),'base64'));
	let signature = token.substring(token.lastIndexOf('.')+1);

	if (header.algorithm === null) {
		res.status(403).send('{\"error\":\"wrong token\"}');
		return;
	}
	
	let base64_header = Buffer.from(JSON.stringify(header)).toString('base64');
	let base64_payload = Buffer.from(JSON.stringify(payload)).toString('base64');
			
	let signatureVerification = crypto.createHmac(header.algorithm, secret).update(base64_header + '.' + base64_payload).digest('base64');
	
	if (signatureVerification !== signature) {
		res.status(403).send('{\"error\":\"invalid token\"}');
		return;
	}
	
	if (payload.refresh_time < Date.now()) {
		res.status(403).send('{\"error\":\"expired token\"}');
		return;
	}
	
	if (payload.expiration_time < Date.now()) {
		crypto.randomBytes(48, function (err, buff) {
			if (err) {
				res.status(500).send('{\"error\":\"there was an internal error\"}');
				return;
			}

			payload.expiration_time = Date.now() + (1000 * 60 * 10);
			payload.refresh_time = Date.now() + (1000 * 60 * 60 * 24 * 3);
			payload.token = Buffer.from(buff).toString('base64');
				
			let base64_payload = Buffer.from(JSON.stringify(payload)).toString('base64');
			
			let signature = crypto.createHmac(header.algorithm, secret).update(base64_header + '.' + base64_payload).digest('base64');
			
			res.set('Token-Auth-App00', base64_header+'.'+base64_payload+'.'+signature);
			
			res.status(200).end();
			return;
		});
	}
	else {
		res.status(200).end();
	}
	return;
});

//OBTENER CULTIVOS
app.post('/app00/get_systems', function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});
	
	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
	
	if (req.body.username === undefined){
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}

	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err1, data) {
		if (err1) {
			console.log(err1);
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
	
		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let index = -1;
		for (let i in json.users) {
			if (json.users[i].username === Buffer.from(req.body.username, 'hex').toString()) {
				index = i;
				break;
			}
		}
		
		if(index === -1){
			res.status(403).send('{\"error\":\"username not registered\"}');
			return;
		}
		
		let response = {
			systems: json.users[index].systems,
			harvests: json.users[index].harvests
		};

		res.status(200).send(JSON.stringify(response));

		return;
	});
	return;
});

// ACTUALIZAR DATOS DE CULTIVO
app.post('/app00/check_system', function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});
	
	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
	
	if (req.body.username === undefined){
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}
	
	if (req.body.id === undefined){
		res.status(400).send('{\"error\":\"no \"id\" property\"}');
		return;
	}
	
	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err, data) {
		if (err) {
			console.log(err);
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
	
		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let index = -1;
		for (let i in json.users) {
			if (json.users[i].username === Buffer.from(req.body.username, 'hex').toString()) {
				index = i;
				break;
			}
		}
		
		if(index === -1){
			res.status(403).send('{\"error\":\"username not registered\"}');
			return;
		}
		
		let system = json.users[index].systems[req.body.id];

		if (system === undefined) {
			res.status(403).send('{\"error\":\"the id of the system is not valid\"}');
			return;
		}

		let auth_system =  system.username + ':'+ system.password;
		let auth_encoded =  Buffer.from(auth_system).toString('base64');
		
		let socket = new net.Socket();
		try {
			socket.connect(system.port, system.ip, function(){
				let message = {
					auth: 'Basic ' + auth_encoded,
					method: 'get-system',
					id: system.id
				};
				socket.end(JSON.stringify(message), 'utf8');
			});
		} catch (Exception){
			res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
			return;
		}

		socket.on('data', function(data){
			let response;
			try {
				response = JSON.parse(data);
				console.log(response);
			} catch {
				res.status(500).send('{\"error\":\"there was an error processing the request\"}');
				return;
			}
			if(response.status === 401) {
				res.status(401).send("{\"error\":\"wrong credentials\"}");
				return;
			}
			if(response.status === 409) {
				res.status(409).send("{\"error\":\"there are no registered systems\"}");
				return;
			}
			if(response.status === 404) {
				res.status(410).send("{\"error\":\"the system is not connected to the middleware network\"}");
				return;
			}
			if(response.status === 408) {
				res.status(408).send("{\"error\":\"the system is connected to the middleware network but did not respond correctly\"}");
				return;
			}
			if(response.status !== 200) {
				console.log(response);
				res.status(500).send('{\"error\":\"there was an error processing the request\"}');
				return;
			}
			let object = {
				ph: parseFloat(response.data.water_ph),
			    airTemperature: parseFloat(response.data.air_temperature),
			    airHumidity: parseFloat(response.data.air_humidity),
			    soilHumidity: ((1023 - parseFloat(response.data.soil_humidity))/1023) * 100,
			    rain: ((1023 - parseFloat(response.data.rain_sensor))/1023) * 100,
			    light: ((1136 - parseFloat(response.data.light_sensor))/1136) * 100,
			    lastWatering: parseFloat(response.data.last_water)
			};
			res.status(200).send(JSON.stringify(object));
			return;
		});

		socket.on('error', function(err) {
			res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
			return;
		});
	});
	return;
});

//AÑADIR CULTIVO
app.post('/app00/add_system' , function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});
	
	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
	
	if (req.body.ip === undefined) {
		res.status(400).send('{\"error\":\"no \"ip\" property\"}');
		return;
	}
	
	if (req.body.port === undefined) {
		res.status(400).send('{\"error\":\"no \"port\" property\"}');
		return;
	}
	
	if (req.body.username === undefined) {
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}
	
	if (req.body.password === undefined) {
		res.status(400).send('{\"error\":\"no \"password\" property\"}');
		return;
	}

	let properties = {};

	if (req.body.harvest !== undefined) {
		if (req.body.harvest.ph !== undefined) {
			properties['objective_ph'] = req.body.harvest.ph;
		}
		if (req.body.harvest.threshold !== undefined) {
			properties['ph_umbral'] = req.body.harvest.threshold;
		}
	}

	let username = '';
	if (req.body.username !== undefined) {
		username = req.body.username.toString();
	}
	
	let password = '';
	if (req.body.password !== undefined) {
		password = req.body.password.toString();
	}
	
	let auth_system =  username + ':'+ password;
	let auth_encoded =  Buffer.from(auth_system).toString('base64');
	

	let socket = new net.Socket();
	try {
		socket.connect(req.body.port, req.body.ip, function() {
			let message = {
				auth: 'Basic ' + auth_encoded,
				method: 'add-system'
			};
			if (req.body.harvest !== undefined) {
				message['properties'] = properties;
			}
			socket.end(JSON.stringify(message), 'utf8');
		});
	}
	catch (error){
        console.log(error);
		res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
		return;
	}

	socket.on('data', function(data){
		let response;
		try {
		  response = JSON.parse(data);
		  console.log(response);
		}
		catch {
			res.status(500).send('{\"error\":\"there was an error processing the request\"}');
			return;
		}
		if (response.status === 401) {
			res.status(401).send("{\"error\":\"wrong credentials\"}");
			return;
		}
		else if(response.status === 409) {
			res.status(409).send("{\"error\":\"the limit of registered systems was reached\"}");
			return;
		}
		else if(response.status === 404) {
			res.status(410).send("{\"error\":\"no connected system was found\"}");
			return;
		}
		else if(response.status !== 200) {
			res.status(500).send('{\"error\":\"there was an error processing the request\"}');
			return;
		}
		
		res.status(200).end();
		return;
	});

	socket.on('error', function(err) {
		console.log('error: ' + err);
		res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
		return;
	});
	
	return;
});

//GUARDAR EN SISTEMA DE FICHEROS
app.post('/app00/set_systems', function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});
	
	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
		
	if (req.body.systems === undefined) {
		res.status(400).send('{\"error\":\"no \"systems\" property\"}');
		return;
	}		
	
	if (req.body.username === undefined) {
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}
	
	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err, data) {
		if (err) {
			console.log(err);
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
	
		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let success = false;
		for (let i in json.users) {
			if (json.users[i].username === Buffer.from(req.body.username, 'hex').toString()) {
				json.users[i].systems = req.body.systems
				success = true;
				break;
			}
		}
		
		if (!success) {
			res.status(403).send('{\"error\":\"username not registered\"}');
			return;
		}

		fs.writeFile('server/apps/app00/usuarios/users.json', JSON.stringify(json), function (err2) {
			if (err2) {
				console.log(err2);
				res.status(500).send('{\"error\":\"there was an internal error\"}');
				return;
			}
			res.status(200).end();
		});
		return;
	});
	return;
});

//ELIMINAR CULTIVO
app.post('/app00/delete_system' , function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});

	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
	
	if (req.body.username === undefined) {
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}
	
	if (req.body.id === undefined) {
		res.status(400).send('{\"error\":\"no \"id\" property\"}');
		return;
	}
	
	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err, data) {
		if (err) {
			console.log(err);
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
	
		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let index = -1;
		for (let i in json.users) {
			if (json.users[i].username === Buffer.from(req.body.username, 'hex').toString()) {
				index = i;
				break;
			}
		}
		
		if(index === -1) {
			res.status(403).send('{\"error\":\"username not registered\"}');
			return;
		}
		
		let system = json.users[index].systems[req.body.id];

		if (system === undefined) {
			res.status(403).send('{\"error\":\"the id of the system is not valid\"}');
			return;
		}

		let auth_system =  system.username + ':'+ system.password;
		let auth_encoded =  Buffer.from(auth_system).toString('base64');
		
		let socket = new net.Socket();
		try {
			socket.connect(system.port, system.ip, function() {
				let message = {
					auth: 'Basic ' + auth_encoded,
					method: 'delete-system',
					id: system.id
				};
				socket.end(JSON.stringify(message), 'utf8');
			});
		} catch (Exception) {
			res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
			return;
		}

		socket.on('data', function(data) {
			let response;
			try {
				response = JSON.parse(data);
				console.log(response);
			} catch {
				res.status(500).send('{\"error\":\"there was an error processing the request\"}');
				return;
			}

			if (response.status === 401) {
				res.status(401).send("{\"error\":\"wrong credentials\"}");
				return;
			}
			if(response.status === 409) {
				res.status(409).send("{\"error\":\"there are no registered systems\"}");
				return;
			}
			if(response.status === 404) {
				res.status(410).send("{\"error\":\"there is not a system registered in that id\"}");
				return;
			}
			if(response.status !== 200) {
				res.status(500).send('{\"error\":\"there was an error processing the request\"}');
				return;
			}

			res.status(200).end();
			return;
		});

		socket.on('error', function(err) {
			res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
			return;
		});
	});
	return;
});

// ACTUALIZAR PH DE CULTIVO
app.post('/app00/edit_system' , function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});

	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
	
	if (req.body.username === undefined) {
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}
	
	if (req.body.id === undefined) {
		res.status(400).send('{\"error\":\"no \"id\" property\"}');
		return;
	}

	if (req.body.properties === undefined) {
		res.status(400).send('{\"error\":\"no \"properties\" property\"}');
		return;
	}
	
	
	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err, data) {
		if (err) {
			console.log(err);
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
	
		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let index = -1;
		for (let i in json.users) {
			if (json.users[i].username === Buffer.from(req.body.username, 'hex').toString()) {
				index = i;
				break;
			}
		}
		
		if(index === -1) {
			res.status(403).send('{\"error\":\"username not registered\"}');
			return;
		}
		
		let system = json.users[index].systems[req.body.id];

		if (system === undefined) {
			res.status(403).send('{\"error\":\"the id of the system is not valid\"}');
			return;
		}

		let properties = {}
	
		if (req.body.properties.ph === undefined) {
			properties['objective_ph'] = system.harvest.ph.toString();
		} else {
			properties['objective_ph'] = req.body.properties.ph.toString();
		}
		
		if (req.body.properties.threshold === undefined) {
			properties['ph_umbral'] = system.harvest.threshold.toString();
		} else {
			properties['ph_umbral'] = req.body.properties.threshold.toString();
		}

		let auth_system =  system.username + ':'+ system.password;
		let auth_encoded =  Buffer.from(auth_system).toString('base64');
		
		let socket = new net.Socket();
		try {
			socket.connect(system.port, system.ip, function() {
				let message = {
					auth: 'Basic ' + auth_encoded,
					method: 'edit-system',
					id: system.id,
					properties: properties
				};
				socket.end(JSON.stringify(message), 'utf8');
			});
		} catch (Exception) {
			res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
			return;
		}

		socket.on('data', function(data) {
			let response;
			try {
				response = JSON.parse(data);
				console.log(response);
			} catch {
				res.status(500).send('{\"error\":\"there was an error processing the request\"}');
				return;
			}

			if (response.status === 401) {
				res.status(401).send("{\"error\":\"wrong credentials\"}");
				return;
			}
			if(response.status === 409) {
				res.status(409).send("{\"error\":\"there are no registered systems\"}");
				return;
			}
			if(response.status === 404) {
				res.status(410).send("{\"error\":\"the system is not connected to the middleware network\"}");
				return;
			}
			if(response.status === 408) {
				res.status(408).send("{\"error\":\"the system is connected to the middleware network but did not respond correctly\"}");
				return;
			}
			if(response.status !== 200) {
				res.status(500).send('{\"error\":\"there was an error processing the request\"}');
				return;
			}

			res.status(200).end();
			return;
		});

		socket.on('error', function(err) {
			res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
			return;
		});
	});
	return;
});

//ENVIAR ACCION
app.post('/app00/send_action' , function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});

	if (req.body === undefined) {
		res.status(400).send('{\"error\":\"no content body\"}');
		return;
	}
	
	if (req.body.username === undefined) {
		res.status(400).send('{\"error\":\"no \"username\" property\"}');
		return;
	}
	
	if (req.body.id === undefined) {
		res.status(400).send('{\"error\":\"no \"id\" property\"}');
		return;
	}

	if (req.body.code === undefined) {
		res.status(400).send('{\"error\":\"no \"code\" property\"}');
		return;
	}
	
	let code;
	try {
		code = parseInt(req.body.code);
	} catch (SyntaxError) {
		res.status(400).send('{\"error\":\"the \"code\" property is not a number\"}');
		return;
	}
	
	if ((code != 1) && (code != 2))	{
		res.status(400).send('{\"error\":\"the \"code\" property is not a valid number\"}');
		return;
	}
	
	fs.readFile('server/apps/app00/usuarios/users.json', 'utf8', function (err, data) {
		if (err) {
			console.log(err);
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}
	
		let json;
		
		try {
			json =  JSON.parse(data);
		} catch (SyntaxError) {
			res.status(500).send('{\"error\":\"there was an internal error\"}');
			return;
		}

		let index = -1;
		for (let i in json.users) {
			if (json.users[i].username === Buffer.from(req.body.username, 'hex').toString()) {
				index = i;
				break;
			}
		}
		
		if(index === -1) {
			res.status(403).send('{\"error\":\"username not registered\"}');
			return;
		}
		
		let system = json.users[index].systems[req.body.id];

		if (system === undefined) {
			res.status(403).send('{\"error\":\"the id of the system is not valid\"}');
			return;
		}

		let auth_system =  system.username + ':'+ system.password;
		let auth_encoded =  Buffer.from(auth_system).toString('base64');
		
		let socket = new net.Socket();
		try {
			socket.connect(system.port, system.ip, function() {
				let message = {
					auth: 'Basic ' + auth_encoded,
					method: 'send-action',
					id: system.id,
					code: code
				};
				socket.end(JSON.stringify(message), 'utf8');
			});
		} catch (Exception) {
			res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
			return;
		}

		socket.on('data', function(data) {
			let response;
			try {
				response = JSON.parse(data);
				console.log(response);
			} catch {
				res.status(500).send('{\"error\":\"there was an error processing the request\"}');
				return;
			}

			if (response.status === 401) {
				res.status(401).send("{\"error\":\"wrong credentials\"}");
				return;
			}
			if(response.status === 409) {
				res.status(409).send("{\"error\":\"there are no registered systems\"}");
				return;
			}
			if(response.status === 404) {
				res.status(410).send("{\"error\":\"the system is not connected to the middleware network\"}");
				return;
			}
			if(response.status === 408) {
				res.status(408).send("{\"error\":\"the system is connected to the middleware network but did not respond correctly\"}");
				return;
			}
			if(response.status !== 200) {
				res.status(500).send('{\"error\":\"there was an error processing the request\"}');
				return;
			}

			res.status(200).end();
			return;
		});

		socket.on('error', function(err) {
			res.status(404).send('{\"error\":\"service not found on the ip and port provided\"}');
			return;
		});
	});
	return;
});

//ENVIAR APP WEB
app.get( '**', function (req, res) {
	req.on('error', () => {
		res.status(400).end();
		return;
	});
	res.sendFile(path.join(__dirname, 'dist/servidor/index.html'));
	return;
});

//REDIRECCION A HTTPS
http.createServer(function (req, res) {
	res.writeHead(301, { "Location": "https://" + req.headers['host'] + req.url });
	res.end();
}).listen(80);
https.createServer(credentials, app).listen(443);
