#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>


#define LOG_TAG "id3tag-test"
#include "log_util.h"
#include "id3tag.h"
#include "tag.h"

enum {
  PLAYER_OPTION_SHUFFLE      = 0x0001,
  PLAYER_OPTION_DOWNSAMPLE   = 0x0002,
  PLAYER_OPTION_IGNORECRC    = 0x0004,
  PLAYER_OPTION_IGNOREVOLADJ = 0x0008,

  PLAYER_OPTION_SKIP         = 0x0010,
  PLAYER_OPTION_TIMED        = 0x0020,
  PLAYER_OPTION_TTYCONTROL   = 0x0040,
  PLAYER_OPTION_STREAMID3    = 0x0080,

  PLAYER_OPTION_FADEIN       = 0x0100,
  PLAYER_OPTION_FADEOUT      = 0x0200,
  PLAYER_OPTION_GAP          = 0x0400,
  PLAYER_OPTION_CROSSFADE    = 0x0800,
  PLAYER_OPTION_SHOWTAGSONLY = 0x4000
};

enum {
  TAG_XING = 0x0001,
  TAG_LAME = 0x0002,
  TAG_VBR  = 0x0100
};

enum {
  TAG_XING_FRAMES = 0x00000001L,
  TAG_XING_BYTES  = 0x00000002L,
  TAG_XING_TOC    = 0x00000004L,
  TAG_XING_SCALE  = 0x00000008L
};

struct tag_xing {
  long flags;		   /* valid fields (see above) */
  unsigned long frames;	   /* total number of frames */
  unsigned long bytes;	   /* total number of bytes */
  unsigned char toc[100];  /* 100-point seek table */
  long scale;		   /* VBR quality indicator (0 best - 100 worst) */
};

enum {
  TAG_LAME_NSPSYTUNE    = 0x01,
  TAG_LAME_NSSAFEJOINT  = 0x02,
  TAG_LAME_NOGAP_NEXT   = 0x04,
  TAG_LAME_NOGAP_PREV   = 0x08,

  TAG_LAME_UNWISE       = 0x10
};

enum tag_lame_vbr {
  TAG_LAME_VBR_CONSTANT      = 1,
  TAG_LAME_VBR_ABR           = 2,
  TAG_LAME_VBR_METHOD1       = 3,
  TAG_LAME_VBR_METHOD2       = 4,
  TAG_LAME_VBR_METHOD3       = 5,
  TAG_LAME_VBR_METHOD4       = 6,
  TAG_LAME_VBR_CONSTANT2PASS = 8,
  TAG_LAME_VBR_ABR2PASS      = 9
};

enum tag_lame_source {
  TAG_LAME_SOURCE_32LOWER  = 0x00,
  TAG_LAME_SOURCE_44_1     = 0x01,
  TAG_LAME_SOURCE_48       = 0x02,
  TAG_LAME_SOURCE_HIGHER48 = 0x03
};

enum tag_lame_mode {
  TAG_LAME_MODE_MONO      = 0x00,
  TAG_LAME_MODE_STEREO    = 0x01,
  TAG_LAME_MODE_DUAL      = 0x02,
  TAG_LAME_MODE_JOINT     = 0x03,
  TAG_LAME_MODE_FORCE     = 0x04,
  TAG_LAME_MODE_AUTO      = 0x05,
  TAG_LAME_MODE_INTENSITY = 0x06,
  TAG_LAME_MODE_UNDEFINED = 0x07
};

enum tag_lame_surround {
  TAG_LAME_SURROUND_NONE      = 0,
  TAG_LAME_SURROUND_DPL       = 1,
  TAG_LAME_SURROUND_DPL2      = 2,
  TAG_LAME_SURROUND_AMBISONIC = 3
};

enum tag_lame_preset {
  TAG_LAME_PRESET_NONE          =    0,
  TAG_LAME_PRESET_V9            =  410,
  TAG_LAME_PRESET_V8            =  420,
  TAG_LAME_PRESET_V7            =  430,
  TAG_LAME_PRESET_V6            =  440,
  TAG_LAME_PRESET_V5            =  450,
  TAG_LAME_PRESET_V4            =  460,
  TAG_LAME_PRESET_V3            =  470,
  TAG_LAME_PRESET_V2            =  480,
  TAG_LAME_PRESET_V1            =  490,
  TAG_LAME_PRESET_V0            =  500,
  TAG_LAME_PRESET_R3MIX         = 1000,
  TAG_LAME_PRESET_STANDARD      = 1001,
  TAG_LAME_PRESET_EXTREME       = 1002,
  TAG_LAME_PRESET_INSANE        = 1003,
  TAG_LAME_PRESET_STANDARD_FAST = 1004,
  TAG_LAME_PRESET_EXTREME_FAST  = 1005,
  TAG_LAME_PRESET_MEDIUM        = 1006,
  TAG_LAME_PRESET_MEDIUM_FAST   = 1007
};

struct tag_lame {
  unsigned char revision;
  unsigned char flags;

  enum tag_lame_vbr vbr_method;
  unsigned short lowpass_filter;

  unsigned char ath_type;
  unsigned char bitrate;

  unsigned short start_delay;
  unsigned short end_padding;

  enum tag_lame_source source_samplerate;
  enum tag_lame_mode stereo_mode;
  unsigned char noise_shaping;

  signed char gain;
  enum tag_lame_surround surround;
  enum tag_lame_preset preset;

  unsigned long music_length;
  unsigned short music_crc;
};

struct tag {
  int flags;
  struct tag_xing xing;
  struct tag_lame lame;
  char encoder[21];
};

#define RGAIN_REFERENCE  83		/* reference level (dB SPL) */

enum rgain_name {
  RGAIN_NAME_NOT_SET    = 0x0,
  RGAIN_NAME_RADIO      = 0x1,
  RGAIN_NAME_AUDIOPHILE = 0x2
};

enum rgain_originator {
  RGAIN_ORIGINATOR_UNSPECIFIED = 0x0,
  RGAIN_ORIGINATOR_PRESET      = 0x1,
  RGAIN_ORIGINATOR_USER        = 0x2,
  RGAIN_ORIGINATOR_AUTOMATIC   = 0x3
};

struct rgain {
  enum rgain_name name;			/* profile (see above) */
  enum rgain_originator originator;	/* source (see above) */
  signed short adjustment;		/* in units of 0.1 dB */
};

#define RGAIN_SET(rgain)	((rgain)->name != RGAIN_NAME_NOT_SET)

#define RGAIN_VALID(rgain)  \
  (((rgain)->name == RGAIN_NAME_RADIO ||  \
    (rgain)->name == RGAIN_NAME_AUDIOPHILE) &&  \
   (rgain)->originator != RGAIN_ORIGINATOR_UNSPECIFIED)

#define RGAIN_DB(rgain)  ((rgain)->adjustment / 10.0)




#define gettext_noop(String) String
#define N_(text)	gettext_noop(text)
#define _


struct player {
	int verbosity;
	int options;
};

/*
 * NAME:	detail()
 * DESCRIPTION:	show right-aligned label and line-wrap corresponding text
 */
static
void detail(char const *label, char const *format, ...)
{
  char const spaces[] = "               ";
  va_list args;

# define LINEWRAP  (80 - sizeof(spaces) - 2 - 2)

  if (label) {
    unsigned int len;

    len = strlen(label);
    assert(len < sizeof(spaces));

    fprintf(stderr, "%s%s: ", &spaces[len], label);
  }
  else
    fprintf(stderr, "%s  ", spaces);

  va_start(args, format);

  if (format) {
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
  }
  else {
    char *ptr, *newline, *linebreak;

    /* N.B. this argument must be mutable! */
    ptr = va_arg(args, char *);

    do {
      newline = strchr(ptr, '\n');
      if (newline)
	*newline = 0;

      if (strlen(ptr) > LINEWRAP) {
	linebreak = ptr + LINEWRAP;

	while (linebreak > ptr && *linebreak != ' ')
	  --linebreak;

	if (*linebreak == ' ') {
	  if (newline)
	    *newline = '\n';

	  *(newline = linebreak) = 0;
	}
      }

      fprintf(stderr, "%s\n", ptr);

      if (newline) {
	ptr = newline + 1;
	fprintf(stderr, "%s  ", spaces);
      }
    }
    while (newline);
  }

  va_end(args);
}

/*
 * NAME:	show_id3()
 * DESCRIPTION:	display ID3 tag information
 */
static
void show_id3(struct id3_tag const *tag)
{
  unsigned int i;
  struct id3_frame const *frame;
  id3_ucs4_t const *ucs4;
  id3_latin1_t *latin1;

  static struct {
    char const *id;
    char const *label;
  } const info[] = {
    { ID3_FRAME_TITLE,  N_("Title")     },
    { "TIT3",           0               },  /* Subtitle */
    { "TCOP",           0               },  /* Copyright */
    { "TPRO",           0               },  /* Produced */
    { "TCOM",           N_("Composer")  },
    { ID3_FRAME_ARTIST, N_("Artist")    },
    { "TPE2",           N_("Orchestra") },
    { "TPE3",           N_("Conductor") },
    { "TEXT",           N_("Lyricist")  },
    { ID3_FRAME_ALBUM,  N_("Album")     },
    { ID3_FRAME_TRACK,  N_("Track")     },
    { ID3_FRAME_YEAR,   N_("Year")      },
    { "TPUB",           N_("Publisher") },
    { ID3_FRAME_GENRE,  N_("Genre")     },
    { "TRSN",           N_("Station")   },
    { "TENC",           N_("Encoder")   }
  };

  /* text information */
  for (i = 0; i < sizeof(info) / sizeof(info[0]); ++i) {
  	//debug("id: %s, label: %s", info[i].id, info[i].label);
    union id3_field const *field;
    unsigned int nstrings, j;

    frame = id3_tag_findframe(tag, info[i].id, 0);
    if (frame == 0)
      continue;

    field    = id3_frame_field(frame, 1);
    nstrings = id3_field_getnstrings(field);

    for (j = 0; j < nstrings; ++j) {
      ucs4 = id3_field_getstrings(field, j);
      assert(ucs4);

      if (strcmp(info[i].id, ID3_FRAME_GENRE) == 0)
	ucs4 = id3_genre_name(ucs4);

      latin1 = id3_ucs4_latin1duplicate(ucs4);
      if (latin1 == 0)
	goto fail;

      if (j == 0 && info[i].label)
	detail(gettext(info[i].label), 0, latin1);
      else {
	if (strcmp(info[i].id, "TCOP") == 0 ||
	    strcmp(info[i].id, "TPRO") == 0) {
	  detail(0, "%s %s", (info[i].id[1] == 'C') ?
		 _("Copyright (C)") : _("Produced (P)"), latin1);
	}
	else
	  detail(0, 0, latin1);
      }

      free(latin1);
    }
  }

  /* comments */

  i = 0;
  while ((frame = id3_tag_findframe(tag, ID3_FRAME_COMMENT, i++))) {
    ucs4 = id3_field_getstring(id3_frame_field(frame, 2));
    assert(ucs4);

    if (*ucs4)
      continue;

    ucs4 = id3_field_getfullstring(id3_frame_field(frame, 3));
    assert(ucs4);

    latin1 = id3_ucs4_latin1duplicate(ucs4);
    if (latin1 == 0)
      goto fail;

    detail(_("Comment"), 0, latin1);

    free(latin1);
    break;
  }

  if (0) {
  fail:
    error("id3", _("not enough memory to display tag"));
  }
}

/*
 * NAME:	process_id3()
 * DESCRIPTION:	display and process ID3 tag information
 */
static void process_id3(struct id3_tag const *tag, struct player *player)
{
  struct id3_frame const *frame;

  /* display the tag */

  if (player->verbosity >= 0 || (player->options & PLAYER_OPTION_SHOWTAGSONLY))
    show_id3(tag);

  /*
   * The following is based on information from the
   * ID3 tag version 2.4.0 Native Frames informal standard.
   */
  /* length information */
  frame = id3_tag_findframe(tag, "TLEN", 0);
  if (frame) {
    union id3_field const *field;
    unsigned int nstrings;

    field    = id3_frame_field(frame, 1);
    nstrings = id3_field_getnstrings(field);

    if (nstrings > 0) {
      id3_latin1_t *latin1;

      latin1 = id3_ucs4_latin1duplicate(id3_field_getstrings(field, 0));
      if (latin1) {
	signed long ms;

	/*
	 * "The 'Length' frame contains the length of the audio file
	 * in milliseconds, represented as a numeric string."
	 */

	ms = atol(latin1);
	if (ms > 0)
	  //mad_timer_set(&player->stats.total_time, 0, ms, 1000);
	  ALOGD("mad_timer_set");

	free(latin1);
      }
    }
  }

  /* relative volume adjustment information */

  if ((player->options & PLAYER_OPTION_SHOWTAGSONLY) ||
      !(player->options & PLAYER_OPTION_IGNOREVOLADJ)) {
    frame = id3_tag_findframe(tag, "RVA2", 0);
    if (frame) {
      id3_latin1_t const *id;
      id3_byte_t const *data;
      id3_length_t length;

      enum {
	CHANNEL_OTHER         = 0x00,
	CHANNEL_MASTER_VOLUME = 0x01,
	CHANNEL_FRONT_RIGHT   = 0x02,
	CHANNEL_FRONT_LEFT    = 0x03,
	CHANNEL_BACK_RIGHT    = 0x04,
	CHANNEL_BACK_LEFT     = 0x05,
	CHANNEL_FRONT_CENTRE  = 0x06,
	CHANNEL_BACK_CENTRE   = 0x07,
	CHANNEL_SUBWOOFER     = 0x08
      };

      id   = id3_field_getlatin1(id3_frame_field(frame, 0));
      data = id3_field_getbinarydata(id3_frame_field(frame, 1), &length);

      assert(id && data);

      /*
       * "The 'identification' string is used to identify the situation
       * and/or device where this adjustment should apply. The following is
       * then repeated for every channel
       *
       *   Type of channel         $xx
       *   Volume adjustment       $xx xx
       *   Bits representing peak  $xx
       *   Peak volume             $xx (xx ...)"
       */

      while (length >= 4) {
	unsigned int peak_bytes;

	peak_bytes = (data[3] + 7) / 8;
	if (4 + peak_bytes > length)
	  break;

	if (data[0] == CHANNEL_MASTER_VOLUME) {
	  signed int voladj_fixed;
	  double voladj_float;

	  /*
	   * "The volume adjustment is encoded as a fixed point decibel
	   * value, 16 bit signed integer representing (adjustment*512),
	   * giving +/- 64 dB with a precision of 0.001953125 dB."
	   */

	  voladj_fixed  = (data[1] << 8) | (data[2] << 0);
	  voladj_fixed |= -(voladj_fixed & 0x8000);

	  voladj_float  = (double) voladj_fixed / 512;

	  //set_gain(player, GAIN_VOLADJ, voladj_float);
	  ALOGD("set_gain NOT support!");

	  if (player->verbosity >= 0) {
	    detail(_("Relative Volume"),
		   _("%+.1f dB adjustment (%s)"), voladj_float, id);
	  }

	  break;
	}

	data   += 4 + peak_bytes;
	length -= 4 + peak_bytes;
      }
    }
  }

  /* Replay Gain */

  if ((player->options & PLAYER_OPTION_SHOWTAGSONLY)) {
    frame = id3_tag_findframe(tag, "RGAD", 0);
    if (frame) {
      id3_byte_t const *data;
      id3_length_t length;

      data = id3_field_getbinarydata(id3_frame_field(frame, 0), &length);
      assert(data);

      /*
       * Peak Amplitude                          $xx $xx $xx $xx
       * Radio Replay Gain Adjustment            $xx $xx
       * Audiophile Replay Gain Adjustment       $xx $xx
       */

      if (length >= 8) {
		//struct mad_bitptr ptr;
		//mad_fixed_t peak;
		//struct rgain rgain[2];

		//mad_bit_init(&ptr, data);
		ALOGD("mad_bit_init NOT support!");

		//peak = mad_bit_read(&ptr, 32) << 5;

		//rgain_parse(&rgain[0], &ptr);
		//rgain_parse(&rgain[1], &ptr);
		ALOGD("rgain_parse NOT support!");

		//use_rgain(player, rgain);
		ALOGD("use_rgain NOT support!");

		//mad_bit_finish(&ptr);
		ALOGD("mad_bit_finish NOT support!");
      }
    }
  }
}


#define FILE_NAME "../song.mp3"
/* try reading ID3 tag information now (else read later from stream) */
#ifdef ID3TAG_TEST
int main(int argc, char *argv[])
#else
int id3tag_main(int argc, char *argv[])
#endif
{
	int fd;
	struct player player;
	player.verbosity = 1;
	struct id3_file *file;
	fd = open(FILE_NAME, O_RDONLY);
	if (fd <= 0) {
		ALOGD("open %s error!", FILE_NAME);
		goto exit;
	}

	file = id3_file_fdopen(fd, ID3_FILE_MODE_READONLY);
	if (file == 0) {
		ALOGD("id3_file_fdopen error!");
	  	close(fd);
	} else {
		ALOGD("go to process id3");
		process_id3(id3_file_tag(file), &player);
		id3_file_close(file);
	}

	close(fd);
exit:
	return 0;
}

