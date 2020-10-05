// ---------------------------------------------------------------------------
// Created by Antoine Beauchamp based on the code from the ToneLibrary
//
// See "NonBlockingRtttl.h" for purpose, syntax, version history, links, and more.
// ---------------------------------------------------------------------------

#include "Arduino.h"
#include "NonBlockingRtttl.h"

/*********************************************************
 * RTTTL Library data
 *********************************************************/

namespace rtttl
{


const int notes[] = { 0,
NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6,
NOTE_C7, NOTE_CS7, NOTE_D7, NOTE_DS7, NOTE_E7, NOTE_F7, NOTE_FS7, NOTE_G7, NOTE_GS7, NOTE_A7, NOTE_AS7, NOTE_B7
};

//#define isdigit(n) (n >= '0' && n <= '9')
#define OCTAVE_OFFSET 0

const char * _headBuffer=0;
bool _flashBuffer=false;

char _auxBuff[MAX_RTTTL_LOCAL_BUFF];

const char * buffer = "";
int bufferIndex = -32760;
byte default_dur = 4;
byte default_oct = 5;
int bpm = 63;
long wholenote;
long wholenote_original;
byte pin = -1;
unsigned long noteDelay = 0; //m will always be after which means the last note is done playing
bool playing = false;


bool _playingNumber=true;

int   _playingNumberDec;
int   _playingNumberUnits;
int   _playingNumberDec_Remaining;
int   _playingNumberUnits_Remaining;
bool  _playingNumberPauseNeeded=false;


//pre-declaration
int  nextnote();

// tone() and noTone() are not implemented for Arduino core for the ESP32
// See https://github.com/espressif/arduino-esp32/issues/980
// and https://github.com/espressif/arduino-esp32/issues/1720
#if defined(ESP32)
void noTone(){
  ledcWrite(0, 0); // channel, volume
}

void noTone(int pin){
  noTone();
}

void tone(int frq) {
  ledcWriteTone(0, frq); // channel, freq
  ledcWrite(0, 255); // channel, volume
}

void tone(int pin, int frq, int duration){
  tone(frq);
}
#endif


void changeSpeed(int i)
{
  wholenote=wholenote_original/i;
}


void beginF(byte iPin, const char * iSongBuffer)
{
//DUMMY workaround that need long buffer in ram strncpy_P(_auxBuff,iSongBuffer,MAX_RTTTL_LOCAL_BUFF);
//Better implementation just reading by chunks, ram needed is only the chunk.
return begin(iPin,iSongBuffer,true);
}


void begin(byte iPin, const char * iSongBuffer,bool flash )
{
  _headBuffer=iSongBuffer;
  _flashBuffer=flash;
  _auxBuff[MAX_RTTTL_LOCAL_BUFF-1]=0;
  if (_flashBuffer) {
    strncpy_P(_auxBuff,_headBuffer,MAX_RTTTL_LOCAL_BUFF-1);
  }
  else {
    strncpy(_auxBuff,_headBuffer,MAX_RTTTL_LOCAL_BUFF-1);
  }    

  #ifdef RTTTL_NONBLOCKING_DEBUG
  Serial.print("playing: ");
  Serial.print(_auxBuff);
  Serial.println("...");
  Serial.print("BUFFER ADDR: ");
  Serial.print((int)_headBuffer);
  Serial.print(" FLASH ");
  Serial.println(_flashBuffer);
  #endif

  
  //init values
  pin = iPin;
  #if defined(ESP32)
  ledcSetup(0, 1000, 10); // resolution always seems to be 10bit, no matter what is given
  ledcAttachPin(pin, 0);
  #endif
  /* <ringing-tones-text-transfer-language> := <name> <sep> [<defaults>] <sep> <note-command>+ */


  buffer = _auxBuff;
  bufferIndex = 0;
  default_dur = 4;
  default_oct = 6;
  bpm=63;
  playing = true;
  noteDelay = 0;
  #ifdef RTTTL_NONBLOCKING_DEBUG
  Serial.print("noteDelay=");
  Serial.println(noteDelay);
  #endif
  
  //stop current note
  noTone(pin);

  //read buffer until first note
  int num;

  // format: d=N,o=N,b=NNN:
  // find the start (skip name, etc)

  while(*buffer != ':') // ignore name 
  { 
    buffer++;                    
    bufferIndex++;
  }
  buffer++; // skip ':'
  bufferIndex++;

  // get default duration
  if(*buffer == 'd')
  {    
    buffer++; buffer++;              // skip "d="
    bufferIndex+=2; 
    num = 0;
    while(isdigit(*buffer))
    {
      num = (num * 10) + (*buffer++ - '0');
      bufferIndex++; 
    }
    if(num > 0) default_dur = num;
    buffer++;                   // skip comma
    bufferIndex++; 
  }

  #ifdef RTTTL_NONBLOCKING_INFO
  Serial.print("ddur: "); Serial.println(default_dur, 10);
  #endif
  
  // get default octave
  if(*buffer == 'o')
  {
    buffer++; buffer++;              // skip "o="
    bufferIndex+=2; 
    num = *buffer++ - '0';
    bufferIndex++;
    if(num >= 3 && num <=7) default_oct = num;
    buffer++;                   // skip comma
    bufferIndex++; 
  }

  #ifdef RTTTL_NONBLOCKING_INFO
  Serial.print("doct: "); Serial.println(default_oct, 10);
  #endif
  
  // get BPM
  if(*buffer == 'b')
  {
    buffer++; buffer++;              // skip "b="
    bufferIndex+=2; 
    num = 0;
    while(isdigit(*buffer))
    {
      num = (num * 10) + (*buffer++ - '0');
      bufferIndex++; 
    }
    bpm = num;
    buffer++;                   // skip colon
    bufferIndex++; 
  }

  #ifdef RTTTL_NONBLOCKING_INFO
  Serial.print("bpm: "); Serial.println(bpm, 10);
  #endif

  // BPM usually expresses the number of quarter notes per minute
  wholenote = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)
  wholenote_original=wholenote;
  #ifdef RTTTL_NONBLOCKING_INFO
  Serial.print("wn: "); Serial.println(wholenote, 10);
  #endif


  #ifdef RTTTL_NONBLOCKING_DEBUG
  Serial.print("RTTTL index: ");
  Serial.println(bufferIndex);
  Serial.print("BUFFER ADDR: ");
  Serial.print((int)_headBuffer);
  Serial.print(" FLASH ");
  Serial.println(_flashBuffer);
  #endif
}
//------------------------------------------------------------
void playNumber(byte iPin,byte num)
{
  // Play 2 digit number....
  _playingNumber=true;

  _playingNumberDec   = (int) num/10;
  _playingNumberUnits = (num - _playingNumberDec*10);

  _playingNumberDec_Remaining=_playingNumberDec;
  _playingNumberUnits_Remaining=_playingNumberUnits;
 

  pin=iPin;
  buffer = _auxBuff;
  bufferIndex = 0;
  default_dur = 4;
  default_oct = 6;
  bpm=63;
  playing = true;
  noteDelay = 0;

  //stop current note
  noTone(pin);

  #ifdef RTTTL_NONBLOCKING_DEBUG
  Serial.print("noteDelay=");
  Serial.println(noteDelay);
  Serial.print("ddur: "); Serial.println(default_dur, 10);
  Serial.print("doct: "); Serial.println(default_oct, 10);
  Serial.print("bpm: "); Serial.println(bpm, 10);
  #endif

  // BPM usually expresses the number of quarter notes per minute
  wholenote = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)

  #ifdef RTTTL_NONBLOCKING_INFO
  Serial.print("wn: "); Serial.println(wholenote, 10);
  #endif

  #ifdef RTTTL_NONBLOCKING_DEBUG
  Serial.print("RTTTL index: ");
  Serial.println(bufferIndex);
  Serial.print("BUFFER ADDR: ");
  Serial.print((int)_headBuffer);
  Serial.print(" FLASH ");
  Serial.println(_flashBuffer);
  #endif
}
 
//------------------------------------------------------------
int nextnote()
{
  long duration;
  byte note;
  byte scale;
  char aux;

  #ifdef RTTTL_NONBLOCKING_DEBUG
  if (_playingNumber){
    Serial.print("RTTTL playingNumber: ");
    Serial.print(_playingNumberDec);
    Serial.print(_playingNumberUnits);
    Serial.print(" Remaining: ");
    Serial.print(_playingNumberDec_Remaining);
    Serial.println(_playingNumberUnits_Remaining);
    bufferIndex=0;
    _headBuffer=0;
    _flashBuffer=0;
  }
  Serial.print("RTTTL index: ");
  Serial.println(bufferIndex);
  Serial.print("BUFFER ADDR: ");
  Serial.print((int)_headBuffer);
  Serial.print(" FLASH ");
  Serial.println(_flashBuffer);
  #endif


  if (_playingNumber){
    if (_playingNumberPauseNeeded) { 
      strcpy(_auxBuff,"16p");
      _playingNumberPauseNeeded=false;
    }
    else {
      _playingNumberPauseNeeded=true; //It will be next one really
      if (_playingNumberDec_Remaining!=0){
        strcpy(_auxBuff,"16e6");
        _playingNumberDec_Remaining--;
      }
      else if (_playingNumberUnits_Remaining!=0){
        strcpy(_auxBuff,"16b6");
        _playingNumberUnits_Remaining--;
      }   
      else{
        _playingNumber=false;
        _auxBuff[0]=0;
      }
    }
  }
  else {
    for(int i=0;i<10;i++){
      if (_flashBuffer) {
        aux=pgm_read_byte_near(_headBuffer + bufferIndex + i);
      }
      else {
        aux=_headBuffer[bufferIndex+i];
      }
      _auxBuff[i]=aux;
      if ((aux == 0) || (aux == ',')) { _auxBuff[i]=0; break; }
    }
  }


  buffer=_auxBuff;
  if (_auxBuff[0]==0) {
    #ifdef RTTTL_NONBLOCKING_DEBUG
    Serial.println("No more notes");
    #endif
    return -1;  //No more tones
  }  


  #ifdef RTTTL_NONBLOCKING_DEBUG
  Serial.print("nextNote: ");
  Serial.print(_auxBuff);
  Serial.println("...");
  #endif




  //stop current note
  noTone(pin);

  // first, get note duration, if available
  int num = 0;
  while(isdigit(*buffer))
  {
    num = (num * 10) + (*buffer++ - '0');
    bufferIndex++; 
  }
  
  if (num) duration = wholenote / num;
  else duration = wholenote / default_dur;  // we will need to check if we are a dotted note after

  // now get the note
  note = 0;

  switch(*buffer)
  {
    case 'c':
      note = 1;
      break;
    case 'd':
      note = 3;
      break;
    case 'e':
      note = 5;
      break;
    case 'f':
      note = 6;
      break;
    case 'g':
      note = 8;
      break;
    case 'a':
      note = 10;
      break;
    case 'b':
      note = 12;
      break;
    case 'p':
    default:
      note = 0;
  }
  buffer++;
  bufferIndex++; 

  // now, get optional '#' sharp
  if(*buffer == '#')
  {
    note++;
    buffer++;
    bufferIndex++; 
  }

  // now, get optional '.' dotted note
  if(*buffer == '.')
  {
    duration += duration/2;
    buffer++;
    bufferIndex++; 
  }

  // now, get scale
  if(isdigit(*buffer))
  {
    scale = *buffer - '0';
    buffer++;
    bufferIndex++; 
  }
  else
  {
    scale = default_oct;
  }

  scale += OCTAVE_OFFSET;

  if(*buffer == ',')
    buffer++;       // skip comma for next note (or we may be at the end)
    bufferIndex++; 

  // now play the note

  if(note)
  {
    #ifdef RTTTL_NONBLOCKING_INFO
    Serial.print("Playing: ");
    Serial.print(scale, 10); Serial.print(' ');
    Serial.print(note, 10); Serial.print(" (");
    Serial.print(notes[(scale - 4) * 12 + note], 10);
    Serial.print(") ");
    Serial.println(duration, 10);
    #endif
    
    tone(pin, notes[(scale - 4) * 12 + note], duration);
    
    noteDelay = millis() + (duration+1);
  }
  else
  {
    #ifdef RTTTL_NONBLOCKING_INFO
    Serial.print("Pausing: ");
    Serial.println(duration, 10);
    #endif
    
    noteDelay = millis() + (duration);
  }
  return 0;
}

void play()
{
  //if done playing the song, return
  if (!playing)
  {
    #ifdef RTTTL_NONBLOCKING_DEBUG
    //Serial.println("done playing...");
    #endif
    
    return;
  }
  
  //are we still playing a note ?
  unsigned long m = millis();
  if (m < noteDelay)
  {
    #ifdef RTTTL_NONBLOCKING_DEBUG
    //Serial.println("still playing a note...");
    #endif
    
    //wait until the note is completed
    return;
  }

  if (nextnote()!=0)
  {  
    //no more notes. Reached the end of the last note

    #ifdef RTTTL_NONBLOCKING_DEBUG
    Serial.println("end of note...");
    #endif
    
    //issue #6: Bug for ESP8266 environment - noTone() not called at end of sound.
    //disable sound at the end of a normal playback.
    stop();

    return; //end of the song
  }
}

void stop()
{
  if (playing)
  {
    //issue #6: Bug for ESP8266 environment - noTone() not called at end of sound.
    //disable sound if one abort playback using the stop() command.
    noTone(pin);

    playing = false;
  }
}

bool done()
{
  return !playing;
}

bool isPlaying()
{
  return playing;
}

}; //namespace
