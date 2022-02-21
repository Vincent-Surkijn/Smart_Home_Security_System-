const BTSerialPort = require("bluetooth-serial-port");
const btSerial = new BTSerialPort.BluetoothSerialPort();
const fs = require("fs");

const express = require("express");
const bodyParser = require("body-parser");
const path = require("path");
const ip = require("ip");

const certPath = path.join(__dirname, './preferences_history/last_preferences.json');

const errFunction = (err) => {
  if (err) {
    console.log("Error", err);
  }
};

var currentTemperature = 0;
var currentLight = 0;
var writeValues = false;
var doorBtn = null;
var alarmBtn = null;
var valueTurnLightsOnOrNot = null;
var valueTurnACOnOrNot = null;
var datetimeAlarm = null;
var setAutoAlarm = false;
var minimumNoiseValue = 0.05;
var triggerAlarmNoise = false;
var preferences;

// New app using express module
const app = express();
app.use(bodyParser.urlencoded({ extended: true }));

function bootstrap () {
  preferences = JSON.parse(fs.readFileSync(certPath));
  doorBtn = preferences.doorBtn;
  alarmBtn = preferences.alarmBtn,
  valueTurnLightsOnOrNot = preferences.valueTurnLightsOnOrNot;
  valueTurnACOnOrNot = preferences.valueTurnACOnOrNot;
  datetimeAlarm = preferences.datetimeAlarm;
	writeValues=true;
}

// only for injection purposes
app.get("/style", function(req, res) {
  res.sendFile(__dirname + "/style.css");
});
app.get("/background", function(req, res) {
  res.sendFile(__dirname + "/files/background.png");
});
app.get("/mic", function(req, res) {
  res.sendFile(__dirname + "/files/microphone.jpg");
});
app.get("/libraries/p5.speech.js", function(req, res) {
  res.sendFile(__dirname + "/libraries/p5.speech.js");
});
app.get("/audio.js", function(req, res) {
  res.sendFile(__dirname + "/audio.js");
});

app.get("/", function (req, res) {
  res.sendFile(__dirname + "/index.html");
});

app.get("/datetimeAlarm", function (req, res) {
  res.send({datetimeAlarm: datetimeAlarm});
});

app.get("/setAutoAlarm", function (req, res) {
  console.log("Auto alarm enabled");
  setAutoAlarm = true;
  res.send('OK');
  datetimeAlarm = "2100-01-01T23:15:15.999Z"; 
  alarmBtn='yes';
});

app.get("/smoothie.js", function (req, res) {
  res.sendFile(__dirname + "/smoothie.js");
});

app.get("/sync", function (req, res) {
  let mLevel = parseFloat(req.query.micLevel).toFixed(2);
  if(mLevel > minimumNoiseValue) {
      console.log("ALARM ON due to noise! " + mLevel + " is above the minimum value: " + minimumNoiseValue);
      triggerAlarmNoise = true;
  }

  res.send({ 
    temp: currentTemperature, 
    light: currentLight, 
    preferences_doorBtn: doorBtn,
    preferences_alarmBtn: alarmBtn,
    preferences_valueTurnLightsOnOrNot: valueTurnLightsOnOrNot,
    preferences_valueTurnACOnOrNot: valueTurnACOnOrNot,
    preferences_datetimeAlarm: datetimeAlarm
  });
});

app.post("/", function (req, res) {
  req.body.doorBtn == undefined ? doorBtn = 'no' : doorBtn = req.body.doorBtn;
  req.body.alarmBtn == undefined ? alarmBtn = 'no' : alarmBtn = req.body.alarmBtn;
  valueTurnLightsOnOrNot = req.body.valueTurnLightsOnOrNot;
  valueTurnACOnOrNot = req.body.valueTurnACOnOrNot;
  datetimeAlarm = req.body.datetimeAlarm;

  writeValues = true;
  res.sendFile(__dirname + "/index.html");

});

setInterval(function () {
  var jsonData = {
    date: new Date().toISOString(),
    doorBtn: doorBtn,
    alarmBtn: alarmBtn,
    valueTurnLightsOnOrNot: valueTurnLightsOnOrNot,
    valueTurnACOnOrNot: valueTurnACOnOrNot,
    datetimeAlarm: datetimeAlarm
  };
  
  var toAppend = JSON.stringify(jsonData);

  fs.open(
    path.join(__dirname, "./preferences_history/last_preferences.json"),
    "r",
    function (err, fd) {
        fs.writeFile(
          path.join(__dirname, "./preferences_history/last_preferences.json"),
          toAppend,
          function (err) {
            err ? console.log(err): console.log("Preferences json was saved!");
          }
        );
    }
  );

  fs.open(path.join(__dirname, "./preferences_history/history_log.txt"), "r", function (err, fd) {
    if (err) {
      fs.writeFile(
        path.join(__dirname, "./preferences_history/history_log.txt"),
        toAppend + '\n',
        function (err) {
          err ? console.log(err): console.log("History log was saved!");
        }
      );
    } else {
      fs.appendFile(
        path.join(__dirname, "./preferences_history/history_log.txt"),
        toAppend + '\n',
        function (err) {
          err ? console.log(err): console.log("History log was saved!");
        }
      );
    }
  });
}, 60000);

//---------------------Test-----------------------//
/*
var receivedData = "Temperature:12\n";
setInterval(function () {
  //if(receivedData.includes("Temperature")) {
  currentTemperature = Math.random() * 1000;
  //} else if(receivedData.includes("Light")) {
  currentLight = Math.random() * 1000;
  //} else {
  //console.log("ups");
  //}
}, 1000);
*/
//-----------------Real deal----------------------//

//btSerial.on('found', function(address, name) {
 //   console.log("address: " + address + " and name: " + name);
  //  if(name.toLowerCase().includes('aaaa')){
      // STATIC ARDUINO MAC ADDRESS
      let address="98:D3:51:FD:EC:02";
      btSerial.findSerialPortChannel(address, function(channel) {
        btSerial.connect(address, channel, function() {
	  let readInput='';
          btSerial.on('data', function(bufferData) {

            var receivedData = Buffer.from(bufferData).toString();
	    readInput = readInput + '' + receivedData;
            if (receivedData.includes('\n')) {
		if (readInput != '\n') {
            		console.log("-----: " + readInput);
	   		let auxSplit = readInput.split(":");
			let setting = auxSplit[0];
			if (auxSplit[1] == undefined) {
				console.log("ERROR: Undefined string " + auxSplit);
			}
			let value = auxSplit[1].split('\n')[0];
            		if(setting == "Temperature") {
            		  currentTemperature = value;
			            console.log("Receiving: Current Temperature = " + currentTemperature + "ºC");
            		} else if(setting == "Light") {
            		  currentLight = value; 
			            console.log("Receiving: Current Light = " + currentLight);
            		} else if(setting == "Alarm") {
				if (value.includes("\r")){
					value = value.split("\r")[0];
				}
                    alarmBtn = value;
			            console.log("Receiving: Current Alarm = " + alarmBtn);
                } else if(setting == "Door") {
			if (value.includes("\r")){
				value = value.split("\r")[0];
			}
                    doorBtn = value;
			            console.log("Receiving: Current Door = " + doorBtn);
                } else if (setting == "SetTemp") {
			if (value.includes("\r")){
				value = value.split("\r")[0];
			}
			valueTurnACOnOrNot = value;
			            console.log("Receiving: Minimum temperature value = " + valueTurnACOnOrNot);
		} else if (setting == "SetLight" ) {
			if (value.includes("\r")){
				value = value.split("\r")[0];
			}	
			valueTurnLightsOnOrNot = value;
			            console.log("Receiving: Minimum light value = " + valueTurnLightsOnOrNot);
		} else { 
                  console.log("Receiving: Invalid input string= " + receivedData);
                }
                
            		if(writeValues){
            		  console.log("WRITING VALUES BACK TO VINCENT: --------------------------------------");
				console.log("Values:" + doorBtn + ";"+ alarmBtn + ";"+ valueTurnLightsOnOrNot + ";"+ valueTurnACOnOrNot + ";\n");
			  btSerial.write(Buffer.from("Values:" + doorBtn + ";"+ alarmBtn + ";"+ valueTurnLightsOnOrNot + ";"+ valueTurnACOnOrNot + ";\n"), errFunction);
            		  writeValues = false;
            		}
            		if(setAutoAlarm){
			  console.log("Sending: AutoAlarm enabled");
            		  btSerial.write(Buffer.from("AutoAlarm;\n"), errFunction);
            		  setAutoAlarm = false;
            		}
            		if(triggerAlarmNoise){
			  console.log("Sending: Intruder Alert");
            		  btSerial.write(Buffer.from("IntruderAlert;\n"), errFunction);
            		  triggerAlarmNoise = false;
           		}
		}
		readInput='';
	}
          });  
        }, errFunction);
      },errFunction);
    //}else{
   //   console.log('Not connecting to: ', name);
   // }
//});

// Starts looking for Bluetooth devices and calls the function btSerial.on('found'
//btSerial.inquire();

bootstrap();

app.listen(3000, function () {
  console.log("server is running on port 3000");
});
