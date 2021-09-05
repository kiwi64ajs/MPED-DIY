// MPED DIY: Basic Accessory Turnout Decoder
//
// Version: 1.0
// Author: Marko Pinteric 2021-08-30.
// Based on the work by Geoff Bunza.
// 
// This sketch requires the NmraDcc Library, which can be found and installed via the Arduino IDE Library Manager.
//
// This is a simple sketch for controlling a turnout according to NMRA recommendations.
// It uses the values per_close and per_throw CV, to control the pulse period for closing and throwing the turnout.
// It also controls two LEDs that indicate the state of the turnout.  When the state is unknown after power-up, per_blink CV controls the blinking period.

#include <NmraDcc.h>
#include "PinPulser.h"

#define DCC_PIN                 2

// You can also print other Debug Messages uncommenting the line below
#define DEBUG_MSG

// Un-Comment the line below to Enable DCC ACK for Service Mode Programming Read CV Capablilty 
#define ENABLE_DCC_ACK  15  // This is A1 on the Iowa Scaled Engineering ARD-DCCSHIELD DCC Shield

#define DEFAULT_DECODER_ADDRESS 1
#define PIN_CLOSE 3               // pin for closing turnout
#define PIN_THROW 4               // pin for throwing turnout
#define LED_CLOSE 5               // pin for closing turnout
#define LED_THROW 6               // pin for throwing turnout

  // Uncomment the line below to enable Decoder Address setting to the address of the next Turnout Command 
#define ADDR_PROG_JUMPER_PIN 7
#ifdef  ADDR_PROG_JUMPER_PIN  
typedef enum
{
  ADDR_SET_DISABLED = 0,
  ADDR_SET_DONE,
  ADDR_SET_ENABLED
} ADDR_SET_MODE;

ADDR_SET_MODE AddrSetMode = ADDR_SET_DISABLED ;        //  Keeps track of whether the programming jumper is set
#endif

uint16_t Accessory_Address;  // Command address, Program address in Programmer mode
uint16_t onMs;
uint16_t cduRechargeMs;
uint8_t per_blink;           // blink period
uint8_t state = 0;           // turnout state
uint8_t sta_blink = 0;       // blink state
uint8_t sta_time = 0;        // time state

// Structure for CV Values Table
struct CVPair
{
  uint16_t  CV;
  uint8_t   Value;
};

// CV Addresses we will be using
#define CV_ACCESSORY_DECODER_OUTPUT_PULSE_TIME 2  // CV for the Output Pulse ON ms
#define CV_ACCESSORY_DECODER_CDU_RECHARGE_TIME 3  // CV for the delay in ms to allow a CDU to recharge
#define CV_ACCESSORY_DECODER_BLINK_PERIOD      4  // CV address for blink period in cs

// Default CV Values Table
CVPair FactoryDefaultCVs [] =
{
// Command address, Program address in Programmer mode
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_DECODER_ADDRESS & 0xFF},
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, (DEFAULT_DECODER_ADDRESS >> 8) & 0x07},
  {CV_ACCESSORY_DECODER_OUTPUT_PULSE_TIME, 50},   // x 10mS for the output pulse duration
  {CV_ACCESSORY_DECODER_CDU_RECHARGE_TIME, 30},   // x 10mS for the CDU recharge delay time
  {CV_ACCESSORY_DECODER_BLINK_PERIOD, 25},
  {CV_29_CONFIG, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE},
};

NmraDcc  Dcc;
PinPulser pinPulser;

void initPinPulser()
{
  uint16_t onMs              = Dcc.getCV(CV_ACCESSORY_DECODER_OUTPUT_PULSE_TIME) * 10;
  uint16_t cduRechargeMs     = Dcc.getCV(CV_ACCESSORY_DECODER_CDU_RECHARGE_TIME) * 10;

    // Init the PinPulser with the new settings 
  pinPulser.init(onMs, cduRechargeMs, HIGH);
}

// This call-back function is called when a CV Value changes so we can update CVs we're using
void notifyCVChange( uint16_t CV, uint8_t Value)
{
#ifdef DEBUG_MSG
  Serial.print("notifyCVChange: CV: "); Serial.print(CV); Serial.print("  Val: "); Serial.println(Value);
#endif
  switch(CV)
  {
    case CV_ACCESSORY_DECODER_OUTPUT_PULSE_TIME:
      onMs = Value * 10;
      pinPulser.changeTimings(onMs, cduRechargeMs);
      break;
      
    case CV_ACCESSORY_DECODER_CDU_RECHARGE_TIME:
      cduRechargeMs = Value * 10;
      pinPulser.changeTimings(onMs, cduRechargeMs);
      break;

    case CV_ACCESSORY_DECODER_BLINK_PERIOD:
      per_blink = Value;
      break;

    case CV_ACCESSORY_DECODER_ADDRESS_LSB:
    case CV_ACCESSORY_DECODER_ADDRESS_MSB:
      Accessory_Address = Dcc.getAddr();
      break;
  }
}

uint8_t FactoryDefaultCVIndex = 0;
void notifyCVResetFactoryDefault()
{
#ifdef DEBUG_MSG
  Serial.println("Reset to Factory Defaults triggered");
#endif

  // Write 8 to CV8 to trigger a reset to Factory Defaults
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs)/sizeof(CVPair);
};

extern void notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower )
{  
  #ifdef DEBUG_MSG
  Serial.print(F("notifyDccAccTurnoutOutput: "));
  Serial.print(Addr,DEC) ;
  Serial.print(',');
  Serial.print(Direction,DEC) ;
  Serial.print(',');
  Serial.println(OutputPower, HEX) ;
  #endif
  
  if (Addr == Accessory_Address)
  {
    if (Direction==0)
    {
      digitalWrite(LED_THROW, LOW);

      pinPulser.addPin(PIN_THROW);

      state = 1;
      digitalWrite(LED_THROW, HIGH);
    }
    else
    {
      digitalWrite(LED_CLOSE, LOW);
      digitalWrite(LED_THROW, LOW);

      pinPulser.addPin(PIN_CLOSE);

      state = 2;
      digitalWrite(LED_CLOSE, HIGH);
    }
  }
}

#ifdef ADDR_PROG_JUMPER_PIN  
void notifyDccAccOutputAddrSet( uint16_t OutputAddr )
{
  #ifdef DEBUG_MSG
  Serial.print(F("notifyDccAccOutputAddrSet Output Addr: "));
  Serial.println( OutputAddr );
  #endif
  
  Accessory_Address = Dcc.getAddr();

#ifdef CV_OPS_MODE_ADDRESS_LSB
  Dcc.setCV(CV_OPS_MODE_ADDRESS_LSB,     OutputAddr & 0x00FF);
  Dcc.setCV(CV_OPS_MODE_ADDRESS_LSB + 1, (OutputAddr >> 8) & 0x00FF);
#endif
  
  AddrSetMode = ADDR_SET_DONE;
}
#endif

// This function is called by the NmraDcc library when a DCC ACK needs to be sent
// Calling this function should cause an increased 60ma current drain on the power supply for 6ms to ACK a CV Read 
#ifdef  ENABLE_DCC_ACK
void notifyCVAck(void)
{
#ifdef DEBUG_MSG
  Serial.println("notifyCVAck") ;
#endif

  // Configure the DCC CV Programing ACK pin for an output
  pinMode( ENABLE_DCC_ACK, OUTPUT );

  // Generate the DCC ACK 60mA pulse
  digitalWrite( ENABLE_DCC_ACK, HIGH );
  delay( 10 );  // The DCC Spec says 6ms but 10 makes sure... ;)
  digitalWrite( ENABLE_DCC_ACK, LOW );
}
#endif

void setup()
{
#ifdef DEBUG_MSG
  Serial.begin(115200);
  Serial.println("MPED DIY: Basic Accessory Turnout Decoder");
#endif
  
  // Setup the Pins for the close/throw solenoids
   pinMode(PIN_CLOSE, OUTPUT);
   pinMode(PIN_THROW, OUTPUT);

  // Setup the Pins for the direction LEDs
   pinMode(LED_CLOSE, OUTPUT);
   pinMode(LED_THROW, OUTPUT);

   digitalWrite(PIN_CLOSE, LOW);
   digitalWrite(PIN_THROW, LOW);
   digitalWrite(LED_CLOSE, LOW);
   digitalWrite(LED_THROW, LOW);

#ifdef ADDR_PROG_JUMPER_PIN
  // Configure the Decoder Address setting pin to jumper to GND to enter Address Setting mode to
  // set the decoder address to the address of the next DCC Accessory Decoder Command
  pinMode(ADDR_PROG_JUMPER_PIN, INPUT_PULLUP);
#endif

  // Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up 
  Dcc.pin(DCC_PIN, 0);

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 1, FLAGS_OUTPUT_ADDRESS_MODE | FLAGS_DCC_ACCESSORY_DECODER, 0);

  // Read the current CV values
  per_blink = Dcc.getCV(CV_ACCESSORY_DECODER_BLINK_PERIOD);

  Accessory_Address = Dcc.getAddr();

#ifdef DEBUG_MSG
  Serial.print("Decoder Output Address: "); Serial.println(Accessory_Address);
#endif

  // Uncomment the line below for force Reset to Factory Defaults
  notifyCVResetFactoryDefault();
}

void loop()
{
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

  pinPulser.process();

  if(state==0 and per_blink!=0)
  {
    sta_time = (millis()/(10*per_blink)) % 2 + 1;
    if(sta_blink!=sta_time)
    {
      if(sta_time==1)
      {
        digitalWrite(LED_CLOSE, HIGH);
        digitalWrite(LED_THROW, LOW);
      }
      else
      {
        digitalWrite(LED_CLOSE, LOW);
        digitalWrite(LED_THROW, HIGH);
      }
      sta_blink = sta_time;
    }
  }

  // Handle resetting CVs back to Factory Defaults
  if( FactoryDefaultCVIndex && Dcc.isSetCVReady())
  {
    FactoryDefaultCVIndex--; // Decrement first as initially it is the size of the array 
    
#ifdef DEBUG_MSG
      Serial.print("loop: Write Factory Defaults: CV: "); Serial.print(FactoryDefaultCVs[FactoryDefaultCVIndex].CV); 
      Serial.print("  Val: "); Serial.println(FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
#endif

    Dcc.setCV( FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
  }

  #ifdef ADDR_PROG_JUMPER_PIN  
    // Enable Decoder Address Setting from next received Accessory Decoder packet
  switch(AddrSetMode)
  {
  case ADDR_SET_DISABLED:
    if(digitalRead(ADDR_PROG_JUMPER_PIN) == LOW)
    {
      #ifdef DEBUG_MSG
      Serial.println(F("Enable DCC Address Set Mode"));
      #endif
      AddrSetMode = ADDR_SET_ENABLED;
      Dcc.setAccDecDCCAddrNextReceived(1);  
    }
    break;

  default:
    if(digitalRead(ADDR_PROG_JUMPER_PIN) == HIGH)
    {
      #ifdef DEBUG_MSG
      Serial.println(F("Disable DCC Address Set Mode"));
      #endif
      if(AddrSetMode == ADDR_SET_ENABLED)
        Dcc.setAccDecDCCAddrNextReceived(0);
          
      AddrSetMode = ADDR_SET_DISABLED;
    }
  }
#endif  
}
