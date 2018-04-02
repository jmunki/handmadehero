#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>

#define Pi32 3.14159265359f

//---------------------------
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;
//---------------------------
struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};
struct win32_window_dimension
{
	int Width;
	int Height;
};
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

static void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
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

static void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer->Width; ++X)
		{
			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);

			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
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
		uint32 VKCode = WParam;
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

struct win32_sound_output
{
	int ToneHz;
	int ToneVolume;
	uint32 RunningSampleIndex;
	int SamplesPerSecond;
	int WavePeriod;
	int HalfWavePeriod;
	int BytesPerSample;
	int SecondaryBufferSize;
};

static void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{

		int16 *SampleOut = (int16 *)Region1;
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			real32 t = 2.0f * Pi32 * (real32)SoundOutput->RunningSampleIndex / (real32)SoundOutput->WavePeriod;
			real32 SineValue = sinf(t);
			int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}

		SampleOut = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			real32 t = 2.0f * Pi32 * (real32)SoundOutput->RunningSampleIndex / (real32)SoundOutput->WavePeriod;
			real32 SineValue = sinf(t);
			int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1200, 700);

	WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	//WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	RegisterClass(&WindowClass);

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

		//	Graphics Test
		int XOffset = 0;
		int YOffset = 0;

		//	Sound Test
		win32_sound_output SoundOutput = {};

		SoundOutput.ToneHz = 528;
		SoundOutput.ToneVolume = 1000;
		SoundOutput.RunningSampleIndex = 0;
		SoundOutput.SamplesPerSecond = 48000;
		SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
		SoundOutput.HalfWavePeriod = SoundOutput.WavePeriod / 2;
		SoundOutput.BytesPerSample = sizeof(int16) * 2;
		SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;		
		Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
		Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
		GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

		GlobalRunning = true;
		LARGE_INTEGER LastCounter;
		QueryPerformanceCounter(&LastCounter);
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

			for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex)
			{
				XINPUT_STATE ControllerState;
				if (XInputGetState(controllerIndex, &ControllerState) == ERROR_SUCCESS)
				{
					XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

					bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
					bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
					bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
					bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
					bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
					bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
					bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
					bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
					bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
					bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
					bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
					bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

					int16 StickX = Pad->sThumbLX;
					int16 StickY = Pad->sThumbLY;

					if (Up)
					{
						YOffset += 2;
					}

					if (Left)
					{
						XOffset += 2;
					}

					if (Down)
					{
						YOffset -= 2;
					}

					if (Right)
					{
						XOffset -= 2;
					}

					XINPUT_VIBRATION Vibration;
					Vibration.wLeftMotorSpeed = 0;
					Vibration.wRightMotorSpeed = 0;
					if (LeftShoulder)
					{
						Vibration.wLeftMotorSpeed = 60000;
					}
					if (RightShoulder)
					{
						Vibration.wRightMotorSpeed = 60000;
					}
					XInputSetState(0, &Vibration);
				}
				else
				{

				}
			}

			XOffset += 1;

			RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

			//	Direct Sound output test
			DWORD PlayCursor;
			DWORD WriteCursor;
			if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
			{
				DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
				DWORD BytesToWrite;
				if (ByteToLock == PlayCursor)
				{
					BytesToWrite = 0;
				}
				else if (ByteToLock > PlayCursor)
				{
					BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
					BytesToWrite += PlayCursor;
				}
				else
				{
					BytesToWrite = PlayCursor - ByteToLock;
				}

				Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
			}

			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer, 0, 0, Dimension.Width, Dimension.Height);

			LARGE_INTEGER EndCounter;
			QueryPerformanceCounter(&EndCounter);

			int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
			int32 MSPerFrame = (int32)((1000*CounterElapsed) / PerfCountFrequency);
			int32 FPS = PerfCountFrequency / CounterElapsed;

			char Buffer[256];
			wsprintf(Buffer, "\nMS per frame: %dms\nFPS: %d", MSPerFrame, FPS);
			OutputDebugStringA(Buffer);

			LastCounter = EndCounter;
		}
	}

	return(0);
}