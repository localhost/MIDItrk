//******************************************************************************
// MIDItrk - A simple but working MIDI tracker by Hermit (Mihaly Horvath) 2013 *
//******************************************************************************/
//#define APPversion 1.0

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <SDL/SDL.h>
#include "RtMidi.h"

#ifdef __WINDOWS__KS__ //if kernel-streaming mode is selected
#include "ks.h"
#include "ksmedia.h"
#endif

//========declaration of XPM program-icon, Mouse-icon, Character set ==================
#include "MIDITRKicon.xpm"
#include "PETSCIIset.xpm"
#include "MIDITRKmouse.xpm"
#include "MIDITRKdecor.xpm"
#include "MIDITRKbuttons.xpm"

#define icon_width  32
#define icon_height 32
#define icon_depth 32
unsigned char icon_pixels[icon_width*icon_height*icon_depth];
unsigned char decor_pixels[icon_width*icon_height*icon_depth];
unsigned char button_pixels[icon_width*icon_height*icon_depth];

#define chrset_width  8*256
#define chrset_height 8
#define chrset_depth  32
unsigned char chrset_pixels[chrset_width*chrset_height*chrset_depth];

SDL_Cursor *MouseCursor;

//==========================================Constants, Variables, Arrays=====================================

//MIDI format descriptor constants======================================
//Header-chunk (every value is in big-endian format)
#define MIDI_ID_OFFSET 0
char MIDI_ID[]="MThd";
#define MIDI_ID_SIZE 4
#define MIDI_HEADERSIZE_OFFSET 4 //the header-size of the MIDI is a big-endian double-word 
unsigned char MIDI_HEADERSIZE[4]={0,0,0,0x6}; //always 00 00 00 06 for standard MIDI files
#define MIDI_VERSION_OFFSET 8  //big endian 2-byte word
unsigned int InputMIDIver; //(0:single-track,1:multiple-track,2:multiple song)
#define MIDI_TRACK_AMOUNT_OFFSET 0xA //2 byte big-endian word containing number of tracks
unsigned int MIDI_TRACK_AMOUNT;
#define MIDI_DIVISION_OFFSET 0xC //2 byte - If the value is positive, then it represents the units per beat. For example, +96 would mean 96 ticks per beat.
signed int MIDI_DIVISION;
#define MIDI_TRACKS_OFFSET 0xE //here tracks are starting
 //Track-chunk - offset values here are relative to track-chunks' beginnings
 #define MIDI_TRACK_ID_OFFSET 0
 char MIDI_TRACK_ID[]="MTrk"; //4 bytes - the literal string MTrk. This marks the beginning of a track. 
 #define MIDI_TRACK_SIZE_OFFSET 4 //4 bytes - the number of bytes in the track chunk following this number. 
 unsigned long MIDI_TRACK_SIZE[64]; //collecting track-sizes here
 unsigned long MIDI_TRACK_OFFSET[64]; //calculating from sizes
 #define MIDI_TRACK_EVENT_OFFSET 8 //a sequenced track event
  //Track-event format (consists of a delta time since the last event, and one of three types of events.)
  #define MIDI_DELTA_TIME_OFFSET 0 //variable-length big-endian value, last databyte has bit7 cleared (others have it set)
  #define MIDI_EVENT_OFFSET 0
  #define MIDI_META_EVENT_OFFSET 0
  #define MIDI_SYSEX_EVENT_OFFSET 0
   //Meta-event format
   #define MIDI_META_TYPE_OFFSET 0
   #define MIDI_META_LENGTH_OFFSET 1 //length of meta event data expressed as a variable length value. 
   #define MIDI_META_EVENT_DATA_OFFSET 0
   //System-exclusive event format
    //A system exclusive event can take one of two forms:
    //sysex_event = 0xF0 + <data_bytes> 0xF7 or sysex_event = 0xF7 + <data_bytes> 0xF7
    //In the first case, the resultant MIDI data stream would include the 0xF0. In the second case the 0xF0 is omitted.  


//MIDItrk MUSICDATA-structure description====================================
#define ORDERLIST_FX_MIN 0xfe //
#define ORDERLIST_FX_JUMP 0xfe
#define ORDERLIST_FX_END 0xff
#define PATTERN_FX_MIN 0xfe
#define NOTE_MAX 10*12 //10 Octaves
#define GATEOFF_NOTEFX 0xfe
#define GATEON_NOTEFX 0xff
const int MaxPtnAmount=ORDERLIST_FX_MIN;
const int MaxPtnLength=256;
const int PtnColumns=3; //note-column, effect-column, effect-value column
const int MaxSeqLength=255;
const int TrackAmount=16;
const int PortAmount=256;
const int MaxInstAmount=128;
const int InstrumSize=32;
//Tune-file header definition
#define TrackerIDsize 16
unsigned char TRACKERID[TrackerIDsize]={"MIDItrk - 1.0 -"};
#define TuneSettingSize 16
unsigned char TUNESETTING[TuneSettingSize]={TrackAmount,PtnColumns,(unsigned char)MaxPtnAmount-1,MaxInstAmount,0,4,0,0,0,0,0,0,0,0,0,0}; //various (e.g. file-format,size) related settings
#define TUNE_CHANAMOUNT 0
#define TUNE_PTNCOLUMNS 1
#define TUNE_PTNAMOUNT  2
#define TUNE_INSTAMOUNT 3
#define TUNE_MIDIPORTIN 4
#define TUNE_HIGHLIGHT  5
#define TUNE_AUTOFOLLOW 6
unsigned char DefaultIns[TrackAmount]={0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0}; //default instrument-setting for all tracks
//Tune-file header end
unsigned char SEQUENCE[TrackAmount][MaxSeqLength];
unsigned char PATTLENG[MaxPtnAmount];
unsigned char PATTERNS[MaxPtnAmount][PtnColumns][MaxPtnLength];
unsigned char INSTRUMENT[MaxInstAmount][InstrumSize]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//Instrument description
#define INST_PORT 0
#define INST_CHVOL 1
#define INST_PATCH 2
#define INST_NAME 16
//END of MUSICDATA-structure description=====================================


int BitsPerPixel=24, WinSizeX=640, WinSizeY=480;
const char CharSizeX=8, CharSizeY=8;

Uint32 chrdata[]={0x556677,0x8899aa,0x113322,0x334422,0xaa99bb,0xaaddee,0xffee11,0xeabc12};
SDL_Rect FontRect,CursoRect,IconRect,BlitRect;

SDL_Surface *screen, *CharSet, *cursor, *MIDItrk_Icon, *MIDItrk_Decor, *MIDItrk_Buttons;
SDL_Event event;
Uint32 rmask, gmask, bmask, amask;

RtMidiIn  *midiin = 0;
RtMidiOut *midiout = 0;
std::vector<unsigned char> PlayerMessage;
int UsedInPort=0, DispNote=0, InPortCount=0, OutPortCount=0;
int SelInst=0, MIDIselInst=0xFF, Octave=3, Advance=1, KeyMode=0; //KeyMode=1:Edit, KeyMode=2:Jam
unsigned char HiLight=4; 
//char TiMinute=00, TiSecond=00;
char PlayMode=0,PrevPlayMode=0; //0:paused/stopped, 1: Tune-play, 2: pattern-play
bool FollowPlay=false, AutoFollow=false, fastfwd=false, ExportMode=false; //, FullScreen=false;
int ffwdspeed=6;

int PattPosX=2, PattPosY=4, PattDimX=8, PattDimY=40, OrdListPosX=PattPosX, OrdListPosY=48, OrDimX=20, OrDimY=8, InsDimX=3, InsDimY=7, StatPosY=(WinSizeY/CharSizeY)-1;
int InstPosX=68, InstPosY=OrdListPosY;
Uint8* keystate; 
int CurPosX=0,CurPosY=0;
int CurColor=0x00, ColDir=1;
int CurWide=3; //1 or 3 depending on note/value position
char repspd1=12, repspd2=2;
char repecnt=0;
int PrevCurX,PrevCurY;

char Window=0;  //0=pattern-window, 1=orderlist, 2=instrument/menu?
char WinPos1[4]={0,0,0,0}, WinPos1Max[4]={PattDimX-1,OrDimX-1,InsDimX-1,0}, WinPos2[4]={0,0,0,0}, WinPos2Max[4]={PattDimY-1,OrDimY-1,0,0}, WinPos3[4]={0,0,0,0}, WinPos3Max[4]={4,1,1,0};
char TrkPos=0; //display-position of the track-frame in the whole track-field
bool ESCapes=false;

SDL_TimerID FrameClk;
#define TIMEREVENT_CODE 33
const int TimerInterval=20; //20ms (50Hz) timer

RtMidiOut *PortOut[PortAmount];
int pattpos; 
int seqpos; 
unsigned char selpatt[TrackAmount]={0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};
bool mutesolo[TrackAmount]={true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true};
bool SoloState=false;
unsigned char PtClipBoard[PtnColumns+1][MaxPtnLength+16];
unsigned char PtClipSize=0;
unsigned char PtClipSourcePtn=0xFF;
unsigned char PtClipSourcePos=0;
unsigned char SeqClipBoard[MaxSeqLength+16];
unsigned char SeqClipSize=0;
unsigned char SeqClipSourceChn=0xFF;
unsigned char SeqClipSourcePos=0;

FILE *TuneFile;

const unsigned char HelpDimX=60, HelpDimY=58;
const char helptxt[HelpDimY][HelpDimX+1]={
        "+@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@+",
        "]     MIDItrk 1.0 instant help (see more in README.txt)    ]",
        "]@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@]",
        "] This is a tracker-style MIDI editor based on SDL-library ]",
        "] and RtMIDI-library, so it might well support other kinds ]",
        "] of operating systems. This tracker is different from the ]",
        "] rest in the orderlist/sequence arrangement, every track  ]",
        "] has its own orderlist, so composing is more flexible...  ]",
        "] (The only up-to-date MIDI-tracker for Linux was Tutka    ]",
        "]  with a similar approach & purpose but it uses blocks.)  ]",
        "] The MIDI inputs & outputs on your system are automati-   ]",
        "] cally detected, you can choose which MIDI-controller and ]",
        "] sound engine (e.g. Qsynth sampler) you'd like to use.    ]",
        "] (Press & hold RETURN in instrument-window to list them.) ]",
        "]                                                          ]",
        "] Keys you can use (very similar to SID-Wizard layout):    ]",
        "] -----------------------------------------------------    ]",
        "]F1:play from beginning,   ] Enter:  noteOff/goPatt/GMset  ]",
        "]F2:from mark (Shift+SPACE)] SPACE:  Toggle jam/edit modes ]",
        "]F3:play selected patterns ] TAB:    Cycle through tracks  ]",
        "]F4:stop/continue playback ] Del/Ins Delete/Insert + move  ]",
        "]Ctrl + F1..F4:follow-play ] BackSpc Del. & advance fwd/bwd]",
        "]Back Quote: fast-forward  ] Ctrl+Del/BackSpc. Delete row  ]",
        "]Sh+BackQuote:toggle Follow] Ctrl+ E 1st empty in Orderlist]",
        "]Ctrl+BackQuote:Auto-Follow] Ctrl+Enter Set pattern-length ]",
        "]Sh+SPACE:play from cursor ] Sh/Ctrl+Q/W Transp.note/octave]",
        "]or set mark for all tracks] Ctrl+C  Copy from cursor-pos. ]",
        "]Ctrl+SPACE:set 1 mark only] Shift+C Mark end of copying   ]",
        "]F5:go to pattern-editor   ] Ctrl+X  Cut&Copy from cursor  ]",
        "]F6:go to orderlist-editor ] Ctrl+V  Paste at cursor-posit.]",
        "]F7:go to instrument-editor] ------------------------------]",
        "]Alt +/-  Select MIDI-in   ] Sh+1..F Mute/unmute track 1..F]",
        "]Alt ,/.  Select def. Instr] Shift+M Mute/unmute the track ]",
        "]F8:Load Workfile/MIDI     ] Shift+S Solo/unsolo track     ]",
        "]F9:Save Workfile/MIDI     ]  + / -  Inc./Dec. Instrument  ]",
        "]F10:Clear the whole tune  ] Ctrl+0..9 Select note-Octave  ]",
        "]Alt+F11:Toggle FullScreen ] Ctrl +/-  Inc/Dec. note-Octave]",
        "]F12:Call this Help        ] Shift+H/J Inc/Dec. Highlight  ]",
        "]Esc:Quit from MIDItrk     ] Shift+A/Z Inc/Dec. AutoAdvance]",
        "]                                                          ]",
        "] The Orderlist at the bottom of the screen holds the track]",
        "] time-line with pattern numbers and 'FE xx' effect. The   ]",
        "] patterns can hold notes and effects in this fashion:     ]",
        "] C-5:0 000 -> <note>:<velocity 1..F> <effect><effet value>]",
        "]                                                          ]",
        "] pattern effects:                    A-ChannelVolume (CC7)]",
        "] ----------------    5-Attack-time   B-MIDI-CC fx & value ]",
        "] 1-PitchSlide up     6-Decay-time    C-Set trk.instrument ]",
        "] 2-PitchSlide down   7-Release-time  D-Aftertouch (press.)]",
        "] 3-Note-portamento   8-Vibrato-rate  E-Set PitchWheel pos.]",
        "] 4-Vibrato strength  9-Delay note    F-Tempo, 80..FF:Track]",
        "]                                                          ]",
        "] MIDI-CC fx: 1-bank, 2-breath, 3-foot, 4-portamento-time, ]",
        "] 5-pan, 6-expression, 7-Fx1, 8-Fx2, 9-general1, A-general2]",
        "] B-legato, C-reverb, D-tremolo, E-chorus, F-detune level  ]",
        "]@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@]",
        "] 2013 Hermit Software (Mihaly Horvath: hermit*t-email.hu) ]",
        "+@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@+",
 };


//-------------------------------------function prototypes---------------------------------------------------
void PutChar(int x, int y, unsigned char chcode, int BGcol, int FGcol); 
void put2digit (int x, int y, char number); void put1digit (int x, int y, char number);
void PutString(int x, int y, const std::string& Gstring); //char ascii2petscii(char Character);
void PutString(int x, int y, const std::string& Gstring, int length); //for some cases
void DisplayStatic(), Display(); void DispOrderL(); void DispInstr(); void DispCursor(); void DisplayTime();
int EnumDevices(); void KeyHandler(); void RefreshCursor (), DisplaySettings(); void DisPattData();
void put1hex (int x, int y, unsigned char number); void put2hex (int x, int y, unsigned char number);
void WaitKeyPress(); char ascii2petscii(char Char);
void SetTimer(); void RemoveTimer(); void InitMusicData(bool putTemplate); int AlertBox(const char* text);
void InitGUI(), DisplayHelp(); void DispMIDIevent();
unsigned char HexKeyVal(); unsigned char NoteKeyVal(); unsigned char NumPadKeyVal(); void EnterNote(); void EnterHex(); 
void InitRoutine(); void PlayRoutine(); void DisPattCnt(int j); void DispSeqCnt(int i);
void SetInDevice(int port); void SelectIns(unsigned char instr); void DispTrkInfo();
int TypeFileName(); int LoadTune(); int SaveTune(); int ExportMIDI(); void DisplayGMset();
void AllNotesOff(unsigned char instr); int CarefulMessage(unsigned char instr);
void MessageToExport(unsigned char instr); void ToggleFullScreen(); void ChangeMouseCursor();
void XPMtoPixels(char* source[], unsigned char* target); void ResetPos();
void WaitKeyRelease(); void WaitButtonRelease(); int cmpstr(char *string1, char *string2);
void MIDIcallback( double deltatime, std::vector< unsigned char > *message, void *userData );
void CurUp(); void CurDown(); int MouseField(); void SoloUnsolo(int track);
char* FilExt(char *filename); void CutExt(char *filename); void ChangeExt(char *filename,char *newExt);
int LoadTuneFile(); inline bool fexists (const std::string& name);

//*****************************************************************************************************
//=============================MAIN ROUTINE============================================================
int main(int argc, char *argv[]) 
{
 int i, PortChkTimer=0, PortChkPeriod=100;
	
 printf("\nMIDItrk 1.0 - a fast & dirty MIDI tracker by Hermit (Mihaly Horvath) in 2013\n");

 //--------------------MIDI initialization-----------------------
 EnumDevices();
 midiin->setCallback( &MIDIcallback );
 midiin->ignoreTypes( true, true, true );   //ignore sysex, timing, or active sensing messages.
 SetInDevice(UsedInPort);
                  //midiin->openVirtualPort("MIDItrk-input");    //seems not supported on Windows (no matter if mm/ks)
                  //midiout->openVirtualPort("MIDItrk-output"); //seems not supported on Windows (no matter if mm/ks)
 for (i=0; i<PortAmount; i++) 
 {
  try {PortOut[i] = new RtMidiOut();}   catch ( RtError &error ) {error.printMessage(); exit( EXIT_FAILURE ); }
  if (i < midiout->getPortCount())
  {
   try {PortOut[i]->openPort(i);} catch ( RtError &error ) {error.printMessage();}
  }
 }

 //---------------------SDL initialization-----------------------

 SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER);
 
/* SDL interprets each pixel as a 32-bit number, so our masks must depend
 on the endianness (byte order) of the machine */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

 //create Charset surface from XPM
 XPMtoPixels(PETSCIIset_xpm,chrset_pixels); 
 CharSet = SDL_CreateRGBSurfaceFrom( (void*)chrset_pixels, chrset_width, chrset_height, chrset_depth, chrset_width * chrset_depth/8, rmask,gmask,bmask,amask); 
                                       //CharSet=SDL_LoadBMP("graphics/PETSCII-set-small.bmp");
 
 FontRect.w=CharSizeX;BlitRect.w=CharSizeX;
 FontRect.h=CharSizeY;BlitRect.h=CharSizeY;

 //create window titlebar & icon
 SDL_WM_SetCaption("MIDI-tracker 1.0 (by Hermit)", "MIDI-tracker 1.0");
 XPMtoPixels(MIDITRKicon_xpm,icon_pixels); XPMtoPixels(MIDITRKdecor_xpm,decor_pixels); XPMtoPixels(MIDITRKbuttons_xpm,button_pixels);
 MIDItrk_Icon = SDL_CreateRGBSurfaceFrom( (void*)icon_pixels, icon_width, icon_height, icon_depth, icon_width * icon_depth/8, rmask, gmask, bmask, amask );
 MIDItrk_Decor = SDL_CreateRGBSurfaceFrom( (void*)decor_pixels, icon_width, icon_height, icon_depth, icon_width * icon_depth/8, rmask, gmask, bmask, amask );
 MIDItrk_Buttons = SDL_CreateRGBSurfaceFrom( (void*)button_pixels, icon_width, icon_height, icon_depth, icon_width * icon_depth/8, rmask, gmask, bmask, amask );
 SDL_WM_SetIcon(MIDItrk_Icon, NULL); //SDL_WM_SetIcon(SDL_LoadBMP("MIDItrk.png"), NULL);
 screen = SDL_SetVideoMode(WinSizeX, WinSizeY, BitsPerPixel, SDL_HWSURFACE|SDL_ANYFORMAT); //|SDL_DOUBLEBUF|SDL_FULLSCREEN|SDL_SWSURFACE|SDL_RESIZABLE);

 ChangeMouseCursor();

 InitMusicData(true);SelectIns(SelInst);

 if (argc == 2) //checks whether not less/more than one command line option, which should be filename
 {//MIME to path - open the file from command line parameter
  printf("Opening tune from command-line:\n%s\n",argv[1]);
  if (fexists(argv[1]))
  {
   TuneFile=fopen(argv[1],"rb"); // !!!important "b" is for binary (not to check text-control characters)
   int errcod=LoadTuneFile();
   if (errcod==0) printf("\nFile was successfully loaded...\n"); 
   else if (errcod==1) {printf("\nLoad Error... exiting...\n");exit(1);}
   else if (errcod==2) LoadTune();
   RemoveTimer();
  }
  else {printf("\nFile was not found, exiting...\n");exit(1);}
 }

 InitGUI();

 SetTimer();
 
 bool done=false;
 //main event-handler loop
 while(!done) 
 {
  while (SDL_PollEvent(&event))
  {
   if(event.type==SDL_KEYDOWN)
   { //main keys 
       //printf("%2X ",event.type); //printf("%2x ",event.key.keysym.sym);
    switch (event.key.keysym.sym) //be careful with threads
    {
     case SDLK_F8: LoadTune(); //load
        break;
     case SDLK_F9: //save worktune / export MIDI
        keystate = SDL_GetKeyState(NULL);
        if (!keystate[SDLK_LSHIFT] && !keystate[SDLK_RSHIFT]) SaveTune(); else ExportMIDI(); //save
        break;
     case SDLK_F10: 
        RemoveTimer();
        if (AlertBox("Do you want to clear the whole tune? Y/N"))
        {
         InitMusicData(false); Display(); 
        }
        SetTimer();
        break;
     case SDLK_F11: keystate = SDL_GetKeyState(NULL);
        if (keystate[SDLK_LALT] || keystate[SDLK_RALT]) 
           ToggleFullScreen();
        break;
     case SDLK_F12: //help
        RemoveTimer(); DisplayHelp();
        InitGUI(); SetTimer();
        break;
     case SDLK_RETURN: //show General MIDI instrument set
        if (Window==2) { RemoveTimer(); DisplayGMset(); InitGUI(); SetTimer(); }
        break;
     case SDLK_ESCAPE: //quit
        ESCapes=true; 
        break;
         
     default: break;
    }
   }
    
   else if (event.type == SDL_USEREVENT && event.user.code==TIMEREVENT_CODE)
   {
    if (!INSTRUMENT[SelInst][0] && !INSTRUMENT[SelInst][1] && !INSTRUMENT[SelInst][2]) DispNote=0; //avoid loopback/feedback at least for empty instruments
    /*if (MIDIselInst!=0xFF) //selecting instrument through MIDI-input? - on Linux Midi-thourgh causes loop
    {
     SelInst=MIDIselInst; SelectIns(SelInst); DispInstr(); MIDIselInst=0xFF;
    }*/
    KeyHandler();
    DispCursor();
    if (PlayMode>0) {PlayRoutine();DisplayTime();}
    DispMIDIevent();
    if (PortChkTimer--<=0)
    {
     PortChkTimer=PortChkPeriod; i=midiin->getPortCount();
     if (InPortCount!=i) //check if change happened meanwhile
     {
      SetInDevice(UsedInPort); //if MIDI controller connected/disconnected, refresh callback & display 
      DisplaySettings();
     }
     InPortCount=i;
    }
   }

   else if (event.type == SDL_MOUSEBUTTONDOWN)
   {
    unsigned char mouseChX=event.button.x/CharSizeX, mouseChY=event.button.y/CharSizeY;
    if (event.button.button==SDL_BUTTON_LEFT)
    {
     if (event.button.y<38 && (event.button.x>WinSizeX-32 || event.button.x<32))
     { 
      if (event.button.x<32 || event.button.y>24) { RemoveTimer(); DisplayHelp(); InitGUI(); SetTimer(); } //help-button
      else if (event.button.y<20)
      { //buttons at the top
       if (event.button.x>=WinSizeX-17) ESCapes=true;
       else if (event.button.x>=WinSizeX-32 && event.button.x<WinSizeX-20) { WaitButtonRelease(); ToggleFullScreen(); }
      }
     }
     else if (MouseField()==0) {Window=0;WinPos1[0]=(mouseChX-PattPosX-1)/9; WinPos2[0]=mouseChY-PattPosY-2; Display();}
     else if (MouseField()==1) {Window=1;WinPos1[1]=(mouseChX-OrdListPosX-2)/3; WinPos2[1]=mouseChY-OrdListPosY-2; Display();}
     else if (MouseField()==2) {Window=2;WinPos1[2]=(mouseChX-InstPosX-2)/3; Display();}
     else if (MouseField()==3) {mutesolo[TrkPos+(mouseChX-PattPosX-1)/9]^=true; Display();}
    }
    else if (event.button.button==SDL_BUTTON_RIGHT)
    {
     if (MouseField()==3) SoloUnsolo( TrkPos+(mouseChX-PattPosX-1)/9 );
    }
    else if (event.button.button==SDL_BUTTON_WHEELUP)
    {
     if (MouseField()==0) {for(i=0;i<8;i++) if(pattpos>0 && !FollowPlay) pattpos--; DisPattData();}
     else if (MouseField()==1) { if (TrkPos>0) TrkPos--; Display();}
     else if (MouseField()==2) { if (SelInst>0) {SelInst--; SelectIns(SelInst); DispInstr();}}
    }
    else if (event.button.button==SDL_BUTTON_WHEELDOWN)
    {
     if (MouseField()==0) {for(i=0;i<8;i++) if(pattpos<0x100-PattDimY && !FollowPlay) pattpos++; DisPattData();}
     else if (MouseField()==1) { if (TrkPos<TrackAmount-OrDimY) TrkPos++; Display();}
     else if (MouseField()==2) { if (SelInst<MaxInstAmount-1) {SelInst++;SelectIns(SelInst); DispInstr();} }
    }
   }

   else if (event.type == SDL_QUIT || ESCapes)
   {
    RemoveTimer();
    if (!AlertBox("Are You sure to quit? Y/N")) {ESCapes=done=false; SetTimer();} //goto retreat;}
    else {done=true;}
   }
  }
  SDL_Delay(1);
 }

 RemoveTimer();
 SDL_Quit();
 delete midiin;
 delete midiout;
 
 //WaitKeyPress();
 return 0;
}

int MouseField()
{
 int mouseX, mouseY, mouseChX, mouseChY;
 SDL_PumpEvents();
 SDL_GetMouseState(&mouseX, &mouseY);
 mouseChX=mouseX/CharSizeX;mouseChY=mouseY/CharSizeY;

 if (mouseChX>PattPosX+1 && mouseChX<PattPosX+PattDimX*9+1 && mouseChY>PattPosY+1 && mouseChY<PattPosY+2+PattDimY) return 0;
 else if (mouseChX>OrdListPosX+1 && mouseChX<OrdListPosX+OrDimX*3+1 && mouseChY>OrdListPosY+1 && mouseChY<OrdListPosY+2+OrDimY) return 1;
 else if (mouseChX>InstPosX-1 && mouseChY>InstPosY && mouseChY<InstPosY+InsDimY) return 2;
 else if (mouseChX>PattPosX+1 && mouseChX<PattPosX+PattDimX*9+1 && mouseChY<=PattPosY) return 3;
 else return 0xff;
}

//*********************************************************************************************************
//========================================== MUSIC-PLAYER ROUTINE==========================================
#define deftempo 6
int PATTCNT[TrackAmount], SEQCNT[TrackAmount];
unsigned char prevnote[TrackAmount]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, PrevJamNote=0;
int SPDCNT[TrackAmount]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int TEMPO[TrackAmount]={deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo,deftempo};
int DELAYCNT[TrackAmount]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
unsigned char PLAYEDINS[TrackAmount]={0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};
bool EndOfTune=false, EndOfTrack[TrackAmount]={false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
unsigned char F2playMarker[TrackAmount]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
bool Vibrato[TrackAmount]={false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
int SlideSpeed[TrackAmount]={0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};
int SlideCnt[TrackAmount]={0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};

int CarefulMessage(unsigned char instr)
{
 if (ExportMode==false)
 { //playback-mode
  try{ PortOut[INSTRUMENT[instr][INST_PORT]]->sendMessage( &PlayerMessage ); } catch( RtError &error ) {error.printMessage();}
  PlayerMessage.clear(); 
 }
 else
 { //MIDI-export mode
  MessageToExport(instr); 
 }
 return 0;
}

//-----------------------------------------------
void NoteOn(unsigned char instr, unsigned char note, unsigned char velo)
{
 unsigned int mulvelo = (INSTRUMENT[instr][INST_CHVOL]&0xF) ? (velo*(INSTRUMENT[instr][INST_CHVOL]&0xF))/16 : velo; //0 means full volume
 PlayerMessage.push_back(0x90+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(note-1); PlayerMessage.push_back(mulvelo);
 CarefulMessage(instr);
}

void NoteOff(unsigned char instr, unsigned char note, unsigned char velo)
{
 unsigned int mulvelo = (INSTRUMENT[instr][INST_CHVOL]&0xF) ? (velo*(INSTRUMENT[instr][INST_CHVOL]&0xF))/16 : velo; //0 means full volume
 PlayerMessage.push_back(0x80+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(note-1); PlayerMessage.push_back(mulvelo);
 CarefulMessage(instr);
}

void AllNotesOff(unsigned char instr)
{
 PlayerMessage.push_back(0xB0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(0x7B); PlayerMessage.push_back(0);
 CarefulMessage(instr);
}

void SelectIns(unsigned char instr)
{
 if (!INSTRUMENT[instr][0] && !INSTRUMENT[instr][1] && !INSTRUMENT[instr][2]) return; //avoid empty instrument (can cause loopback) //if (instr<1 || instr>0x80) return;
 PlayerMessage.push_back(0xC0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(INSTRUMENT[instr][INST_PATCH]-1); //instruments in MIDItrk start from 1
 CarefulMessage(instr);
}

void SetVolume(unsigned char instr, unsigned char volume)
{
 PlayerMessage.push_back(0xB0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(0x07); PlayerMessage.push_back(volume);
 CarefulMessage(instr); 
}

void SetAfterTouch(unsigned char instr, unsigned char aftertouch)
{
 PlayerMessage.push_back(0xD0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(aftertouch);
 CarefulMessage(instr);
}

void SetPitchWheel(unsigned char instr, unsigned int pitchwheel) //pitchwheel walue: 0000...2000(middle)...3FFF
{
 PlayerMessage.push_back(0xE0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(pitchwheel&0x1F); PlayerMessage.push_back(pitchwheel/0x80);
 CarefulMessage(instr);
}

void UniqueCC(unsigned char instr, unsigned char CCnum, unsigned char value)
{
 PlayerMessage.push_back(0xB0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(CCnum); PlayerMessage.push_back(value);
 CarefulMessage(instr); 
}

void SetPortamento(unsigned char instr, unsigned char time)
{
 PlayerMessage.push_back(0xB0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(0x05); PlayerMessage.push_back(time);
 PlayerMessage.push_back(0xB0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(65); PlayerMessage.push_back((time)?64:63);
 CarefulMessage(instr); 
}

unsigned char SmallCCFXlist[16]={
                  0x00, //0 select bank
                  0x02, //1 breath controller
                  0x04, //2 foot controller
                  0x05, //3 portamento-time 
                  0x08, //4 balance
                  0x0a, //5 panning
                  0x0b, //6 expression
                  0x0c, //7 effect1
                  0x0d, //8 effect2
                  0x10, //9 general purpose 1
                  0x11, //A general purpose 2
                  0x44, //B switches: Damper Pedal(0x40), Portamento On/Off (0x41), Sustenuto On/Off (0x42), Soft Pedal On/Off (0x43), Legato footswitch (0x44), Hold 2 (0x45)
                  0x5b, //C Effect1 Reverb level
                  0x5c, //D Effect2 Tremolo level
                  0x5d, //E Effect3 Chorus level
                  0x5e};//F Effect4 Detune level

void SmallCCFX(unsigned char instr, unsigned char CCfx)
{
 PlayerMessage.push_back(0xB0+(INSTRUMENT[instr][INST_CHVOL]/16)); PlayerMessage.push_back(SmallCCFXlist[CCfx/16]); PlayerMessage.push_back((CCfx&0xF)*8);
 CarefulMessage(instr); 
}

void KUSS()
{
 int i;
 for (i=0;i<MaxInstAmount;i++) if(INSTRUMENT[i][0]||INSTRUMENT[i][1]||INSTRUMENT[i][2]) AllNotesOff(i);
}

void InitRoutine(bool PlayFromBeginning)
{
 int i;
 if (ExportMode==false) KUSS();
 for (i=0;i<TrackAmount;i++)
 {
  SEQCNT[i]=(PlayFromBeginning)? 0 : F2playMarker[i];
  PATTCNT[i]=0; Vibrato[i]=false; SlideSpeed[i]=SlideCnt[i]=0;
  TEMPO[i]=deftempo; SPDCNT[i]=TEMPO[i]; DELAYCNT[i]=-1;
  PLAYEDINS[i]=DefaultIns[i]; SelectIns(PLAYEDINS[i]); SetVolume(PLAYEDINS[i],0x7F);
  //UniqueCC(PLAYEDINS[i],01,0x00); SetPitchWheel(PLAYEDINS[i],0x2000); //reset by new notes
  EndOfTrack[i]=false;
  DisPattCnt(i);
  
  if (FollowPlay) { selpatt[i]=SEQUENCE[i][SEQCNT[i]]; }
 }
 if (FollowPlay) pattpos=0; DisPattData();
}


void ContiPlay(int i)
{
 if (SlideSpeed[i]!=0) { SlideCnt[i] += SlideSpeed[i]*16; SetPitchWheel(PLAYEDINS[i],0x2000+SlideCnt[i]);}
}

void PlayRoutine()
{
 int h,i,j,chptn;
 unsigned char notedata,fxdata,fxvalue;
 for (h=0;h<fastfwd*ffwdspeed+1;h++)
 {
  for (i=TrackAmount-1;i>=0;i--)  //i counts backwards: 1st channel has the priority for common effects like tempo-change
  {
   if (EndOfTrack[i]) continue;
   if (DELAYCNT[i]>0) {DELAYCNT[i]--; continue;}
   if (SPDCNT[i]<=TEMPO[i]) SPDCNT[i]++; 
   else 
   {
    if (PlayMode!=2) chptn=SEQUENCE[i][SEQCNT[i]]; else chptn=selpatt[i];
    notedata=PATTERNS[chptn][0][PATTCNT[i]];
    fxdata=PATTERNS[chptn][1][PATTCNT[i]]; fxvalue=PATTERNS[chptn][2][PATTCNT[i]];

    if (DELAYCNT[i]==-1) 
    { 
     if ((fxdata&0xf)==0x9) {DELAYCNT[i] = fxvalue; continue;}   //delay note by given frames, 
     SPDCNT[i]=0;
    }   
    else if (DELAYCNT[i]==0) 
    {
     DELAYCNT[i]=-1; if (fxvalue<TEMPO[i]) SPDCNT[i]=fxvalue+1;  //but regain tempo afterwards, if possible
    }


    switch (fxdata&0xF) 
    {
     case 0x5: UniqueCC(PLAYEDINS[i],73,fxvalue); //Attack
            break;
     case 0x6: UniqueCC(PLAYEDINS[i],75,fxvalue); //Decay
            break;
     case 0x7: UniqueCC(PLAYEDINS[i],72,fxvalue); //Release
            break;
     case 0x8: UniqueCC(PLAYEDINS[i],76,fxvalue); //Vibrato Rate
            break;
     
     case 0xA: SetVolume(PLAYEDINS[i],fxvalue);
            break;
     case 0xB: SmallCCFX(PLAYEDINS[i],fxvalue);
            break;
     case 0xC: PLAYEDINS[i]=fxvalue; SelectIns(fxvalue); DispTrkInfo();
            break;
     case 0xD: SetAfterTouch(PLAYEDINS[i],fxvalue);
            break;
     case 0xE: SetPitchWheel(PLAYEDINS[i],fxvalue*64); SlideSpeed[i]=SlideCnt[i]=0;
            break;
     case 0xF: if (fxvalue<0x80) for(j=0;j<TrackAmount;j++) { TEMPO[j]=fxvalue; } //SPDCNT[j]=TEMPO[j];}
               else { TEMPO[i]=fxvalue&0x7f; } //SPDCNT[i]=TEMPO[i];}
            break;
     default: break;
    }
    
    if (notedata>0)
    { 
     if (notedata<NOTE_MAX)
     { 
      if (prevnote[i]>0) NoteOff(PLAYEDINS[i],prevnote[i],0x0); 
      if (mutesolo[i]) { NoteOn(PLAYEDINS[i],notedata,((fxdata/16-1)&0xF)*8); prevnote[i]=notedata; }
      if (Vibrato[i]) {UniqueCC(PLAYEDINS[i],01,0x00); Vibrato[i]=false;} //new note resets Modulation wheel (Vibrato Amplitude)
      if (SlideSpeed[i]!=0 || SlideCnt[i]!=0) {SetPitchWheel(PLAYEDINS[i],0x2000); SlideSpeed[i]=SlideCnt[i]=0;}
     }
     else if (notedata==GATEOFF_NOTEFX && prevnote[i]>0) NoteOff(PLAYEDINS[i],prevnote[i],0x0); 
     else if (mutesolo[i] && notedata==GATEON_NOTEFX) NoteOn(PLAYEDINS[i],prevnote[i],((fxdata/16-1)&0xF)*8); 
    }
    
    switch (fxdata&0xF) //After-effects
    {
     case 0x1: SlideSpeed[i]=fxvalue;
            break;
     case 0x2: SlideSpeed[i]=fxvalue*-1;
            break;
     case 0x3: SetPortamento(PLAYEDINS[i],fxvalue);
            break;
     case 0x4: UniqueCC(PLAYEDINS[i],01,fxvalue); Vibrato[i]=true; //Modulation wheel (Vibrato Amplitude)
            break;
     default: break;
    }

    if (!FollowPlay) DisPattCnt(i);

    if (PATTCNT[i]<PATTLENG[chptn]-1) PATTCNT[i]++;
    else 
    {
     PATTCNT[i]=0; 
     if (PlayMode!=2) //check if pattern-play mode
     {
      SEQCNT[i]++; 
      if (SEQUENCE[i][SEQCNT[i]]==ORDERLIST_FX_JUMP) 
      {
       if (ExportMode) EndOfTrack[i]=true;
       else SEQCNT[i]=SEQUENCE[i][SEQCNT[i]+1];
      }
      else if (SEQUENCE[i][SEQCNT[i]]==ORDERLIST_FX_END) EndOfTrack[i]=true;
      if (FollowPlay && !EndOfTrack[i]) 
      { 
       if (SEQUENCE[i][SEQCNT[i]]<ORDERLIST_FX_MIN) 
       {
        selpatt[i]=SEQUENCE[i][SEQCNT[i]]; pattpos=0;
       }
      }
     }
     else if (FollowPlay) pattpos=0;
     DispSeqCnt(i);
    }

   } //SPDCNT
   ContiPlay(i);
  } //i
  
  EndOfTune=true; for (i=0;i<TrackAmount;i++) if (!EndOfTrack[i]) EndOfTune=false;
  if (EndOfTune && FollowPlay) {FollowPlay=false; DisPattData();}
  
  if (FollowPlay && SPDCNT[WinPos1[0]+TrkPos]==0 && !EndOfTune) 
  {
   if (PATTCNT[WinPos1[0]+TrkPos] >= PattDimY/2 && PATTCNT[WinPos1[0]+TrkPos]-1 < PATTLENG[selpatt[WinPos1[0]+TrkPos]]-PattDimY/2) 
   {
    pattpos=PATTCNT[WinPos1[0]+TrkPos]-PattDimY/2; WinPos2[0]=PattDimY/2-1;
   }
   else 
   { 
    if (PATTCNT[WinPos1[0]+TrkPos] < PattDimY/2) pattpos=0; 
    else pattpos=PATTLENG[selpatt[WinPos1[0]+TrkPos]]-PattDimY;
    WinPos2[0]=(PATTCNT[WinPos1[0]+TrkPos]-pattpos)-1;
   }
   if (WinPos2[0]<0) WinPos2[0]=0; //safety check
   if (WinPos2[0]>=PattDimY) WinPos2[0]=PattDimY-1; //safety check
   DisPattData();
  }
  
 }
 
}

//**********************************************************************************************
//=============================== FUNCTIONS ====================================================
Uint32 StartTime=0,StopTime=0;

//timer-callback to tell main loop when to call music routine & keyhandler
Uint32 timerCallback(Uint32 interval, void *param)
{
  SDL_Event event;

  event.type = SDL_USEREVENT;
  event.user.code = TIMEREVENT_CODE;
  event.user.data1 = NULL; //(void *)SDL_GetTicks();
  event.user.data2 = NULL; //(void *)0;

  SDL_PushEvent(&event);

  return interval;
}

void SetTimer()
{
 FrameClk = SDL_AddTimer(TimerInterval, timerCallback,NULL);
}

void RemoveTimer()
{
 SDL_RemoveTimer(FrameClk);
 SDL_Delay(20);
}


void DisplayTime()
{
 Uint32 diff=SDL_GetTicks()-StartTime;
 Uint32 Minutes=diff/(1000*60), Seconds=((diff-Minutes)/1000)%60;
 put2digit (6,StatPosY,Minutes);  put2digit (9,StatPosY,Seconds);
 SDL_UpdateRect(screen, (6)*CharSizeX, StatPosY*CharSizeY, 5*CharSizeX,CharSizeY);
}


//=================================================================================================
//------------------------------------Key/Mouse-handling functions--------------------------------
int repeatex()
{
 static int prevkey;
 //SDL_PollEvent(&event);
 //if (event.type==SDL_KEYDOWN ) { if (prevkey!=event.key.keysym.sym) repecnt=repspd1;}
 //prevkey=event.key.keysym.sym; 
 if (repecnt==repspd1) { repecnt--; return 0; }
 else if (repecnt==0) {repecnt=repspd2; return 0; }
 else {repecnt--; return 1;}
}

void CurLeft()
{
 if (WinPos3[Window]>0) WinPos3[Window]--;
 else 
 { 
  WinPos3[Window]=WinPos3Max[Window]; 
  if (WinPos1[Window]>0) WinPos1[Window]--; 
  else if (Window==0) 
  {
   if (TrkPos>0) TrkPos--;
   else {TrkPos=TrackAmount-PattDimX; WinPos1[0]=WinPos1Max[Window];}
   WinPos3[0]=WinPos3Max[0];
   DispOrderL(); DispTrkInfo();
  }
  else if (Window==1) if (seqpos>0) seqpos--;
 }
}
void KeyLeft()
{
 if (repeatex()) return;
 CurLeft(); RefreshCursor();
}

void CurRight()
{
 if (WinPos3[Window]<WinPos3Max[Window]) WinPos3[Window]++;
 else 
 { 
  WinPos3[Window]=0; 
  if (WinPos1[Window]<WinPos1Max[Window]) WinPos1[Window]++; 
  else if (Window==0) 
  {
   if (TrkPos+PattDimX<TrackAmount) {TrkPos++;WinPos3[0]=0;}
   else TrkPos=WinPos1[0]=WinPos3[0]=0;
   DispOrderL(); DispTrkInfo();
  }
  else if (Window==1) if (seqpos<MaxSeqLength-OrDimX) seqpos++; //else seqpos=0; 
 }
}
void KeyRight()
{
 if (repeatex()) return;
 CurRight(); RefreshCursor();
}

void CurDown()
{
 if (Window==2) { if (SelInst<MaxInstAmount-1) {SelInst++;SelectIns(SelInst);} return;}
 if (WinPos2[Window]<WinPos2Max[Window]) WinPos2[Window]++;
 else
 {
  if (Window==0) { if (pattpos<0x100-PattDimY) pattpos++; }
  else if (Window==1) if (TrkPos<TrackAmount-OrDimY) { TrkPos++; DisPattData(); DispTrkInfo(); }
 }
}
void KeyDown()
{
 if (repeatex()) return;
 CurDown(); RefreshCursor();
}

void CurUp()
{
 if (Window==2) { if (SelInst>0) {SelInst--;SelectIns(SelInst);} return;}
 if (WinPos2[Window]>0) WinPos2[Window]--;
 else
 {
  if (Window==0) { if (pattpos>0) pattpos--; }
  else if (Window==1) if (TrkPos>0) {TrkPos--;DisPattData();DispTrkInfo();}
 }
}
void KeyUp()
{
 if (repeatex()) return;
 CurUp(); RefreshCursor();
}

void CursorAdvance()
{
 if (Window==0) for (int i=0;i<Advance;i++) CurDown();
 if (Window==1) CurRight();
}

void SetSelPatt()
{
 int i;
 for (i=0;i<TrackAmount;i++) if(SEQUENCE[i][seqpos+WinPos1[1]]<ORDERLIST_FX_MIN) selpatt[i]=SEQUENCE[i][seqpos+WinPos1[1]];
}

void SoloUnsolo(int track)
{  //solo/unsolo the track
 int i;
 if (SoloState==false)
 {
  SoloState=true; 
  for(i=0;i<TrackAmount;i++)
  {
   if (i!=track) 
   {
    SoloState=true; mutesolo[i]=false;
    if (prevnote[i]>0) NoteOff(PLAYEDINS[i],prevnote[i],0);
   }
   else mutesolo[i]=true;
  }
 }
 else
 {
  SoloState=false; for(i=0;i<TrackAmount;i++) mutesolo[i]=true;
 }
 DisPattData();
}

//-------------------------------------
bool SHIFTstate,CTRLstate,ALTstate;
void KeyHandler()
{
 int i,j;
 //int mouseX, mouseY;
 //Uint8 mousebutt;
 
 SDL_PumpEvents();
 keystate = SDL_GetKeyState(NULL);
 //mousebutt= SDL_GetMouseState(&mouseX, &mouseY);
 SHIFTstate = (keystate[SDLK_LSHIFT] || keystate[SDLK_RSHIFT]);
 CTRLstate = (keystate[SDLK_LCTRL] || keystate[SDLK_RCTRL]);
 ALTstate = (keystate[SDLK_LALT] || keystate[SDLK_RALT]);
 
 //continuous-response keys
 if (keystate[SDLK_LEFT]||keystate[SDLK_RIGHT]||keystate[SDLK_UP]||keystate[SDLK_DOWN])
 {
  if(keystate[SDLK_LEFT]) KeyLeft();
  if(keystate[SDLK_RIGHT]) KeyRight();
  if(keystate[SDLK_UP]) KeyUp();
  if(keystate[SDLK_DOWN]) KeyDown();
 }
 else if(keystate[SDLK_F5])
 {
  if (repeatex()==0) { Window=0;Display(); }
 }
 else if(keystate[SDLK_F6])
 {
  if (repeatex()==0) { Window=1;Display(); }
 }
 else if(keystate[SDLK_F7])
 {
  if (repeatex()==0) { Window=2;Display(); }
 }
 else if (keystate[SDLK_SPACE]) 
 {
  if(repeatex()==0) 
  {
   if (!SHIFTstate && !CTRLstate) { KeyMode=(KeyMode)?0:1; CurColor=0; DisplaySettings(); }
   else
   {
    if (Window==0)
    { //play patterns from cursor-position
     for(i=0;i<TrackAmount;i++) { PATTCNT[i]=pattpos+WinPos2[0]; SPDCNT[i]=0; }
     PlayMode=2; StartTime=SDL_GetTicks();
    }
    else if (Window==1) 
    { //set marker for F2 playback
     if (CTRLstate) F2playMarker[WinPos2[1]+TrkPos]=seqpos+WinPos1[1];
     else for (i=0;i<TrackAmount;i++) F2playMarker[i]=seqpos+WinPos1[1];
     DispOrderL();
    }
   } 
  }
 }
 else if (keystate[SDLK_TAB])
 {
  if(repeatex()==0) 
  {
   if (!SHIFTstate) 
   { //TAB forward 
    if (WinPos1[Window]<WinPos1Max[Window]) 
    {
     WinPos1[Window]++; RefreshCursor();
    } 
    else 
    { //scroll tracks
     if (Window==0) { if (TrkPos+PattDimX<TrackAmount) TrkPos++; else TrkPos=WinPos1[0]=0; DisPattData();DispOrderL();DispTrkInfo(); }
     else { WinPos1[Window]=0; RefreshCursor();}
    } 
   }
   else 
   { //TAB backward
    if (WinPos1[Window]>0) 
    {
     WinPos1[Window]--; RefreshCursor();
    } 
    else 
    { //scroll tracks
     if (Window==0) { if (TrkPos>0) TrkPos--; else {TrkPos=TrackAmount-PattDimX;WinPos1[0]=WinPos1Max[0];} DisPattData();DispOrderL();DispTrkInfo(); }
     else { WinPos1[Window]=WinPos1Max[Window]; RefreshCursor(); }
     
    }
   }
  }
 } 
 else if (keystate[SDLK_PAGEDOWN])
 {
  if(repeatex()==0) 
  {
   if (Window==0 || Window==2) for (i=0;i<8;i++) CurDown();  else for (i=0;i<4*2;i++)CurRight(); 
   RefreshCursor();
  }
 }
 else if (keystate[SDLK_PAGEUP])
 {
  if(repeatex()==0) 
  {
   if (Window==0 || Window==2) for(i=0;i<8;i++) CurUp();   else for(i=0;i<4*2;i++) CurLeft();
   RefreshCursor();
  }
 }
 else if (keystate[SDLK_HOME])
 {
  if(repeatex()==0) 
  {
   if (Window==0) 
   {
    if (pattpos==0 && WinPos2[0]==0) TrkPos=WinPos1[0]=WinPos3[0]=0;
    else if (WinPos2[0]>0) { WinPos2[0]=WinPos3[0]=0;} else pattpos=0;
   }
   else if (Window==1) if (WinPos1[1]>0) { WinPos1[1]=WinPos3[1]=0;} else seqpos=0;
   else if (Window==2) {SelInst=WinPos1[2]=WinPos3[2]=0;SelectIns(SelInst);}
   RefreshCursor();
  }
 }
 else if (keystate[SDLK_END])
 {
  if(repeatex()==0) 
  {
   if (Window==0) if (WinPos2[0]<WinPos2Max[0]) { WinPos2[0]=WinPos2Max[0]; WinPos3[0]=0;} else pattpos=PATTLENG[selpatt[WinPos1[0]+TrkPos]]-PattDimY;
   else if (Window==1) if (WinPos1[1]<WinPos1Max[1]) { WinPos1[1]=WinPos1Max[1]; WinPos3[1]=0;} else seqpos=MaxSeqLength-OrDimX;
   RefreshCursor();
  }
 }
 else if (keystate[SDLK_RETURN])
 {
  if(repeatex()==0) 
  {
   if (Window==0)
   {
    if (CTRLstate) {PATTLENG[selpatt[WinPos1[0]+TrkPos]]=WinPos2[0]+pattpos; DisPattData();}
    else if (WinPos3[0]==0 && KeyMode==1 && !FollowPlay) { PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [WinPos2[0]+pattpos] = (!SHIFTstate)?GATEOFF_NOTEFX:GATEON_NOTEFX; CursorAdvance();DisPattData(); }
    else if (WinPos3[0]>=2 && (PATTERNS [selpatt[WinPos1[0]+TrkPos]] [1] [WinPos2[0]+pattpos] &0xF) == 0xC ) 
    { 
     SelInst=PATTERNS [selpatt[WinPos1[0]+TrkPos]] [2] [WinPos2[0]+pattpos]; SelectIns(SelInst); Window=2; Display();
    } 
   }
   else if (Window==1) 
   { 
    SetSelPatt();
    Window=0; WinPos1[0]=WinPos2[1]; WinPos2[0]=WinPos3[0]=pattpos=0; DispOrderL(); DisPattData();
   }
  }
 }
 else if (keystate[SDLK_BACKSPACE])
 {
  if(repeatex()==0) 
  {
   if (Window==0) 
   {
    if (!FollowPlay)
    { 
     if (WinPos3[0]==0) PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [WinPos2[0]+pattpos] = 0; 
     else if (WinPos3[0]==1) PATTERNS [selpatt[WinPos1[0]+TrkPos]] [1] [WinPos2[0]+pattpos] &= 0x0F;
     else if (WinPos3[0]>=2) { PATTERNS [selpatt[WinPos1[0]+TrkPos]] [1] [WinPos2[0]+pattpos] &= 0xF0; PATTERNS [selpatt[WinPos1[0]+TrkPos]] [2] [WinPos2[0]+pattpos] = 0; } 
     if (CTRLstate) { for (i=0;i<=2;i++) PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [WinPos2[0]+pattpos] = 0; }
     if (!SHIFTstate) CurUp(); else CursorAdvance(); 
     DisPattData(); 
    }
   }
   if (Window==1) 
   { 
    //SEQUENCE [WinPos2[1]+TrkPos] [seqpos+WinPos1[1]] &= (WinPos3[1])?0xF0:0x0F; if (!SHIFTstate) CurLeft(); else CurRight(); DispOrderL();
    SEQUENCE [WinPos2[1]+TrkPos] [seqpos+WinPos1[1]] = ORDERLIST_FX_END; 
    if (!SHIFTstate) {CurLeft();CurLeft();} else {CurRight();CurRight();}  DispOrderL();
   }
  }
 }
 else if (keystate[SDLK_DELETE])
 {
  if(repeatex()==0) 
  {
   if (Window==0) 
   { 
    if (!FollowPlay)
    {
     if (!SHIFTstate && !CTRLstate) 
     for(i=0;i<PtnColumns;i++)
     {
      for(j=WinPos2[0]+pattpos;j<PATTLENG[selpatt[WinPos1[0]+TrkPos]]-1;j++) 
      { 
       PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [j] = PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [j+1]; 
      }
      PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [j] = 0 ;
     }
     else
     {
      PATTERNS [selpatt[WinPos1[0]+TrkPos]] [(WinPos3[0]==0)?0:1] [WinPos2[0]+pattpos] = 0;
      if (CTRLstate) { for(i=0;i<=2;i++) PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [WinPos2[0]+pattpos] = 0; }
     }
     DisPattData(); //printf("$%2x\n",PATTLENG[selpatt[WinPos1[0]+TrkPos]]);
    }
   }
   else if (Window==1)
   {
    for (i=seqpos+WinPos1[1];i<MaxSeqLength-1;i++) SEQUENCE[WinPos2[1]+TrkPos][i] = SEQUENCE[WinPos2[1]+TrkPos][i+1];    SEQUENCE[WinPos2[1]+TrkPos][i]=ORDERLIST_FX_END;  
    DispOrderL();
   }
  }
 }
 else if (keystate[SDLK_INSERT])
 {
  if(repeatex()==0) 
  {
   if (Window==0) 
   { 
    if (!FollowPlay)
    {
     for(i=0;i<PtnColumns;i++)
     {
      for(j=PATTLENG[selpatt[WinPos1[0]+TrkPos]]-1;j>=WinPos2[0]+pattpos;j--) 
      { 
       PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [j+1] = PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [j]; 
      }
      PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [j+1] = 0 ;
     }
     DisPattData();
    }
   }
   else if (Window==1)
   {
    for (i=MaxSeqLength-2;i>=seqpos+WinPos1[1];i--) SEQUENCE[WinPos2[1]+TrkPos][i+1] = SEQUENCE[WinPos2[1]+TrkPos][i];    SEQUENCE[WinPos2[1]+TrkPos][i+1]=0; //0xff;  
    DispOrderL();
   }
  }
 }
 else if (keystate[SDLK_q])
 {
  if (!CTRLstate && !SHIFTstate) EnterNote();
  else
  { //transpose down 1 note/octave
   if (Window==0 && !FollowPlay)
   {
    if (repeatex()==0)
    {
     for (i=0;i<1+11*!SHIFTstate;i++)
     {
      for (j=WinPos2[0]+pattpos;j<PATTLENG[selpatt[WinPos1[0]+TrkPos]]; j++)
      {  
       if ( (PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [j] & 0x7f) > 1 && (PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [j] &0x7f) <= NOTE_MAX ) PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [j] -= 1;
      }
     }
     DisPattData();
    }
   }
  }
 }
 else if (keystate[SDLK_w])
 {
  if (!CTRLstate && !SHIFTstate) EnterNote();
  else
  { //transpose down 1 note/octave
   if (Window==0 && !FollowPlay)
   {
    if (repeatex()==0)
    {
     for (i=0;i<1+11*!SHIFTstate;i++)
     {
      for (j=WinPos2[0]+pattpos;j<PATTLENG[selpatt[WinPos1[0]+TrkPos]]; j++)
      {  
       if ( (PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [j] &0x7f) && (PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [j] &0x7f) < NOTE_MAX) 
       PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [j]++;
      }
     }
     DisPattData();
    }
   }
  }
 }
 else if (keystate[SDLK_c])
 {
  if (!CTRLstate && !SHIFTstate) EnterHex();
  else
  {
   if(repeatex()==0) 
   {
    if (Window==0)
    {
     if (!FollowPlay)
     {
      if (CTRLstate) 
      { //copy pattern from cursor-position to clipboard
       PtClipSourcePtn=selpatt[WinPos1[0]+TrkPos]; PtClipSourcePos=WinPos2[0]+pattpos;
       for (i=0;i<PtnColumns;i++)
        for (j=0;j<PATTLENG[PtClipSourcePtn]-(PtClipSourcePos);j++) 
         PtClipBoard[i][j]=PATTERNS [PtClipSourcePtn] [i] [PtClipSourcePos+j];
        PtClipSize=PATTLENG[PtClipSourcePtn]-(PtClipSourcePos); //j;
        DisPattData();
      }
      if (SHIFTstate)
      {
       if (selpatt[WinPos1[0]+TrkPos] == PtClipSourcePtn && WinPos2[0]+pattpos>PtClipSourcePos) PtClipSize=WinPos2[0]+pattpos-PtClipSourcePos;
       DisPattData();
      }
     }
    }
    if (Window==1)
    {
     if (CTRLstate) 
     { //copy sequence from cursor-position to clipboard
      SeqClipSourceChn=WinPos2[1]+TrkPos; SeqClipSourcePos=seqpos+WinPos1[1];
      for (i=0;i<MaxSeqLength-(SeqClipSourcePos);i++) SeqClipBoard[i]=SEQUENCE[SeqClipSourceChn][i+SeqClipSourcePos];
      SeqClipSize=MaxSeqLength-(SeqClipSourcePos); //i;
     }
     if (SHIFTstate)
     {
      if (WinPos2[1]+TrkPos == SeqClipSourceChn && seqpos+WinPos1[1]>SeqClipSourcePos) SeqClipSize=seqpos+WinPos1[1]-SeqClipSourcePos;
     }
     DispOrderL();
    }
   }
  }
 }
 else if (keystate[SDLK_x])
 {
  if (Window==0)
  {
   if (!FollowPlay)
   {
    if (CTRLstate && repeatex()==0)
    { //copy pattern from cursor-position to clipboard
     for (i=0;i<PtnColumns;i++)
      for (j=0;j<PATTLENG[selpatt[WinPos1[0]+TrkPos]]-(WinPos2[0]+pattpos);j++) 
      { PtClipBoard[i][j]=PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [WinPos2[0]+pattpos+j];
        PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [WinPos2[0]+pattpos+j]=0;
      }
     PtClipSourcePtn=0xFF; PtClipSize=PATTLENG[selpatt[WinPos1[0]+TrkPos]]-(WinPos2[0]+pattpos); //j;
     DisPattData();
    }
    else EnterNote();
   }
  }
  if (Window==1 && repeatex()==0)
  {
   if (CTRLstate) 
   { //copy sequence from cursor-position to clipboard
    for (i=0;i<MaxSeqLength-(seqpos+WinPos1[1]);i++) 
    { SeqClipBoard[i]=SEQUENCE[WinPos2[1]+TrkPos][i+seqpos+WinPos1[1]]; SEQUENCE[WinPos2[1]+TrkPos][i+seqpos+WinPos1[1]]=ORDERLIST_FX_END; }
    SeqClipSize=MaxSeqLength-(seqpos+WinPos1[1]); //i;
    DispOrderL();
   }
  }
 }
 else if (keystate[SDLK_v])
 {
  if (!CTRLstate) EnterNote();
  else
  {
   if (repeatex()==0) 
   {
    if (Window==0)  
    { //paste to pattern at cursor-position from clipboard
     if (!FollowPlay)
     {
      for (i=0;i<PtnColumns;i++)
       for (j=0;j<PtClipSize && WinPos2[0]+pattpos+j<PATTLENG[selpatt[WinPos1[0]+TrkPos]]; j++) 
        PATTERNS [selpatt[WinPos1[0]+TrkPos]] [i] [WinPos2[0]+pattpos+j] = PtClipBoard[i][j];
      DisPattData();
     }
    }
    if (Window==1)  
    { //paste sequence at cursor-position from clipboard
     for (i=0;i<SeqClipSize && i<MaxSeqLength-(seqpos+WinPos1[1]);i++) SEQUENCE[WinPos2[1]+TrkPos][seqpos+WinPos1[1]+i] = SeqClipBoard[i];
     DispOrderL();
    }
   }
  }
 }
 else if (keystate[SDLK_a])
 {
  if (!SHIFTstate && Window==0 && WinPos3[0]==0 && repeatex()==0) 
  {
   if (FollowPlay==0 && KeyMode==1) {PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [WinPos2[0]+pattpos]=0; CursorAdvance(); DisPattData();} //empty note
  }
  else if (!SHIFTstate) EnterHex();
  else if (SHIFTstate && repeatex()==0) if (Advance<0x10) { Advance++; DisplaySettings(); }
 }
 else if (keystate[SDLK_z])
 {
  if (!SHIFTstate) EnterNote();
  else if(repeatex()==0) if (Advance>0) { Advance--; DisplaySettings(); }
 }
 else if (keystate[SDLK_h])
 {
  if (!SHIFTstate) EnterNote();
  else if(repeatex()==0) if (HiLight>1) { HiLight--; DisPattData(); }
 }
 else if (keystate[SDLK_j])
 {
  if (!SHIFTstate) EnterNote();
  else if (repeatex()==0) if (HiLight<0x10) { HiLight++; DisPattData(); }
 }
 else if (keystate[SDLK_KP_MULTIPLY])
 {
  if(repeatex()==0) if (Octave<9) { Octave++; DisplaySettings();}
 }
 else if (keystate[SDLK_KP_DIVIDE])
 {
  if(repeatex()==0) if (Octave>0)  { Octave--; DisplaySettings();}
 }
 else if (NumPadKeyVal()!=0xff)
 {
  if(repeatex()==0) {Octave=NumPadKeyVal(); DisplaySettings();}
 }
 else if (keystate[SDLK_PLUS]||keystate[SDLK_KP_PLUS])
 {
  if(repeatex()==0) 
  {
   if (!CTRLstate && !ALTstate)
   { 
    if (Window==2) { if (INSTRUMENT[SelInst][WinPos1[2]]<0x80) { INSTRUMENT[SelInst][WinPos1[2]]++; SelectIns(SelInst); DispInstr();} }
    else { if (SelInst<MaxInstAmount-1) {SelInst++;SelectIns(SelInst); DispInstr();} }
   }
   if (CTRLstate) if (Octave<9) Octave++;
   if (ALTstate) if (UsedInPort+1<midiin->getPortCount()) 
   { 
    UsedInPort++; 
    SetInDevice(UsedInPort); 
   }
   DisplaySettings();
  }
 }
 else if (keystate[SDLK_MINUS]||keystate[SDLK_KP_MINUS])
 {
  if(repeatex()==0) 
  {
   if (!CTRLstate && !ALTstate) 
   {
    if (Window==2) { if (INSTRUMENT[SelInst][WinPos1[2]]>0) { INSTRUMENT[SelInst][WinPos1[2]]--; SelectIns(SelInst); DispInstr();} }
    else { if (SelInst>0) {SelInst--;SelectIns(SelInst); DispInstr();} }
   }
   if (CTRLstate) if (Octave>0) Octave--;
   if (ALTstate) if (UsedInPort>0) 
   { 
    UsedInPort--; 
    SetInDevice(UsedInPort); 
   }
   DisplaySettings();
  }
 }
 else if (keystate[SDLK_F1])
 {
  if(repeatex()==0) 
  {
   if (CTRLstate || AutoFollow) FollowPlay=true; else FollowPlay=false;
   InitRoutine(true); PlayMode=1; StartTime=SDL_GetTicks();
   DispOrderL();
  }
 }
 else if (keystate[SDLK_F2])
 {
  if(repeatex()==0) 
  {
   if (CTRLstate || AutoFollow) FollowPlay=true; else FollowPlay=false;
   InitRoutine(false); PlayMode=1; StartTime=SDL_GetTicks();
  }
  DispOrderL();
 }
 else if (keystate[SDLK_F3])
 {
  if(repeatex()==0) 
  {
   for(i=0;i<TrackAmount;i++) { PATTCNT[i]=SPDCNT[i]=0; DELAYCNT[i]=-1; }
   KUSS(); PlayMode=2; StartTime=SDL_GetTicks();
   if (CTRLstate || AutoFollow) {FollowPlay=true; pattpos=0; DisPattData();}  else FollowPlay=false;
  }
 }
 else if (keystate[SDLK_F4])
 {
  if(repeatex()==0) 
  {
   if (PlayMode==0) 
   {
    PlayMode=PrevPlayMode; 
    if (CTRLstate || AutoFollow) { FollowPlay=true; for(i=0;i<TrackAmount;i++) selpatt[i]=SEQUENCE[i][SEQCNT[i]]; DisPattData(); } 
    else FollowPlay=false;
    DispTrkInfo(); 
   }
   else 
   {
    PrevPlayMode=PlayMode; PlayMode=0; FollowPlay=false; StopTime=SDL_GetTicks();
    for (i=0;i<TrackAmount;i++) { if (prevnote[i]>0) NoteOff(PLAYEDINS[i],prevnote[i],0);} //AllNotesOff (PLAYEDINS[i]);}
    KUSS();
    DispTrkInfo();
   }
  }
 }
 else if (keystate[SDLK_COMMA])
 {
  if (!ALTstate) EnterNote();
  else
  {
   if(repeatex()==0) 
   {
    if (DefaultIns[WinPos1[0]+TrkPos]>0) DefaultIns[WinPos1[0]+TrkPos]--;
    DispTrkInfo();
   }
  }
 }
 else if (keystate[SDLK_PERIOD])
 {
  if (!ALTstate) EnterNote();
  else
  {
   if(repeatex()==0) 
   {
    if (DefaultIns[WinPos1[0]+TrkPos]+1<0x80) DefaultIns[WinPos1[0]+TrkPos]++;
    DispTrkInfo();
   }
  }
 }
 else if (keystate[SDLK_BACKQUOTE] || keystate[0x13a] || keystate[SDLK_KP_ENTER]) //for dvorak/us/hu layouts
 {
  if (!SHIFTstate && !CTRLstate) fastfwd=true;
  else if (SHIFTstate)
  { 
   if (repeatex()==0) 
   { 
    if (FollowPlay) FollowPlay=false;
    else 
    { 
     FollowPlay=true; 
     for(i=0;i<TrackAmount;i++) if(PlayMode!=2) selpatt[i]=SEQUENCE[i][SEQCNT[i]]; 
     Display(); 
    } 
   }
  }
  else if (CTRLstate) if (repeatex()==0) { AutoFollow=(AutoFollow)?false:true; DisplaySettings(); }
 }
 else if (keystate[SDLK_m])
 { //mute/unmute the track
  if (SHIFTstate)
  {
   if (repeatex()==0)
   {
    if  ( mutesolo[WinPos1[0]+TrkPos] ) 
    { 
     mutesolo[WinPos1[0]+TrkPos] = false; 
     if (prevnote[WinPos1[0]+TrkPos]>0) NoteOff(PLAYEDINS[WinPos1[0]+TrkPos],prevnote[WinPos1[0]+TrkPos],0);
    }
    else mutesolo[WinPos1[0]+TrkPos]=true;
    DisPattData();
   }
  }
  else EnterNote();
 }
 else if (keystate[SDLK_s])
 {
  if (SHIFTstate)
  {
   if (repeatex()==0) SoloUnsolo(WinPos1[0]+TrkPos);
  }
  else EnterNote();
 }
 else if (keystate[SDLK_e])
 {
  if (!CTRLstate) EnterHex();
  else
  {
   if (Window==1 && repeatex()==0)
   { //find and place first unused pattern
    int yetmax=1;
    for (i=0;i<TrackAmount;i++)
    {
     for (j=0;j<MaxSeqLength;j++)
     {
      if (SEQUENCE[i][j]<ORDERLIST_FX_MIN && SEQUENCE[i][j]>yetmax) yetmax=SEQUENCE[i][j];
     }
    }
    if (yetmax<ORDERLIST_FX_MIN) {SEQUENCE[WinPos2[1]][WinPos1[1]+seqpos]=yetmax+1; DispOrderL();}
   }
  }
 }
 else if (HexKeyVal()!=0xFF)
 {
  EnterHex();
 }
 else if (NoteKeyVal()!=0xFF)
 {
  EnterNote();
 }
 else
 { //no (useful) key pressed
  repecnt=repspd1; fastfwd=false;
  if (PrevJamNote) {NoteOff(SelInst,PrevJamNote,0x7f); PrevJamNote=0; if(PlayMode==0)AllNotesOff(SelInst); } //prevnote[WinPos1[0]+TrkPos]=0;}
 }
}

//----------------------------------------
void KeyInstNote()
{
 if (NoteKeyVal()<0x80 && PrevJamNote!=NoteKeyVal()&0x7F)
 {
  if (PrevJamNote&0x7F!=0) NoteOff(SelInst,PrevJamNote,0x7F); //if notes played legato (without break inbetween)
  NoteOn(SelInst,NoteKeyVal(),0x7f); UniqueCC(SelInst,01,0x00); //new note resets Modulation wheel (Vibrato Amplitude)
 }
 PrevJamNote=NoteKeyVal()&0x7F;
}

void EnterNote()
{
 //if (NoteKeyVal==0) return;
    if (KeyMode==1 && Window==0 && WinPos3[0]==0)
    { //edit
     if (!FollowPlay)
     { 
      if(repeatex()==0 && NoteKeyVal()!=0xFF && (NoteKeyVal()&0x7f)<=NOTE_MAX) 
      {
       PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [WinPos2[0]+pattpos]=NoteKeyVal()&0x7F;
       //PATTERNS [selpatt[WinPos1[0]+TrkPos]] [1] [WinPos2[0]+pattpos] &= 0x0F;
       //PATTERNS [selpatt[WinPos1[0]+TrkPos]] [1] [WinPos2[0]+pattpos] |= 0xF0;
       KeyInstNote();
       CursorAdvance();DisPattData();
      }
     }
    }
    else
    { //jam
     KeyInstNote();
    }
}

void EnterHex()
{
 int i;
 if (SHIFTstate)
 { //mute/unmute track 1..9
  if (HexKeyVal()>0 && HexKeyVal()<=9 && repeatex()==0) 
  { 
   if  ( mutesolo[HexKeyVal()-1] ) 
   { 
    mutesolo[HexKeyVal()-1] = false; 
    if (prevnote[HexKeyVal()-1]>0) NoteOff(PLAYEDINS[HexKeyVal()-1],prevnote[HexKeyVal()-1],0);
   }
   else mutesolo[HexKeyVal()-1]=true;
   DisPattData();
  }
  return;
 }
 if (CTRLstate) 
 { 
  if (HexKeyVal()<10 && repeatex()==0) { Octave=HexKeyVal(); DisplaySettings(); }  
  return; 
 }
 if (Window==0)
 {
  if (WinPos3[0]>0)
  { //hex.entry
   if (!FollowPlay)
   {
    if(repeatex()==0) 
    {
     if ((WinPos3[0]-1)&1) 
     {
      PATTERNS [selpatt[WinPos1[0]+TrkPos]] [(WinPos3[0]-1)/2+1] [WinPos2[0]+pattpos] &= 0xF0;
      PATTERNS [selpatt[WinPos1[0]+TrkPos]] [(WinPos3[0]-1)/2+1] [WinPos2[0]+pattpos] |= HexKeyVal();
      if (WinPos3[0]==2) CurRight();
     }
     else 
     {
      PATTERNS [selpatt[WinPos1[0]+TrkPos]] [(WinPos3[0]-1)/2+1] [WinPos2[0]+pattpos] &= 0x0F;
      PATTERNS [selpatt[WinPos1[0]+TrkPos]] [(WinPos3[0]-1)/2+1] [WinPos2[0]+pattpos] |= HexKeyVal()*16;
      if (WinPos3[0]==3) CurRight(); 
      else if (WinPos3[0]=1) CursorAdvance();
     }
     DisPattData();
    }
   }
  }
  else
  { //note entry/jam
   if (HexKeyVal()==1 && FollowPlay==0 && KeyMode==1 ) { if (repeatex()==0) {PATTERNS [selpatt[WinPos1[0]+TrkPos]] [0] [WinPos2[0]+pattpos]=0; CursorAdvance(); DisPattData();} } //empty note
   else EnterNote();
  }
 }
 else if (Window==1)
 {
  if (repeatex()==0) 
  {
   if (SEQUENCE[WinPos2[1]+TrkPos][seqpos+WinPos1[1]]==0xFF) SEQUENCE[WinPos2[1]+TrkPos][seqpos+WinPos1[1]]=0x00;
   if (WinPos3[1]==0) {SEQUENCE[WinPos2[1]+TrkPos][seqpos+WinPos1[1]] &=0x0F; SEQUENCE[WinPos2[1]+TrkPos][seqpos+WinPos1[1]] |= HexKeyVal()*16; CurRight();} 
   else  {SEQUENCE[WinPos2[1]+TrkPos][seqpos+WinPos1[1]] &=0xF0; SEQUENCE[WinPos2[1]+TrkPos][seqpos+WinPos1[1]] |= HexKeyVal(); } 
   DispOrderL(); 
  } 
 }
 else if (Window==2)
 {
  if (repeatex()==0) 
  {
   if (WinPos3[2]==0) {INSTRUMENT[SelInst][WinPos1[2]] &=0x0F; INSTRUMENT[SelInst][WinPos1[2]] |= HexKeyVal()*16; CurRight();} 
   else  {INSTRUMENT[SelInst][WinPos1[2]]  &=0xF0; INSTRUMENT[SelInst][WinPos1[2]]  |= HexKeyVal(); if(WinPos1[2]==WinPos1Max[2]) WinPos3[2]--;} 
   SelectIns(SelInst); DispInstr(); 
  } 
 }
}

unsigned char HexKeys[17]={SDLK_0,SDLK_1,SDLK_2,SDLK_3,SDLK_4,SDLK_5,SDLK_6,SDLK_7,SDLK_8,SDLK_9,SDLK_a,SDLK_b,SDLK_c,SDLK_d,SDLK_e,SDLK_f,0};

unsigned char HexKeyVal()
{
 int i;
 for (i=0;i<=16;i++)
 {
  if (keystate[HexKeys[i]]) break;
 }
 if (i>16) return 0xff;
 return i;
}

unsigned int NumPadNumKeys[11]={SDLK_KP0,SDLK_KP1,SDLK_KP2,SDLK_KP3,SDLK_KP4,SDLK_KP5,SDLK_KP6,SDLK_KP7,SDLK_KP8,SDLK_KP9,0}; 
unsigned char NumPadKeyVal()
{
 int i;
 for (i=0;i<=10;i++)
 {
  if (keystate[NumPadNumKeys[i]]) break;
 }
 if (i>10) return 0xff;
 return i;
}

#define NoteKeyAmount 30
//English (US) piano keycodes
unsigned char NoteKeys[NoteKeyAmount]={SDLK_z,SDLK_s,SDLK_x,SDLK_d,SDLK_c,SDLK_v,SDLK_g,SDLK_b,SDLK_h,SDLK_n,SDLK_j,SDLK_m,SDLK_q,SDLK_2,SDLK_w,SDLK_3,SDLK_e,SDLK_r,SDLK_5,SDLK_t,SDLK_6,SDLK_y,SDLK_7,SDLK_u,SDLK_i,SDLK_9,SDLK_o,SDLK_0,SDLK_p,0};
unsigned char NoteKey2[4]={SDLK_COMMA,SDLK_l,SDLK_PERIOD,0};

//English DVORAK piano keycodes
//unsigned char NoteKeys[NoteKeyAmount]={SDLK_SEMICOLON,SDLK_o,SDLK_q,SDLK_e,SDLK_j,SDLK_k,SDLK_i,SDLK_x,SDLK_d,SDLK_b,SDLK_h,SDLK_m,0x27,SDLK_2,SDLK_COMMA,SDLK_3,SDLK_PERIOD,SDLK_p,SDLK_5,SDLK_y,SDLK_6,SDLK_f,SDLK_7,SDLK_g,SDLK_c,SDLK_9,SDLK_r,SDLK_0,SDLK_l};
//unsigned char NoteKey2[4]={SDLK_w,SDLK_n,SDLK_v,0};

//Hungarian (QWERTZ) piano keycodes
//unsigned char NoteKeys[NoteKeyAmount]={SDLK_y,SDLK_s,SDLK_x,SDLK_d,SDLK_c,SDLK_v,SDLK_g,SDLK_b,SDLK_h,SDLK_n,SDLK_j,SDLK_m,SDLK_q,SDLK_2,SDLK_w,SDLK_3,SDLK_e,SDLK_r,SDLK_5,SDLK_t,SDLK_6,SDLK_z,SDLK_7,SDLK_u,SDLK_i,SDLK_9,SDLK_o,0xF6,SDLK_p,0};
//unsigned char NoteKey2[4]={SDLK_COMMA,SDLK_l,SDLK_PERIOD,0};

//SDL1.3+:scancodes
//unsigned char NoteKeys[24]={SDL_SCANCODE_Z,SDL_SCANCODE_S,SDL_SCANCODE_X,SDL_SCANCODE_D,SDL_SCANCODE_C,SDL_SCANCODE_V,SDL_SCANCODE_G,SDL_SCANCODE_B,SDL_SCANCODE_H,SDL_SCANCODE_N,SDL_SCANCODE_J,SDL_SCANCODE_M,SDL_SCANCODE_Q,SDL_SCANCODE_2,SDL_SCANCODE_W,SDL_SCANCODE_3,SDL_SCANCODE_E,SDL_SCANCODE_R,SDL_SCANCODE_5,SDL_SCANCODE_T,SDL_SCANCODE_6,SDL_SCANCODE_Y,SDL_SCANCODE_7,SDL_SCANCODE_U,SDL_SCANCODE_I,SDL_SCANCODE_9,SDL_SCANCODE_O,SDL_SCANCODE_0,SDL_SCANCODE_P};
unsigned char NoteKeyVal()
{
 int i;
 if (DispNote!=0) return DispNote; //|0x80; //this prevents extra software-audition of MIDI-keyboard 
 for (i=0;i<=NoteKeyAmount-2;i++) if (keystate[NoteKeys[i]]) break;
 if (i>NoteKeyAmount-2) 
 {
  for(i=12;i<=12+3;i++) if (keystate[NoteKey2[i-12]]) break;
  if (i>12+3) return 0xff;
 }
 return i+Octave*12+1;
}


//====================================================================================================
//---------------------------------- GUI displayer functions -----------------------------------------

void DispTrkInfo()
{
 int i;
 for (i=0;i<PattDimX;i++)
 {
  PutString (PattPosX+2+i*9,1," Port  :"); put2hex(PattPosX+7+i*9,1,(PlayMode)?INSTRUMENT[PLAYEDINS[i+TrkPos]+1][INST_PORT]:INSTRUMENT[DefaultIns[i+TrkPos]][INST_PORT]);
  PutString (PattPosX+2+i*9,2,midiout->getPortName((PlayMode)?INSTRUMENT[PLAYEDINS[i+TrkPos]][INST_PORT]:INSTRUMENT[DefaultIns[i+TrkPos]][INST_PORT]),8 );
  PutString (PattPosX+2+i*9,3,"Ins00-C0"); put2hex(PattPosX+5+i*9,3,(PlayMode)?PLAYEDINS[i+TrkPos]:DefaultIns[i+TrkPos]); 
  put1hex(PattPosX+9+i*9,3,(PlayMode)?INSTRUMENT[PLAYEDINS[i+TrkPos]][INST_CHVOL]/16:INSTRUMENT[DefaultIns[i+TrkPos]][INST_CHVOL]/16);
 }
 SDL_UpdateRect(screen, (PattPosX-2)*CharSizeX, 0, (PattDimX*9+5)*CharSizeX, 4*CharSizeY);
}

const char NoteString[256][5]={ "...:",
 "C-0:","C#0:","D-0:","D#0:","E-0:","F-0:","F#0:","G-0:","G#0:","A-0:","A#0:","H-0:",
 "C-1:","C#1:","D-1:","D#1:","E-1:","F-1:","F#1:","G-1:","G#1:","A-1:","A#1:","H-1:",
 "C-2:","C#2:","D-2:","D#2:","E-2:","F-2:","F#2:","G-2:","G#2:","A-2:","A#2:","H-2:",
 "C-3:","C#3:","D-3:","D#3:","E-3:","F-3:","F#3:","G-3:","G#3:","A-3:","A#3:","H-3:",
 "C-4:","C#4:","D-4:","D#4:","E-4:","F-4:","F#4:","G-4:","G#4:","A-4:","A#4:","H-4:",
 "C-5:","C#5:","D-5:","D#5:","E-5:","F-5:","F#5:","G-5:","G#5:","A-5:","A#5:","H-5:",
 "C-6:","C#6:","D-6:","D#6:","E-6:","F-6:","F#6:","G-6:","G#6:","A-6:","A#6:","H-6:",
 "C-7:","C#7:","D-7:","D#7:","E-7:","F-7:","F#7:","G-7:","G#7:","A-7:","A#7:","H-7:",
 "C-8:","C#8:","D-8:","D#8:","E-8:","F-8:","F#8:","G-8:","G#8:","A-8:","A#8:","H-8:",
 "C-9:","C#9:","D-9:","D#9:","E-9:","F-9:","F#9:","G-9:","G#9:","A-9:","A#9:","H-9:",
 ".v0:",".v1:",".v2:",".v3:",".v4:",".v5:",".v6:",".v7:", 
 "C-0]","C#0]","D-0]","D#0]","E-0]","F-0]","F#0]","G-0]","G#0]","A-0]","A#0]","H-0]",
 "C-1]","C#1]","D-1]","D#1]","E-1]","F-1]","F#1]","G-1]","G#1]","A-1]","A#1]","H-1]",
 "C-2]","C#2]","D-2]","D#2]","E-2]","F-2]","F#2]","G-2]","G#2]","A-2]","A#2]","H-2]",
 "C-3]","C#3]","D-3]","D#3]","E-3]","F-3]","F#3]","G-3]","G#3]","A-3]","A#3]","H-3]",
 "C-4]","C#4]","D-4]","D#4]","E-4]","F-4]","F#4]","G-4]","G#4]","A-4]","A#4]","H-4]",
 "C-5]","C#5]","D-5]","D#5]","E-5]","F-5]","F#5]","G-5]","G#5]","A-5]","A#5]","H-5]",
 "C-6]","C#6]","D-6]","D#6]","E-6]","F-6]","F#6]","G-6]","G#6]","A-6]","A#6]","H-6]",
 "C-7]","C#7]","D-7]","D#7]","E-7]","F-7]","F#7]","G-7]","G#7]","A-7]","A#7]","H-7]",
 "C-8]","C#8]","D-8]","D#8]","E-8]","F-8]","F#8]","G-8]","G#8]","A-8]","A#8]","H-8]",
 "C-9]","C#9]","D-9]","D#9]","E-9]","F-9]","F#9]","G-9]","G#9]","A-9]","A#9]","H-9]",
 ".p1:",".p2:",".p3:",".p4:",".p5:","---:","+++:"
};

void DisPattData()
{
 int i,j; unsigned char notedata,fx,fxval; float ratio;
 for (j=0;j<PattDimX;j++)
 {
  PutString (PattPosX+2+j*9,PattPosY,(mutesolo[j+TrkPos])?"Tr":"--"); put2digit(PattPosX+4+j*9,PattPosY,j+1+TrkPos);
  PutString (PattPosX+6+j*9,PattPosY,"-P"); put2hex(PattPosX+8+j*9,PattPosY,selpatt[j+TrkPos]);
 }
 PutString (PattPosX+1,PattPosY+1,"@@@@@@@@@ @@@@@@@@ @@@@@@@@ @@@@@@@@ @@@@@@@@ @@@@@@@@ @@@@@@@@ @@@@@@@@@");
 for (j=0;j<PattDimX;j++)
 {
  for(i=0;i<PattDimY;i++)
  {
   if (i+pattpos<PATTLENG[selpatt[j+TrkPos]])
   {
    notedata=PATTERNS[selpatt[j+TrkPos]][0][i+pattpos]; fx=PATTERNS[selpatt[j+TrkPos]][1][i+pattpos]; fxval=PATTERNS[selpatt[j+TrkPos]][2][i+pattpos];
    PutString (PattPosX+2+j*9,PattPosY+2+i,NoteString[notedata]); 
    if (PtClipSourcePtn==selpatt[j+TrkPos] && PtClipSourcePos<=i+pattpos && i+pattpos<PtClipSize+PtClipSourcePos)
    { //show selection
     PutChar(PattPosX+5+j*9,PattPosY+2+i,'*',0,0);
    }
    if (fx&0xF0) put1hex (PattPosX+6+j*9,PattPosY+2+i,fx/16); else PutChar(PattPosX+6+j*9,PattPosY+2+i,'.',0,0);
    if (fx&0x0F) put1hex (PattPosX+7+j*9,PattPosY+2+i,fx&0xF); else PutChar(PattPosX+7+j*9,PattPosY+2+i,'.',0,0);
    if (fxval || fx&0x0F) put2hex (PattPosX+8+j*9,PattPosY+2+i,fxval); else PutString(PattPosX+8+j*9,PattPosY+2+i,"..");
   }
   else
   { //was end of pattern
    PutString(PattPosX+2+j*9,PattPosY+2+i," -- - -  ");
   }
   PutChar (PattPosX+10+j*9,PattPosY+2+i,(PATTCNT[j+TrkPos]==(i+1)+pattpos && (selpatt[TrkPos+j]==SEQUENCE[j+TrkPos][SEQCNT[j+TrkPos]] || PlayMode==2))?'<':' ',0,0);
  } 
 }
 ratio=MaxPtnLength/PattDimY;
 for(i=0;i<PattDimY;i++)
 { //vertical highlighting and scroll-bar
  put2hex(PattPosX-1,PattPosY+2+i,i+pattpos);
  PutChar (PattPosX+1,PattPosY+2+i,((i+pattpos)%HiLight)?' ':0x7e, 0x000000,0x000000);
  PutChar (PattPosX+8*9+2,PattPosY+2+i,((i+pattpos)%HiLight)?' ':'|',0x000000,0x000000);
  PutChar (PattPosX+8*9+3,PattPosY+2+i,(i*ratio>=pattpos-5 && i*ratio<=pattpos+24)?0xe3:'_',0x000000,0x000000); 
 }
 ratio=TrackAmount/PattDimX;
 for(i=0;i<PattDimX*9+1;i++)
 { //horizontal scroll-bar
  PutChar (PattPosX+2+i,OrdListPosY-2,(i*ratio>=TrkPos*9 && i*ratio<=(TrkPos+PattDimX)*9)?0xe3:'_',0,0);
 }
 SDL_UpdateRect(screen, (PattPosX-2)*CharSizeX, (PattPosY+0)*CharSizeY-1, (PattDimX*9+6)*CharSizeX+1, (PattDimY+3)*CharSizeY+1);
}

void DisPattCnt(int j)
{
 int i;
 if (j<TrkPos || j>=TrkPos+PattDimX) return; //displayability check
 for(i=0;i<PattDimY;i++)
 {
  PutChar (PattPosX+10+(j-TrkPos)*9,PattPosY+2+i,(PATTCNT[j]==i+pattpos && (selpatt[j]==SEQUENCE[j][SEQCNT[j]] || PlayMode==2))?'<':' ',0,0);
 }
 SDL_UpdateRect(screen, (PattPosX+(j-TrkPos)*9+10)*CharSizeX, (PattPosY+0)*CharSizeY-1, CharSizeX, (PattDimY+2)*CharSizeY+1);
}

void DispSeqCnt(int i)
{
 int j;
 if (i<TrkPos || i>=TrkPos+OrDimY) return; //displayability check
 for (j=0;j<OrDimX;j++)
 {
  if (F2playMarker[i]==j+seqpos+1) PutChar(OrdListPosX+4+j*3,(i-TrkPos)+2+OrdListPosY,'>',0,0); 
  else if (SEQCNT[i]==j+seqpos) PutChar(OrdListPosX+4+j*3,(i-TrkPos)+2+OrdListPosY,101,0,0); 
  else if (SEQCNT[i]==j+seqpos+1) PutChar(OrdListPosX+4+j*3,(i-TrkPos)+2+OrdListPosY,103,0,0);
  else PutChar(OrdListPosX+4+j*3,(i-TrkPos)+2+OrdListPosY,' ',0,0); 
 }
 SDL_UpdateRect(screen, (OrdListPosX+2)*CharSizeX-1, (OrdListPosY+(i-TrkPos)+2)*CharSizeY-1, (OrDimX*3+1)*CharSizeX, CharSizeY+1);
}

void DispOrderL()
{
 int i,j; unsigned char olidata;
 for (i=0;i<=OrDimY+1;i++)
 {
  if (i>1) put2digit(OrdListPosX-1,OrdListPosY+i,i+TrkPos-1);
  PutChar(OrdListPosX+1,i+OrdListPosY,0x5d,0,0); //because of cursor-refresh
  for (j=0;j<OrDimX;j++)
  {
   if (i==0) { put2hex(OrdListPosX+2+j*3,OrdListPosY,j+seqpos); PutChar(OrdListPosX+2+j*3+2,OrdListPosY,'-',0x000000,0x000000); } 
   else if (i==1) PutString(OrdListPosX+2+j*3,OrdListPosY+1,"@@-");
   else 
   {
    olidata=SEQUENCE[i+TrkPos-2][j+seqpos];
    if (olidata!=0xFF) put2hex(OrdListPosX+2+j*3, i+OrdListPosY,olidata); 
    else PutString (OrdListPosX+2+j*3, i+OrdListPosY,"..");
    if (F2playMarker[i+TrkPos-2]==j+seqpos+1) PutChar(OrdListPosX+4+j*3,i+OrdListPosY,'>',0,0); 
    else if (SEQCNT[i+TrkPos-2]==j+seqpos) PutChar(OrdListPosX+4+j*3,i+OrdListPosY,101,0,0); 
    else if (SEQCNT[i+TrkPos-2]==j+seqpos+1) PutChar(OrdListPosX+4+j*3,i+OrdListPosY,103,0,0);
    else if (SeqClipSourceChn==(i+TrkPos-2) && SeqClipSourcePos<=j+seqpos && j+seqpos<SeqClipSourcePos+SeqClipSize-1)
    { //display selection
     PutChar(OrdListPosX+4+j*3,i+OrdListPosY,'-',0,0); 
    }
    else PutChar(OrdListPosX+4+j*3,i+OrdListPosY,' ',0,0); 
   }
  }
 }
 SDL_UpdateRect(screen, (OrdListPosX-1)*CharSizeX-1, (OrdListPosY+0)*CharSizeY-1, (OrDimX*3+3)*CharSizeX, (OrDimY+2)*CharSizeY+1);
}

const char GMinstName[128+1][15]={ "Press F12:Help",
 "Grand Piano","Bright Piano","Electr. Grand","Honky Tonky","Electr.Piano 1","Electr.Piano 2","Harpsicord","Clavicord",
 "Celesta","Glockenspiel","Music Box","Vibraphone","Marimba","Xylophone","Tubular Bells","Dulcimer",
 "Drawbar Organ","Perc. Organ","Rock Organ","Church Organ","Reed Organ","Accordion","Harmonica","TangoAccordion",
 "Nylon Guitar","Steel Guitar","Jazz Guitar","Clean Guitar","Muted Guitar","Overdrive","Distort.Guitar","Guit.Harmonics",
 "Acoustic Bass","Finger Bass","Picked Bass","Fretless Bass","Slap Bass 1","Slap Bass 2","Synth Bass 1","Synth Bass 2",
 "Violin","Viola","Cello","ContraBass","Tremolo String","Pizzicato Str.","Harp","Timpani",
 "StringSection1","StringSection2","Synth.String 1","Synth.String 2","Choir Aah","Choir Ooh","Synth. Voice","Orchestra Hit",
 "Trumpet","Trombone","Tuba","Muted Trumpet","French Horn","Brass Section","Synth. Brass 1","Synth. Brass 2",
 "Soprano Sax.","Alto Sax.","Tenor Sax.","Bariton Sax.","Oboe","English Horn","Bassoon","Clarinet",
 "Piccolo Flute","Flute","Recorder","Panflute","Blown Bottle","Shakuhachi","Whistle","Ocarina",
 "Square Lead","Saw Lead","Caliope Lead","Chiff Lead","Charang Lead","Voice Lead","5th Lead","Bass & Lead",
 "New Age Pad","Warm Pad","PolySynth Pad","Choir Pad","Bowed Pad","Metal Pad","Halo Pad","Sweep Pad",
 "Rain FX","SoundTrack FX","Crystal FX","Atmosphere FX","Bright FX","Goblins FX","Echoes FX","Sci-Fi FX",
 "Sitar","Banjo","Shamisen","Koto","Kalimba","Bagpipe","Fiddle","Shanai",
 "Tinkle Bell","Agogo","Steel Drums","Wood Block","Taiko Drums","Melodic Tom","Synth. Drum","Reverse Cymbal",
 "FretNoise FX","BreathNoise FX","SeaShore FX","BirdTweet FX","Telephone FX","Helicopter FX","Applause FX","GunShot FX"
};

void DispInstr()
{
 int i;
 PutString(InstPosX+1,InstPosY+4,"-  -  -   ");
 for (i=0;i<InsDimY;i++)
 {
  if (SelInst+i-3<0 || SelInst+i-3>MaxInstAmount-1) PutString(InstPosX-2,InstPosY+1+i,"..  .. .. .. ");
  else
  {
   put2hex(InstPosX-2,InstPosY+1+i,SelInst+i-3);
   put2hex(InstPosX+2,InstPosY+1+i,INSTRUMENT[SelInst+i-3][INST_PORT]);
   put2hex(InstPosX+5,InstPosY+1+i,INSTRUMENT[SelInst+i-3][INST_CHVOL]);
   put2hex(InstPosX+8,InstPosY+1+i,INSTRUMENT[SelInst+i-3][INST_PATCH]);
   PutChar(InstPosX+10,InstPosY+1+i,' ',0,0);
  }
 }
 //PutString(InstPosX-3,InstPosY+InsDimY+3,"              ");
 //for (i=0; i<InstrumSize-INST_NAME && INSTRUMENT[SelInst][INST_NAME+i]!=0; i++) 
 // { PutChar(InstPosX+i-3,InstPosY+InsDimY+3,ascii2petscii(INSTRUMENT[SelInst][INST_NAME+i]),0,0); }
 //PutString(InstPosX-3,InstPosY+InsDimY+2,"              ");
 PutString(InstPosX-3,InstPosY+InsDimY+2,GMinstName[INSTRUMENT[SelInst][INST_PATCH]],14);
 SDL_UpdateRect(screen, (InstPosX-3)*CharSizeX, (InstPosY+1)*CharSizeY, WinSizeX-(InstPosX-2)*CharSizeX+1, (InsDimY+3)*CharSizeY+1);
}

char PtnPoss[]={0,4,5,6,7};
void DispCursor()
{
 if (Window==0) {CurPosX=PattPosX+2+WinPos1[0]*9+PtnPoss[WinPos3[0]]; CurPosY=PattPosY+2+WinPos2[0]; CurWide=(WinPos3[0]<1) ? 3 : 1; }
 else if (Window==1) {CurPosX=OrdListPosX+2+WinPos1[1]*3+WinPos3[1]; CurPosY=OrdListPosY+2+WinPos2[1]; CurWide=1;}
 else if (Window==2) {CurPosX=InstPosX+2+WinPos1[2]*3+WinPos3[2];CurPosY=InstPosY+4;CurWide=1;}
 else {CurPosX=0; CurPosY=0;}
 if (CurPosX<=0 || CurPosX+1>=WinSizeX/CharSizeX || CurPosY<=0 || CurPosY+1>=WinSizeY/CharSizeY) return;
 CursoRect.x=CurPosX*8; CursoRect.y=CurPosY*8-1; CursoRect.w=CharSizeX*CurWide; CursoRect.h=1;
 SDL_FillRect(screen, &CursoRect, SDL_MapRGB(screen->format, CurColor, CurColor, CurColor));
 CursoRect.x=CurPosX*8-1; CursoRect.y=CurPosY*8-1; CursoRect.w=2; CursoRect.h=CharSizeY+1;
 SDL_FillRect(screen, &CursoRect, SDL_MapRGB(screen->format, CurColor, CurColor, CurColor));
 CursoRect.x=CurPosX*8; CursoRect.y=CurPosY*8+CharSizeY-1; CursoRect.w=CharSizeX*CurWide; CursoRect.h=1;
 SDL_FillRect(screen, &CursoRect, SDL_MapRGB(screen->format, CurColor, CurColor, CurColor));
 CursoRect.x=CurPosX*8+CharSizeX*CurWide-1; CursoRect.y=CurPosY*8-1; CursoRect.w=2; CursoRect.h=CharSizeY+1;
 SDL_FillRect(screen, &CursoRect, SDL_MapRGB(screen->format, CurColor, CurColor, CurColor));
 CursoRect.x=CurPosX*8; CursoRect.y=CurPosY*8; CursoRect.w=CharSizeX; CursoRect.h=CharSizeY;
 SDL_UpdateRect(screen, CursoRect.x-1, CursoRect.y-1, CursoRect.w*CurWide+2, CursoRect.h*CurWide+2); 
 if (CurColor>=256-32) ColDir=-1; else if (CurColor<=0) ColDir=1;
 CurColor+=8*((KeyMode+1)*2)*ColDir;
}

void RefreshCursor ()
{
 //if (CurPosX!=PrevCurX || CurPosY!=PrevCurY) Display();
 if (Window==0) DisPattData();
 else if (Window==1) DispOrderL();
 else if (Window==2) DispInstr();
}

void DisplayStatic()
{
 int y=InstPosY+0;
 PutString (InstPosX-2,y++,"Ins-Po-CV-Pat");
 PutString (InstPosX-2,y++,"00  00 00 00 ");
 PutString (InstPosX-2,y++,"00  00 00 00 ");
 PutString (InstPosX-2,y++,"00  00 00 00 ");
 PutString (InstPosX-2,y++,"00--00-00-00 ");
 PutString (InstPosX-2,y++,"00  00 00 00 ");
 PutString (InstPosX-2,y++,"00  00 00 00 ");
 PutString (InstPosX-2,y++,"00  00 00 00 ");
 PutString (1,StatPosY,"Time 00:00"); 
 PutString (12,StatPosY,"Octave0"); 
 PutString (20,StatPosY,"Adv00");
 PutString (26,StatPosY,"KeyMode:Edit");
 PutString (39,StatPosY,"Follow");
 PutString (46,StatPosY,"Input:... port00:");
}

void DisplaySettings()
{
 //put2digit(InstPosX+5,InstPosY+7,TiMinute); put2digit(InstPosX+8,InstPosY+7,TiSecond);
 //put2hex(InstPosX+0,InstPosY+7,SelInst); 
 put1hex(12+6,StatPosY,Octave); 
 put2digit(20+3,StatPosY,Advance);
 PutString(26+8,StatPosY,(KeyMode)? "Edit" : "Jam ");
 PutString (39,StatPosY,(AutoFollow)?"AutFlw":"ManFlw");
 put2digit(46+14,StatPosY,UsedInPort+0); 
 PutString (46+17,StatPosY,"                "); 
   if (UsedInPort<midiin->getPortCount()) PutString (46+17,StatPosY,midiin->getPortName(UsedInPort),16);
 SDL_UpdateRect(screen, 0, StatPosY*CharSizeY, WinSizeX, CharSizeY);
}

void DispMIDIevent()
{
 PutString(46+6,StatPosY,NoteString[DispNote],3);
 SDL_UpdateRect(screen, (46+6)*CharSizeX, StatPosY*CharSizeY, CharSizeX*3, CharSizeY);
}

void Display()
{
 DispCursor();
 DisPattData();
 DispOrderL();
 DispInstr();
 DispTrkInfo();
 DisplaySettings();
}

void DisplayHelp()
{
 int HelpX=(WinSizeX/CharSizeX)/2-HelpDimX/2, HelpY=(WinSizeY/CharSizeY)/2-HelpDimY/2, i,j;
 
 //PlayMode=0; FollowPlay=false; StopTime=SDL_GetTicks();
 for (i=0;i<TrackAmount;i++) { if (prevnote[i]>0) NoteOff(PLAYEDINS[i],prevnote[i],0); AllNotesOff (PLAYEDINS[i]);}
 
 for(i=HelpDimY/2;i>=0;i--)
 {
  SDL_Delay(5);
  for(j=0;j<HelpDimX;j++)
  {
   if (helptxt[i][j]) PutChar(HelpX+j,HelpY+i,ascii2petscii(helptxt[i][j]),0,0);
   else break;
  }
  for(j=0;j<HelpDimX;j++)
  {
   if (helptxt[HelpDimY-i][j]) PutChar(HelpX+j,HelpY+(HelpDimY-i),ascii2petscii(helptxt[HelpDimY-i][j]),0,0);
   else break;
  }
  SDL_UpdateRect(screen, 0, 0, 0, 0);
 }

 SDL_UpdateRect(screen, 0, 0, 0, 0);
 bool stillreading=true;
 
 while (stillreading)
 {
  SDL_PollEvent(&event);
  if ( event.type == SDL_KEYDOWN && (event.key.keysym.sym==SDLK_RETURN || event.key.keysym.sym==SDLK_ESCAPE)) stillreading=false;
  SDL_Delay(5);
 }

 WaitKeyRelease();
}

void DisplayGMset()
{
 int HelpX=(WinSizeX/CharSizeX)/2-(HelpDimX+12)/2, HelpY=(WinSizeY/CharSizeY)/2-HelpDimY/2, i,j,OutPorts=0;
 for(i=0;i<HelpDimY;i++)
 {
  PutString(HelpX,HelpY+i,"                                                                           ");
  if (i>0 && i<HelpDimY-1) //border
  {
   put2hex(HelpX+4,HelpY+i,i); PutString(HelpX+3+4,HelpY+i,GMinstName[i]); 
   put2hex(HelpX+24,HelpY+i,i+HelpDimY-2); PutString(HelpX+3+24,HelpY+i,GMinstName[i+(HelpDimY-2)]);
   if ((HelpDimY-2)*2+i<=128) {put2hex(HelpX+44,HelpY+i,i+(HelpDimY-2)*2); PutString(HelpX+3+44,HelpY+i,GMinstName[i+(HelpDimY-2)*2]);}
   else if (HelpDimY*2+i==136) PutString(HelpX+42,HelpY+i,"Available MIDI output-ports:");
   else if (HelpDimY*2+i>=139 && (HelpDimY*2+i-139)<midiout->getPortCount()) 
   { 
    put2hex(HelpX+42,HelpY+i,HelpDimY*2+i-139); PutString(HelpX+42+3,HelpY+i,midiout->getPortName(HelpDimY*2+i-139),30); OutPorts++;
   }
   else if (HelpDimY*2+i==142+OutPorts) PutString(HelpX+42,HelpY+i,"Available MIDI input-ports:");
   else if ( HelpDimY*2+i>=145+OutPorts && HelpDimY*2+i-(145+OutPorts)<midiin->getPortCount() ) 
   {
    put2hex(HelpX+42,HelpY+i,HelpDimY*2+i-(145+OutPorts)); PutString(HelpX+42+3,HelpY+i,midiin->getPortName(HelpDimY*2+i-(145+OutPorts)),30);
   }
  }
 }
 SDL_UpdateRect(screen, 0, 0, 0, 0);
 SDL_PollEvent(&event);
 //keystate = SDL_GetKeyState(NULL);
 while (event.type != SDL_KEYUP) //(keystate[SDLK_RETURN] || keystate[SDLK_ESCAPE]) ; //wait to release keys
 {
   SDL_PollEvent(&event);
  //keystate = SDL_GetKeyState(NULL); SDL_Delay(5);
 }
 SDL_PollEvent(&event); SDL_Delay(100);
}

void InitGUI()
{
 SDL_FillRect(screen, &screen->clip_rect, SDL_MapRGB(screen->format, 0x00, 0x20, 0x40)); // fill the screen with color
 
 //CharSet = SDL_CreateRGBSurfaceFrom(chrdata, CharSizeX, CharSizeY, BitsPerPixel, CharSizeX*BitsPerPixel, rmask, gmask, bmask, amask);
 //SDL_UnlockSurface(CharSet); //SDL_FreeSurface(CharSet);

 //SDL_SetColorKey(CharSet, SDL_SRCCOLORKEY, SDL_MapRGB(CharSet->format, 0, 0, 0) );

//retreat:
 DisplayStatic(); Display();

 IconRect.x=IconRect.y=0; BlitRect.w=IconRect.w = BlitRect.h=IconRect.h=32;
 if (screen->flags&SDL_FULLSCREEN) 
 {
  BlitRect.x=0; BlitRect.y=6;
  SDL_BlitSurface(MIDItrk_Icon, &IconRect, screen, &BlitRect);
  BlitRect.x=WinSizeX-(4+32); SDL_BlitSurface(MIDItrk_Buttons, &IconRect, screen, &BlitRect);
 }
 else 
 {
  BlitRect.x=4; BlitRect.y=7; SDL_BlitSurface(MIDItrk_Decor, &IconRect, screen, &BlitRect);
  BlitRect.x=WinSizeX-(4+32); BlitRect.y=7; SDL_BlitSurface(MIDItrk_Buttons, &IconRect, screen, &BlitRect);
 }

 SDL_UpdateRect(screen, 0, 0, 0, 0); // update the screen buffer
}

char ascii2petscii(char Char)
{
 if (Char>='A' && Char<='Z') return (Char-'A')+65;
 else if (Char >='a' && Char <='z') return (Char-'a')+1;
 return Char;
}

void PutString(int x, int y, const std::string& Gstring) 
{
 static int i;
 for (i=0;i<256;i++)
 {
  if (Gstring[i]==0) break;
  PutChar(x+i, y, ascii2petscii(Gstring[i]), 0, 0);
 }
}

void PutString(int x, int y, const std::string& Gstring, int length) 
{
 static int i;
 static bool ended;
 ended=false;
 for (i=0;i<256 && i<length;i++)
 {
  if (Gstring[i]==0) ended=true; //break;
  if (ended) PutChar(x+i,y,' ',0,0);
  else PutChar(x+i, y, ascii2petscii(Gstring[i]), 0, 0);
 }
}

void put1digit (int x, int y, char number)
{
 PutChar(x, y, number%10+'0', 0, 0);
} 

void put2digit (int x, int y, char number)
{
 PutChar(x, y, number/10+'0', 0, 0);
 put1digit (x+1,y,number);
}

char hexchar[]="0123456789ABCDEF"; 
void put1hex (int x, int y, unsigned char number)
{
 PutChar(x, y, hexchar[number&0xF], 0, 0);
}
void put2hex (int x, int y, unsigned char number)
{
 PutChar(x, y, hexchar[number/16], 0, 0);
 put1hex (x+1,y,number);
}

void PutChar(int x, int y, unsigned char chcode, int BGcol, int FGcol)
{
 FontRect.x=chcode*CharSizeX; FontRect.y=0; //y*CharSizeY;
 BlitRect.x=x*CharSizeX; BlitRect.y=y*CharSizeY;
 SDL_BlitSurface(CharSet, &FontRect, screen, &BlitRect);
}


//=================================================================================================
//---------------------------------TUNE-FILE OPERATIONS--------------------------------------------
const unsigned char InsNameStr[]="..............";

void InitMusicData(bool putTemplate)
{
 int i,j,k;
 for (i=0;i<TrackAmount;i++)
 {
  for (j=0;j<MaxSeqLength;j++)
  {
   SEQUENCE[i][j]= 0xFF; //i+j/16;
  }
  DefaultIns[i]=0;
 }
 for (i=0;i<MaxPtnAmount;i++)
 {
  for (k=0;k<MaxPtnLength;k++)
  {
   for (j=0;j<PtnColumns;j++)
   {
    PATTERNS[i][j][k] = 0; //i+j+(k*16)&0xFF;
   }
  }
 }
 for (i=0;i<MaxPtnAmount;i++)
 {
  PATTLENG[i]= 0x40; //i;
 }
 for (i=0;i<MaxInstAmount;i++)
 {
  for(j=0;j<INST_NAME;j++) INSTRUMENT[i][j]=0; //i;
  for(j=INST_NAME;j<InstrumSize;j++) INSTRUMENT[i][j]=InsNameStr[j-INST_NAME];
 }
 if (putTemplate)
 {
  //a little template to start with
  SEQUENCE[0][0]=1;SEQUENCE[0][1]=ORDERLIST_FX_JUMP;SEQUENCE[0][2]=0;
  SEQUENCE[1][0]=2;SEQUENCE[1][1]=ORDERLIST_FX_JUMP;SEQUENCE[1][2]=0;
  SEQUENCE[2][0]=3;SEQUENCE[2][1]=ORDERLIST_FX_JUMP;SEQUENCE[2][2]=0;
  SEQUENCE[3][0]=4;SEQUENCE[3][1]=ORDERLIST_FX_JUMP;SEQUENCE[3][2]=0;
  SEQUENCE[4][0]=0;SEQUENCE[5][0]=0;SEQUENCE[6][0]=0;SEQUENCE[7][0]=0;
  PATTERNS[1][1][0]=0x0C; PATTERNS[1][2][0]=0x01;
  PATTERNS[0][1][0]=0x0F; PATTERNS[0][2][0]=0x06;
  INSTRUMENT[0][INST_PORT]=0x00; INSTRUMENT[0][INST_CHVOL]=0x90; INSTRUMENT[1][INST_PATCH]=0x00; //DRUMKIT
  INSTRUMENT[1][INST_PORT]=0x01; INSTRUMENT[1][INST_CHVOL]=0x00; INSTRUMENT[1][INST_PATCH]=0x01; //PIANO
  INSTRUMENT[2][INST_PORT]=0x02; INSTRUMENT[2][INST_CHVOL]=0x10; INSTRUMENT[2][INST_PATCH]=0x51; //SOLO
 }
 PtClipSourcePtn=0xFF;
 InitRoutine(true); ResetPos(); PlayMode=0; SetSelPatt();
}

//------------------------------------------------------------------------------------
#define PATH_LENGTH_MAX 256
#define DIR_FILES_MAX 16384
#define FILENAME_LENGTH_MAX 58
char path[PATH_LENGTH_MAX]="";
char FileName[FILENAME_LENGTH_MAX+8]={""};
char extensions[][8]={".mit",".mid",".midi",".MIT",".MID",".MIDI"};

#define NameKeyAmount 40
unsigned char NameKeys[NameKeyAmount]={ 
 SDLK_a,SDLK_b,SDLK_c,SDLK_d,SDLK_e,SDLK_f,SDLK_g,SDLK_h,SDLK_i,SDLK_j,SDLK_k,SDLK_l,
 SDLK_m,SDLK_n,SDLK_o,SDLK_p,SDLK_q,SDLK_r,SDLK_s,SDLK_t,SDLK_u,SDLK_v,SDLK_w,SDLK_x,SDLK_y,SDLK_z,
 SDLK_SPACE,SDLK_0,SDLK_1,SDLK_2,SDLK_3,SDLK_4,SDLK_5,SDLK_6,SDLK_7,SDLK_8,SDLK_9,
 SDLK_MINUS,SDLK_COMMA,SDLK_PERIOD
};
unsigned char NameKeyChar[NameKeyAmount]={
 'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
 ' ','0','1','2','3','4','5','6','7','8','9',
 '-',',','.'
};
unsigned char ShiftNameKeyChar[NameKeyAmount]={
 'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
 ' ',')','!','@','#','$','%','^','&','*','(',
 '_','<','>'
};
unsigned char NameKeyVal()
{
 int i;
 for (i=0;i<NameKeyAmount;i++) if (keystate[NameKeys[i]]) break;
 if (i>NameKeyAmount-1) return 0xff;
 return i;
}

int FileSelector(const char* title)
{
 static int i,j,first,amount,FilerSizeX=62, FilerPosX=(WinSizeX/CharSizeX)/2-FilerSizeX/2, FilerPosY=1, FilerSizeY=WinSizeY/CharSizeY-2;
 static const int listX=15,listY=7,typerY=FilerSizeY-4,ListSizeY=typerY-listY-2,typerX=1;
 static int dirpos=0, fcurpos=0, typepos, fmode=0, selplace=0, curcount=0, flashstate=0x80;  //fmode: 0-file, 1-place
 static unsigned char curchar=' ';
 static const int flashspd=20, placemax=18; 
 DIR *Directory;
 struct dirent *DirEntry; struct stat st;
 typedef struct { char *Name; unsigned char IsFile; } direntry;
 direntry dirlist[DIR_FILES_MAX], entrytmp;

 for (i=0;i<TrackAmount;i++) { if (prevnote[i]>0) NoteOff(PLAYEDINS[i],prevnote[i],0); AllNotesOff (PLAYEDINS[i]);}

 static const char FilerSidebar[][14] = {
 "Places:","@@@@@@@","","  /(FileSys)","  /root","  /home","  /media","  /mnt","","  /Volumes","  /Users","  /Downloads","","   A:/","   B:/","   C:/","   D:/","   E:/","   F:/","   G:/","   H:/","   I:/","   J:/","",
 "@@@@@@@@@@@@","Press CURSOR","up/down to","select file,","or folder.","Press ENTER","to load the","selected fi-","le or enter","the folder.",
 "Press TAB to","select Place","","Or use the","alphanumeric","and number","keys to","enter file-","name, then","press ENTER.","",
 "Use ESC key","to exit.","",""
 };

 static const char DirPlaces[][16]={"/","/root/","/home/","/media/","/mnt/", "/Volumes/","/Users/","/Downloads/", "A:\\","B:\\","C:\\","D:\\","E:\\","F:\\","G:\\","H:\\","I:\\","J:\\"};
 static const char PlacePos[placemax]={0,1,2,3,4, 6,7,8, 10,11,12,13,14,15,16,17,18,19};

 typepos=strlen(FileName);

 //RemoveTimer();
REREADIR: 
 WaitKeyRelease();
 dirpos=fcurpos=0; //init cursors
 //display file-list and selector-layout
 for (i=0;i<FilerSizeY;i++) 
 {
  if (i==0 || i==FilerSizeY-1) PutString(FilerPosX,i+FilerPosY,"+@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@+");
  else PutString(FilerPosX,i+FilerPosY,"]                                                           ]");
  if (i==listY-2 || i==typerY-1) PutString(FilerPosX,i+FilerPosY,"]@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@]");
  if (i>=listY-1 && i<typerY-1) { PutChar(FilerPosX+listX-2,i+FilerPosY,']',0,0); PutChar(FilerPosX+listX+43,i+FilerPosY,'_',0,0); }
 }
 PutString(WinSizeX/CharSizeX/2-strlen(title)/2,FilerPosY+2,title);
 PutString(FilerPosX+1,FilerPosY+3,"Path:");
 PutString(FilerPosX+1,FilerPosY+typerY,"Filename:"); // .............................................."); 
//#ifdef __WIN32__
 for (i=0;i<=ListSizeY+1;i++) PutString(FilerPosX+1,listY+i,FilerSidebar[i]);
//#endif

    //if (strlen(path)) chdir(path);  // Set initial path (if exists)
 getcwd(path, 255); 
 if (strlen(path)<FilerSizeX-4) PutString(FilerPosX+1,FilerPosY+4,path,FilerSizeX-4);
 else PutString(FilerPosX+1,FilerPosY+4,path+(strlen(path)-(FilerSizeX-4)),FilerSizeX-4);
    //for (i = 0; i < DIR_FILES_MAX; i++) if (dirlist[i].Name) { free(dirlist[i].Name); dirlist[i].Name=NULL; }   // Deallocate old names
 // Process directory
#ifdef __amigaos__
 Directory = opendir("");
#else
 Directory = opendir(".");
#endif

 amount=0; //counts entries
 while ((DirEntry = readdir(Directory)))
 {
  stat(DirEntry->d_name, &st); if (st.st_mode & S_IFDIR) dirlist[amount].IsFile=0; else dirlist[amount].IsFile=1;
  if ( !dirlist[amount].IsFile || !strcmp(extensions[0],FilExt(DirEntry->d_name)) || !strcmp(extensions[1],FilExt(DirEntry->d_name)) 
       || !strcmp(extensions[2],FilExt(DirEntry->d_name)) || !strcmp(extensions[3],FilExt(DirEntry->d_name)) 
       || !strcmp(extensions[4],FilExt(DirEntry->d_name)) || !strcmp(extensions[5],FilExt(DirEntry->d_name)) )
  { 
   dirlist[amount].Name=strdup(DirEntry->d_name);
   //printf("%c%s%c\n", (IsFile[amount]==false)?'[':' ',dirlist[amount],(IsFile[amount]==false)?']':' ');
   amount++;
  }
 }
 closedir(Directory);

 //sort directory and filenames
 PutString(FilerPosX+listX,FilerPosY+listY+ListSizeY/2,"Please wait. Sorting filenames... 00%");   
  SDL_UpdateRect(screen,FilerPosX*CharSizeX,FilerPosY*CharSizeY,FilerSizeX*CharSizeX,FilerSizeY*CharSizeY);
 for (i=0;i<amount;i++)
 {
  first=i;
  for (j=i+1;j<amount;j++) if (dirlist[j].IsFile<dirlist[first].IsFile || (dirlist[j].IsFile==dirlist[first].IsFile && cmpstr(dirlist[j].Name,dirlist[first].Name)<0 ) ) first=j;
  if (first!=i) { entrytmp=dirlist[i]; dirlist[i]=dirlist[first]; dirlist[first]=entrytmp; }
  put2digit(FilerPosX+listX+34,FilerPosY+listY+ListSizeY/2,i*100/amount);
 }
 PutString(FilerPosX+listX,FilerPosY+listY+ListSizeY/2,"",42);

 //control (keyhandler) loop---------------------
 bool selected=false;
 while (!selected)
 {
  SDL_PumpEvents();
  keystate = SDL_GetKeyState(NULL);
  SHIFTstate = (keystate[SDLK_LSHIFT] || keystate[SDLK_RSHIFT]);
  if (keystate[SDLK_ESCAPE]) { WaitKeyRelease(); return 0; }
  else if (keystate[SDLK_LEFT])
  {
   if (repeatex()==0) if (typepos>0) typepos--;
  }
  else if (keystate[SDLK_RIGHT])
  {
   if (repeatex()==0) if (typepos<strlen(FileName)) typepos++;
  }
  else if (keystate[SDLK_DOWN])
  {
   if (repeatex()==0) 
   { 
    if (fmode==0)
    {
     if(dirpos+fcurpos<amount-1) { if(fcurpos<ListSizeY-1) fcurpos++; else dirpos++; }
     if (dirlist[dirpos+fcurpos].IsFile) {strcpy(FileName,dirlist[dirpos+fcurpos].Name); typepos=strlen(FileName);}
    }
    else { if (selplace<placemax-1) selplace++; }
   }
  }
  else if (keystate[SDLK_UP])
  {
   if (repeatex()==0) 
   { 
    if (fmode==0)
    {
     if(fcurpos>0) fcurpos--; else {if(dirpos>0)dirpos--;}
     if (dirlist[dirpos+fcurpos].IsFile) {strcpy(FileName,dirlist[dirpos+fcurpos].Name); typepos=strlen(FileName);}
    }
    else { if (selplace>0) selplace--;}
   }
  }
  else if (keystate[SDLK_PAGEDOWN])
  {
   if (repeatex()==0) 
   { 
    for (i=0;i<8;i++) if(dirpos+fcurpos<amount-1) {if(fcurpos<ListSizeY-1) fcurpos++; else dirpos++;}
    if (dirlist[dirpos+fcurpos].IsFile) {strcpy(FileName,dirlist[dirpos+fcurpos].Name); typepos=strlen(FileName);}
   }
  }
  else if (keystate[SDLK_PAGEUP])
  {
   if (repeatex()==0) 
   { 
    for (i=0;i<8;i++) if(fcurpos>0)fcurpos--; else if(dirpos>0)dirpos--;
    if (dirlist[dirpos+fcurpos].IsFile) {strcpy(FileName,dirlist[dirpos+fcurpos].Name); typepos=strlen(FileName);}
   }
  }
  else if (keystate[SDLK_HOME]) { if (repeatex()==0) { if (fcurpos) fcurpos=0; else dirpos=0;} }
  else if (keystate[SDLK_END])
  { 
   if (repeatex()==0) 
   { 
    if (amount>=ListSizeY) { if(fcurpos<ListSizeY-1) fcurpos=ListSizeY-1; else dirpos=amount-ListSizeY;}
    else fcurpos=amount-1;
   } 
  }
  else if (keystate[SDLK_RETURN])
  {
   if (repeatex()==0)
   {
    if (fmode==0)
    { //load file / enter dir
     if (!dirlist[dirpos+fcurpos].IsFile && dirpos+fcurpos>0) { chdir(dirlist[dirpos+fcurpos].Name); goto REREADIR; }
     else { WaitKeyRelease(); selected=true; } // strcpy(FileName,dirlist[dirpos+fcurpos].Name); }
    }
    else
    { //enter place
     chdir(DirPlaces[selplace]); fmode=0; goto REREADIR;
    }
   } 
  }
  else if (keystate[SDLK_TAB]) {if (repeatex()==0) fmode=(fmode)?0:1;}
  else if (keystate[SDLK_BACKSPACE])
  {
   if (repeatex()==0) if (typepos>0) {typepos--; for(i=typepos;i<strlen(FileName);i++) FileName[i]=FileName[i+1];} 
  }
  else if (NameKeyVal()!=0xFF)
  {
   if (repeatex()==0)
   {
    if (typepos<FILENAME_LENGTH_MAX && strlen(FileName)<FILENAME_LENGTH_MAX) 
    {
     for (i=FILENAME_LENGTH_MAX;i>=typepos;i--) FileName[i+1]=FileName[i];
     FileName[typepos] = (!SHIFTstate) ? NameKeyChar[NameKeyVal()] : ShiftNameKeyChar[NameKeyVal()]  ;  
     typepos++; 
    }
   }
  }
  else repecnt=repspd1;
 
  //Display Directory Entries
  for (i=0;i<ListSizeY;i++)
  {
   if (i+dirpos<amount)
   {
    PutChar(FilerPosX+listX-1,FilerPosY+listY+i,(fcurpos==i && fmode==0)?'>':' ',0,0);
    if(!dirlist[i+dirpos].IsFile) PutChar(FilerPosX+listX,FilerPosY+listY+i,27,0,0);
    else PutChar(FilerPosX+listX,FilerPosY+listY+i,' ',0,0); 
    PutString(FilerPosX+listX+1-dirlist[i+dirpos].IsFile,FilerPosY+listY+i,dirlist[i+dirpos].Name,42);
    if(!dirlist[i+dirpos].IsFile && strlen(dirlist[i+dirpos].Name)<42) PutChar(FilerPosX+listX+strlen(dirlist[i+dirpos].Name)+1,FilerPosY+listY+i,29,0,0); 
   }
   //else PutString(FilerPosX+listX+1,FilerPosY+listY+i,"...");
  }
  //Display Place-selector arrow
  for (i=0;i<placemax;i++) PutString(FilerPosX+1,FilerPosY+listY+2+PlacePos[i],(selplace==i && fmode==1)?"->":"  ");
  //Display Typed/Selected filename
  PutString (FilerPosX+typerX,FilerPosY+typerY+1,FileName,FILENAME_LENGTH_MAX+1);
  curchar=(FileName[typepos]) ? ascii2petscii(FileName[typepos]) : ' ';
  PutChar(FilerPosX+typerX+typepos,FilerPosY+typerY+1,curchar+flashstate,0,0); //typer-cursor
  curcount--; if (curcount<=0) {curcount=flashspd; flashstate^=0x80;}
  SDL_UpdateRect(screen,FilerPosX*CharSizeX,FilerPosY*CharSizeY,FilerSizeX*CharSizeX,FilerSizeY*CharSizeY);

  SDL_Delay(12);
 }

 //SetTimer();
 return 1;
}

//------------------------------------------------------------------------
unsigned char IDtemp[TrackerIDsize+1]="";

int LoadTune()
{
 RemoveTimer();
tryload:
 if (FileSelector("Please select tune (.mit/.mid) to load.")==0) {InitGUI(); SetTimer();return 0;} //if (AlertBox("Really Load? Y/N")==0) {InitGUI(); SetTimer();return 0;}
 TuneFile=fopen(FileName,"rb"); // !!!important "wb" is for binary (not to check text-control characters)
 if (TuneFile==NULL) 
 {
  InitGUI();
  if (AlertBox("Error Opening file! Try again? Y/N")==0) { InitGUI(); SetTimer(); return 1; }
  else {goto tryload;}
 }
 if (LoadTuneFile()==2) goto tryload;
 return 0;
}

int LoadTuneFile() //needs TuneFile opened
{
 int i,j,k,readata=0,seqlength=MaxSeqLength;
 //check file-header for matching type-ID
 fread(IDtemp,TrackerIDsize,sizeof(unsigned char),TuneFile); IDtemp[TrackerIDsize]=0;
 if ( strcmp((const char*)IDtemp,(const char*)TRACKERID) )
 {
  fclose(TuneFile); InitGUI();
  if (AlertBox("Not Supported File Type to load! Select other? Y/N")==0) { InitGUI(); SetTimer(); return 1; }
  else {return 2;}
 }
 InitMusicData(false);
 //get file-header with settings
 fread(TUNESETTING,TuneSettingSize,sizeof(unsigned char),TuneFile);
 UsedInPort=TUNESETTING[TUNE_MIDIPORTIN]; SetInDevice(UsedInPort); 
 HiLight=TUNESETTING[TUNE_HIGHLIGHT]; if (HiLight==0) HiLight=1; //avoid division by zero
 AutoFollow=TUNESETTING[TUNE_AUTOFOLLOW];

 //get default instruments
 for (i=0;i<TUNESETTING[TUNE_CHANAMOUNT];i++) { DefaultIns[i]=fgetc(TuneFile); } //Default instrument setting for all channels

 //get orderlist (sequences)
 for (i=0;i<TUNESETTING[TUNE_CHANAMOUNT];i++)
 {
  seqlength=fgetc(TuneFile); //size of sequence
  for (j=0;j<MaxSeqLength && j<seqlength;j++)
  {
   readata=fgetc(TuneFile);
   //if (readata!=0xFF) 
   SEQUENCE[i][j]=readata; 
   //else break;
  }
  fgetc(TuneFile); //the checking 0xFF
 }

 //get patterns
 if (TUNESETTING[TUNE_PTNAMOUNT])
 {
  for (i=0; i<MaxPtnAmount && i<=TUNESETTING[TUNE_PTNAMOUNT]; i++)
  {
   readata=fgetc(TuneFile); //Size of pattern
   //if (readata==EOF) break;
   PATTLENG[i]=readata;
   for (k=0;k<readata;k++)
   {
    for (j=0;j<TUNESETTING[TUNE_PTNCOLUMNS];j++)
    {
     PATTERNS[i][j][k]=fgetc(TuneFile);
    }
   }
  }
 }

 //get instruments
 if (TUNESETTING[TUNE_INSTAMOUNT])
 {
  for (i=0; i<MaxInstAmount && i<TUNESETTING[TUNE_INSTAMOUNT];i++)
  {
   //if (readata==EOF) break;
   for (j=0;j<InstrumSize;j++)
   {
    INSTRUMENT[i][j]=fgetc(TuneFile); 
   }
  }
 }
 fclose(TuneFile);
 InitRoutine(true); ResetPos(); PlayMode=0; SetSelPatt();
 InitGUI(); //Display();
 SetTimer();
 return 0;
}

inline bool fexists (const std::string& name) 
{
 if (FILE *file = fopen(name.c_str(), "r")) { fclose(file); return true; } 
 else return false;
}

//---------------------------------------------------------------------------------
int SaveTune()
{
 int i,j,k,maxptn=0,maxinst=0,seqlength=MaxSeqLength;
 unsigned char SeqTemp[MaxSeqLength];
 RemoveTimer();
 if (strcmp(FilExt(FileName),".mit")) ChangeExt(FileName,".mit"); // add/correct extension
trysave:
 if (FileSelector("Please give folder & filename (.mit) to save tune.")==0) {InitGUI(); SetTimer();return 0;} //if (AlertBox("Really Save? Y/N")==0) { InitGUI(); SetTimer(); return 0;}
 if (strcmp(FilExt(FileName),".mit")) ChangeExt(FileName,".mit"); // add/correct extension
 if (fexists(FileName))
 { 
  InitGUI();
  if (AlertBox("File Exists! Do you want to overwrite it? (Y/N)")==0) goto trysave;
 }
 TuneFile=fopen(FileName,"wb"); // !!!important "wb" is for binary (not to check text-control characters)
 if (TuneFile==NULL) 
 {
  InitGUI();
  if (AlertBox("Error Creating File! Try again? (Y/N)")==0) { InitGUI(); SetTimer(); return 1; }
  else {goto trysave;}
 }

 TUNESETTING[TUNE_CHANAMOUNT]=TrackAmount;
 TUNESETTING[TUNE_PTNCOLUMNS]=PtnColumns;
 for (i=0;i<TrackAmount;i++) for (j=0;j<MaxSeqLength;j++) if(SEQUENCE[i][j]>maxptn && SEQUENCE[i][j]<ORDERLIST_FX_MIN) maxptn=SEQUENCE[i][j];
 TUNESETTING[TUNE_PTNAMOUNT]=maxptn;
 for(maxinst=MaxInstAmount-1;maxinst>=0;maxinst--) if (INSTRUMENT[maxinst][INST_PORT]!=0 || INSTRUMENT[maxinst][INST_CHVOL]!=0 || INSTRUMENT[maxinst][INST_PATCH]!=0) break;
 maxinst++;
 TUNESETTING[TUNE_INSTAMOUNT]=maxinst;
 TUNESETTING[TUNE_MIDIPORTIN]=UsedInPort;
 TUNESETTING[TUNE_HIGHLIGHT]=HiLight;
 TUNESETTING[TUNE_AUTOFOLLOW]=AutoFollow;

 //write header
 fwrite(TRACKERID,TrackerIDsize,sizeof(unsigned char),TuneFile);
 fwrite(TUNESETTING,TuneSettingSize,sizeof(unsigned char),TuneFile);

 //write default instrument-numbers
 for (i=0;i<TrackAmount;i++) fputc(DefaultIns[i],TuneFile); //default instrument setting for all channels

 //write sequences (orderlist)
 for (i=0;i<TrackAmount;i++)
 {
  for (seqlength=MaxSeqLength-1;seqlength>=0;seqlength--) if (SEQUENCE[i][seqlength]!=ORDERLIST_FX_END) break; //test real sequence length
  seqlength++;
  fputc(seqlength,TuneFile); //store sequence-length
  for (j=0; j<MaxSeqLength && j<seqlength; j++)
  {
   fputc(SEQUENCE[i][j],TuneFile);
   //if(SEQUENCE[i][j]>maxptn) maxptn=SEQUENCE[i][j];
  }
  fputc(0xFF,TuneFile); //0xFF signs end of Sequence
 }

 //write patterns
 if(maxptn)
 {
  for (i=0;i<MaxPtnAmount && i<=maxptn; i++)
  {
   fputc(PATTLENG[i],TuneFile); //Size of pattern
   for (k=0;k<PATTLENG[i];k++)
   {
    for (j=0;j<PtnColumns;j++)
    {
     fputc(PATTERNS[i][j][k],TuneFile);
    }
   }
  }
 }
 
 //write instruments
 if (maxinst)
 {
  for(i=0;i<MaxInstAmount && i<maxinst;i++)
  {
   for(j=0;j<InstrumSize;j++) fputc(INSTRUMENT[i][j],TuneFile);
  }
 }
 fclose(TuneFile);
 InitGUI();
 SetTimer();
 return 0;
}

//--------------------------------------------------------------------------------------
FILE *MIDIfile;

char LowByte(int num) //returns low-byte of integer
{
 return num-(num/256)*256; //integer division - no need for 'floor' function
}
char HiByte(int num) //returns hi-byte of integer
{
 return (num/256); //integer division - no need for 'floor' function
}
int BEwordToFile(unsigned int DataToWrite) //put word in big-endian to output-file
{
 fputc(HiByte(DataToWrite),MIDIfile);  
 return fputc(LowByte(DataToWrite),MIDIfile);
}

unsigned int MIDItrackPointer[TrackAmount]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
unsigned int DeltaCount[TrackAmount]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
unsigned int PPQN=0x60,PALpulses=PPQN/(4*6),RowDelta;
unsigned char TrackTemp[TrackAmount][65536]; //pattern is collected here before writing to file, because size must be detected beforehand

void MessageToExport(unsigned char instr)
{ //route PlayerMessage vector to MIDI-file export
 int i;
 unsigned char channel=INSTRUMENT[instr][INST_CHVOL]/16;

 //write VARIABLE LENTGH (!) MIDI delta-timing value
 if (DeltaCount[channel]>(128*128-1)) { TrackTemp[channel][MIDItrackPointer[channel]]=((DeltaCount[channel]&0x1FC000)/0x4000)|0x80; MIDItrackPointer[channel]++; }  
 if (DeltaCount[channel]>0x7F) { TrackTemp[channel][MIDItrackPointer[channel]]=((DeltaCount[channel]&0x3F80)/128)|0x80; MIDItrackPointer[channel]++; }
 TrackTemp[channel][MIDItrackPointer[channel]]=DeltaCount[channel]&0x7F; MIDItrackPointer[channel]++;
 
 for (i=0;i<PlayerMessage.size();i++)
 {   TrackTemp[channel][MIDItrackPointer[channel]]=PlayerMessage.at(i); MIDItrackPointer[channel]++; } 

 DeltaCount[channel]=0;
 PlayerMessage.clear(); 
}

int ExportMIDI()
{
 int i,j,trackamount;
 RemoveTimer();
 if (strcmp(FilExt(FileName),".mid")) ChangeExt(FileName,".mid"); // add/correct extension
tryexport:
 if (FileSelector("Please give folder & filename to export MIDI.")==0) {InitGUI(); SetTimer();return 0;} //if (AlertBox("Export MIDI? Y/N")==0) {InitGUI(); SetTimer(); return 0;}
 if (strcmp(FilExt(FileName),".mid")) ChangeExt(FileName,".mid"); // add/correct extension
 if (fexists(FileName))
 { 
  InitGUI();
  if (AlertBox("File Exists! Do you want to overwrite it? (Y/N)")==0) goto tryexport;
 }
 //Generate the MIDI file------------------------
 MIDIfile=fopen(FileName,"wb"); 
 if (MIDIfile==NULL) 
 {
  InitGUI();
  if (AlertBox("Error Creating File! Try again? Y/N")==0) { InitGUI(); SetTimer(); return 1; }
  else {goto tryexport;}
 }

 ExportMode=true; PlayMode=TrkPos=seqpos=0; //all MIDI-out data of the player-routine will be catched by the exporter 
 InitGUI(); //will show process
 
 //assemble MIDI track chunks
 RowDelta=PALpulses; //init tune-tempo
 for (i=0;i<TrackAmount;i++) MIDItrackPointer[i]=DeltaCount[i]=0; //init
 FollowPlay=false; InitRoutine(true); //init tune
 EndOfTune=false; 
 while (!EndOfTune)
 {
  PlayRoutine();
  for (i=0;i<TrackAmount;i++) DeltaCount[i]+=RowDelta;
 }
 
 trackamount=0; for (i=0;i<TrackAmount;i++) if (MIDItrackPointer[i]>0) trackamount++;

 //write MIDI header chunk
 fputs(MIDI_ID,MIDIfile);       //put MIDI-ID to the output-file
 BEwordToFile(0);BEwordToFile(6); //MIDI header-size
 BEwordToFile(1);                 //MIDI version 1 (each track separated)
 BEwordToFile(trackamount);     //number of separate MIDI tracks
 BEwordToFile(PPQN);             //PPQN - MIDI pulses per quarter-note
 //write MIDI track chunks
 for (i=0;i<TrackAmount;i++)
 {
  if (MIDItrackPointer[i]>0) 
  {
   fputs(MIDI_TRACK_ID,MIDIfile); //put MIDI-Track ID 'MTrk' into output file
   BEwordToFile(0);BEwordToFile(MIDItrackPointer[i]+4); //put size of track to MIDI file
   for(j=0;j<MIDItrackPointer[i];j++) fputc(TrackTemp[i][j],MIDIfile); //write entire track to MIDI file
   fputc(RowDelta&0x7F,MIDIfile);fputc(0xFF,MIDIfile);fputc(0x2F,MIDIfile); fputc(0,MIDIfile); //put an end to the track
  }
 }
 fclose(MIDIfile);
 
 InitRoutine(true); ExportMode=false; PlayerMessage.clear(); Display();
 SetTimer();
 return 0;
}


//===================================================================================================
//---------------------- MIDI & audio related functions ----------------------
int EnumDevices()
{
  // RtMidiIn constructor
  try {
    midiin = new RtMidiIn();
  }
  catch ( RtError &error ) {
    error.printMessage();
    return 1; //exit( EXIT_FAILURE );
  }

  // Check inputs.
  unsigned int nPorts = midiin->getPortCount();
  std::cout << "\nThere are " << nPorts << " MIDI input sources available.\n";
  InPortCount=nPorts;
  std::string portName;
  for ( unsigned int i=0; i<nPorts; i++ ) {
    try {
      portName = midiin->getPortName(i);
    }
    catch ( RtError &error ) {
      error.printMessage();
      return 1;
    }
    std::cout << "  Input Port #" << i+0 << ": " << portName << '\n';
  }

  // RtMidiOut constructor
  try {
    midiout = new RtMidiOut();
  }
  catch ( RtError &error ) {
    error.printMessage();
    exit( EXIT_FAILURE );
  }

  // Check outputs.
  nPorts = midiout->getPortCount();
  std::cout << "\nThere are " << nPorts << " MIDI output ports available.\n";
  OutPortCount=nPorts;
  for ( unsigned int i=0; i<nPorts; i++ ) {
    try {
      portName = midiout->getPortName(i);
    }
    catch (RtError &error) {
      error.printMessage();
      return 2;
    }
    std::cout << "  Output Port #" << i+0 << ": " << portName << '\n';
  }
  std::cout << '\n';

  return 0;
}

void MIDIcallback( double deltatime, std::vector< unsigned char > *message, void *userData )
{
 unsigned int nBytes = message->size();
 unsigned char previnote;
 if (nBytes>0) 
 {
  //connecting input with output caused freezes, should be worked around later for polyphonic jamming:
  //try{ PortOut[INSTRUMENT[SelInst][INST_PORT]]->sendMessage( message ); } catch( RtError &error ) {error.printMessage();}
 
  if ((int)message->at(0)==0x90 && (int)message->at(2)!=0x00 ) { DispNote=(int)message->at(1)+1; previnote=DispNote;}
  else if ((int)message->at(0)==0x80) DispNote=0; //&& (int)message->at(1)+1==prevnote) DispNote=0;
  else if ((int)message->at(0)==0x90) DispNote=0; //&& (int)message->at(1)+1==prevnote && (int)message->at(2)==0x00 ) DispNote=0;
  else if ((int)message->at(0)==0xC0) {MIDIselInst=(int)message->at(1); } //don't call DispInstr(), due to thread-safeness
 }
 message->clear();
}

void SetInDevice(int port)
{
 if (port < midiin->getPortCount())
 {
  midiin->closePort();               //midiin->cancelCallback(); 
  try{midiin->openPort( port );} 
  catch ( RtError &error ) { error.printMessage(); return; }
 }
}

//---------------------------------- Misc. functions -------------------------------------

void ResetPos()
{
 int i;
 seqpos=WinPos1[1]=WinPos2[1]=WinPos3[1]=pattpos=TrkPos=WinPos1[0]=WinPos2[0]=WinPos3[0]=SelInst=0;
 for (i=0;i<TrackAmount;i++) F2playMarker[i]=0;
}

int AlertBox(const char* text)
{
 int i;
 //RemoveTimer();
 int AlertX=(WinSizeX/2)/CharSizeX-strlen(text)/2, AlertY=(WinSizeY/2)/CharSizeY-3;
 
 //PlayMode=0; FollowPlay=false; StopTime=SDL_GetTicks();
 for (i=0;i<TrackAmount;i++) { if (prevnote[i]>0) NoteOff(PLAYEDINS[i],prevnote[i],0); AllNotesOff (PLAYEDINS[i]);}
 
 for (int i=0;i<strlen(text)+6;i++) for (int j=0;j<7;j++) PutChar(AlertX+i-3,AlertY+j-3,' ',0,0);
 PutString(AlertX,AlertY,text); 
 
 SDL_UpdateRect(screen, 0, 0, 0, 0);
 SDL_PollEvent(&event);
 bool decide=false; int Answer=0;
 
  while (decide==false)
  {
   SDL_PollEvent(&event);
   if ( event.type == SDL_KEYDOWN && event.key.keysym.sym==SDLK_y ) {decide=true; Answer=1;}
   if ( event.type == SDL_KEYDOWN && event.key.keysym.sym==SDLK_n ) {decide=true; Answer=0;}
   SDL_Delay(5);
  }

 while (event.type != SDL_KEYUP) //(keystate[SDLK_y] || keystate[SDLK_n]) ; //wait to release keys
 {
   SDL_PollEvent(&event);
  //keystate = SDL_GetKeyState(NULL); SDL_Delay(5);
 }
 
 //SetTimer(); 
 Display();
 
 return Answer;
}

void WaitKeyPress()
{ //for console only
 printf("\nPress ENTER...\n\n");
 getchar();
}

void WaitKeyRelease()
{
 RemoveTimer();
 while (SDL_PollEvent(&event)) //wait to release keys
 {
  if ( event.type == SDL_KEYUP && (event.key.keysym.sym==SDLK_ESCAPE || event.key.keysym.sym==SDLK_RETURN) ) break;
 }
 SDL_Delay(100);
}

void WaitButtonRelease() //wait to release mouse button
{
 RemoveTimer();
 while(SDL_PollEvent(&event))
 {
  if (event.type==SDL_MOUSEBUTTONUP) break;
 }
}

char* FilExt(char *filename) //get pointer of file-extension from filename string
{  //if no '.' found, point to end of the string
 char* LastDotPos = strrchr(filename,'.');
 if (LastDotPos == NULL) return (filename+strlen(filename)); //make strcmp not to find match, otherwise it would be segmentation fault
 return LastDotPos;
}

void CutExt(char *filename) //cut the extension of the filename by putting 0 at position of '.'
{
 *FilExt(filename)=0; //omit original extension with 0 string-delimiter
}

void ChangeExt(char *filename,char *newExt) //change the extension of the file 
{
 CutExt(filename); //omit original extension with 0 string-delimiter
 strcat(filename,newExt); //expand with new extension
}


int cmpstr(char *string1, char *string2)
{
  for (;;)
  {
    unsigned char char1 = tolower(*string1++);
    unsigned char char2 = tolower(*string2++);
    if (char1 < char2) return -1;
    if (char1 > char2) return 1;
    if ((!char1) || (!char2)) return 0;
  }
}

void XPMtoPixels(char* source[], unsigned char* target)
{
 int i,j,k,sourceindex,targetindex,width,height,colours,charpix,transpchar=0xff;
 unsigned int colr[256],colg[256],colb[256];
 unsigned char colchar[256], colcode,colindex;
 
 sscanf(source[0],"%d %d %d %d",&width,&height,&colours,&charpix); //printf("width:%d, height:%d, colours:%d, charpix:%d",width,height,colours,charpix);
 for (i=0;i<colours;i++)
 {
  if (sscanf(source[1+i],"%c%*s #%2X%2X%2X",&colchar[i],&colr[i],&colg[i],&colb[i])!=4) transpchar=colchar[i]; //printf("colour%2x: %2.2X %2.2X %2.2X \n",i,colr[i],colg[i],colb[i]);
 }           //printf("%2.X ",transpchar);
 for (i=0;i<height;i++) 
 {
  for (j=0;j<width;j++)
  {
   sourceindex=(i*width+j); colcode=source[colours+1+i][j]; for(k=0;k<colours;k++) if (colcode==colchar[k]) break; colindex=k;
   targetindex=(i*width+j)*icon_depth/8;
   target[targetindex+0] = colr[colindex]; //Red
   target[targetindex+1] = colg[colindex]; //Green
   target[targetindex+2] = colb[colindex]; //Blue
   target[targetindex+3] = (colcode==transpchar)?0x00:0xff ;//Aplha - 0:fully transparent...0xFF:fully opaque
  }
 }
}

/* Creates a new mouse cursor from an XPM */
static SDL_Cursor *init_system_cursor(char *image[])
{
  int i, row, col;
  Uint8 data[4*32];
  Uint8 mask[4*32];
  int hot_x, hot_y;

  i = -1;
  for ( row=0; row<32; ++row ) {
    for ( col=0; col<32; ++col ) {
      if ( col % 8 ) {
        data[i] <<= 1;
        mask[i] <<= 1;
      } else {
        ++i;
        data[i] = mask[i] = 0;
      }
      switch (image[4+row][col]) {
        case '0':
          data[i] |= 0x01;
          mask[i] |= 0x01;
          break;
        case '1':
          mask[i] |= 0x01;
          break;
        case ' ':
          break;
      }
    }
  }
  hot_x=hot_y=0; //sscanf(image[4+row], "%d,%d", &hot_x, &hot_y);
  return SDL_CreateCursor(data, mask, 32, 32, hot_x, hot_y);
}

void ChangeMouseCursor()
{
 MouseCursor = init_system_cursor(MIDITRKmouse_xpm); 
 SDL_SetCursor(MouseCursor); //SDL_FreeCursor(MouseCursor);
}

void ToggleFullScreen()
{
 RemoveTimer();

 /* -- Portable Fullscreen Toggling --
 As of SDL 1.2.10, if width and height are both 0, SDL_SetVideoMode will use the
 width and height of the current video mode (or the desktop mode, if no mode has been set).
 Use 0 for Height, Width, and Color Depth to keep the current values. */
 Uint32 flags;

 flags = screen->flags; /* Save the current flags in case toggling fails */
 screen = SDL_SetVideoMode(0, 0, 0, screen->flags ^ SDL_FULLSCREEN); /*Toggles FullScreen Mode */
 if(screen == NULL) screen = SDL_SetVideoMode(0, 0, 0, flags); /* If toggle FullScreen failed, then switch back */
 if(screen == NULL) exit(1); /* If you can't switch back for some reason, then epic fail */

 InitGUI(); SetTimer();
}

//===========================================================================================
