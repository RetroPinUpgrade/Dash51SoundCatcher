
/*
 * Pin connections for this sketch (on the -51 sound card):
 *  J1 : pin 8 = hooked to Arduino Pin 2
 *  J1 : pin 1 = hooked to Arduino Pin 6
 *  J1 : pin 2 = hooked to Arduino Pin 7
 *  J1 : pin 3 = hooked to Arduino Pin 8
 *  J1 : pin 4 = hooked to Arduino Pin 9
 *  J1 : pin 12 = hooked to Arduino Pin 10
 *
 *  +5 = Arduino +5
 *  GND = Arduino GND
 */



volatile boolean SoundLatchHigh = false;

#define MAX_SOUND_BYTES_WRITTEN 100

unsigned long NumInterruptsSeen = 0;

#if defined(__AVR_ATmega2560__)  
#define SOUND_CALL_STACK_SIZE   500
#else 
#define SOUND_CALL_STACK_SIZE   225
#endif
#define SOUND_CALL_STACK_EMPTY  0xFF
volatile unsigned short SoundCallStackFirst;
volatile unsigned short SoundCallStackLast;
byte SoundCallStack[SOUND_CALL_STACK_SIZE];
unsigned long SoundCallStackTime[SOUND_CALL_STACK_SIZE];


int SpaceLeftOnSoundCallStack() {
  if (SoundCallStackFirst>=SOUND_CALL_STACK_SIZE || SoundCallStackLast>=SOUND_CALL_STACK_SIZE) return 0;
  if (SoundCallStackLast>=SoundCallStackFirst) return ((SOUND_CALL_STACK_SIZE-1) - (SoundCallStackLast-SoundCallStackFirst));
  return (SoundCallStackFirst - SoundCallStackLast) - 1;
}

void PushToSoundCallStack(byte soundByte) {
  // If the switch stack last index is out of range, then it's an error - return
  if (SpaceLeftOnSoundCallStack()==0) return;

  SoundCallStack[SoundCallStackLast] = soundByte;
  SoundCallStackTime[SoundCallStackLast] = millis();
  
  SoundCallStackLast += 1;
  if (SoundCallStackLast==SOUND_CALL_STACK_SIZE) {
    // If the end index is off the end, then wrap
    SoundCallStackLast = 0;
  }
}


byte PullFirstFromSoundCallStack(unsigned long *callTime=NULL) {
  // If first and last are equal, there's nothing on the stack
  if (SoundCallStackFirst==SoundCallStackLast) return SOUND_CALL_STACK_EMPTY;

  byte retVal = SoundCallStack[SoundCallStackFirst];
  if (callTime!=NULL) *callTime = SoundCallStackTime[SoundCallStackFirst];

  SoundCallStackFirst += 1;
  if (SoundCallStackFirst>=SOUND_CALL_STACK_SIZE) SoundCallStackFirst = 0;

  return retVal;
}



#define DELAY_CYCLES(n) __builtin_avr_delay_cycles(n)


void SoundInterrupt() {  

  NumInterruptsSeen += 1;

  delayMicroseconds(100);
  byte soundData;

#if defined(__AVR_ATmega2560__)  
  soundData = (PINH & 0x78)>>3;
  soundData |= (PINB & 0x10);
#else
  soundData = ((PIND/64) | (PINB*4)) & 0x1F;
#endif
  
  PushToSoundCallStack(soundData);  

}


void setup() {
  Serial.begin(115200);
  Serial.write("Monitor started\n\r");

  pinMode(2, INPUT); // IRQ
  pinMode(3, INPUT); // CLK
  pinMode(4, INPUT); // VMA
  pinMode(5, INPUT); // R/W
  for (byte count=6; count<13; count++) pinMode(count, INPUT); // D0-D6
  pinMode(13, INPUT); // Switch
#if defined(__AVR_ATmega2560__)    
  pinMode(14, INPUT); // Halt
  pinMode(15, INPUT); // D7
  for (byte count=16; count<32; count++) pinMode(count, INPUT); // Address lines are output
#endif 

  SoundCallStackFirst = 0;
  SoundCallStackLast = 0;

  // this sketch uses pin 2 (normally IRQ) as an interrupt 
  // for the "Sound Interrupt" aka Sol. Bank Select
  // pin, which is J1:pin 8 on the -51 sound card
  attachInterrupt(digitalPinToInterrupt(2), SoundInterrupt, RISING);
}

unsigned long LastReport = 0;
unsigned long LastCallTime = 0;

void loop() {
  unsigned long currentTime = millis();

  unsigned long callTime;
  byte soundStackEntry = PullFirstFromSoundCallStack(&callTime);
  if (soundStackEntry!=SOUND_CALL_STACK_EMPTY) {
    char buf[256];
    sprintf(buf, "Sound data = 0x%02X, call time=%lu, time since last call=%lu\n", soundStackEntry, callTime, callTime-LastCallTime);
    Serial.write(buf);
    LastCallTime = callTime;
  }

  if ((currentTime-LastReport)>5000) {
    char buf[256];
    sprintf(buf, "Heartbeat: num interrupts seen = %lu\n\r", NumInterruptsSeen);
//    Serial.write(buf);
    LastReport = currentTime;
  }
}
