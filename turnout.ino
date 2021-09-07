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

#define DCC_PIN                 2
#define DEFAULT_DECODER_ADDRESS 1
#define PIN_CLOSE 0               // pin for closing turnout
#define PIN_THROW 1               // pin for throwing turnout
#define LED_CLOSE 3               // pin for closing turnout
#define LED_THROW 4               // pin for throwing turnout

uint16_t Accessory_Address;  // Command address, Program address in Programmer mode
uint8_t per_close;           // pulse period for close
uint8_t per_throw;           // pulse period for throw
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
#define CV_CLOSE_PERIOD           33  // CV address for close period in cs
#define CV_THROW_PERIOD           34  // CV address for throw period in cs
#define CV_BLINK_PERIOD           35  // CV address for blink period in cs
#define CV_DECODER_MASTER_RESET   120 // Master reset CV address
#define CV_TO_STORE_SUPPL_ADDRESS 121 // CV address for Supplementary address, that is Program address in Command mode

// Default CV Values Table
CVPair FactoryDefaultCVs [] =
{
// Command address, Program address in Programmer mode
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_DECODER_ADDRESS&0xFF},
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, (DEFAULT_DECODER_ADDRESS>>8)&0x07},
// Program address in Command mode for encoder
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, DEFAULT_DECODER_ADDRESS&0xFF},
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, (DEFAULT_DECODER_ADDRESS>>8)|0xC0},
// Program address in Command mode for decoder
  {CV_TO_STORE_SUPPL_ADDRESS, DEFAULT_DECODER_ADDRESS&0xFF },
  {CV_TO_STORE_SUPPL_ADDRESS+1,(DEFAULT_DECODER_ADDRESS>>8)&0x3F },

  {CV_VERSION_ID, 1},
  {CV_MANUFACTURER_ID, MAN_ID_DIY},
  {CV_CLOSE_PERIOD, 5},
  {CV_THROW_PERIOD, 5},
  {CV_BLINK_PERIOD, 25},
  {CV_DECODER_MASTER_RESET, 0},

  {CV_29_CONFIG,CV29_ACCESSORY_DECODER|CV29_OUTPUT_ADDRESS_MODE},
};

NmraDcc  Dcc;

// This call-back function is called when a CV Value changes so we can update CVs we're using
void notifyCVChange( uint16_t CV, uint8_t Value)
{
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
      Accessory_Address = Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB) + 256 * Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB);
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

    if (Addr == Accessory_Address)
  {
    if (Direction==0)
    {
      digitalWrite(LED_CLOSE, LOW);
      digitalWrite(LED_THROW, LOW);
      
      digitalWrite(PIN_THROW,HIGH);
      delay(10*per_throw);
      digitalWrite(PIN_THROW,LOW);

      state = 1;
      digitalWrite(LED_THROW, HIGH);
    }
    else
    {
      digitalWrite(LED_CLOSE, LOW);
      digitalWrite(LED_THROW, LOW);

      digitalWrite(PIN_CLOSE,HIGH);
      delay(10*per_close);
      digitalWrite(PIN_CLOSE,LOW);

      state = 2;
      digitalWrite(LED_CLOSE, HIGH);
    }
  }
}

// This call-back function is called by the NmraDcc library when a DCC ACK needs to be sent
// Calling this function should cause an increased 60ma current drain on the power supply for 6ms to ACK a CV Read
// So we will just turn the close solenoid on for 8ms and then turn it off again.
void notifyCVAck(void)
{
  digitalWrite(PIN_CLOSE, HIGH);
  delay( 8 );  
  digitalWrite(PIN_CLOSE, LOW);
}

void setup()
{
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

  // Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up 
  Dcc.pin(0, DCC_PIN, 0);

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 1, FLAGS_OUTPUT_ADDRESS_MODE | FLAGS_DCC_ACCESSORY_DECODER, CV_TO_STORE_SUPPL_ADDRESS);

  // trigger master reset
  if(Dcc.getCV(CV_DECODER_MASTER_RESET) == CV_DECODER_MASTER_RESET) notifyCVResetFactoryDefault();

  // Read the current CV values
  Accessory_Address = Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB) + 256 * Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB);
  per_close = Dcc.getCV(CV_CLOSE_PERIOD);
  per_throw = Dcc.getCV(CV_THROW_PERIOD);
  per_blink = Dcc.getCV(CV_BLINK_PERIOD);
}

void loop()
{
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

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
    Dcc.setCV( FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
  }
}
