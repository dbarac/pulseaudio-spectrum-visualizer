#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#define BUFSIZE 128
#define ROWS 30
#define COLS 128

void display_spectrum(float buf[BUFSIZE])
{
	char spectrum[ROWS*COLS+1];
	spectrum[ROWS*COLS] = '\0';

	for (int i = 0; i < COLS; i++) {
		int magnitude = fabs(buf[i]) * 250;
		for (int j = 0; j < ROWS; j++) {
			spectrum[j*COLS + i] = (magnitude >= ROWS - j) ? '*' : ' ';
		}
	}
	for (int i = 1; i <= ROWS; i++)
		spectrum[i*COLS-1] = '\n';
	printf("%s", spectrum);
}

int main(int argc, char *argv[])
{
	//setup pulseaudio
	//sampling setup
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_FLOAT32LE,
		.rate = 44100,
		.channels = 1
	};
	//buffer setup
	static const pa_buffer_attr buff_attr = {
		.fragsize = (uint32_t) -1,
		.maxlength = 2048,
		.minreq = (uint32_t) -1,
		.prebuf = (uint32_t) -1,
		.tlength = (uint32_t) -1
	};
	
	pa_simple *stream = NULL;
	int error;
	char *device_name = NULL;
	if (argc == 2)
		device_name = argv[1];
	stream = pa_simple_new(NULL, "spectrum-visualizer", PA_STREAM_RECORD, device_name,
	                	   "record", &ss, NULL, &buff_attr, &error);
	if (!stream) {
		printf("pa_simple_new() failed\n");
		return 1;
	} else {
		printf("NICE\n");
	}

	float buf[BUFSIZE];
		//TODO: sigint handler to enable cursor again
	//printf("\e[?25l"); //disable cursor
	//enable cursor printf("\e[?25h");
	while (1) {
		/*if (pa_simple_flush(stream, &error)) {
			printf("flush failed\n");
		}*/
		//get samples
		if (pa_simple_read(stream, buf, sizeof(buf), &error) < 0) {
			printf("sample read failed\n");
			return 1;
		}
		//process

		//run fft

		display_spectrum(buf);
		usleep(10000);
		//sleep(1);
		system("clear");
	}
}
