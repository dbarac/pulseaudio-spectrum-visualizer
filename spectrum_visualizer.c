#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#define N_SAMPLES 1024
#define SCALING_FACTOR 20
#define MAX_TERM_SIZE 400*400

uint16_t ROWS, COLS;
uint16_t indices[N_SAMPLES];

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

void get_terminal_size(int signo)
{
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	ROWS = w.ws_row;
	COLS = w.ws_col;
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
	result.re = z1.re*z2.re - z1.im*z2.im;
	result.im = z1.re*z2.im  + z1.im*z2.re;
	return result;
}

void reverse_index_bits()
{
	uint16_t reversed;
	uint16_t bits = log2(N_SAMPLES);
	for (uint16_t i = 0; i < N_SAMPLES; i++) {
		indices[i] = i;
		reversed = 0;
		for (uint16_t bit = 0; bit < bits; bit++) {
			reversed <<= 1;
			reversed += indices[i] & 1;
			indices[i] >>= 1;
		}
		indices[i] = reversed;
	}
}

//iterative in-place Cooley-Tukey FFT
void fft(Complex result[N_SAMPLES], float buffer[N_SAMPLES])
{
	//smallest FFT subproblem, each frequency bin is the same as the sample
	//with index = indices[bin] = reverse_bits(bin)
	for (uint16_t bin = 0; bin < N_SAMPLES; bin++) {
		result[bin].re = buffer[indices[bin]];
	}
	for (uint16_t bin = 0; bin < N_SAMPLES; bin++) {
		result[bin].im = 0.0;
	}
	//combine solutions to FFT subproblems
	//M = current subproblem size (number of bins)
	for (uint16_t M = 2; M <= N_SAMPLES; M *= 2) {
		for (uint16_t subproblem = 0; subproblem < N_SAMPLES/M; subproblem++) {
			for (uint16_t k = 0; k < M/2; k++) {
				Complex w = {.re = 0, .im = -2 * M_PI * k / M};
				Complex twiddle_factor = cexp(w);
				
				uint16_t even_index = subproblem*M + k;
				uint16_t odd_index = subproblem*M + k + M/2;
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

void apply_hann_window(float samples[N_SAMPLES])
{
	for (uint16_t i = 0; i < N_SAMPLES; i++) {
		float ratio = (float)i / (N_SAMPLES - 1);
		float weight = 0.5 * (1 - cos(2 * M_PI * ratio));
		samples[i] *= weight;
	}
}

void apply_hamming_window(float samples[N_SAMPLES])
{
	for (uint16_t i = 0; i < N_SAMPLES; i++) {
		float ratio = (float)i / (N_SAMPLES - 1);
		float weight = 0.54 - (0.46 * cos(2 * M_PI * ratio));
		samples[i] *= weight;
	}
}

void display_spectrum(Complex fft_res[N_SAMPLES])
{
	static char spectrum[MAX_TERM_SIZE];
	spectrum[ROWS*COLS] = '\0';
	int bins = N_SAMPLES / 4 / COLS; //display 1/4 of result bins
	for (int i = 0; i < COLS; i++) {
		int magnitude = 0;
		for (int j = 0; j < bins; j++) {
			Complex freq = fft_res[bins*i+j];
			magnitude += SCALING_FACTOR * sqrt(freq.re*freq.re + freq.im*freq.im);
		}
		magnitude /= bins;
		for (int j = 0; j < ROWS; j++) {
			spectrum[j*COLS+i] = (magnitude >= ROWS - j) ? '*' : ' ';
		}
	}
	for (int i = 1; i <= ROWS; i++)
		spectrum[i*COLS-1] = '\n';
	printf("%s", spectrum);
}

int main(int argc, char *argv[])
{
	//setup pulseaudio
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_FLOAT32LE,
		.rate = 44100,
		.channels = 1
	};
	static const pa_buffer_attr buff_attr = {
		.fragsize = (uint32_t) -1,
		.maxlength = 32768,
		.minreq = (uint32_t) -1,
		.prebuf = (uint32_t) -1,
		.tlength = (uint32_t) -1
	};
	int error;
	pa_simple *stream = NULL;
	char *device_name = NULL;
	if (argc == 2)
		device_name = argv[1];
	stream = pa_simple_new(NULL, "spectrum-visualizer", PA_STREAM_RECORD, device_name,
	                	   "record", &ss, NULL, &buff_attr, &error);
	if (!stream) {
		printf("pa_simple_new() failed\n");
		return 1;
	}

	reverse_index_bits();
	float buf[N_SAMPLES];
	Complex result[N_SAMPLES];

	printf("\e[?25l"); //disable cursor
	printf("\033[1;36m"); //change color
	signal(SIGINT, sigint_handler);
	signal(SIGWINCH, get_terminal_size);
	get_terminal_size(SIGWINCH);

	while (1) {
		//get samples
		if (pa_simple_read(stream, buf, sizeof(buf), &error) < 0) {
			printf("sample reading failed\n");
			return 1;
		}

		apply_hann_window(buf);
		fft(result, buf);
		system("clear");
		display_spectrum(result);
		//usleep(500);
	}
}
