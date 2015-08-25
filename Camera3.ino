/*File: camera3.ino
  Name: Shruti Misra
  Date: 08/25/2015
  Description: This code programs the Photon side of the camera sensor project. 
               It sets up and initializaes the camera and its functions.
               Creates a server. Takes and input from the motion sensor. 
               If the input is HIGH, sends camera a command to freeze frame.
               Checks if a client is connected. Reads image from the camera and 
               writes it to the client. 
               This code has been derived from the Adafruit TTL Arduino library and the 
               Wifi Secutiry Camera project on particle.io
  */
//Define camera functions

#define VC0706_RESET  0x26
#define VC0706_GEN_VERSION 0x11
#define VC0706_SET_PORT 0x24
#define VC0706_READ_FBUF 0x32
#define VC0706_GET_FBUF_LEN 0x34
#define VC0706_FBUF_CTRL 0x36
#define VC0706_DOWNSIZE_CTRL 0x54
#define VC0706_DOWNSIZE_STATUS 0x55
#define VC0706_READ_DATA 0x30
#define VC0706_WRITE_DATA 0x31
#define VC0706_COMM_MOTION_CTRL 0x37
#define VC0706_COMM_MOTION_STATUS 0x38
#define VC0706_COMM_MOTION_DETECTED 0x39
#define VC0706_MOTION_CTRL 0x42
#define VC0706_MOTION_STATUS 0x43
#define VC0706_TVOUT_CTRL 0x44
#define VC0706_OSD_ADD_CHAR 0x45

#define VC0706_STOPCURRENTFRAME 0x0
#define VC0706_STOPNEXTFRAME 0x1
#define VC0706_RESUMEFRAME 0x3
#define VC0706_STEPFRAME 0x2

#define VC0706_640x480 0x00
#define VC0706_320x240 0x11
#define VC0706_160x120 0x22

#define VC0706_MOTIONCONTROL 0x0
#define VC0706_UARTMOTION 0x01
#define VC0706_ACTIVATEMOTION 0x01

#define VC0706_SET_ZOOM 0x52
#define VC0706_GET_ZOOM 0x53
#define CAMERABUFFSIZ 100
#define CAMERADELAY 10
// serial number
uint8_t  serialNum;
//Stores data recieved from the camera in this buffer
uint8_t  camerabuff[CAMERABUFFSIZ+1];
//Stores buffer length
uint8_t  bufferLen;
//Stores the position of the frame pointer
uint16_t frameptr;

//Store IP Address
char myIpAddress[24];

//TCP information
//Initialize Clients
TCPClient jpgClient;
TCPClient checkClient;
//Initiatlize server at port 3443
TCPServer jpgServer = TCPServer(3443);
//PIR sensor
int PIR = D0;
//Initialize test LEDS
int green = D3;
int red = D4;


//Sets these settings at startup

void setup() 

{
  
  //Initialize green and red LEDs as Outputs
  pinMode(green, OUTPUT);
  pinMode(red, OUTPUT);
  //Initialize PIR as input
  pinMode(PIR, INPUT);
  //Begin communication with USB port- Debugging
  Serial.begin(9600);
  Serial.println("VC0706 Camera snapshot test");
    // Initialize camera
      common_init();
    // Begin camera- Check if available
     if (camBegin(38400)) {
      Serial.println("Camera Found:");
    } else {
      Serial.println("No camera found?");
      return;
    }
    //Set camera image size
     camSetImageSize(VC0706_640x480);  
    //Send variable to Spark cloud
    Spark.variable("ipAddress", myIpAddress, STRING);
    //Get local Wifi IP adrdress
    IPAddress myIp = WiFi.localIP();
   //attach myIP to myIpAddress 
    sprintf(myIpAddress, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);
    
    // begin Photon server
    jpgServer.begin();


}

void loop()
{
    //Check if motion is detected
    if(digitalRead(PIR) == HIGH)
    {
        //Turn on red LED to indicate motion detection
        digitalWrite(red, HIGH);
          //Freeze camera frame 
          if (!camTakePicture())
            Serial.println("Failed to snap!");
          else
            Serial.println("Picture taken!");
        //Check if client available, returns a client object if so   
        checkClient = jpgServer.available();
        if(checkClient.connected())
        {   
            digitalWrite(D4,LOW);
            //Set jpg client to the returned client object
            jpgClient = checkClient;
            //send data to the client
            sendPicture(jpgClient);
            //Resume the frame
            camRsumeVideo();
            //Disconnect from client
            jpgClient.stop();
            
        }
        
        
         
    }
    
}

/*****************CAMERA FUNCTIONS *****************************/ 

//Initialization for the camera. 
//Here the serial number is 0 because only one device is connected to the camera (See documentation)
//Frame pointer and buffer length are 0 because nothing has been read yet
void common_init(void) {
  frameptr  = 0;
  bufferLen = 0;
  serialNum = 0;
}

//Sends command to the camera
//cmd = Command being sent, args[] = data length/data being sent (See VC0706 protocol/Adafruit library)
//argn = number of elements in args/acts as a counter
void sendCommand(uint8_t cmd, uint8_t args[] = 0, uint8_t argn = 0) {
    Serial1.write((byte)0x56);       // Send protocol number indicating that it's a command being sent
    Serial1.write((byte)serialNum);  //Send serial Number
    Serial1.write((byte)cmd);       //Send command code        
    
    //For the number of elements in args (= argn), write args to Serial1
    //I.e send each element of args to camera
    for (uint8_t i=0; i<argn; i++) {
      Serial1.write((byte)args[i]);

    }

}
//Reads response from camera
//numbytes = Number of bytes to be read, timeout = set a timer 
uint8_t readResponse(uint8_t numbytes, uint8_t timeout) {
  uint8_t counter = 0;
  bufferLen = 0;
  int avail;
  
  //While we have not run out of time and the buffer lenght is not upto the 
  //required length do...
  while ((timeout != counter) && (bufferLen != numbytes)){
    avail = Serial1.available();
    //Check if there is anyy remaining data in the buffer
    if (avail <= 0) {
      //delay till all data is flushed out
      delay(1);
      //increment counter
      counter++;
      continue;
    }
    counter = 0;
    // Read data from the Serial buffer and store it in the camerabuff global variable
    //Store the data in each element of the array corresponding to the buffer length
    //Increment buffLen, so as to go to the next element in the camerabuff array
    camerabuff[bufferLen++] = Serial1.read();
  }
  
  //Return the buffer length now that the data has been read
  return bufferLen;
}
//Check if the response from the camera is valid
//The first four elements should correspond to the appropriate
//Protocol number, serial no., command code and status code. 
//Return true if all the conditions are fulfilled/verified
boolean verifyResponse(uint8_t command) {
  if ((camerabuff[0] != 0x76) ||
      (camerabuff[1] != serialNum) ||
      (camerabuff[2] != command) ||
      (camerabuff[3] != 0x0))
      return false;
  return true;

}

//cmd, args, argn the same as send command
//Resplen = length of response from camera
//Flushflag true means flush remaining data from serial buffer
//Sends a command to the camera and verifies response
//Returns true if all the conditions have been fulfiled 
boolean runCommand(uint8_t cmd, uint8_t *args, uint8_t argn,
               uint8_t resplen, boolean flushflag) {
  // flush out anything in the buffer?
  if (flushflag) {
    readResponse(100, 10);
  }
  //Send command to the camera
  sendCommand(cmd, args, argn);
  //If the resplen is not equal to the appropriate/required length
  //then false
  if (readResponse(resplen, 200) != resplen)
    return false;
  //If the response is not valid, then false
  if (! verifyResponse(cmd))
    return false;
  return true;
}


//---------- Camera Functions

//Reset camera to default functions
//Return true if runCommand function is true
boolean camReset() {
  uint8_t args[] = {0x0};

  return runCommand(VC0706_RESET, args, 1, 5, true);
}

//Begin Serial communication with camera at the specified baud rate
//Reset camera after setting up communication
boolean camBegin(uint16_t baud) {
  Serial1.begin(baud);
  return camReset();
}
//Get the current image size
//args set by variables in the Adafruit library
//If everything goes well, i.e runCommand is true, then return the
//data about the image size stored in camerabuff
uint8_t camGetImageSize() {
  uint8_t args[] = {0x4, 0x4, 0x1, 0x00, 0x19};
  if (! runCommand(VC0706_READ_DATA, args, sizeof(args), 6, true))
    return -1;

  return camerabuff[5];
}

//Set the image size
//x = image size of choice
//args set by the VC0706 protocol/Adafruit library 
//return tru if runCommand is true which means everything went well
boolean camSetImageSize(uint8_t x) {
  uint8_t args[] = {0x05, 0x04, 0x01, 0x00, 0x19, x};

  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5, true);
}

//Control the frame buffer
//command = 0 for freeze frame and 3 for resume frame
//Return true if runCommand is true
boolean cameraFrameBuffCtrl(uint8_t command) {
  uint8_t args[] = {0x1, command};
  return runCommand(VC0706_FBUF_CTRL, args, sizeof(args), 5, true);
}

//Freeze frame
//Use the cameraFrameBuffCtrl function to freeze frame
//Returns what the above function returns
boolean camTakePicture() {
  frameptr = 0;
  return cameraFrameBuffCtrl(VC0706_STOPCURRENTFRAME);
}

//Resumt frame by calling the cameraFrameBuffCtrl function
boolean camRsumeVideo() {
  return cameraFrameBuffCtrl(VC0706_RESUMEFRAME);
}

//Get the frame length
//args set by the VC0706 protocol
//return 0 if things go wrong, i.e runCommand is false
//Returns the length if all goes well
uint32_t camFrameLength(void) {
  uint8_t args[] = {0x01, 0x00};
  if (!runCommand(VC0706_GET_FBUF_LEN, args, sizeof(args), 9, true))
    return 0;
  //Define a variable to store frame length, 32 bit variable
  uint32_t len;
  //Frame length is stores in elements 5,6,7 and 8 of the camerabuff array
  //Need to bit shitft by 8 everytime the next element is stored in len
  //This will result in the whole length data being stored in len
  len = camerabuff[5];
  len <<= 8;
  len |= camerabuff[6];
  len <<= 8;
  len |= camerabuff[7];
  len <<= 8;
  len |= camerabuff[8];
  
  //return buffer length
  return len;
}

//Check if the camera is available by checking the buffer length
uint8_t camAvailable(void) {
  return bufferLen;
}

//Read the image data 
//args set by VC0706 protocol/Adafruit Library
//frameptr determines which chunk of buffer memory is being read from 
//n determines the number of bytes being read
uint8_t * camReadPicture(uint8_t n) {
  uint8_t args[] = {0x0C, 0x0, 0x0A,
                    0, 0, frameptr >> 8, frameptr & 0xFF,
                    0, 0, 0, n,
                    CAMERADELAY >> 8, CAMERADELAY & 0xFF};
   //Return nothing if things go wrong
  if (! runCommand(VC0706_READ_FBUF, args, sizeof(args), 5, false))
    return 0;


  // if the response length is 0, return false
  if (readResponse(n+5, CAMERADELAY) == 0)
      return 0;

  //Increment frame pointer by the number of bytes read so that
  //when the function is called again, it reads starting from the memory address
  //it last stopped at
  frameptr += n;
  
  //Return the image data
  return camerabuff;
}

//Send picture to the client in chunks
//Takes in the client as a parameter
void sendPicture(TCPClient socket)
{
  

  // Get the size of the image (frame) taken
  uint16_t jpglen = camFrameLength();
   byte wCount = 0; // For counting # of writes
    while (jpglen > 0) {
      //Turn green LED on indicating start of data transfer
      digitalWrite(green, HIGH);
      // read 32 bytes at a time;
      //define the buffer variable to store image data
      uint8_t *buffer;
      uint8_t bytesToRead = min(64, jpglen); // change 32 to 64 for a speedup but may not work with all setups!
      //Store data in the buffer variable
      buffer = camReadPicture(bytesToRead);
      Serial.println("Buffer data");
      //Write to client
      socket.write(buffer, bytesToRead);
      //Subtract what was just read from the total image length
      jpglen -= bytesToRead;
    }
    //Turn off green LED to indicate completion of data transfer
    digitalWrite(D3, LOW);
  
  
}

