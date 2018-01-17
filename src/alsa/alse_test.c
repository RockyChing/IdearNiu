#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

#include <log_util.h>

#define WAV_FILE "./src/alsa/ring.wav"

/*
 What's a WAV (RIFF) File?
 A WAV (RIFF) file is a multi-format file that contains a header and data.
 For the purposes of this document, only a simple PCM file will be explored.
 A WAV file contains a header and the raw data, in time format.

 What's bit size?
 Bit size determines how much information can be stored in a file.
 For most of today's purposes, bit size should be 16 bit. 8 bit files are smaller (1/2 the size),
 but have less resolution.

 Bit size deals with amplitude. In 8 bit recordings, a total of 256 (0 to 255) amplitude levels are available.
 In 16 bit, a total of 65,536 (-32768 to 32767) amplitude levels are available. The greater the resolution of
 the file is, the greater the realistic dynamic range of the file. CD-Audio uses 16 bit samples.

 What is Sample Rate?
 Sample rate is the number of samples per second. CD-Audio has a sample rate of 44,100. This means that 1 second
 of audio has 44,100 samples. DAT tapes have a sample rate of 48,000.

 When looking at frequency response, the highest frequency can be considered to be 1/2 of the sample rate.
 
 What are Channels?
 Channels are the number of separate recording elements in the data. For a real quick example,
 one channel is mono and two channels are stereo. In this document, both single and dual channel recordings
 will be discussed.

 What is the data?
 The data is the individual samples. An individual sample is the bit size times the number of channels.
 For example, a monaural (single channel), eight bit recording has an individual sample size of 8 bits.
 A monaural sixteen-bit recording has an individual sample size of 16 bits.
 A stereo sixteen-bit recording has an individual sample size of 32 bits.

 Samples are placed end-to-end to form the data. So, for example, if you have four samples (s1, s2, s3, s4)
 then the data would look like: s1s2s3s4.

 What is the header?
 The header is the beginning of a WAV (RIFF) file. The header is used to provide specifications on the file type,
 sample rate, sample size and bit size of the file, as well as its overall length.
 
 The header of a WAV (RIFF) file is 44 bytes long and has the following format:
 */
typedef struct _wav_header {
	char rld[4]; // "RIFF". Marks the file as a riff file. Characters are each 1 byte long.
	int rLen; // Size of the overall file - 8 bytes, in bytes (32-bit integer). Typically, you'd fill this in after creation.
	char wld[4]; // "WAVE". File Type Header. For our purposes, it always equals "WAVE".
	char fld[4]; // "fmt". Format chunk marker. Includes trailing null

	int fLen; // Length of format data as listed above
	short wFormatTag; // Type of format (1 is PCM) - 2 byte integer
	short wChannels; // Number of Channels - 2 byte integer
	int nSampleRate; // Sample Rate - 32 byte integer. Common values are 44100 (CD), 48000 (DAT). Sample Rate = Number of Samples per second, or Hertz.
	int nAvgBitsSampleRate; // (Sample Rate * BitsPerSample * Channels) / 8.
	short wBlockAlign; // 	(BitsPerSample * Channels) / 8.
					   // 1 - 8 bit mono
					   // 2 - 8 bit stereo/16 bit mono
					   // 4 - 16 bit stereo
	short wBitsPerSample; // Bits per sample
	char reserve[68];
	char dld[4]; // "data", chunk header. Marks the beginning of the data section.
	int wDataLength; // Size of the data section.
} wav_header_t;

wav_header_t wav_header;
#if 0
static void usage(const char *cmd)
{
	printf("Usage: %s ./ring.wav\n", cmd);
	assert_param(0);
}
#endif
static void dump_wav_header(wav_header_t *heder)
{
	printf("RIFF: %c%c%c%c\n", heder->rld[0], heder->rld[1], heder->rld[2], heder->rld[3]);
	printf("rLen: %d\n", heder->rLen);
	printf("wld: %c%c%c%c\n", heder->wld[0], heder->wld[1], heder->wld[2], heder->wld[3]);
	printf("fld: %c%c%c%c\n", heder->fld[0], heder->fld[1], heder->fld[2], heder->fld[3]);
	printf("fLen: %d\n", heder->fLen);
	printf("wFormatTag: %d\n", heder->wFormatTag);
	printf("wChannels: %d\n", heder->wChannels);
	printf("sample rate: %d\n", heder->nSampleRate);
	printf("nAvgBitsPerSample: %d\n", heder->nAvgBitsSampleRate);
	printf("wBlockAlign: %d\n", heder->wBlockAlign);
	printf("sample bits: %d\n", heder->wBitsPerSample);
	printf("data: %c%c%c%c\n", heder->dld[0], heder->dld[1], heder->dld[2], heder->dld[3]);
	printf("wSampleLength: %d\n", heder->wDataLength);
}

static int play(FILE *fp)
{
	assert_param(fp);
	int ret;
	int size;
	int dir = 0;
	char *buffer;
	int channels = wav_header.wChannels;
    unsigned int sample_rate = (unsigned int) wav_header.nSampleRate;
    int bit = wav_header.wBitsPerSample;
    int datablock = wav_header.wBlockAlign;
    //unsigned char ch[100]; //用来存储wav文件的头信息

	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
	ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	assert_return(ret >= 0);

	snd_pcm_hw_params_alloca(&params); // allocate memory for params
	ret = snd_pcm_hw_params_any(handle, params);// init params
	assert_return(ret >= 0);

	/** access permission */
	ret = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	assert_return(ret >= 0);

	switch (bit / 8) {
	case 1:
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
		break ;
	case 2:
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
		break ;
	case 4:
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S24_LE);
		break ;
	default:
		assert_param(0);
	}

	/** 1-Mono Channel, 2-stereo */
	ret = snd_pcm_hw_params_set_channels(handle, params, channels);
	assert_return(ret >= 0);

	ret = snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, &dir);
	assert_return(ret >= 0);

	ret = snd_pcm_hw_params(handle, params);
    assert_return(ret >= 0);

	ret = snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    assert_return(ret >= 0);

    size = frames * datablock;
    buffer = (char*) malloc(size);
	assert_return(buffer);
	/**
	 * 定位歌曲到数据区
	 */
    fseek(fp, sizeof(wav_header), SEEK_SET);
	while (1) {
		memset(buffer, 0, size);
        ret = fread(buffer, 1, size, fp);
		if(ret == 0) {
			sys_debug(1, "WAV read EOF");
			break;
		}

		/* write data to PCM device */
        while((ret = snd_pcm_writei(handle, buffer, frames)) < 0) {
			usleep(2000); 
			if (ret == -EPIPE) {
				/* EPIPE means underrun */
				sys_debug(3, "ERROR: underrun occurred\n");
				snd_pcm_prepare(handle);
			} else if (ret < 0) {
				sys_debug(3, "ERROR: error from writei: %s\n", snd_strerror(ret));
			}
		}
	}

	snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return 0;
}

void alsa_test_entry()
{
	func_enter();

	//int nr;
	FILE *fp;
	fp = fopen(WAV_FILE, "rb");
	assert_return(fp != NULL);

	fread(&wav_header, 1, sizeof(wav_header), fp);
	dump_wav_header(&wav_header);

	play(fp);
	fclose(fp);
	func_exit();
}

