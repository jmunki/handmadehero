#include <stdint.h>


#include "handmade.h"
#include "handmade.cpp"

#include <windows.h>
#include <stdio.h>
#include <Xinput.h>
#include <dsound.h>
#include "win32_handmade.h"

//---------START GLOBALS------------------

static bool GlobalRunning;
static win32_offscreen_buffer GlobalBackBuffer;
LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

//---------END GLOBALS------------------

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//---------------------------
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

//---------------------------
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);
//DirectSoundCreate(_In_opt_ LPCGUID pcGuidDevice, _Outptr_ LPDIRECTSOUND *ppDS, _Pre_null_ LPUNKNOWN pUnkOuter);

static void Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState) { XInputGetState = XInputGetStateStub; }

		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
	}
}

static void Win32InitDSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize)
{
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if (DSoundLibrary)
	{
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					PrimaryBuffer->SetFormat(&WaveFormat);
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						OutputDebugStringA("\n\nPrimary Sound Buffer Created!\n\n");
					}
					else
					{

					}
				}
			}
			else
			{

			}

			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				OutputDebugStringA("\n\nSecondary Sound Buffer Created!\n\n");
			}
		}
		else
		{

		}
	}
	else
	{

	}
}

win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

static void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

static void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight,
	win32_offscreen_buffer Buffer,
	int X, int Y, int Width, int Height)
{

	StretchDIBits(DeviceContext,
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer.Width, Buffer.Height,
		Buffer.Memory,
		&Buffer.Info,
		DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
	case WM_SIZE:
	{
		break;
	}
	case WM_DESTROY:
	{
		GlobalRunning = false;
		break;
	}
	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
		break;
	}
	case WM_CLOSE:
	{
		GlobalRunning = false;
		break;
	}
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		uint32_t VKCode = WParam;
		bool WasDown = ((LParam & (1 << 30)) != 0);
		bool IsDown = ((LParam & (1 << 31)) == 0);

		if (WasDown != IsDown)
		{
			if (VKCode == 'W')
			{
				OutputDebugString("W");
			}
		}

		bool AltKeyWasDown = ((LParam & (1 << 29)) != 0);
		if ((VKCode == VK_F4) && AltKeyWasDown)
		{
			GlobalRunning = false;
		}
	}
	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);

		int X = Paint.rcPaint.left;
		int Y = Paint.rcPaint.top;
		int Width = Paint.rcPaint.right - Paint.rcPaint.left;
		int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

		win32_window_dimension Dimension = Win32GetWindowDimension(Window);
		Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer, X, Y, Width, Height);
		EndPaint(Window, &Paint);
		break;
	}
	default:
	{
		Result = DefWindowProc(Window, Message, WParam, LParam);
		break;
	}
	}

	return Result;
}

static void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{
		int8_t *DestSample = (int8_t *)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
		{
			*DestSample++ = 0;
		}

		DestSample = (int8_t *)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
		{
			*DestSample++ = 0;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

static void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{

		int16_t *DestSample = (int16_t *)Region1;
		int16_t *SourceSample = SourceBuffer->Samples;

		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
		DestSample = (int16_t *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

static void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit, game_button_state *NewState)
{
	NewState->EndedDown = (XInputButtonState & ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	int64_t PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1200, 700);

	WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	//WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	RegisterClassA(&WindowClass);

	HWND Window = CreateWindowExA(
		0,
		WindowClass.lpszClassName,
		"Handmade Hero",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		0,
		0,
		Instance,
		0);

	if (Window)
	{

		HDC DeviceContext = GetDC(Window);
		win32_sound_output SoundOutput = {};

		SoundOutput.SamplesPerSecond = 48000;
		SoundOutput.BytesPerSample = sizeof(int16_t) * 2;
		SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;	
		SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
		Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
		Win32ClearBuffer(&SoundOutput);
		GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

		GlobalRunning = true;

		int16_t *Samples = (int16_t *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);


		game_input Input[2] = {};
		game_input *NewInput = &Input[0];
		game_input *OldInput = &Input[1];


		LARGE_INTEGER LastCounter;
		QueryPerformanceCounter(&LastCounter);
		int64_t LastCycleCounter = __rdtsc();
		while (GlobalRunning)
		{
			MSG Message;
			while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
			{
				if (Message.message == WM_QUIT)
				{
					GlobalRunning = false;
				}
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}

			int MaxControllerCount = XUSER_MAX_COUNT;
			if (MaxControllerCount > ArrayCount(NewInput->Controllers))
			{
				MaxControllerCount = ArrayCount(NewInput->Controllers);
			}

			for (DWORD controllerIndex = 0; controllerIndex < MaxControllerCount; ++controllerIndex)
			{
				game_controller_input *OldController = &OldInput->Controllers[controllerIndex];
				game_controller_input *NewController = &NewInput->Controllers[controllerIndex];

				XINPUT_STATE ControllerState;
				if (XInputGetState(controllerIndex, &ControllerState) == ERROR_SUCCESS)
				{
					XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

					bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
					bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
					bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
					bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

					NewController->IsAnalog = true;
					NewController->StartX = OldController->EndX;
					NewController->StartY = OldController->EndY;

					float X;
					if (Pad->sThumbLX < 0)
					{
						X = (float)Pad->sThumbLX / 32768.0f;
					}
					else
					{
						X = (float)Pad->sThumbLX / 32767.0f;
					}
					NewController->MinX = NewController->MaxX = NewController->EndX = X;

					float Y;
					if (Pad->sThumbLY < 0)
					{
						Y = (float)Pad->sThumbLY / 32768.0f;
					}
					else
					{
						Y = (float)Pad->sThumbLY / 32767.0f;
					}
					NewController->MinY = NewController->MaxY = NewController->EndY = Y;
					
					Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Down, XINPUT_GAMEPAD_A, &NewController->Down);
					Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Right, XINPUT_GAMEPAD_B, &NewController->Right);
					Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Left, XINPUT_GAMEPAD_X, &NewController->Left);
					Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Up, XINPUT_GAMEPAD_Y, &NewController->Up);
					Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
					Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
				}
				else
				{

				}
			}

			DWORD ByteToLock;
			DWORD TargetCursor;
			DWORD BytesToWrite;
			DWORD PlayCursor;
			DWORD WriteCursor;
			bool SoundIsValid = false;
			if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
			{
				ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
				TargetCursor = ((PlayCursor + (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize);

				if (ByteToLock > TargetCursor)
				{
					BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
					BytesToWrite += TargetCursor;
				}
				else
				{
					BytesToWrite = TargetCursor - ByteToLock;
				}
				SoundIsValid = true;
			}

			game_sound_output_buffer SoundBuffer = {};
			SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
			SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
			SoundBuffer.Samples = Samples;


			game_offscreen_buffer Buffer = {};
			Buffer.Memory = GlobalBackBuffer.Memory;
			Buffer.Width = GlobalBackBuffer.Width;
			Buffer.Height = GlobalBackBuffer.Height;
			Buffer.Pitch = GlobalBackBuffer.Pitch;
			GameUpdateAndRender(NewInput, &Buffer, &SoundBuffer);

			if(SoundIsValid)
			{
				Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
			}

			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer, 0, 0, Dimension.Width, Dimension.Height);

			int64_t EndCycleCounter = __rdtsc();


			LARGE_INTEGER EndCounter;
			QueryPerformanceCounter(&EndCounter);

			int64_t CyclesElapsed = EndCycleCounter - LastCycleCounter;
			int64_t CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
			int32_t MSPerFrame = (int32_t)((1000*CounterElapsed) / PerfCountFrequency);
			int32_t FPS = PerfCountFrequency / CounterElapsed;
			int32_t MegaCyclesPerFrame = (int32_t)CyclesElapsed / (1000 * 1000);
			int32_t ProcessorSpeed = (int32_t) FPS * MegaCyclesPerFrame;

#if 0
			char Buffer[256];
			wsprintf(Buffer, "\n\n  %d ms/frame (%d FPS)\n  %d Mcycles/frame (%d MHz)", MSPerFrame, FPS, MegaCyclesPerFrame, ProcessorSpeed);
			OutputDebugStringA(Buffer);
#endif
			LastCounter = EndCounter;
			LastCycleCounter = EndCycleCounter;

			game_input *Temp = NewInput;
			NewInput = OldInput;
			OldInput = Temp;
		}
	}

	return(0);
}