"use strict";

// Dependencies

var awsIot = require('aws-iot-device-sdk');
var CronJob = require('cron').CronJob;
var request = require('request');
var SerialPort = require('serialport');
var winston = require('winston');
const Readline = require('@serialport/parser-readline');

// Device Data

// format is [{"hour": 12, "minute": 40, "duration": 20}]; //starttime mins
var pumpData = [];
var pumpCrons = [];
var dataCron = null;
var sendDataInterval; // in hours 

// Logs

const logger = winston.createLogger({
  levels: {
    pump: 0,
    sentData: 1,
    pumpCron: 2,
    awsError: 3,
    sentCron: 4,
    recvData: 5
  },
  format: winston.format.json(),
  defaultMeta: { service: 'sp-service' },
  transports: [
    // - Write to all logs with level `info` and below to `combined.log` 
    new winston.transports.File({ filename: 'combined.log', level: 'recvData' })
  ]
});

// Topics
 var receiveTopic = 'topic_1';
 var sendTopic = 'topic_2';

// AWS device data
var URL_SERVICE = 'https://qtscqc0ln4.execute-api.us-east-1.amazonaws.com/dev/device/param/';
var device = awsIot.device({
      keyPath: "./certs/testYun.private.key",
      certPath: "./certs/testYun.cert.pem",
      caPath: "./certs/root-CA.crt",
      clientId: "basicPubSub",
      host: "a199i8mfkofeen-ats.iot.us-east-1.amazonaws.com"
 });


// Serial port over USB connection between the Raspberry Pi and the Arduino
var arduinoSerialPort = '/dev/ttyACM0';	
// Setting data to SerialPort library
const port = new SerialPort(arduinoSerialPort, { baudRate: 9600 });
const parser = new Readline();
port.pipe(parser); 

// Start data
defaultParams() // set Params by default
createCronData(); // create cron for send data to aws (see interval param) with default params
createCronPump(); // create cron for pump with default params

//
// Device is an instance returned by mqtt.Client(), see mqtt.js for full
// documentation.
//
device
  .on('connect', function() {
    console.log('connect');
    device.subscribe(receiveTopic);
    updatePumpCrons(); // when internet up, update crons for pump
    updateSampleInterval(); // // when internet up, update sampleInterval
    
    
    parser
      .on('data', function (data) {
        //When a new line of text is received from Arduino over Serial
        try {
            console.log(data); 
            var toSend = JSON.stringify(JSON.parse(data)); // validate received JSON
            //Forward the Arduino information to AWS IoT
            device.publish(sendTopic, toSend);
            logger.log('sentData', {"date": new Date(), "data": toSend}); // write to log

        }
        catch (err) {
            console.warn(err);
            logger.log('awsError', {"date": new Date(), "data": String(err)}); // write to log
        }
    });

});

device
  .on('message', function(topic, payload) {
    console.log('Recv message on ', topic, ': ', payload.toString());
    var payloadJson = JSON.parse(payload);
    // If message is 'updPumpSched' then update crons for pump
    if(payloadJson.message == 'updPumpSched') {
      updatePumpCrons();
    };
    if(payloadJson.message == 'updSampleInterval') {
      updateSampleInterval();
    };
    if(payloadJson.message == 'resetArduino') {
      resetArduino();
    };
    logger.log('recvData', {"date": new Date(), "message": payloadJson});
    port.write(payloadJson.message + '\n'); // Send data to Arduino over Serial
});

device
  .on('close', function() {
   console.log('close');
});

device
  .on('reconnect', function() {
   console.log('reconnect');
});

device
  .on('offline', function() {
    console.log('offline');
    logger.log('awsError', {"date": new Date(), "data": 'Offline'});
});

device
  .on('error', function(error) {
   console.log('error', error);
   logger.log('awsError', {"date": new Date(), "data": String(error)});
});

// Sets pumpData by defaults, so if not internet it runs by default
function defaultParams() {
  sendDataInterval = 6; // each 6 hours
  pumpData = [
      {
        "duration": 60,
        "hour": 23,
        "minute": 0
      },
      {
        "duration": 60,
        "hour": 12,
        "minute": 10
      }
    ];
}

// Get cronsDataPump from Api and create crons
function updatePumpCrons() {
  pumpData = []; // clear array
  const url = URL_SERVICE + 'get?topic=' + receiveTopic + '&param=pumpSched';
  request(url, function (error, response, body) {
    if (!error && response.statusCode == 200) {
      var recvData = JSON.parse(body);
      pumpData = recvData[0].data
      logger.log('pump', {"date": new Date(), "data": pumpData }); // write to log
      clearCronPump(); // clear cronsPump
      createCronPump(); // call cronPump creation
    }
  });
}

// Create crons for Pump from pumpData array
function createCronPump() {
  // Create array of crons
  pumpData.forEach((job, i) => { // for each cron in pumpData
    var time = pumpData[i].minute + ' ' + pumpData[i].hour + ' * * *';
    // Create a new CronJob
    pumpCrons[i] = new CronJob(time, () => {
      console.log('Running Job: ' + JSON.stringify(job));
      var ardComm = 'm ' + pumpData[i].duration + '\n'; // command to send to Arduino
      port.write(ardComm); // Send data to Arduino over Serial
      logger.log('pumpCron', {"date": new Date(), "data": ardComm }); // write to log
    });
    pumpCrons[i].start();
  });
}

// Clear and Stop all Pump Cron jobs
function clearCronPump() {
  pumpCrons.forEach((job,i) => {
    pumpCrons[i].stop();
  });
  pumpCrons = [];
}

// Get data (ph,temp,orp) from Arduino stoppig Pump and send to Aws
function getDataFromArduino() {
  var ardComm = 'g' + '\n'; // command to send to Arduino
  port.write(ardComm); // Send data to Arduino over Serial
}

// Get sendDataInterval (SampleInterval) from Api
function updateSampleInterval() {
  const url = URL_SERVICE + 'get?topic=' + receiveTopic + '&param=sampleInterval';
  request(url, function (error, response, body) {
    if (!error && response.statusCode == 200) {
      var recvData = JSON.parse(body);
      sendDataInterval = recvData[0].data; // update data
      logger.log('sentCron', {"date": new Date(), "data": 'SampleInterval updated: '+ String(sendDataInterval) }); // write to log
      // if cron exists, then delete
      if (dataCron !== null) { 
        dataCron.stop();
        dataCron = null;
      }
      createCronData(); // call cronData creation
    }
  });
}

// Create cron for get data (ph,temp,orp) from Arduino and send to Aws
function createCronData() {
  var time = '0 */'+ sendDataInterval +' * * *';
  // Create a new CronJob
  dataCron = new CronJob(time, () => {
    getDataFromArduino();
    logger.log('sentCronLog', {"date": new Date(), "message":  "Running Cron to get Data" }); // write to log
  });
}

// Reset Arduino
function resetArduino() {
  var ardComm = 'r' + '\n'; // command to send to Arduino
  port.write(ardComm); // Send data to Arduino over Serial
}
