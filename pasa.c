#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <ncurses.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <fftw3.h>

#define PI 3.14159265358979323846264338327f

struct sigaction old_sigint;
bool run;

void onSigInt()
{
	// reset SIGINT.
	sigaction(SIGINT, &old_sigint, NULL);
	
	// tell main loop to exit.
	run = false;
}

// hanning window.
float windowFunction(int n, int N)
{
	return 0.5f * (1.0f - cosf(2.0f * PI * n / (N - 1.0f)));
}

void printUsage()
{

}

int main(int argc, char* argv[])
{
	static const pa_sample_spec ss =
	{
		.format = PA_SAMPLE_FLOAT32LE,
		.rate = 44100,
		.channels = 1
	};

	// configuration.
	int framesPerSecond = 30;
	double upperFrequency = 3520.0; // A7
	int barChar = ACS_VLINE;

	// parse command line arguments.
	int c;
	while ((c = getopt(argc, argv, "r:f:h")) != -1)
	{
		switch(c)
		{
			case 'r':
				framesPerSecond = atoi(optarg);
				break;

			case 'f':
				upperFrequency = atof(optarg);
				break;

			case 'h':
			case '?':
				printUsage();
				return 1;

			default:
				abort();
		}
	}

	// open record device
	int error;
	pa_simple *s = pa_simple_new(NULL, "pasa", PA_STREAM_RECORD, optind < argc ? argv[optind] : NULL, "pasa", &ss, NULL, NULL, &error);

	// check error
	if (!s)
	{
		fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
		return 1;
	}

	// input buffer.
	const int size = 44100 / framesPerSecond;
	const double scale = 2.0 / size;
	float window[size];
	float buffer[size];

	// compute window.
	for(int n = 0; n < size; n++)
		window[n] = windowFunction(n, size);

	// fftw setup
	double *in = (double*)fftw_malloc(sizeof(double) * size);
	fftw_complex *out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size);
	fftw_plan plan = fftw_plan_dft_r2c_1d(size, in, out, FFTW_MEASURE);

	// replace SIGINT handler.
	struct sigaction sigIntAction;
	memset(&sigIntAction, 0, sizeof(sigIntAction));
	sigIntAction.sa_handler = &onSigInt;
	sigaction(SIGINT, &sigIntAction, &old_sigint);

	run = true;

	// ncurses setup.
	initscr();
	//start_color();
	curs_set(0);

	// record loop
	while(run)
	{
		// read from device.
		if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0)
		{
			pa_simple_free(s);
			fprintf(stderr, "pa_simple_read() failed: %s\n", pa_strerror(error));
			return 1;
		}

		// convert input data.
		int i;
		for (i = 0; i < size; i++)
			in[i] = (double)(window[i] * buffer[i]);
	
		// fast fourier transform.
		fftw_execute(plan);
		
		// draw bars.
		erase();
		
		// todo: use the float-point value and implement proper interpolation.
		double barWidthD = (upperFrequency / framesPerSecond) / COLS;
		int barWidth = (int)ceil(barWidthD);

		//mvprintw(0, 0, "size = %d, barWidth = %f, COLS = %d", size, barWidthD, COLS);

		i = 0;
		int col = 0;
		while(i < size)
		{
			// get average.
			double power = 0.0;
			for(int j = 0; j < barWidth && i < size; i++, j++)
			{
				double re = out[i][0] * scale;
				double im = out[i][1] * scale;
				power += re * re + im * im;
			}
			power *= (1.0 / barWidth); // average.
			if(power < 1e-15) power = 1e-15;

			// compute decibels.
			int dB = LINES + (int)(10.0 * log10(power));
			if(dB < 0) dB = 0;
			
			// draw line.
			move(LINES - dB, col);
			vline(barChar, dB);

			// go to next column.
			col++;
		}

		// draw to screen.
		refresh();
	}

	// clean up fftw
	fftw_destroy_plan(plan);
	fftw_free(in);
	fftw_free(out);

	// clean up pulseaudio
	pa_simple_free(s);

	// close ncurses
	endwin();

	return 0;
}

