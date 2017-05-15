#include <SDL2/sdl.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>

#include "vulkfm.h"

static float sampTime;

static void audio_fill_buffer_s16(void* userdata, Uint8* stream, int len)
{
	int16_t* buff = (int16_t*)stream;
	int samples = len / sizeof(int16_t);

	VulkFM* synth = (VulkFM*)userdata;


	for(int i = 0; i < samples;++i)
	{
		float sample = synth->evaluate();
		synth->update(sampTime);
		// Clip instead of overflow
		sample = sample<-1.f?-1.f:(sample>1.f?1.f:sample);
		*buff++ = (int16_t)(sample*(32768>>1));
	}
}

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;

static void open_window()
{
	int reslut = SDL_CreateWindowAndRenderer(640, 480, 0, &window, &renderer );
	if( reslut != 0)
	{
		printf("Could not create window. %s\n", SDL_GetError());
		exit(1);
	}
}

static void render(SDL_Renderer* rend, VulkFM *synth)
{
	SDL_SetRenderDrawColor(rend, 0,0,0,255);
	SDL_RenderClear(rend);

	auto samples = synth->getOutBuffer();
	SDL_SetRenderDrawColor(rend, 255,255,255,255);

	SDL_Point points[512];

	for(int i = 0 ; i < 512; ++i)
	{
		points[i].x = i + ((640-512)>>1);
		points[i].y = samples[i*2] * 100 + 240;
	}

	SDL_RenderDrawLines(rend, points, 512);

	SDL_RenderPresent(rend);
}


int main(int argc, char*argv[])
{
	VulkFM vulkSynth;

	if( 0 != SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) )
	{
		printf("Error\n");
	}

	SDL_AudioSpec want,got;

	memset(&want,0,sizeof(want));

	want.freq = 22050;
	want.format = AUDIO_S16SYS;
	// want.format = AUDIO_F32SYS;
	want.channels = 1;
	want.samples = 800;
	want.userdata = &vulkSynth;
	want.callback = audio_fill_buffer_s16;

	auto audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if( audio_device == 0 )
	{
		printf("open audio error %s\n", SDL_GetError() );
		return 1;
	}


	printf("Got an audio device %d with %d channels, %d frequency and %d samples in %d format.\n", audio_device, got.channels, got.freq, got.samples, got.format);


	int keymap[] {
		SDL_SCANCODE_Q,
		SDL_SCANCODE_2,
		SDL_SCANCODE_W,
		SDL_SCANCODE_3,
		SDL_SCANCODE_E,
		SDL_SCANCODE_R,
		SDL_SCANCODE_5,
		SDL_SCANCODE_T,
		SDL_SCANCODE_6,
		SDL_SCANCODE_Y,
		SDL_SCANCODE_7,
		SDL_SCANCODE_U,
		SDL_SCANCODE_I,
		SDL_SCANCODE_9,
		SDL_SCANCODE_O,
		SDL_SCANCODE_0,
		SDL_SCANCODE_P,
	};


	open_window();
	if( window == nullptr)
		exit(1);

	sampTime = 1.0f/got.freq;

	SDL_PauseAudioDevice(audio_device,0);

	SDL_Event event;
	bool quit = false;

	int octave = 4;

	int lastActive = 0;

	while(!quit)
	{
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
			case SDL_KEYDOWN:
				if(event.key.repeat != 0)
					continue;
				if(event.key.keysym.sym == SDLK_ESCAPE)
					quit = true;
				else {
					auto key = event.key.keysym.scancode;
					for(int i = 0 ; i < sizeof(keymap); ++i)
					{
						if(key == keymap[i]) {
							int note = i + 12*octave;
							vulkSynth.trigger(note,0,127);
						}
					}
				}
				break;

			case SDL_KEYUP:
				if(event.key.repeat == 0) {
					auto key = event.key.keysym.scancode;
					for(int i = 0 ; i < sizeof(keymap); ++i)
					{
						if(key == keymap[i])
						{
							int note = i + 12*octave;
							vulkSynth.release(note,0,127);
						}
					}
				}
				break;
			case SDL_QUIT:
				quit = true;
				break;
			}
		}

		// Render UI / visualization

		render(renderer, &vulkSynth);

		// if( lastActive != vulkSynth.activeVoices() )
		// {
		// 	lastActive = vulkSynth.activeVoices();
		// 	printf("there are %d active voices\n", lastActive);
		// }

		fflush(stdout);
		SDL_Delay(16);
	}

	printf("leaving\n");
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_CloseAudio();
	SDL_Quit();
	return 0;
}