var check=false;
var status="STOPPED";
let finishat = document.querySelector('.finish');
let stat = document.querySelector('.status');
let pic_stat = document.querySelector('.pictures');
// let shutter = document.querySelector('.shutter');
// let pics = document.querySelector('.pics');
// let current = document.querySelector('.current');
// let secs = document.querySelector('.secs');

document.getElementById("start").addEventListener("click", function(){ console.log('starting...'); setTimeout(update,1000); });
document.getElementById("exptime").addEventListener("click", function(){ console.log('exptime...'); update(); });
document.getElementById("waittime").addEventListener("click", function(){ console.log('waittime...'); update(); });
document.getElementById("reps").addEventListener("click", function(){ console.log('reps...'); update(); });
// document.getElementById("reps").addEventListener("click", function(){ console.log('reps...'); update(); });

document.getElementById("speedButton").addEventListener("click", function(){ console.log('set speed...'); updateSpeed(); });

document.getElementById("start").addEventListener("click", function(){ console.log('start...'); start(); });
document.getElementById("stop").addEventListener("click", function(){ console.log('stop...'); stop(); });

async function start() {
    let exptime = document.getElementById("exptime").value;
    let waittime = document.getElementById("waittime").value;
    let reps = document.getElementById("reps").value;
    try {
	const response = await fetch('http://tracker.local/start?exptime='+exptime+'&cooloff='+waittime+'&numPics='+reps);
        if (!response.ok) {
            throw new Error('Error! status: '+response.status);
        }
        const result = await response.json();
        return result;
    } catch (err) {
        console.log(err);
    }
}

async function stop() {
    try {
	const response = await fetch('http://tracker.local/stop');
        if (!response.ok) {
            throw new Error('Error! status: '+response.status);
        }
        const result = await response.json();
        return result;
    } catch (err) {
        console.log(err);
    }
}


function updateSpeed() {
    setSpeed().then(data => {
	console.log(data.speed);
	document.getElementById("speed").setAttribute('value',data.speed);
	document.getElementById("speed").value = data.speed;
    });
}

async function getSpeed() {
    try {
        const response = await fetch('http://tracker.local/speed');
        if (!response.ok) {
            throw new Error('Error! status: '+response.status);
        }
        const result = await response.json();
        return result;
    } catch (err) {
        console.log(err);
    }
}


async function setSpeed() {
    let speed = document.getElementById("speed").value;
    try {
	const response = await fetch('http://tracker.local/speed?speed='+speed);
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
    let bar = document.getElementById("Bar");
    let pictureBar = document.getElementById("PictureBar");
    let tracking = document.getElementById("tracking");
    
/*    getSpeed().then(data => {
	document.getElementById("speed").value = data.speed;

	if ( data.tracking == 1 ) {
	    //	    tracking.style.color = 'green';
	    console.log('Tracking...');
	    tracking.style.backgroundColor = 'green';
	    tracking.textContent = 'Running';
	    
	} else {
//	    tracking.style.color = 'red';
	    console.log('NOT Tracking...');
	    tracking.style.backgroundColor = 'red';
	    tracking.textContent = 'Stopped';
	} 
    });*/
    
    getStatus().then(data => {
	console.log(data.status);
	stat.textContent = 'Status: '+ data.status;
	status = data.status;
	if ( data.status == "STOPPED" ) {
	    check=false;
	    console.log('Disable further updates');

	} else {
	    check=true;
	    setTimeout(update, 1000);
	}
        document.getElementById("speed").value = data.speed;
	if ( data.tracking == 1 ) {
            console.log('Tracking...');
            tracking.style.backgroundColor = 'green';
            tracking.textContent = 'Running';

        } else {
            console.log('NOT Tracking...');
            tracking.style.backgroundColor = 'red';
            tracking.textContent = 'Stopped';
        }

	
//	shutter.textContent = 'Shutter: ' + data.shutter;
//	pics.textContent = 'Pictures: ' + data.completed + '/' + data.pics;
//	current.textContent = 'Current: ' + data.current;
	//	secs.textContent = 'Seconds: ' + data.seconds + '/' + data.exptime;

	bar.style.width = (Number(data.completed)/Number(data.pics)*100)+"%";
	pictureBar.style.width = (Number(data.seconds)/(Number(data.exptime)+Number(data.waittime))*100)+"%";

	if ( status == "RUNNING" ) {
            let start = new Date();
	    let finish = new Date(start.getTime() + (((Number(data.exptime)+Number(data.waittime))*(Number(data.pics)-Number(data.completed))-Number(data.waittime) + Number(data.exptime) - Number(data.seconds))*1000));
	    const hours = finish.getHours().toString().padStart(2,'0');
	    const minutes = finish.getMinutes().toString().padStart(2,'0');
	    const seconds = finish.getSeconds().toString().padStart(2,'0');
	    finishat.textContent = 'Finish at: ' + hours + ':' + minutes + ':' + seconds;
	    document.getElementById("exptime").value = data.exptime;
	    document.getElementById("waittime").value = data.waittime;
	    document.getElementById("reps").value = data.pics;
	    pic_stat.textContent = 'Pictures: ' + (Number(data.completed)+1).toString() + '/' + data.pics;
	};
    });
    if (status != "RUNNING") {
	let exptime = document.getElementById("exptime").value;
	let waittime = document.getElementById("waittime").value;
	let reps = document.getElementById("reps").value;
	let start = new Date();
	let finish = new Date(start.getTime() + (((Number(exptime)+Number(waittime))*Number(reps))-Number(waittime))*1000);
	const hours = finish.getHours().toString().padStart(2,'0');
	const minutes = finish.getMinutes().toString().padStart(2,'0');
	const seconds = finish.getSeconds().toString().padStart(2,'0');
	finishat.textContent = 'Finish at: ' + hours + ':' + minutes + ':' + seconds;
    }
}

update();

async function getStatus() {
    try {
	const response = await fetch('http://tracker.local/status');
	if (!response.ok) {
	    throw new Error('Error! status: '+response.status);
	}
	const result = await response.json();
	return result;
    } catch (err) {
	console.log(err);
    }
}

