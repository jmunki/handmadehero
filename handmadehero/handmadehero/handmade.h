#if !defined(HANDMADE_H)

#include <stdint.h>
#include <math.h>

#define Pi32 3.14159265359
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// PLATFORM LAYER PROVIDES TO GAME


//	GAME PROVIDES TO PLATFORM LAYER
struct game_offscreen_buffer
{
	void *Memory;
	int Width;
	int Height;
	int Pitch;
};

struct game_sound_output_buffer
{
	int16_t *Samples;
	int SampleCount;
	int SamplesPerSecond;
};

struct game_button_state
{
	int HalfTransitionCount;
	bool EndedDown;
};

struct game_controller_input
{
	bool IsAnalog;

	float StartX;
	float StartY;

	float MinX;
	float MinY;

	float MaxX;
	float MaxY;

	float EndX;
	float EndY;

	union
	{
		game_button_state Buttons[6];
		struct
		{
			game_button_state Up;
			game_button_state Down;
			game_button_state Left;
			game_button_state Right;
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
		};
	};
};

struct game_input
{
	game_controller_input Controllers[4];
};

static void GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer);

#define HANDMADE_H
#endif