document.getElementById("save").addEventListener("click", function(){ console.log('save...'); save(); });
document.getElementById("restore").addEventListener("click", function(){ console.log('restore...'); restore(); });
document.getElementById("check").addEventListener("click", check);
document.getElementById("update").addEventListener("click", update_fw);
document.getElementById("restart").addEventListener("click", restart);



let ssid = document.getElementById("ssid");
let password = document.getElementById("password");
let speed = document.getElementById("speed");
let current_version = document.getElementById("current_version");
let new_version = document.getElementById("new_version");


let firmwareTable = document.getElementById('firmware');
async function restart() {
	try {
		const response = await fetch('http://tracker.local/restart');
		if (!response.ok) {
			throw new Error('Error! status: '+response.status);
		}
		const result = await response.json();
		return result;
	} catch (err) {
		console.log(err);
	}
}


function check() {
	console.log('check...');

	get_check().then(data => {
		console.log(data);
		firmwareTable.rows[1].cells[1].innerText = data.new_version;
		if (data.new_version > data.version) {
			document.getElementById("update").disabled = false;
		}
	})
}

async function get_check() {
	try {
		const response = await fetch('http://tracker.local/check');
		if (!response.ok) {
			throw new Error('Error! status: '+response.status);
		}
		const result = await response.json();
		return result;
	} catch (err) {
		console.log(err);
	}
}

async function update_fw() {
	console.log('update...');
	try {
		const response = await fetch('http://tracker.local/update');
		if (!response.ok) {
			throw new Error('Error! status: '+response.status);
		}
		const result = await response.json();
		return result;
	} catch (err) {
		console.log(err);
	}
}

function restore() {
	speed.value = "-4.7537255"
}

function save() {
	setConfig().then(data => {
		console.log(data);
		ssid.value = data.wifi_ssid;
		password.value = data.wifi_password;
		speed.value = data.speed;
	})
}

async function setConfig() {
	try {
		const response = await fetch('http://tracker.local/set?speed='+speed.value+'&ssid='+ssid.value+'&password='+password.value);
		if (!response.ok) {
			throw new Error('Error! status: '+response.status);
		}
		const result = await response.json();
		return result;
	} catch (err) {
		console.log(err);
	}
}

function update() {    
    getSetup().then(data => {
	console.log(data.wifi_ssid);
	ssid.value = data.wifi_ssid;
	password.value = data.wifi_password;
	speed.value = data.speed;
	firmwareTable.rows[0].cells[1].innerText = data.version;
	console.log("Version: "+data.version);
	})
}

update();

async function getSetup() {
    try {
	const response = await fetch('http://tracker.local/setup');
	if (!response.ok) {
	    throw new Error('Error! status: '+response.status);
	}
	const result = await response.json();
	return result;
    } catch (err) {
	console.log(err);
    }
}

