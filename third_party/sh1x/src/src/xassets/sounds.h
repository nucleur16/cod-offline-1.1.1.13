#ifndef __SOUNDS_H__
#define __SOUNDS_H__

#include <stdint.h>

typedef enum 
{
  SAT_UNKNOWN = 0x0,
  SAT_LOADED = 0x1,
  SAT_STREAMED = 0x2,
  SAT_PRIMED = 0x3,
  SAT_COUNT = 0x4,
}snd_alias_type_t;


/* CoDOL curves carry up to sixteen knots (record +4 is a 16-bit count, C1
 * evaluator 0x7375b0, interpolation 0x618d80 with an 8-byte stride), so the
 * knot array is sized for the CoDOL source rather than IW3's eight. */
#define SND_CURVE_KNOTS 16

/* C1 layout (loader 0x4b82b0, evaluator 0x7375b0): name, 16-bit knot count at +4, knots from +8; 136 bytes. */
struct SndCurve
{
  const char *filename;
  uint16_t knotCount;
  uint16_t pad;
  float knots[SND_CURVE_KNOTS][2];
};


typedef struct SpeakerLevels_s
{
  int speaker;
  int numLevels;
  float levels[2];
}SpeakerLevels_t;
 
typedef struct ChannelMap_s
{
  int entryCount;	// how many entries are used
  SpeakerLevels_t speakers[6];
}ChannelMap_t;
 
typedef struct SpeakerMap_s
{
  byte isDefault;
  byte pad[3];
  const char *name;
  ChannelMap_t channelMaps[2][2];
}SpeakerMap_t;



typedef struct _AILSOUNDINFO
{
  signed int format;
  const void *data_ptr;
  unsigned int data_len;
  unsigned int rate;
  signed int bits;
  signed int channels;
  unsigned int samples;
  unsigned int block_size;
  const void *initial_ptr;
}AILSOUNDINFO;

typedef struct MssSound_s
{
  AILSOUNDINFO ailInfo;
  void *data;
}MssSound_t;

/* C1 sound info at LoadedSound+4 (C1 0x73e22c passes it to AIL_set_sample_info): forty bytes, one word more
 * than mss32's AILSOUNDINFO. Roles measured against the shipped zones (sound_source.c). */
typedef struct SoundInfo_s
{
  int32_t format;            /* +0x04 */
  const void *data_ptr;      /* +0x08 */
  uint32_t data_len;         /* +0x0c */
  uint32_t rate;             /* +0x10 */
  int32_t bits;              /* +0x14 */
  int32_t channels;          /* +0x18 */
  uint32_t sentinel;         /* +0x1c, constant in shipped data */
  uint32_t samples;          /* +0x20 */
  uint32_t block_size;       /* +0x24 */
  const void *initial_ptr;   /* +0x28 */
}SoundInfo_t;

/* C1 layout (loader 0x4b80b0, payload relocation at +0x2c): 48 bytes. */
typedef struct LoadedSound_s
{
  const char *name;
  SoundInfo_t info;
  void *data;                /* +0x2c */
}LoadedSound_t;


struct SoundFileInfo
{
  const char *sndfilename;
  MssSound_t ailsoundinfo;
};


typedef struct StreamFileNamePacked_s
{
  uint64_t offset;
  uint64_t length;
}StreamFileNamePacked_t;
 
typedef struct StreamFileNameRaw_s
{
  const char *dir;
  const char *name;
}StreamFileNameRaw_t;
 
typedef union StreamFileInfo_s
{
  StreamFileNameRaw_t raw;
  StreamFileNamePacked_t packed;
}StreamFileInfo_t;
 
typedef struct StreamFileName_s
{
  StreamFileInfo_t info;
  uint16_t isLocalized;
  uint16_t fileIndex;
}StreamFileName_t;
 
typedef struct StreamedSound_s
{
  StreamFileName_t filename;
  unsigned int totalMsec;
}StreamedSound_t;
 


/* C1 layout (loader 0x4b8240, validity check 0x7384e0): type, exists, then a LoadedSound pointer (type 1) or
 * the streamed directory/name pair (types 2 and 3); 12 bytes. */
typedef struct SoundFile_s
{
    byte type;
    byte exists;
    byte pad[2];
    union
    {
        LoadedSound_t *loadSnd;
        struct { const char *dir, *name; } stream;
    } u;
}SoundFile_t;



/* Measured CoDOL flag semantics. Bit 0 is the looping flag (C1 0x738665 and
 * 0x738330); bits 9..14 hold the mixer channel index (C1 0x738547). Bits
 * 3, 4, 5, 6, 7 and 8 are used by C1 but their roles are not established. */
#define SND_ALIAS_LOOPING 0x1u
#define SND_ALIAS_CHANNEL_SHIFT 9
#define SND_ALIAS_CHANNEL_MASK 0x3fu
#define SND_AliasChannel(alias) (((alias)->flags >> SND_ALIAS_CHANNEL_SHIFT) & SND_ALIAS_CHANNEL_MASK)
#define SND_AliasLooping(alias) (((alias)->flags & SND_ALIAS_LOOPING) != 0)

/* C1 snd_alias_t (loader 0x4b84b0), 108 bytes, the layout C1 holds in memory (feat/iw4-asset-layout). Roles:
 * names at +0/+8/+12 (0x738638 secondary lookup, 0x73d0c6 chain check), +4 subtitle by loader position, +20
 * sound file (0x73e22c, 0x7384e0), +24 sequence and +72 probability (weighted picker 0x61ea30), +28/+32 volume
 * and +36/+40 pitch (randomizer 0x7380a0), +44/+48 distances (range test 0x738540), +56 flags, +60 priority
 * byte, +84 start delay (0x7388a8), +88 falloff curve (0x7375b0), +104 speaker map. Words with no measured
 * consumer are named for their offset. */
struct snd_alias_t
{
  const char *aliasName;          /* +0x00 */
  const char *subtitle;           /* +0x04 */
  const char *secondaryAliasName; /* +0x08 */
  const char *chainAliasName;     /* +0x0c */
  const char *mixerName;          /* +0x10, fifth name, no measured consumer */
  SoundFile_t *soundFile;         /* +0x14 */
  int sequence;                   /* +0x18 */
  float volMin, volMax;           /* +0x1c, +0x20 */
  float pitchMin, pitchMax;       /* +0x24, +0x28 */
  float distMin, distMax;         /* +0x2c, +0x30 */
  float scalar34;                 /* +0x34 */
  unsigned int flags;             /* +0x38, CoDOL flag word: SND_AliasLooping / SND_AliasChannel */
  unsigned char priority;         /* +0x3c, compared with a global threshold */
  unsigned char pad3d[3];
  float scalar40, scalar44;       /* +0x40, +0x44; +0x44 scales a mix level */
  float probability;              /* +0x48 */
  float scalar4c, scalar50;       /* +0x4c, +0x50 */
  int startDelay;                 /* +0x54 */
  struct SndCurve *volumeFalloffCurve; /* +0x58 */
  float scalar5c, scalar60, scalar64;  /* +0x5c..+0x64 */
  SpeakerMap_t *speakerMap;       /* +0x68 */
};

typedef struct snd_alias_list_t
{
  const char *aliasName;
  struct snd_alias_t *head; //Can be multiple
  int count;
} snd_alias_list_t;

typedef union 
{
  int handle;
}SndEntHandle_t;


typedef struct
{
  struct snd_alias_t *alias0;
  struct snd_alias_t *alias1;
  float lerp;
  SndEntHandle_t sndEnt;
  float org[3];
  float volume;
  float pitch;
  int timeshift;
  float fraction;
  int startDelay;
  byte master;
  byte timescale;
  byte pad[2];
  snd_alias_system_t system;
}SndStartAliasInfo_t;

#ifdef __cplusplus
extern "C"{
#endif


struct snd_alias_t *__cdecl _Com_PickSoundAliasFromList(struct snd_alias_list_t *sndaliaslist);
void SND_PlayAmbientAlias(struct snd_alias_t *alias, int localClientNum, int fadetime);




typedef struct snd_alias_list_t *snd_alias_list_name;

void __cdecl Com_UnloadSoundAliases(snd_alias_system_t type);
#ifdef __cplusplus
}
#endif

#endif
