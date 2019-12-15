#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

/*
# 21.675 x 240 = 5202 scanlines/second
# 15625 / 5202 = 3.00365244136870434448 samples/scanline
# 
# Again, we're stuck with the LRCLK frequency of 15.625KHz. Our only
# remaining recourse is to cheat a little, and encode our audio tracks
# at a rate of 15.606KHz:
# 
# 15606 / 5202 = 3.00
 */

#define VID_FRAMES_PER_SEC		21.675

#define VID_PIXELS_PER_LINE		320
#define VID_LINES_PER_FRAME		240

#define VID_CHUNK_LINES			240

#define VID_AUDIO_SAMPLES_PER_SCANLINE	3

#define VID_AUDIO_CHANNELS		2

#define VID_AUDIO_SAMPLE_RATE		15606 /* 15625 */

#define VID_AUDIO_SAMPLES_PER_CHUNK		\
	(VID_LINES_PER_FRAME *			\
	 VID_AUDIO_SAMPLES_PER_SCANLINE *	\
	 VID_AUDIO_CHANNELS)

#define VID_AUDIO_BYTES_PER_CHUNK		\
	(VID_AUDIO_SAMPLES_PER_CHUNK * 2)

#define VID_BUFSZ			(VID_PIXELS_PER_LINE * VID_CHUNK_LINES)
#define AUD_BUFSZ			(VID_AUDIO_SAMPLES_PER_CHUNK)

#define        roundup(x, y)   ((((x)+((y)-1))/(y))*(y))

/*
 * next_chunk_size is the size of the following chunk, including
 * its CHUNK_HEADER structure
 * cur_vid_size is the size of the video data in the current chunk
 * cur_aud_size is the size of the audio data in the current chunk
 *
 * A .VID file will always start with a CHUNK_HEADER with no data where
 * cur_vid_size and cur_aud_size reflect the largest video and audio block
 * in the file.
 *
 * It will also end with a CHUNK_HEADER where all fields are 0.
 */

typedef struct chunk_header {
	uint64_t		next_chunk_size;
	uint64_t		cur_vid_size;
	uint64_t		cur_aud_size;
	uint64_t		pad; /* pad to 32 byte multiple */
} CHUNK_HEADER;

static void
add_video_data (char * path, FILE * out)
{
	FILE * in;
	uint8_t chunk[1024];
	size_t r;

	in = fopen (path, "r+");

	while (1) {
		r = fread (chunk, 1, sizeof(chunk), in);
		fwrite (chunk, r, 1, out);
		if (r < sizeof(chunk))
			break;
	}

	fclose (in);

	return;
}

static void
add_audio_data (uint16_t * data, size_t len, FILE * out)
{
	fwrite (data, len, 1, out);
	return;
}

static void
add_header (size_t vidlen, size_t audiolen, FILE * out)
{
	CHUNK_HEADER ch;
	fpos_t p;

	ch.next_chunk_size = 0;
	ch.cur_vid_size = vidlen;
	ch.cur_aud_size = audiolen;

	fwrite (&ch, sizeof(ch), 1, out);

	fgetpos (out, &p);

	return;
}

static void
update_prev_header (fpos_t prevpos, size_t nextsize, FILE * out)
{
	fpos_t curpos;
	CHUNK_HEADER ch;

	memset (&ch, 0, sizeof(ch));

	fgetpos (out, &curpos);

	fsetpos (out, &prevpos);

	fread (&ch, sizeof(ch), 1, out);

	ch.next_chunk_size = nextsize;

	fsetpos (out, &prevpos);

	fwrite (&ch, sizeof(ch), 1, out);

	fsetpos (out, &curpos);

	return;
}

int
main (int argc, char * argv[])
{
	FILE * audio;
	FILE * video;
	FILE * out;
	uint16_t samples[AUD_BUFSZ];
	char vidpath[FILENAME_MAX];
	uint32_t vidcnt;
	struct stat st;
	size_t pad;
	size_t r;
	size_t framesize;
	size_t largestvid;
	size_t largestaud;
	fpos_t curpos;
	fpos_t prevpos;
	CHUNK_HEADER ch;

	if (argc > 4)
		exit (1);

	audio = fopen (argv[2], "r+");

        if (audio == NULL) {
		fprintf (stderr, "[%s]: ", argv[2]);
		perror ("file open failed");
		exit (1);
	}

	out = fopen (argv[3], "w+");

	if (out == NULL) {
		fprintf (stderr, "[%s]: ", argv[3]);
		perror ("file open failed");
		exit (1);
	}

	/* Add an initial header */

	add_header (0, 0, out);

	curpos = 0;
	largestvid = 0;
	largestaud = 0;

	vidcnt = 1;

	while (1) {
		sprintf (vidpath, "%s/vidout%08d.jpg", argv[1], vidcnt);
		if (stat (vidpath, &st) != 0)
			break;
		vidcnt++;
		memset (samples, 0, sizeof(samples));
		r = fread (samples, 1, sizeof(samples), audio);
		fgetpos (out, &prevpos);
		/*
		 * We'd like the audio data to start on a cache line
		 * boudary, but the video data size varies due to
		 * compression. So we pad the video data out to a
		 * multiple of a cache line size (32 bytes).
		 */
		pad = roundup (st.st_size, 32) - st.st_size;
		add_header (st.st_size + pad, r, out);
		add_video_data (vidpath, out);
		/* Add pad bytes */
		add_audio_data (samples, pad, out);
		add_audio_data (samples, r, out);
		framesize = sizeof(CHUNK_HEADER) + st.st_size + pad + r;
		update_prev_header (curpos, framesize, out);
		curpos = prevpos;
		if ((st.st_size + pad) > largestvid)
			largestvid = st.st_size + pad;
		if (r > largestaud)
			largestaud = r;
	}

	/* Last block has 'next chunk size' set to 0 */

	update_prev_header (curpos, 0, out);

	/* Add tail header */

	add_header (0, 0, out);

	/*
	 * Update the initial header to contain the largest
	 * video and audio chunk sizes to help the playback
	 * software decide what size buffers to allocate.
	 */

	curpos = 0;
	fsetpos (out, &curpos);
	fread (&ch, sizeof(ch), 1, out);
	ch.cur_vid_size = largestvid;
	ch.cur_aud_size = largestaud;
	fsetpos (out, &curpos);
	fwrite (&ch, sizeof(ch), 1, out);

	fclose (audio);
	fclose (out);

	exit(0);
}
