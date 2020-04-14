/////////////////Includes//////////////////////
#include <Servo.h>
#include <Adafruit_Fingerprint.h>
//////////////// bss variabled ////////////////
Servo servo;
SoftwareSerial fp_sensor(2, 3);
SoftwareSerial uartstm32_control(12,13);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fp_sensor);
int8_t sval;
int btn_selectorval;
int option;
uint8_t id;
bool sensor_present;
char uartret;

void setup() {
  //sensor presence asume not existance
  sensor_present=false;
  //initialize hw serial tx/rx//
  Serial.begin(9600);
  delay(100);
  uartstm32_control.begin(9600);
  delay(100);
  uartret=0;
  Serial.println("\nMCU Biometric Access\n ");
  //set br for finger print sensor
  finger.begin(57600);
  //Set our digital inputs/outputs
  pinMode(10,OUTPUT); // DBG Led
  pinMode(9,OUTPUT); // BLUE LED
  pinMode(8,OUTPUT); // RED LED
  pinMode(7,OUTPUT); // GREEN LED  
  pinMode(6,INPUT);   //DPSW 1  
  servo.attach(5);    //SERVO  
  btn_selectorval=0;
  option=0;
  sval=-2;
  //look for fp sensor
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor attached!");
    sensor_present=true;
    finger.getTemplateCount();
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    sensor_present=false;
   }
  // tell how many finger print templates does the sensor memory has
   Serial.print("Sensor contains "); 
   Serial.print(finger.templateCount); 
   Serial.println(" templates");

  // close the servo lock 
  servo.write(0);
  digitalWrite(10,0);
}

void loop() {
  
  if( sensor_present == true ){
      if(digitalRead(6)==HIGH){
        // do the enroll routine
        digitalWrite(7,1);
        delay(1000);
        digitalWrite(7,0);
        delay(1000);
        enroll();        
      }else {
        // do the fp acces and blink to let know we are waiting for finger
        digitalWrite(9,1);
        delay(100);
        digitalWrite(9,0);
        delay(100);
        sval=getFingerprintIDez(); 
        delay(100);  // give a time of refresh
        uartret= uartstm32_control.read();
        Serial.println(uartret);
   
        if(uartret == 89 )
        {
          // open our lock
          servo.write(180);
          delay(2000);
          servo.write(0);
          digitalWrite(10,1);
          fp_sensor.listen();
        } else if ( (uartret ==  78) || (sval == -1))
        {
          servo.write(0);
          digitalWrite(8,1);
          delay(350);
          digitalWrite(8,0);
          digitalWrite(10,0);
          
        } else
        {
          digitalWrite(8,0);
        }
      }
     } else {
      Serial.println("Error fingerprint sensor not detected ...");
      digitalWrite(8,1);
      delay(1000);
      digitalWrite(8,0);
      delay(1000);  
  }
}

//////////// function definition ////////////////

uint8_t readnumber(void) {
  uint8_t num = 0;
  
  while (num == 0) {
    while (! Serial.available());
    num = Serial.parseInt();
  }
  return num;
}

void enroll ()
{
  Serial.println("Ready to enroll a fingerprint!");
  Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
  id = readnumber();
  if (id == 0) {// ID #0 not allowed, try again!
     return;
  }
  Serial.print("Enrolling ID #");
  Serial.println(id);
  
  //while (!  getFingerprintEnroll() );
  getFingerprintEnroll();
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }
  
  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }
  
  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);
  
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }   
  
  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }   
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }
  
  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }   
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" with confidence of "); Serial.println(finger.confidence); 

  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p_gfimage = finger.getImage();
  uint8_t p_image2tz = finger.image2Tz();
  uint8_t p_ffastsearhc = finger.fingerFastSearch();

  if( (p_gfimage != FINGERPRINT_OK) || ( p_image2tz  != FINGERPRINT_OK) || (p_ffastsearhc != FINGERPRINT_OK) )
  {    
    uartstm32_control.write(68);       
    return -1;
  } else {
    // found a match!
    Serial.print("Found ID #"); Serial.print(finger.fingerID);
    digitalWrite(7,1);
    delay(250);
    digitalWrite(7,0);
    //send data to serial stm32
    uartstm32_control.write(65);
    // enable the sw port to listen
    uartstm32_control.listen();
    delay(150);
   }
  return finger.fingerID; 
}
