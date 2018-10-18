//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: ./synti/fluidsynth/fluidsynti.h $
//
//  Copyright (C) 1999-2011 by Werner Schweer and others
//  (C) Copyright 2016 Tim E. Real (terminator356 on users dot sourceforge dot net)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================
/*
 * MusE FLUID Synth softsynth plugin
 *
 * Copyright (C) 2004 Mathias Lundgren (lunar_shuttle@users.sourcforge.net)
 *
 * $Id: fluidsynti.h,v 1.15.2.5 2009/11/19 04:20:33 terminator356 Exp $
 *
 */

#ifndef __MUSE_FLUIDSYNTI_H__
#define __MUSE_FLUIDSYNTI_H__

#include <fluidsynth.h>
#include <pthread.h>
#include <string>

#include <QThread>
#include <QMutex>
#include "fluidsynthgui.h"
#include "libsynti/mess.h"
#include "muse/debug.h"
#include "muse/mpevent.h"   
#include "muse/midictrl_consts.h"
#include "common_defs.h"

// TODO: Try to not include this. Standalone build of plugin?
#include "config.h"
#ifdef HAVE_INSTPATCH
#include <map>
#endif

#define FS_DEBUG_DATA 0 //Turn on/off debug print of midi data sent to fluidsynth

typedef unsigned char byte;

struct FluidSoundFont
      {
      QString file_name;
      QString name;
      byte extid, intid;
      #ifdef HAVE_INSTPATCH
      std::map < int /*patch*/, std::multimap < int /* note */, std::string > > _noteSampleNameList;
      #endif
      };

struct FluidCtrl {
      const char* name;
      int num;
      int min, max;
      //int val;
      int initval;
      };

// NRPN-controllers:
static const int FS_GAIN            = 0 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_REVERB_ON       = 1 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_REVERB_LEVEL    = 2 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_REVERB_ROOMSIZE = 3 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_REVERB_DAMPING  = 4 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_REVERB_WIDTH    = 5 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_CHORUS_ON       = 6 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_CHORUS_NUM      = 7 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_CHORUS_TYPE     = 8 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_CHORUS_SPEED    = 9 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_CHORUS_DEPTH   = 10 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_CHORUS_LEVEL   = 11 + MusECore::CTRL_NRPN14_OFFSET;
static const int FS_PITCHWHEELSENS  = 0 + MusECore::CTRL_RPN_OFFSET;

// FluidChannel is used to map different soundfonts to different fluid-channels
// This is to be able to select different presets from specific soundfonts, since
// Fluidsynth has a quite strange way of dealing with fontloading and channels
// We also need this since getFirstPatch and getNextPatch only tells us which channel is
// used, so this works as a connection between soundfonts and fluid-channels (one channel
// can only have one soundfont, but one soundfont can have many channels)

struct FluidChannel
{
      byte font_extid, font_intid, preset, drumchannel;
      byte banknum; // hbank
};

class LoadFontWorker : public QObject
{
      Q_OBJECT
  public:
      LoadFontWorker() {}
      void loadFont(void*);
  signals:
      void loadFontSignal(void*);

  private slots:
      void execLoadFont(void*);
};

class FluidSynth : public Mess {

   private:
      bool pushSoundfont (const char*, int);
      void sendSysex(int l, const unsigned char* d);
      void sendLastdir(const char*);
      void sfChannelChange(unsigned char font_id, unsigned char channel);
      void parseInitData(int n, const byte* d);

      byte* initBuffer;
      int initLen;

      byte getFontInternalIdByExtId (byte channel);

      void debug(const char* msg) { if (FS_DEBUG) printf("Debug: %s\n",msg); }
      void dumpInfo(); //Prints out debug info

      FluidChannel channels[FS_MAX_NR_OF_CHANNELS];
      std::string lastdir;
      QThread fontLoadThread;
      LoadFontWorker fontWorker;
      const MidiPatch * getFirstPatch (int channel) const;
      const MidiPatch* getNextPatch (int, const MidiPatch *) const;

      //For reverb and chorus:
      double rev_size, rev_damping, rev_width, rev_level, cho_level, cho_speed, cho_depth;
      bool rev_on, cho_on;
      int cho_num, cho_type;

public:
      FluidSynth(int sr, QMutex &_GlobalSfLoaderMutex);
      virtual ~FluidSynth();
      bool init(const char*);
      // This is only a kludge required to support old songs' midistates. Do not use in any new synth.
      virtual int oldMidiStateHeader(const unsigned char** data) const;
      virtual void processMessages();
      virtual void process(unsigned pos, float**, int, int);
      virtual bool playNote(int channel, int pitch, int velo);
      virtual bool sysex(int, const unsigned char*);
      virtual bool setController(int, int, int);
      void setController(int, int , int, bool);
      virtual void getInitData(int*, const unsigned char**);
      virtual const char* getPatchName(int, int, bool) const;
      virtual const MidiPatch* getPatchInfo(int i, const MidiPatch* patch) const;
      virtual int getControllerInfo(int, const char**, int*, int*, int*, int*) const;
      virtual bool processEvent(const MusECore::MidiPlayEvent&);
      #ifdef HAVE_INSTPATCH
      // True if it found a name.
      virtual bool getNoteSampleName(bool drum, int channel, int patch, int note, const char** name) const;
      #endif

      virtual bool hasNativeGui() const { return true; }
      virtual bool nativeGuiVisible() const;
      virtual void showNativeGui(bool val);

      void sendError(const char*);
      void sendSoundFontData();
      void sendChannelData();
      void rewriteChannelSettings(); //used because fluidsynth does some very nasty things when loading a font!
      bool popSoundfont (int ext_id);

      int getNextAvailableExternalId();

      fluid_synth_t* fluidsynth;
      FluidSynthGui* gui;
      QMutex& _sfLoaderMutex;
      int currentlyLoadedFonts; //To know whether or not to run the init-parameters
      std::list<FluidSoundFont> stack;
      int nrOfSoundfonts;

      void initInternal();

      static FluidCtrl fluidCtrl[];

      };

struct FS_Helper //Only used to pass parameters when calling the loading thread
      {
      FluidSynth* fptr;
      QString file_name;
      int id;
      };

// REMOVE Tim. fs. Added.

// Our own declarations for structures we need for version 1:
      
/**
 * Virtual SoundFont preset.
 */

struct fluid_preset_v1_t {
  void* data;                                   /**< User supplied data */
  fluid_sfont_t* sfont;                         /**< Parent virtual SoundFont */

  /**
   * Method to free a virtual SoundFont preset.
   * @param preset Virtual SoundFont preset
   * @return Should return 0
   */
  int (*free)(fluid_preset_t* preset);

  /**
   * Method to get a virtual SoundFont preset name.
   * @param preset Virtual SoundFont preset
   * @return Should return the name of the preset.  The returned string must be
   *   valid for the duration of the virtual preset (or the duration of the
   *   SoundFont, in the case of preset iteration).
   */
  char* (*get_name)(fluid_preset_t* preset);

  /**
   * Method to get a virtual SoundFont preset MIDI bank number.
   * @param preset Virtual SoundFont preset
   * @param return The bank number of the preset
   */
  int (*get_banknum)(fluid_preset_t* preset);

  /**
   * Method to get a virtual SoundFont preset MIDI program number.
   * @param preset Virtual SoundFont preset
   * @param return The program number of the preset
   */
  int (*get_num)(fluid_preset_t* preset);

  /**
   * Method to handle a noteon event (synthesize the instrument).
   * @param preset Virtual SoundFont preset
   * @param synth Synthesizer instance
   * @param chan MIDI channel number of the note on event
   * @param key MIDI note number (0-127)
   * @param vel MIDI velocity (0-127)
   * @return #FLUID_OK on success (0) or #FLUID_FAILED (-1) otherwise
   *
   * This method may be called from within synthesis context and therefore
   * should be as efficient as possible and not perform any operations considered
   * bad for realtime audio output (memory allocations and other OS calls).
   *
   * Call fluid_synth_alloc_voice() for every sample that has
   * to be played. fluid_synth_alloc_voice() expects a pointer to a
   * #fluid_sample_t structure and returns a pointer to the opaque
   * #fluid_voice_t structure. To set or increment the values of a
   * generator, use fluid_voice_gen_set() or fluid_voice_gen_incr(). When you are
   * finished initializing the voice call fluid_voice_start() to
   * start playing the synthesis voice.  Starting with FluidSynth 1.1.0 all voices
   * created will be started at the same time.
   */
  int (*noteon)(fluid_preset_t* preset, fluid_synth_t* synth, int chan, int key, int vel);

  /**
   * Virtual SoundFont preset notify method.
   * @param preset Virtual SoundFont preset
   * @param reason #FLUID_PRESET_SELECTED or #FLUID_PRESET_UNSELECTED
   * @param chan MIDI channel number
   * @return Should return #FLUID_OK
   *
   * Implement this optional method if the preset needs to be notified about
   * preset select and unselect events.
   *
   * This method may be called from within synthesis context and therefore
   * should be as efficient as possible and not perform any operations considered
   * bad for realtime audio output (memory allocations and other OS calls).
   */
  int (*notify)(fluid_preset_t* preset, int reason, int chan);
};

/**
 * Virtual SoundFont instance structure.
 */
struct fluid_sfont_v1_t {
  void* data;           /**< User defined data */
  unsigned int id;      /**< SoundFont ID */

  /**
   * Method to free a virtual SoundFont bank.
   * @param sfont Virtual SoundFont to free.
   * @return Should return 0 when it was able to free all resources or non-zero
   *   if some of the samples could not be freed because they are still in use,
   *   in which case the free will be tried again later, until success.
   */
  int (*free)(fluid_sfont_t* sfont);

  /**
   * Method to return the name of a virtual SoundFont.
   * @param sfont Virtual SoundFont
   * @return The name of the virtual SoundFont.
   */
  char* (*get_name)(fluid_sfont_t* sfont);

  /**
   * Get a virtual SoundFont preset by bank and program numbers.
   * @param sfont Virtual SoundFont
   * @param bank MIDI bank number (0-16383)
   * @param prenum MIDI preset number (0-127)
   * @return Should return an allocated virtual preset or NULL if it could not
   *   be found.
   */
  fluid_preset_t* (*get_preset)(fluid_sfont_t* sfont, unsigned int bank, unsigned int prenum);

  /**
   * Start virtual SoundFont preset iteration method.
   * @param sfont Virtual SoundFont
   *
   * Starts/re-starts virtual preset iteration in a SoundFont.
   */
  void (*iteration_start)(fluid_sfont_t* sfont);

  /**
   * Virtual SoundFont preset iteration function.
   * @param sfont Virtual SoundFont
   * @param preset Caller supplied preset to fill in with current preset information
   * @return 0 when no more presets are available, 1 otherwise
   *
   * Should store preset information to the caller supplied \a preset structure
   * and advance the internal iteration state to the next preset for subsequent
   * calls.
   */
  int (*iteration_next)(fluid_sfont_t* sfont, fluid_preset_t* preset);
};


// Our own wrappers for functions we need to look up with dlsym:

// //char*(* fluid_preset_t::get_name_V1_t) (fluid_preset_t *preset)
// typedef char* (*fluid_preset_t::get_name_V1_t)(fluid_preset_t* preset);
// extern fluid_preset_t::get_name_V1_t fluid_preset_get_name_V1_fp;
// 
// char *(* get_name )(fluid_preset_t *preset);
// typedef char* (*fluid_preset_get_name_V1_t)(fluid_preset_t* preset);
// extern fluid_preset_get_name_V1_t fluid_preset_get_name_V1_fp;
 
typedef const char* (*fluid_preset_get_name_v2_t)(fluid_preset_t *preset);
extern fluid_preset_get_name_v2_t fluid_preset_get_name_v2_fp;
      
typedef fluid_preset_t* (*fluid_sfont_get_preset_v2_t)(fluid_sfont_t *sfont, int bank, int prenum);
extern fluid_sfont_get_preset_v2_t fluid_sfont_get_preset_v2_fp;
      
      
// static void* fontLoadThread(void* t); // moved to the implementation file -Orcan
#endif /* __MUSE_FLUIDSYNTI_H__ */
