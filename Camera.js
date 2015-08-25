/*File: Camera.js
  Name: Shruti Misra
  Date: 08/25/2015
  Description: This code is used to set up a client to connect to the server of 
               the Photon. It uses Node js and its "fs" and "net" modules.
               The script opens a jpg file and sets up an event listener for the client
               to listen for incoming data. This data is then written to the opened 
               jpg file.This script is run via the command prompt
*/

//Define IP settings, port and address

var settings = {
    ip: "192.168.1.105",
    port: 3443
};

//import fs module
var fs = require("fs");
//Open a writable file called test.jpg
var outStream = fs.createWriteStream("test.jpg");

//import net module
var net = require('net');
//Write to console
console.log("connecting...");
//Create a client with the previously defined IP settings 
//Define a callback function
client = net.connect(settings.port, settings.ip, function () {
    client.setNoDelay(true);
    //Write to the console that the client is connected
    console.log("connected");
});
    //Listen for data
    client.on("data", function (data) {
        //Upon receiving data...
        try {
            //Write to console
            console.log("GOT DATA");
            //Write to file
            outStream.write(data);
           
            console.log("got chunk of " + data.toString('hex'));
        }
        //Account for error
        catch (ex) {
            console.error("Er!" + ex);
        }
    });
    //Account for any connection error and try to reconnect after 5 seconds. 
    client.on('error', function(e) {
    if(e.code == 'ECONNREFUSED') {
        console.log('Is the server running at ' + settings.port + '?');

        client.setTimeout(4000, function() {
            client.connect(PORT, HOST, function(){
                console.log('CONNECTED TO: ' + ':' + settings.ip);
                
            });
        });

        console.log('Timeout for 5 seconds before trying port:' + settings.ip + ' again');

    }   
});
