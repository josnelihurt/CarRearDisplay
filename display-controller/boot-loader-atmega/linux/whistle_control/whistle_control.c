#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>
#include <sys/time.h>
#include <math.h>




#define UNIQUENESS 8


fftw_complex *in, *out;
fftw_plan p;  

void fft_init(int N)
{
	in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
	out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
	p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
}


//this function is called whenever an action has to be taken. the argument determines the action
void remote_action(unsigned char *str)
{
	unsigned char buf[128];

	printf("---------- %s\n", str);

	sprintf(buf, "echo %s > /dev/ttyUSB0", str);
	system(buf);
}




void decke_fsm(int newcommand, int n)
{
	static int lastpeak;
	static int lastaction = 0;

	if(newcommand)
		lastpeak = n;
	else
	{
		if(n > lastpeak + 3)
		{
			if(lastaction == 1)
				remote_action("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
			else
				remote_action("22222222222222222222222222222222222222222222222222222222");

			lastaction = 1 - lastaction;
		}
			

		lastpeak = n;
	}
}


void stisch_fsm(int newcommand, int n)
{
	static int lastpeak;
	static int lastaction = 0;

	if(newcommand)
		lastpeak = n;
	else
	{
		if(n < lastpeak - 3)
		{
			if(lastaction == 1)
				remote_action("CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC");
			else
				remote_action("333333333333333333333333333333333333333333333333333333333");

			lastaction = 1 - lastaction;
		}
			

		lastpeak = n;
	}
}


void fft_peak_analyze(int n)
{
	static struct timeval lasttimeofpeak;


	struct timeval currenttime;
	int newcommand;

	gettimeofday(&currenttime, NULL);

	if(currenttime.tv_sec - lasttimeofpeak.tv_sec > 1 || currenttime.tv_usec - lasttimeofpeak.tv_usec > 1000000)
		newcommand = 1;
	else
		newcommand = 0;		
	

	decke_fsm(newcommand, n);
	stisch_fsm(newcommand, n);


	gettimeofday(&lasttimeofpeak, NULL);
}

void fft_analyze(unsigned char *buf, int length)
{
	int i;

	//center the mic signal around 0
	for(i=0; i<length; ++i)
	{
		in[i][0] = (double)buf[i] - 128;
		in[i][1] = 0;
	}


	fftw_execute(p);


	//oldmax contains the 2nd best maximum (with at least +-1 distance to best maximum)
	double oldmax = 99999999;
	double max = 0;
	int maxpos = 0;
	for(i=0; i<length; ++i)
	{
		double abs = out[i][0] * out[i][0] + out[i][1] * out[i][1]; //build the square of the absolute value of the complex fft output
		if(abs > max)
		{
			if(fabs(maxpos - i) > 1)
				oldmax = max;
			max = abs;
			maxpos = i;
		}
	}

	//check if the ratio between best and 2nd best maximum exceeds our requirements
	if(max / oldmax > UNIQUENESS && maxpos > 10 && maxpos < length/2) //length / 2 is the nyquist frequency
	{
		printf("%d\t%f\n", maxpos, max/oldmax);

		fft_peak_analyze(maxpos);
	}

	
}




main (int argc, char *argv[])
{
	int i;
	int err;
	unsigned char buf[128];
	snd_pcm_t *capture_handle;
	snd_pcm_hw_params_t *hw_params;

	if ((err = snd_pcm_open (&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stderr, "cannot open audio device %s (%s)\n", 
			 "default",
			 snd_strerror (err));
		exit (1);
	}
	   
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
			 
	if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	//signed 8 bit would be better but ALSA refuses to use it
	if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, SND_PCM_FORMAT_U8)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
			 snd_strerror (err));
		exit (1);
	}


	//set the sample rate to 8000hz
	unsigned int val = 8000;
	int dir = 0; 
	if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &val, &dir)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	//mono
	if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 1)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	snd_pcm_hw_params_free (hw_params);

	if ((err = snd_pcm_prepare (capture_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
			 snd_strerror (err));
		exit (1);
	}



	fft_init(sizeof(buf));

	//main loop. read samples from the soundcard and analyze them by doing the FFT
	for (;;) {
		if ((err = snd_pcm_readi (capture_handle, buf, 128)) != 128) {
			fprintf (stderr, "read from audio interface failed (%s)\n",
				 snd_strerror (err));
			exit (1);
		}

		fft_analyze(buf, sizeof(buf));
	}

	snd_pcm_close (capture_handle);
	exit (0);
}

