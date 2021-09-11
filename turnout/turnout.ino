// MPED DIY: Basic Accessory Turnout Decoder
//
// Version: 1.0
// Author: Marko Pinteric 2021-08-30.
// Based on the work by Geoff Bunza.
// 
// This sketch requires the NmraDcc & elapsedMillis Libraries, which can be found and installed via the Arduino IDE Library Manager.
//
// This is a simple sketch for controlling a turnout according to NMRA recommendations.
// It uses the values per_close and per_throw CV, to control the pulse period for closing and throwing the turnout.
// It also controls two LEDs that indicate the state of the turnout.  When the state is unknown after power-up, per_blink CV controls the blinking period.

#include <NmraDcc.h>
#include <elapsedMillis.h>  // This requires the "elapsedMillis" library which is available in the Arduino Library Manager 

// Uncomment the next line to force Reset Decoder CVs to Factory Defaults
//#define RESET_CVS_TO_FACTORY_DEFAULT

#define DCC_PIN                 2
#define DEFAULT_DECODER_ADDRESS 1

#define MY_DECODER_VERSION  1

  // This section define constants for the ATTiny85 Decoder
#ifdef ARDUINO_AVR_ATTINYX5
#define PIN_CLOSE 0           // pin for closing turnout
#define PIN_THROW 1           // pin for throwing turnout
#define LED_CLOSE 3           // pin for closing turnout
#define LED_THROW 4           // pin for throwing turnout
#define ENABLE_DCC_ACK_PIN   PIN_CLOSE  // Use the Closed Turnout Motor Output to generate a DCC ACK Pulse
#endif 

  // This section define constants for the Arduino UNO board + Iowa Scaled Engineering ARD-DCCSHIELD DCC Shield for testing
#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_MINI)
#define PIN_CLOSE  9           // pin for closing turnout
#define PIN_THROW 10           // pin for throwing turnout
#define LED_CLOSE 11           // pin for closing turnout
#define LED_THROW 12           // pin for throwing turnout
#define ENABLE_DCC_ACK_PIN 15 // This is A1 on the Iowa Scaled Engineering ARD-DCCSHIELD DCC Shield

#define ENABLE_DEBUG
#endif

uint16_t Accessory_Address;  // Command address, Program address in Programmer mode
uint8_t per_close;           // pulse period for close
uint8_t per_throw;           // pulse period for throw
uint8_t per_blink;           // blink period

// Structure for CV Values Table
struct CVPair
{
  uint16_t  CV;
  uint8_t   Value;
};

enum DECODER_STATE
{
  DS_Unknown,
  DS_Thrown,
  DS_Closed
};

// CV Addresses we will be using
#define CV_CLOSE_PERIOD           33  // CV address for close period in cs
#define CV_THROW_PERIOD           34  // CV address for throw period in cs
#define CV_BLINK_PERIOD           35  // CV address for blink period in cs

// Default CV Values Table
CVPair FactoryDefaultCVs [] =
{
// Command address, Program address in Programmer mode
  {CV_ACCESSORY_DECODER_ADDRESS_LSB,       DEFAULT_DECODER_ADDRESS & 0xFF},
  {CV_ACCESSORY_DECODER_ADDRESS_MSB,      (DEFAULT_DECODER_ADDRESS >> 8) & 0x07},
  {CV_CLOSE_PERIOD, 5},
  {CV_THROW_PERIOD, 5},
  {CV_BLINK_PERIOD, 25},
};

NmraDcc  Dcc;

DECODER_STATE DecoderState;

elapsedMillis TurnoutOnTimer = 0;
elapsedMillis LEDBlinkTimer = 0;
bool          LEDBlinkState = false;
uint16_t      TurnoutOnMillis;

// This call-back function is called when a CV Value changes so we can update CVs we're using

void notifyCVChange( uint16_t CV, uint8_t Value)
{
#ifdef ENABLE_DEBUG
  Serial.print("notifyCVChange: CV: ");
  Serial.print(CV);
  Serial.print("  Value: ");
  Serial.println(Value);
#endif

  switch(CV)
  {
    case CV_CLOSE_PERIOD:
      per_close = Value;
      break;
      
    case CV_THROW_PERIOD:
      per_throw = Value;
      break;

    case CV_BLINK_PERIOD:
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
  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset 
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs)/sizeof(CVPair);
};

extern void notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower )
{
#ifdef ENABLE_DEBUG
  Serial.print("notifyDccAccTurnoutOutput: Address: ");
  Serial.print(Addr);
  Serial.print("  Direction: ");
  Serial.print(Direction);
  Serial.print("  OutputPower: ");
  Serial.println(OutputPower);
#endif
  if (Addr == Accessory_Address)
  {
    TurnoutOnTimer = 0;
    
    if (Direction==0)
    {
        // Disable the other LED and Turnout Pin   
      digitalWrite(LED_CLOSE, LOW);
      digitalWrite(PIN_CLOSE, LOW);

        // Enable the Thrown Turnout Pin and LED 
      digitalWrite(LED_THROW, HIGH);
      digitalWrite(PIN_THROW, HIGH);

      TurnoutOnMillis = per_throw * 10;
      DecoderState = DS_Thrown;
    }
    else
    {
        // Disable the other LED and Turnout Pin   
      digitalWrite(LED_THROW, LOW);
      digitalWrite(PIN_THROW, LOW);

        // Enable the Closed Turnout Pin and LED 
      digitalWrite(LED_CLOSE, HIGH);
      digitalWrite(PIN_CLOSE, HIGH);

      TurnoutOnMillis = per_close * 10;
      DecoderState = DS_Closed;
    }
#ifdef ENABLE_DEBUG
    Serial.print("notifyDccAccTurnoutOutput: TurnoutOnMillis: "); Serial.println(TurnoutOnMillis);
#endif
  }
}

// This call-back function is called by the NmraDcc library when a DCC ACK needs to be sent
// Calling this function should cause an increased 60ma current drain on the power supply for 6ms to ACK a CV Read
// So we will just turn the close solenoid on for 8ms and then turn it off again.

#ifdef ENABLE_DCC_ACK_PIN
void notifyCVAck(void)
{
  pinMode( ENABLE_DCC_ACK_PIN, OUTPUT );
  
  digitalWrite(ENABLE_DCC_ACK_PIN, HIGH);
  delay( 10 );  
  digitalWrite(ENABLE_DCC_ACK_PIN, LOW);

#ifdef ENABLE_DEBUG
  Serial.println("notifyCVAck");
#endif
}
#endif

void setup()
{
  // Setup the Pins for the close/throw solenoids. Set the desired pin state first before making it an output to avoid any glitch on start 
   digitalWrite(PIN_CLOSE, LOW);
   digitalWrite(PIN_THROW, LOW);
   pinMode(PIN_CLOSE, OUTPUT);
   pinMode(PIN_THROW, OUTPUT);

  // Setup the Pins for the direction LEDs. Set the desired pin state first before making it an output to avoid any glitch on start
   digitalWrite(LED_CLOSE, LOW);
   digitalWrite(LED_THROW, LOW);
   pinMode(LED_CLOSE, OUTPUT);
   pinMode(LED_THROW, OUTPUT);

#ifdef ENABLE_DEBUG
  Serial.begin(115200);
  Serial.println("setup: MPED DIY: Basic Accessory Turnout Decoder");
#endif   

  // Setup which Pin the DCC Signal is on (and its acssociated External Interrupt)
  Dcc.pin(DCC_PIN, 0);

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, MY_DECODER_VERSION, FLAGS_EXTENDED_ACCESSORY_DECODER, 0);

    // Read the current CV values
  Accessory_Address = Dcc.getAddr();
  per_close = Dcc.getCV(CV_CLOSE_PERIOD);
  per_throw = Dcc.getCV(CV_THROW_PERIOD);
  per_blink = Dcc.getCV(CV_BLINK_PERIOD);

#ifdef ENABLE_DEBUG
  Serial.print("setup: Timer Values: Close: "); Serial.print(per_close); 
  Serial.print("  Throw: "); Serial.print(per_throw); 
  Serial.print("  Blink: "); Serial.println(per_blink); 
#endif

#ifdef ENABLE_DEBUG
  Serial.print("setup: Decoder Address: "); Serial.println(Accessory_Address);
#endif

#ifdef RESET_CVS_TO_FACTORY_DEFAULT
#ifdef ENABLE_DEBUG
  Serial.println("setup: Trigger Restore Factory Defaults");
#endif
  notifyCVResetFactoryDefault();
#endif    
}

void loop()
{
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

    // Turn Off the Turnout Outputs if the Timer has elapsed
  if(TurnoutOnMillis && (TurnoutOnTimer >= TurnoutOnMillis))
  {
#ifdef ENABLE_DEBUG
    Serial.println("loop: Disable Turnout pin after Time Delay");
#endif
    digitalWrite(PIN_CLOSE, LOW);
    digitalWrite(PIN_THROW, LOW);
    TurnoutOnMillis = 0;
  }

  if(DecoderState == DS_Unknown and per_blink > 0)
  {
    if(LEDBlinkTimer >= (per_blink * 10))
    {
      LEDBlinkTimer = 0;
      
      digitalWrite(LED_CLOSE, LEDBlinkState);
      LEDBlinkState = !LEDBlinkState;
      digitalWrite(LED_THROW, LEDBlinkState);
    }
  }

  // Handle resetting CVs back to Factory Defaults
  if( FactoryDefaultCVIndex && Dcc.isSetCVReady())
  {
    FactoryDefaultCVIndex--; // Decrement first as initially it is the size of the array 
    Dcc.setCV( FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
  }
}
