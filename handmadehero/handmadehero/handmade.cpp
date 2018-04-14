#include "handmade.h"

static void GameOutputSound(game_sound_output_buffer *SoundBuffer)
{
	static float tSine;
	int16_t ToneVolume = 3000;
	int ToneHz = 256;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	int16_t *SampleOut = SoundBuffer->Samples;
	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
		float SineValue = sinf(tSine);
		int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

		tSine += 2.0f * Pi32 * 1.0f / (float)WavePeriod;
	}
}

static void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	uint8_t *Row = (uint8_t *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32_t *Pixel = (uint32_t *)Row;
		for (int X = 0; X < Buffer->Width; ++X)
		{
			uint8_t Blue = (X + BlueOffset);
			uint8_t Green = (Y + GreenOffset);

			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}

static void GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer)
{
	static int X = 0;
	static int Y = 0;
	static int ToneHz = 256;

	game_controller_input *Input0 = &Input->Controllers[0];
	if (Input0->IsAnalog)
	{
		ToneHz = 256 + (int)(128.0f * (Input0->EndY));
		X += (int)4.0f * (Input0->EndX);
	}
	else
	{

	}

	if (Input0->Down.EndedDown)
	{
		Y += 1;
	}
	GameOutputSound(SoundBuffer);
	RenderWeirdGradient(Buffer, X, Y);
}