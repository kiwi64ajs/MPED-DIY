// Production 17 Switch Acessory DCC Decoder    AccDec_17LED_1Ftn.ino
// Version 6.01  Geoff Bunza 2014,2015,2016,2017,2018
// Now works with both short and long DCC Addesses for CV Control Default 24 (LSB CV 121 ; MSB CV 122)
// ACCESSORY DECODER  DEFAULT ADDRESS IS 40 (MAX 40-56 SWITCHES)
// ACCESSORY DECODER ADDRESS CAN NOW BE SET ABOVE 255
// BE CAREFUL!  DIFFERENT DCC BASE STATIONS  ALLOW DIFFERING MAX ADDRESSES

#include <NmraDcc.h>

const uint8_t pin_close = 0; // pin for closing turnout
const uint8_t pin_throw = 1; // pin for throwing turnout
const uint8_t led_close = 3; // pin for closing turnout
const uint8_t led_throw = 4; // pin for throwing turnout
const uint8_t cv_close = 33; // CV address for close time
const uint8_t cv_throw = 34; // CV address for throw time
const uint8_t cv_blink = 35; // CV address for blink time

uint16_t Accessory_Address = 101;        // Working CV address, Writing CV address (Programmer mode)
uint8_t per_close;           // pulse period for close
uint8_t per_throw;           // pulse period for throw
uint8_t per_blink;           // blink period
uint8_t state = 0;           // turnout state
uint8_t sta_blink = 0;       // blink state
uint8_t sta_time = 0;        // time state

NmraDcc  Dcc;

uint8_t CV_DECODER_MASTER_RESET =   120;  // Master reset CV address
#define SET_CV_Address       101          // Writing CV address (Commander mode)
#define CV_To_Store_SET_CV_Address	121

struct CVPair
{
  uint16_t  CV;
  uint8_t   Value;
};
CVPair FactoryDefaultCVs [] =
{
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, Accessory_Address&0xFF},
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, (Accessory_Address>>8)&0x07},
  
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, 0},
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, 0},

  {CV_29_CONFIG,CV29_ACCESSORY_DECODER|CV29_OUTPUT_ADDRESS_MODE|CV29_F0_LOCATION}, // Accesory Decoder Short Address
  //  {CV_29_CONFIG, CV29_ACCESSORY_DECODER|CV29_OUTPUT_ADDRESS_MODE|CV29_EXT_ADDRESSING | CV29_F0_LOCATION},  // Accesory Decoder Long Address 

  {CV_DECODER_MASTER_RESET, 0},
  {CV_To_Store_SET_CV_Address, SET_CV_Address&0xFF },
  {CV_To_Store_SET_CV_Address+1,(SET_CV_Address>>8)&0x3F },

  {cv_close, 5},  // close time in cs
  {cv_throw, 5},  // throw time in cs
  {cv_blink, 25}, // blink time in cs
};

uint8_t FactoryDefaultCVIndex = 0;
void notifyCVResetFactoryDefault()
{
  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset 
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs)/sizeof(CVPair);
};

void setup()
{
   pinMode(pin_close, OUTPUT);
   pinMode(pin_throw, OUTPUT);
   pinMode(led_close, OUTPUT);
   pinMode(led_throw, OUTPUT);
   digitalWrite(pin_close, LOW);
   digitalWrite(pin_throw, LOW);
   digitalWrite(led_close, LOW);
   digitalWrite(led_throw, LOW);

  if ( Dcc.getCV(CV_DECODER_MASTER_RESET)== CV_DECODER_MASTER_RESET )
    {
      for (int j=0; j < sizeof(FactoryDefaultCVs)/sizeof(CVPair); j++ )
        Dcc.setCV( FactoryDefaultCVs[j].CV, FactoryDefaultCVs[j].Value);

      if(per_blink!=0)
      {
        digitalWrite(led_throw,HIGH);
        digitalWrite(led_close,HIGH);
        delay(500);
        digitalWrite(led_throw,LOW);
        digitalWrite(led_close,LOW);
        delay(500);
      }
    }

  Accessory_Address = Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB);
  per_blink = Dcc.getCV(cv_blink);

  // Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up 
  Dcc.pin(0, 2, 0);
  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 601, FLAGS_OUTPUT_ADDRESS_MODE | FLAGS_DCC_ACCESSORY_DECODER, CV_To_Store_SET_CV_Address);

}

void loop()


{
  if(state==0 and per_blink!=0)
  {
    sta_time = (millis()/(10*per_blink)) % 2 + 1;
    if(sta_blink!=sta_time)
    {
      if(sta_time==1)
      {
        digitalWrite(led_close, HIGH);
        digitalWrite(led_throw, LOW);
      }
      else
      {
        digitalWrite(led_close, LOW);
        digitalWrite(led_throw, HIGH);
      }
      sta_blink = sta_time;
      per_blink = Dcc.getCV(cv_blink);  
    }
  }


  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();
  
}

extern void notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower )
{  
  if (Addr == Accessory_Address)
  {
    per_close = Dcc.getCV(cv_close);
    per_throw = Dcc.getCV(cv_throw);

    if (Direction==0)
    {
      digitalWrite(led_close, LOW);
      digitalWrite(led_throw, LOW);
      
      digitalWrite(pin_throw,HIGH);
      delay(10*per_throw);
      digitalWrite(pin_throw,LOW);

      state = 1;
      digitalWrite(led_throw, HIGH);
    }
    else
    {
      digitalWrite(led_close, LOW);
      digitalWrite(led_throw, LOW);

      digitalWrite(pin_close,HIGH);
      delay(10*per_close);
      digitalWrite(pin_close,LOW);

      state = 2;
      digitalWrite(led_close, HIGH);
    }
  }
}

//void notifyCVAck(void) {
//  for (int i=0; i < numledpins; i++) {
//    digitalWrite(ledpins[i], HIGH); // Switch all LEDs on
//  }
//  delay(8);                         // .. wait for 8 msec
//  for (int i=0; i < numledpins; i++) {
//    digitalWrite(ledpins[i], LOW);  // .. then switch all LEDs off
//  }
//}
