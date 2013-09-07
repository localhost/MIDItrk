=======================================================================
                ------ MIDItrk 1.0 'readme' ------
=======================================================================

Introduction:
=============

  This is a tracker-style MIDI composer/editor application, which means it's pattern-based.
 What's more it uses one-column patterns which can be arranged in individual orderlists for each track.

 The reason for this tool is that I couldn't find an up-to-date tracker to create MIDI-sketch for my band,
 SIDrip Alliance to prepare for rehearsals, and I don't want to use copy-paste piano-roll based tools anymore.
 I prefer tracker style music tools for composing since all controls are under your hand on the keyboard.
 (Which of course doesn't mean you can't use mouse too where you consider it faster...)
 
 The existing trackers which gave me inspiration: Tutka (I used it for some projects, block-based.)
  (They're MIDI trackers working in Linux)        TekTrakker (ttrk, bttrk; they seem to be obsolete)
 
 The native file-format of this tool is called MIT, as pattern-data, orderlist and other information can't be
 stored in MIDI file-dormat easily. On the other hand MIDI files can be exported, of course...
 (At this time I don't implement any packing scheme, the '.mit' file is saved uncompressed, not so big btw...)

 Requirements: Any machine with operating system, that supports the SDL and RtMidi libraries. (Linux,Win,OSX)
               To compile on Linux, you'll need a c++ toolchain (build-essential) and SDL1.2 develepment-library 
               (libsdl1.2-dev) and runtime-library (libsdl1.2). Then go to 'sources' directory, and type:
               'make'    then    'make install'  (the latter puts the program,icon and menu-entry into place)
               To compile on Windows you need SDL installed, mingw32 with g++.exe in PATH, and there are two ways: 
               compile-mm.bat - The older method in Windows. Seems to work still well in XP.
               compile-ks.bat - Didn't work for me. Kernel streaming, more recent technology, bigger executable.
               
               You'll need to have MIDI support and real/virtual MIDI input/output channels, samplers on your PC.
               e.g. Linux has 'timidity' or 'qsynth'/'fluidsynth'. MIDI-in (keyboard/controller) is not required.
               These ports/devices (MIDI inputs/outputs) will be detected at program startup automatically.

 When you start MIDItrk you get a little template, pattern & orderlist & instruments contain some data which 
 eases starting a tune. If you don't want this, you can clear the tune with F10 to be completely empty.

I.Screen Layout & content
=========================
 The screen layout is not so complicated, but in a nutshell I describe the fields:

 1.Pattern-editor (call with key 'F5')
 ----------------
  In this window you can see the tracks that will be played simultaneously. Each track shows one pattern which
  was selected before (from orderlist). 8 tracks can be seen at once but you can scroll sideways. See later...
  A pattern can be up to 256 rows' long, and each pattern has 4 columns:
  
  Column1 Column2 Column3 Column4   Column1: Musical Note with octave, or note off/on ('---' or '+++').
    ...  :  0..F    0..F  00..FF    Column2: The velocity/strength/volume of the note. '0' means full velocity.
                                    Column3: The pattern-effect to apply for the given note or globally...
                                    Column4: The value that the pattern-effect takes as parameter...
  
  These are the pattern-effects:
  ------------------------------                       A - Set Channel-Volume (CC7) 
                                   5 - Attack-time     B - MIDI-CC effect & value  
           1 - Pitch-Slide up      6 - Decay-time      C - Set instrument on track 
           2 - Pitch-Slide down    7 - Release-time    D - Aftertouch (channel pressure) 
           3 - Note-portamento     8 - Vibrato-rate    E - Set Pitch-Wheel position 
           4 - Vibrato strength    9 - Delay note      F - Set Tempo, (80..FF: individual Track-tempo) 
  
           MIDI-CC effects: the effect 'B' above calls other effects, the MIDI CC-effects, which
           ---------------- can be selected with the first number (0..F), and their values can be 0..F.
                           
           1-Select Bank, 2-breath-controller, 3-footswitch, 4-portamento-time,  
           5-set panning, 6-expression, 7-Effect1, 8-Effect2, 9-General Effect 1, A-General Effect 2 
           B-Legato-switch, C-Reverb Strength, D-Tremolo Depth, E-Chorus Strength, F-Detune level   
  
  *Please note that many effects are dependent on the devices and might not work at all.
   (e.g.: I haven't seen a sampler on Linux yet which supports real portamento/legato...)
  *The pitch-slide range depends on the samplers, most of the times it's one whole-note into both directions.
  *At the moment only frame-based tempo-setting is possible. Later I plan to implement BPM (beat/minute) tempo.
  *The "Delay note" effect delays a given amount of 'frames' (20ms), and if possible repairs the timing.

  Above the patterns you can see these fields, where you can check the current playback-status:
   'TrXX'    - number of the track.. seen as '--' if you mute the track (see later)
   'PXX'     - number of the selected pattern (which is edited currently)
   'InsXX-CX'- number and channel of the currently played instrument
   'PortXX:' - shows the name of the port/device which is used by the currently played instrument

 2.Orderlist-editor (call with key 'F6')
 ------------------
  To arrange how the patterns follow one-by-one on each track, you can put pattern-numbers into the orderlist.
  I call the Orderlist's individual rows 'sequences'...
  As I noted above, this tracker handles the orderlist in a similar way like chip-music trackers do.
  Therefore you don't have to copy/paste a new pattern every time you make (most likely) a little modification.
  You can, for example, create a drum-rhythm, and use it throughout the whole tune in a sequence,
  while the other sequences change chords/basses/melodies...and you can put 'fill-in'...
  The patterns can be of any lengths, and tracks can be at any tempo if needed (e.g. solo needs faster division).
  
  The Orderlist shows only 8 tracks at a time, just like pattern-editor, but you can scroll up/down, and it
  scrolls the patterns sideways at the same time vice-versa..
  You can put pattern-numbers up to FD, because FF is reserved (the '..' end-signal), and FE is reserved too,
  it is a jump command, 'FE xx' jumps to position 'xx' in the orderlist...

  For example you type this into Orderlist: 
     00-01-02-03-04-05 <-positions
    --------------------                 And this happens:
  1| 01 01 01 .. .. ..    The first track will play Pattern '01' 3 times and then it will end.
  2| 02 03 04 FE 01 ..    The 2nd track plays Pattern '02' then '03' then '04' then jumps back to position 01.
  3| 00 00 00 .. .. ..    The 3rd track plays Pattern '00' 3 times and ends. Pattern '00' is not audible, but
  4| .. .. .. .. .. ..    you can use it to set tempo or other global settings, or just use it as silent track.
                          The 4th track (and the rest) won't do anything, just stay in place (end immediately).

 3.Instrument-editor (call with key 'F7')
 -------------------
  As MIDItrk doesn't have own sound generation yet, and is only a MIDI sequencer, there are not too much of
  the instrument settings. However they can be used to control many samplers/sound-modules.
  In the instrument-field you can see 3 columns:
   ..  .. .. ..  
   00--02-00-01     Column1: the number of the selected instrument (used to play notes on the keyboard)
   01  02 01 02     Column2: The MIDI-out port/device to use. You can have different devices for each instrument.
   ...etc...        Column3: Left: Channel on the port/device; Right digit: Volume of instrument (0 means max.)
                    Column4: The patch to use on the channel given in Column3. Values make sense between 01..7F
                             (In General MIDI soundset. You can check the patches by pressing 'ENTER' here.)

  Below the instrument-editor you can see the given name and the GM-compatible name for the selected instrument.

  Tip: As you can see, the instrument 'decides' which MIDI-channel it wants, and it doesn't depend on the track.
       You can use one instrument one more tracks to play it polyphonically on the same channel...
       For example, I usually use the drum-channel '09' with Instrument 00...
  
  *On some systems (e.g. Linux) it's possible that a "Midi Through" both used as input and output causes a feedback/
   loop. The typed note is repeated in that case many times. You can geth through this situation by selecting
   another valid instrument. It doesn't happen for completely empty/unused instruments due to a built-in protection.

 4.Status bar
 ------------
  The settings and status of devices can be seen at the very bottom of the screen:
   'Time XX:XX' - playback-time spent. It's reset when you press playback-buttons (F1/F2/F3)
   'OctaveX'    - the octave which the keyboard-entry of notes uses for sound-generation
   'AdvXX'      - the amount of steps the cursor goes down after you type a note. 00 means no advance at all.
   'KeyMode:XX' - If set to 'Edit' the notes are entered into pattern-editor, if 'Jam' pressed notes are only played.
   'ManFlw'     - If 'ManFlw' you need to press Control with F1/F2/F3, if 'AutFlw', follow-playback is the default
   'Input:XXX'  - The pressed note on the MIDI-in controller (e.g. MIDI piano-keyboard)
   'portXX:XXX' - The identifier and name of the selected MIDI-input device. (select with Alt & +/-)


II. Keyboard-layout
===================
  If you ever used/seen SID-Wizard, my Commodore64 tracker you'll realize that the keyboard-shortcuts are
  nearly identical. This is what I consider the most logical and almost generally used keyboard-scheme...
  Navigation keys (cursor up/down/left/right, page up/down, home/end) are trivial, let's see the others:

  Overall keys (used anywhere):
  -----------------------------
            F1           Play tune from the beginning
            F2           Play tune from the play-mark(s) (set by Shift+SPACE / Control+SPACE in Orderlist)
            F3           Play the selected patterns repeatedly
            F4           Stop / Continue playback
     Control + F1..F4    The same as F1..F4, but playback is followed on the screen. (Cursor is taken over.)
     Back-Quote ('`')    Fast-Forward playback at 6x speed
   Shift+BackQuote ('~') Toggle follow-playback during playback
   Control + Back-Quote  Toggle automatic/manual follow-playback ('Auto' means no need to press Control for F1..F4)
  
  *Back-Quote is sometimes tricky to be found for some keyboard-layouts, you can use numeric keyboard's ENTER too.
  
            F5           Go to Pattern-editor field (at the previous position)
            F6           Go to Orderlist-editor field (at the previous position)
            F7           Go to Instrument-editor field
            
            F8           Load '.mit' workfile (or import '.mid') 
            F9           Save '.mit' workfile
         Shift+F9        Export MIDI ('.mid') format of the worktune
            F10          Clear the whole tune and instruments (asks for comfirmation before proceeding)
          Alt+F11        Toggle windowed / full-screen mode (Fine with XP, but didn't seem to work on Windows7.) 
            F12          Show instant help ('cheat-sheet' if you like)
          ESCAPE         Exit from MIDItrk (asks for comfirmation before proceeding)
            
           + / -         Select Instrument for keyboard-jamming (audition of pressed note-keys)
        Alt & + / -      Select MIDI-input (MIDI-keyboard port)
    Control & + / -      Select (increment/decrement) Octave for keyboard-jamming / note-entry
    (NumPad: / or *)     Alternative Octave-selector keys (decrement/increment)
    Control & 0...9      Select Octave 0..9 directly
     (NumPad: 0..9)      Alternative Octave-selector (set octave 0..9 directly)
        Alt & . / ,      Select default instrument for the track the cursor is in. The default instruments
                         get selected automatically when the tune starts playing, saves one Cxx pattern-effect.

      Insert / Delete    Insert/Delete at cursor-position. The rest of data moves backward/forward.
         BackSpace       Delete at cursor-position (row,column), cursor moves backward
      Shift+BackSpace    Delete at cursor-position, cursor moves forward
       Control + C       Copy pattern/sequence from cursor position till end, into clipboard.
         Shift + C       Set end of the copied area (limits the clipboard's length)
       Control + X       Cut content from cursor-position in pattern/sequence and copy it to clipboard
       Control + V       Paste the clipboard-content from cursor-position in pattern/sequence

  Pattern-editor related keys:
  ----------------------------
    TAB / Shift+TAB      Go to next/previous track. If reaches minimum/maximum, cycles to the last/first track.
          SPACE          Toggle 'Jam' or 'Edit' mode. In 'Edit' mode the cursor flashes faster, notes are entered.
      Shift + SPACE      Play patterns from cursor-position
      Shift + M / S      Mute/UnMute or Solo/UnSolo the track where the cursor is in.
      Shift + 1..9       Mute/Unmute tracks 1..9
            
    Y..M,Q..P,S..L,2..0  Piano-keys to enter notes or jamming
           A, 1          Empty piano-keys - they enter empty note
    ENTER / Shift+ENTER  Put note-off ('---') or note-on ('+++') into pattern. 
                         (If pressed on 'Cxx' instrument-selector pattern-Fx, goes to the corresponding intrument.)
            
      Control + ENTER    Set length of the pattern till current cursor-position
     Control + Delete    Delete a whole row of the pattern (note,velocity,Fx), nothing else moves.
    Control+BackSpace    Delete a whole row of the pattern, cursor moves backward (or if Shift pressed: forward) 
        Shift + Q / W    Transpose notes by halfnote-step up/down from cursor-position
      Control + Q / W    Transpose notes by octave up/down from cursor-position
            
      Shift + A / Z      Increment/decrement Auto-advance steps for the cursor (after entering note/velocity)
      Shift + H / J      Increment/decrement pattern highlighting step (kinda time-signature, usually it's 4)

  Orderlist-specific keys:
  -----------------------
      Shift + SPACE      Set the player-mark for all tracks at cursor-position (key 'F2' playback starts here)
    Control + SPACE      Set the player-mark for only the current track the cursor is inside
    Control + E          Find and put the first unused pattern-number into orderlist (eases going forward)
          ENTER          Select the patterns in the cursor position for editing. Goes to pattern-editor.

  Instrument-editor keys:
  -----------------------
          + / -          Depending on the column the cursor is in, it increases/decreases port / channel /patch
          ENTER          Shows the General MIDI instrument-set and the available MIDI Input/Output ports

  *Mouse: You can also use mouse to reach places on the screen, scroll, or mute/solo tracks by clicking on 
          track-headers with left/right button. In the top-right corner there are 2 buttons: "full-screen" & "exit".


III.File Management
-------------------
  The file-selector is pretty straight-forward. You can enter a filename with alphanumeric/numeric keys, 
  and you can select folder with cursor/pageUp/Down/Home/End keys to save into.
  When you enter a folder with ENTER key, you can select filenames with ENTER key.
  If you're at the first '[.]' position of the list, pressing ENTER will use the filename you typed in below.
  In case loading/saving error happens you wille be prompted & asked for retrial...
  (MIDItrk corrects/adds the good ".mit"/".mid" file-extension when you save/export tunes.)

  Use the TAB key (vice-versa) to enter the 'Places' sidebar, where you can select between the most used folders.

   When you save in .mit format, these settings are also stored into the workfile:
  Default instruments, HighLight step amount for patterns, Auto-Follow setting, selected MIDI-input port-number
   What else is saved? Every pattern that is referenced in the Orderlist, every instrument that has any setting
  other than 0. Patterns, that doesn't appear in Orderlist and are above the valid biggest pattern-number, 
  are not saved. Instruments, that has 00 in their fields and exceed the last valid instrument, are not saved.
  Orderlist-sequences, that has anything inside, are saved. Even if they have data only in pieces...

  .mid is saved as SMF (standard MIDI file) version 1 format where each channel is separated into other tracks,
  therefore it's easier to open and edit in other MIDI-editor tools.

  MIDI-file import is not supported right now, but is planned (based on source of SWMconvert.exe of SID-Wizard.)

  The .mit files can be opened as command-line argument too with this simple (usual) syntax:
     MIDItrk <inputfile.mit>

IV.Closing Words
----------------
  I coded this tool for my taste in the first place but hopefully you will find it just as useful.
  I know this kind of composing tool is not for average musicians because of extensive hexadecimal number usage,
  but it is not as hard as it seems at first glance. It's just an other way of composing...

  Good luck with it...  in case help is needed I'm available at   hermit@t-email.hu

2013 Hermit Software (Mihaly Horvath)

===============================================================================================================
TODO-list for MIDItrk
----------------------
*MIDI import
*edit instrument-name and type author-info
*'bpm' based tempo-setting (currently it's 50Hz PAL frame-based, like on C64)
-keyboard-layout configuration (select between eng/hun/dvorak/etc.) or use SDL SCANCODEs instead (above SDL1.3)
-undo/redo (multiple levels)
-default tempo and pattern-length setting for the tune
-vertical copy in orderlist / Alt+V could paste to all tracks instead of only one
-built-in sound generator (preferably analog or instrument-modelling instead of huge samples...)

===============================================================================================================
