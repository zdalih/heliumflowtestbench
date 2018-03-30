/*
 * Author: Mohamed-Ali Hached
 * 
 * Description: Code to run the on arduino UNO
 * on the helium delivery bench. 
 */


#include <Wire.h> //Include the Wire Library 

//This is the I2C address of the MCP4725 by default (A0 Pulled to GND)
//for Devices with A0 Pulled to high use 0x61
#define HEMFC_ADDR 0x60
#define AIRMFC_ADDR 0x61
#define HE_FLOW A1
#define AIR_FLOW A0

//constants for USB commands storage
const int MAX_LEN = 30;   //Maximum Length of a command
char command[MAX_LEN];    //Setup the command buffer

//MFCs constants
double HE_MFC_MAX_FLOW = 300; //SLPM
double AIR_MFC_MAX_FLOW = 10; //SLPM
double goalFlow = 0; //initial value
double goalPurity = 99.5; //%  initial vlaues

//Gas Constant
double HeliumCylinderPurity = 99.9999; //%

void setup() {
  //Begin USB communication 
  Serial.begin(9600);
  //I2S interface startup
  Wire.begin();
  
  //Set A2 and A3 as outputs to make them our GND and VCC
  //which will power the MPC4725
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  digitalWrite(A2, LOW); //set A2 To GND
  digitalWrite(A3, HIGH); //set A3 to VCC
}

void loop() {
  
  //If the following is true, we have received a command, so deal with it
  if(readUSB()){
    executeCommand();
  }

  control();
}

/*
 * Uses NFC constants to write the proper voltage
 * to each MFC to achieve desired goal.
 */
void control(){
  double goalHeliumFlow = (goalFlow*(goalPurity/100 + (100-HeliumCylinderPurity))); //since the cylinder is at 99.9999% purity, this is in SCFM
  double goalAirFlow = (goalFlow - goalHeliumFlow); //since it is purity by volume
  // 1 SCFM = 28.31 SLPM
  int heliumVoltage = 4095*(goalHeliumFlow*28.31)/HE_MFC_MAX_FLOW; //also converted to SLPM
  int airVoltage = 4095*(goalAirFlow*28.31)/AIR_MFC_MAX_FLOW; //also converted to SLPM
  //Serial.println(airVoltage);
  //Serial.println(goalAirFlow*28.31);
  writeVoltage(heliumVoltage, "HEMFC");
  writeVoltage(airVoltage, "AIRMFC");
}

 
/*
 * The following commands takes any byte that is in the serial buffer
 * to construct the command array. If the command array is completly constructed
 * (we received the carriage return) it returns true and will return
 * false otherwhise.
 */
boolean readUSB(){
  static int c = 0;                    //setup a counter
  int inByte = Serial.read();          //check if any byte is in the buffer
  if(inByte != -1){
    //depending on the value of the byte received we decide what to do
    switch(inByte){
      case(13):                        //carriage return - command is complete  
        command[c] = 0;                //make current position a terminating null byte
        c = 0;
        return true;
      case(10):                        //line feed which we ignore
        return false;
      default:
        if( c < (MAX_LEN - 1) ) {
          command[c] = inByte;        //put the received byte in the command  string array until it is full.
          c++;
          return false;
        }
          else{
            char command[MAX_LEN];
            c = 0;
            Serial.flush();
            Serial.println("ERROR COMMAND NOT VALID!");
            return false;
          }
    }
  }
  return false;                       //we only get here if inByte == -1 (nothing was received) so return false
}

/*
 * Execute the command
 */
void executeCommand(){
      
    if(((String) command).indexOf("flow") > -1){
      //assuming the command is well structured and there is a space before the double
      //we extract the desired value 
      //TODO CREATE MECHANISMS TO ENSURE COMMAND IS CORRECT
      String desiredValueString = "";
      int c = 5;
      while(true){
        if((int)command[c] == 0)
          break;
        desiredValueString += (String)command[c];
        c++;
      }
      double tempGoalFlow = atof(desiredValueString.c_str());
      if(tempGoalFlow*28.31 > (HE_MFC_MAX_FLOW+AIR_MFC_MAX_FLOW)){
        Serial.println("ERROR: Can't achieve the desired flow.");
        Serial.print("Maximum achievable flow: ");
        Serial.print((HE_MFC_MAX_FLOW + AIR_MFC_MAX_FLOW)/28.31);
        Serial.println(" SCFM");
      }
      else{
        goalFlow = tempGoalFlow;
        Serial.print("Changed desired flow to: ");
        Serial.print(goalFlow);
        Serial.println(" SCFM");
      }

    }

    if(((String) command).indexOf("purity") > -1){
      //assuming the command is well structured and there is a space before the double
      //we extract the desired value 
      //TODO CREATE MECHANISMS TO ENSURE COMMAND IS CORRECT
      String desiredValueString = "";
      int c = 7;
      while(true){
        if((int)command[c] == 0)
          break;
        desiredValueString += (String)command[c];
        c++;
      }
      double tempGoalPurity = atof(desiredValueString.c_str());
      if((goalFlow - (goalFlow*(tempGoalPurity/100 + (100-HeliumCylinderPurity))))*28.31 > AIR_MFC_MAX_FLOW){
        double minReachablePurity = 100*(goalFlow - (AIR_MFC_MAX_FLOW/28.31))/goalFlow;
        Serial.print("ERROR: Can't achieve this purity for the given goal flow of ");
        Serial.print(goalFlow);
        Serial.println(" SCFM");
        Serial.print("Minimum Reachable Purity: ");
        Serial.print(minReachablePurity);
        Serial.println("%");
      }
      else{
        goalPurity = tempGoalPurity;
        Serial.print("Changed desired purity to: ");
        Serial.print(goalPurity);
        Serial.println("% Helium");
      }

    }

    if(((String) command).indexOf("readFlow(helium)") > -1){
      Serial.print("Helium flow reading from MFC: ");
      Serial.print(readHeliumFlow());
      Serial.println(" SCFM");
    }

    if(((String) command).indexOf("readFlow(air)") > -1){
      Serial.print("Air flow reading from MFC: ");
      Serial.print(readAirFlow());
      Serial.println(" SCFM");
    }
    
    if(((String) command).indexOf("help") > -1){
      Serial.println("-> To change the total flow rate write 'flow [VALUE]' unit: SCFM, it has a maximum of 10.95 SCFM. Can't be negative");
      Serial.println("-> To change the purity of the output write 'purity [VALUE]' in percent. The maximum depends on the current setting of flow rate.");
      Serial.println("-> 'readFlow(helium)' prints the helium flow as read by the MFC in SCFM");
      Serial.println("-> 'readFlow(air)' prints the air flow as read by the MFC in SCFM");
      Serial.println("-> 'getGoalFlows' prins the air and helium flow that we are targeting for the MFCs in SCFM");
    }

    if(((String) command).indexOf("getGoalFlows") > -1){
      double goalHeliumFlow = (goalFlow*(goalPurity/100 + (100-HeliumCylinderPurity))); //since the cylinder is at 99.9999% purity, this is in SCFM
      double goalAirFlow = (goalFlow - goalHeliumFlow); //since it is purity by volume
      Serial.print("Goal Helium Flow: ");
      Serial.print(goalHeliumFlow);
      Serial.println(" SCFM");
      Serial.print("Goal Air Flow: ");
      Serial.print(goalAirFlow);
      Serial.println(" SCFM");

    }
    
}

/*
 * Write voltage to either "AIRMFC" or "HEMFC" based on the  
 * given mfcString. Voltage must be between 0V and 5V.
 */
 void writeVoltage(int voltage, String mfcString){
  if(mfcString == "HEMFC"){
    Wire.beginTransmission(HEMFC_ADDR);
    Wire.write(64);
    Wire.write(voltage/16);
    Wire.write((voltage % 16) << 4 );
    Wire.endTransmission();
  }
  if(mfcString == "AIRMFC"){
    Wire.beginTransmission(AIRMFC_ADDR);
    Wire.write(64);
    Wire.write(voltage/16);
    Wire.write((voltage % 16) << 4 );
    Wire.endTransmission();
  }
}

double readHeliumFlow(){
  return (analogRead(HE_FLOW)/4095)*(HE_MFC_MAX_FLOW/28.31);
}

double readAirFlow(){
  return (analogRead(AIR_FLOW)/4095)*(AIR_MFC_MAX_FLOW/28.31);
}


