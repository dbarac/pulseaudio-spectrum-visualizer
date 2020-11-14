#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <signal.h>

#define BUFSIZE 512
#define ROWS 42
#define COLS 238
#define N 512

uint16_t indices[BUFSIZE];

typedef struct {
	double re;
	double im;
} Complex;

void sigint_handler(int signo)
{
	if (signo == SIGINT) {
		printf("\e[?25h"); //enable cursor
		printf("\033[0m"); //set color to default
		exit(0);
	}
}

//complex exponentiation
Complex cexp(Complex z)
{
	Complex result;
	result.re = cos(z.im);
	result.im = sin(z.im);
	return result;
}

//complex multiplication
Complex cmul(Complex z1, Complex z2)
{
	Complex result;
	result.re = z1.re * z2.re - z1.im * z2.im;
	result.im = z1.re * z2.im  + z1.im * z2.re;
	return result;
}

void reverse_index_bits()
{
	uint16_t reversed;
	int bits = log2(N);
	for (uint16_t i = 0; i < N; i++) {
		indices[i] = i;
		reversed = 0;
		//number of bits to reverse depends on N (how many bits to represent N-1?)
		for (uint16_t bit = 0; bit < bits; bit++) {
			reversed <<= 1;
			reversed += indices[i] & 1;
			indices[i] >>= 1;
		}
		indices[i] = reversed;
	}
}

void fft(Complex result[BUFSIZE], float buffer[BUFSIZE])
{
	//iterative in-place Cooley-Tukey FFT

	//smallest FFT subproblem, each frequency bin is the same as the sample
	//with index = indices[bin] = reverse_bits(bin)
	for (uint16_t bin = 0; bin < N; bin++) {
		result[bin].re = buffer[indices[bin]];
	}
	for (uint16_t bin = 0; bin < N; bin++) {
		result[bin].im = 0.0;
	}
	//combine solutions to FFT subproblems (FFT butterfly)
	for (uint16_t M = 2; M <= N; M *= 2) { //M = current subproblem size (number of bins)

		for (uint16_t subproblem = 0; subproblem < N / M; subproblem++) {

			for (uint16_t k = 0; k < M / 2; k++) {
				Complex w = {.re = 0, .im = -2 * M_PI * k / M};
				Complex twiddle_factor = cexp(w);
				
				uint16_t even_index = subproblem * M + k;
				uint16_t odd_index = subproblem * M + k + M / 2;
				Complex even = result[even_index];
				Complex odd = cmul(twiddle_factor, result[odd_index]);
				result[even_index].re = even.re + odd.re;
				result[even_index].im = even.im + odd.im;
				result[odd_index].re = even.re - odd.re;
				result[odd_index].im = even.im - odd.im;
			}
		}
	}
}

void apply_hann_window(float samples[N]) {
	for (uint16_t i = 0; i < N; i++) {
		float ratio = (float)i / (N - 1);
		float weight = 0.5 * (1 - cos(2 * M_PI * ratio));
		samples[i] *= weight;
	}
}

void apply_hamming_window(Complex samples[N]) {
	for (uint16_t i = 0; i < N; i++) {
		float ratio = (float)i / (N - 1);
		float weight = 0.54 - (0.46 * cos(2 * M_PI * ratio));
		samples[i].im *= weight;
	}
}

void apply_triangle_window(Complex samples[N]) {
	for (uint16_t i = 0; i < N; i++) {
		float weight = 1 - abs(((float)i - (float)(N-1) / 2) / (N - 1));
		samples[i].im *= weight;
	}
}

void display_spectrum(Complex fft_res[BUFSIZE])
{
	static char spectrum[ROWS*COLS+1];
	spectrum[ROWS*COLS] = '\0';

	for (int i = 0; i < COLS; i++) {
		int magnitude = /*(float)i /COLS *150 */10* sqrt(fft_res[i].re*fft_res[i].re + fft_res[i].im*fft_res[i].im);
		
		//int magnitude = fabs(buf[i]) * 250;
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
	reverse_index_bits();
	//setup pulseaudio
	//sampling setup
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_FLOAT32LE,
		.rate = 11000,
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
	}

	float buf[BUFSIZE];
	Complex result[BUFSIZE];
	printf("\e[?25l"); //disable cursor
	printf("\033[1;36m");
	signal(SIGINT, sigint_handler);
	while (1) {
		/*if (pa_simple_flush(stream, &error)) {
			printf("flush failed\n");
		}*/
		//get samples
		if (pa_simple_read(stream, buf, sizeof(buf), &error) < 0) {
			printf("sample read failed\n");
			return 1;
		}
		//run fft
		float mean = 0;
		for (int i = 0; i < BUFSIZE; i++)
			mean += buf[i];
		mean /= N;
		for (int i = 0; i < BUFSIZE; i++)
			buf[i] -= 0.7*mean;
		//apply_hann_window(buf);
		fft(result, buf);
		display_spectrum(result);
		usleep(2000);
		//sleep(1);
		system("clear");
	}
}
