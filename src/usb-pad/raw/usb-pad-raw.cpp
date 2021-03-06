#include <algorithm>
#include "../padproxy.h"
#include "../../USB.h"
#include "../../Win32/Config-win32.h"
#include "raw-config.h"

namespace usb_pad_raw {

static bool sendCrap = false;

class RawInputPad : public Pad
{
public:
	RawInputPad(int port) : Pad(port)
	, doPassthrough(false)
	, usbHandle(INVALID_HANDLE_VALUE)
	{
		if (!InitHid())
			throw PadError("InitHid() failed!");
	}
	~RawInputPad() { Close(); }
	int Open();
	int Close();
	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset() { return 0; }

	static const TCHAR* Name()
	{
		return L"Raw Input";
	}

	static int Configure(int port, void *data);
	static std::vector<CONFIGVARIANT> GetSettings()
	{
		//TODO GetSettings()
		return std::vector<CONFIGVARIANT>();
	}
protected:
	HIDP_CAPS caps;
	HIDD_ATTRIBUTES attr;
	//PHIDP_PREPARSED_DATA pPreparsedData;
	//PHIDP_BUTTON_CAPS pButtonCaps;
	//PHIDP_VALUE_CAPS pValueCaps;
	//ULONG value;// = 0;
	USHORT numberOfButtons;// = 0;
	USHORT numberOfValues;// = 0;
	HANDLE usbHandle;// = (HANDLE)-1;
	//HANDLE readData;// = (HANDLE)-1;
	OVERLAPPED ovl;
	OVERLAPPED ovlW;
	
	uint32_t reportInSize;// = 0;
	uint32_t reportOutSize;// = 0;
	bool doPassthrough;
	wheel_data_t mDataCopy;
};

int RawInputPad::TokenIn(uint8_t *buf, int len)
{
	uint8_t data[64];
	DWORD waitRes;
	ULONG value = 0;
	int ply = 1 - mPort;

	//fprintf(stderr,"usb-pad: poll len=%li\n", len);
	if(this->doPassthrough && this->usbHandle != INVALID_HANDLE_VALUE)
	{
		//ZeroMemory(buf, len);
		ReadFile(this->usbHandle, data, MIN(this->caps.InputReportByteLength, sizeof(data)), 0, &this->ovl);
		waitRes = WaitForSingleObject(this->ovl.hEvent, 50);
		if(waitRes == WAIT_TIMEOUT || waitRes == WAIT_ABANDONED){
			CancelIo(this->usbHandle);
			return 0;
		}
		memcpy(buf, data, len);
		return len;
	}

	//in case compiler magic with bitfields interferes
	wheel_data_t data_summed;
	memset(&data_summed, 0xFF, sizeof(data_summed));
	data_summed.hatswitch = 0x8;
	data_summed.buttons = 0;

	//TODO Atleast GT4 detects DFP then
	if(sendCrap)
	{
		sendCrap = false;
		pad_copy_data(mType, buf, data_summed);
		return len;
	}

	int copied = 0;
	//TODO fix the logics, also Config.cpp
	MapVector::iterator it = mapVector.begin();
	for(; it!=mapVector.end(); it++)
	{

		if (data_summed.steering < (*it).data[ply].steering)
		{
			data_summed.steering = (*it).data[ply].steering;
			copied |= 1;
		}

		//if(data_summed.clutch < (*it).data[ply].clutch)
		//	data_summed.clutch = (*it).data[ply].clutch;

		if (data_summed.throttle < (*it).data[ply].throttle)
		{
			data_summed.throttle = (*it).data[ply].throttle;
			copied |= 2;
		}

		if (data_summed.brake < (*it).data[ply].brake)
		{
			data_summed.brake = (*it).data[ply].brake;
			copied |= 4;
		}

		data_summed.buttons |= (*it).data[ply].buttons;
		if(data_summed.hatswitch > (*it).data[ply].hatswitch)
			data_summed.hatswitch = (*it).data[ply].hatswitch;
	}

	if (!copied)
		memcpy(&data_summed, &mDataCopy, sizeof(wheel_data_t));
	else
	{
		if (!(copied & 1))
			data_summed.steering = mDataCopy.steering;
		if (!(copied & 2))
			data_summed.throttle = mDataCopy.throttle;
		if (!(copied & 4))
			data_summed.brake = mDataCopy.brake;
	}

	pad_copy_data(mType, buf, data_summed);

	if (copied)
		memcpy(&mDataCopy, &data_summed, sizeof(wheel_data_t));
	return len;
}


int RawInputPad::TokenOut(const uint8_t *data, int len)
{
	DWORD out = 0, err = 0, waitRes = 0;
	BOOL res;
	uint8_t outbuf[65];
	if(this->usbHandle == INVALID_HANDLE_VALUE) return 0;

	if(data[0] == 0x8 || data[0] == 0xB) return len;
	//if(data[0] == 0xF8 && data[1] == 0x5) 
	//	sendCrap = true;
	if (data[0] == 0xF8) return len; //don't send extended commands
	//If i'm reading it correctly MOMO report size for output has Report Size(8) and Report Count(7), so that's 7 bytes
	//Now move that 7 bytes over by one and add report id of 0 (right?). Supposedly mandatory for HIDs.
	memcpy(outbuf + 1, data, len - 1);
	outbuf[0] = 0;

	waitRes = WaitForSingleObject(this->ovlW.hEvent, 30);
	if(waitRes == WAIT_TIMEOUT || waitRes == WAIT_ABANDONED)
		CancelIo(this->usbHandle);

	//CancelIo(s->usbHandle); //Mind the ERROR_IO_PENDING, may break FFB
	res = WriteFile(this->usbHandle, outbuf, this->caps.OutputReportByteLength, &out, &this->ovlW);

	//err = GetLastError();
	//fprintf(stderr,"usb-pad: wrote %d, res: %d, err: %d\n", out, res, err);

	return len;
}

static void ParseRawInputHID(PRAWINPUT pRawInput)
{
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;
	HIDP_CAPS            Caps;
	PHIDP_BUTTON_CAPS    pButtonCaps = NULL;
	PHIDP_VALUE_CAPS     pValueCaps = NULL;
	UINT                 bufferSize = 0;
	ULONG                usageLength, value;
	TCHAR                name[1024] = {0};
	UINT                 nameSize = 1024;
	RID_DEVICE_INFO      devInfo = {0};
	std::wstring         devName;
	USHORT               capsLength = 0;
	USAGE                usage[MAX_BUTTONS] = {0};
	Mappings             *mapping = NULL;
	MapVector::iterator  it;
	int                  numberOfButtons;

	GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICENAME, name, &nameSize);

	devName = name;
	std::transform(devName.begin(), devName.end(), devName.begin(), ::toupper);

	for(it = mapVector.begin(); it != mapVector.end(); it++)
	{
		if((*it).hidPath == devName)
		{
			mapping = &(*it);
			break;
		}
	}

	if(mapping == NULL)
		return;

	CHECK( GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0 );
	CHECK( pPreparsedData = (PHIDP_PREPARSED_DATA)malloc(bufferSize) );
	CHECK( (int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0 );
	CHECK( HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS );
	//pSize = sizeof(devInfo);
	//GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICEINFO, &devInfo, &pSize);

	//Unset buttons/axes mapped to this device
	//ResetData(&mapping->data[0]);
	//ResetData(&mapping->data[1]);
	memset(&mapping->data[0], 0xFF, sizeof(wheel_data_t));
	memset(&mapping->data[1], 0xFF, sizeof(wheel_data_t));
	mapping->data[0].buttons = 0;
	mapping->data[1].buttons = 0;
	mapping->data[0].hatswitch = 0x8;
	mapping->data[1].hatswitch = 0x8;

	//Get pressed buttons
	CHECK( pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps) );
	//If fails, maybe wheel only has axes
	capsLength = Caps.NumberInputButtonCaps;
	HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData);

	numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;
	usageLength = numberOfButtons;
	NTSTATUS stat;
	if((stat = HidP_GetUsages(
			HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
			(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid)) == HIDP_STATUS_SUCCESS )
	{
		for(uint32_t i = 0; i < usageLength; i++)
		{
			uint16_t btn = mapping->btnMap[usage[i] - pButtonCaps->Range.UsageMin];
			for(int j=0; j<2; j++)
			{
				PS2WheelTypes wt = (PS2WheelTypes)conf.WheelType[j];
				if(PLY_IS_MAPPED(j, btn))
				{
					uint32_t wtbtn = (1 << convert_wt_btn(wt, PLY_GET_VALUE(j, btn))) & 0xFFF; //12bit mask
					mapping->data[j].buttons |= wtbtn;
				}
			}
		}
	}

	/// Get axes' values
	CHECK( pValueCaps = (PHIDP_VALUE_CAPS)malloc(sizeof(HIDP_VALUE_CAPS) * Caps.NumberInputValueCaps) );
	capsLength = Caps.NumberInputValueCaps;
	if(HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )
	{
		for(USHORT i = 0; i < capsLength; i++)
		{
			if(HidP_GetUsageValue(
					HidP_Input, pValueCaps[i].UsagePage, 0,
					pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
					(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
				) != HIDP_STATUS_SUCCESS )
			{
				continue; // if here then maybe something is up with HIDP_CAPS.NumberInputValueCaps
			}

			//fprintf(stderr, "Min/max %d/%d\t", pValueCaps[i].LogicalMin, pValueCaps[i].LogicalMax);
			//TODO can be simpler?
			//Get mapped axis for physical axis
			uint16_t v = 0;
			switch(pValueCaps[i].Range.UsageMin)
			{
				// X-axis 0x30
				case HID_USAGE_GENERIC_X: 
					v = mapping->axisMap[0]; 
					break;
				// Y-axis
				case HID_USAGE_GENERIC_Y: 
					v = mapping->axisMap[1]; 
					break;
				// Z-axis
				case HID_USAGE_GENERIC_Z: 
					v = mapping->axisMap[2]; 
					break;
				// Rotate-X
				case HID_USAGE_GENERIC_RX: 
					v = mapping->axisMap[3]; 
					break;
				// Rotate-Y
				case HID_USAGE_GENERIC_RY: 
					v = mapping->axisMap[4]; 
					break;
				// Rotate-Z 0x35
				case HID_USAGE_GENERIC_RZ: 
					v = mapping->axisMap[5]; 
					break;
				case HID_USAGE_GENERIC_HATSWITCH:
					//fprintf(stderr, "Hat: %02X\n", value);
					v = mapping->axisMap[6];
					break;
			}

			int type = 0;
			for(int j=0; j<2; j++)
			{
				if(!PLY_IS_MAPPED(j, v))
					continue;

				type = conf.WheelType[j];

				switch(PLY_GET_VALUE(j, v))
				{
				case PAD_AXIS_X: // X-axis
					//fprintf(stderr, "X: %d\n", value);
					// Need for logical min too?
					//generic_data.axis_x = ((value - pValueCaps[i].LogicalMin) * 0x3FF) / (pValueCaps[i].LogicalMax - pValueCaps[i].LogicalMin);
					if(type == WT_DRIVING_FORCE_PRO)
						mapping->data[j].steering = (value * 0x3FFF) / pValueCaps[i].LogicalMax;
					else
						//XXX Limit value range to 0..1023 if using 'generic' wheel descriptor
						mapping->data[j].steering = (value * 0x3FF) / pValueCaps[i].LogicalMax;
					break;

				case PAD_AXIS_Y: // Y-axis
					if(!(devInfo.hid.dwVendorId == 0x046D && devInfo.hid.dwProductId == 0xCA03))
						//XXX Limit value range to 0..255
						mapping->data[j].clutch = (value * 0xFF) / pValueCaps[i].LogicalMax;
					break;

				case PAD_AXIS_Z: // Z-axis
					//fprintf(stderr, "Z: %d\n", value);
					//XXX Limit value range to 0..255
					mapping->data[j].throttle = (value * 0xFF) / pValueCaps[i].LogicalMax;
					break;

				case PAD_AXIS_RZ: // Rotate-Z
					//fprintf(stderr, "Rz: %d\n", value);
					//XXX Limit value range to 0..255
					mapping->data[j].brake = (value * 0xFF) / pValueCaps[i].LogicalMax;
					break;

				case PAD_AXIS_HAT: // TODO Hat Switch
					//fprintf(stderr, "Hat: %02X\n", value);
					//TODO 4 vs 8 direction hat switch
					if(pValueCaps[i].LogicalMax == 4 && value < 4)
						mapping->data[j].hatswitch = HATS_8TO4[value];
					else
						mapping->data[j].hatswitch = value;
					break;
				}
			}
		}
	}

	Error:
	SAFE_FREE(pPreparsedData);
	SAFE_FREE(pButtonCaps);
	SAFE_FREE(pValueCaps);
}

static void ParseRawInputKB(PRAWINPUT pRawInput)
{
	Mappings			*mapping = NULL;
	MapVector::iterator	it;

	for(it = mapVector.begin(); it != mapVector.end(); it++)
	{
		if(!it->hidPath.compare(TEXT("Keyboard")))
		{
			mapping = &(*it);
			break;
		}
	}

	if(mapping == NULL)
		return;

	for(uint32_t i = 0; i < PAD_BUTTON_COUNT; i++)
	{
		uint16_t btn = mapping->btnMap[i];
		for(int j=0; j<2; j++)
		{
			if(PLY_IS_MAPPED(j, btn))
			{
				PS2WheelTypes wt = (PS2WheelTypes)conf.WheelType[j];
				if(PLY_GET_VALUE(j, mapping->btnMap[i]) == pRawInput->data.keyboard.VKey)
				{
					uint32_t wtbtn = convert_wt_btn(wt, i);
					if(pRawInput->data.keyboard.Flags & RI_KEY_BREAK)
						mapping->data[j].buttons &= ~(1 << wtbtn); //unset
					else //if(pRawInput->data.keyboard.Flags == RI_KEY_MAKE)
						mapping->data[j].buttons |= (1 << wtbtn); //set
				}
			}
		}
	}

	for(uint32_t i = 0; i < 4 /*PAD_HAT_COUNT*/; i++)
	{
		uint16_t btn = mapping->hatMap[i];
		for(int j=0; j<2; j++)
		{
			if(PLY_IS_MAPPED(j, btn))
			{
				if(PLY_GET_VALUE(j, mapping->hatMap[i]) == pRawInput->data.keyboard.VKey)
				if(pRawInput->data.keyboard.Flags & RI_KEY_BREAK)
					mapping->data[j].hatswitch = 0x8;
				else //if(pRawInput->data.keyboard.Flags == RI_KEY_MAKE)
					mapping->data[j].hatswitch = HATS_8TO4[i];
			}
		}
	}
}

int RawInputPad::Open()
{
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;

	//TODO Better place?
	LoadMappings(mapVector);

	memset(&this->ovl, 0, sizeof(OVERLAPPED));
	memset(&this->ovlW, 0, sizeof(OVERLAPPED));

	//this->padState.initStage = 0;
	this->doPassthrough = !!conf.DFPPass;//TODO per player
	this->usbHandle = INVALID_HANDLE_VALUE;
	std::wstring path;
	{
		CONFIGVARIANT var(N_JOYSTICK, CONFIG_TYPE_WCHAR);
		if (LoadSetting(mPort, APINAME, var))
			path = var.wstrValue;
		else
			return 1;
	}

	this->usbHandle = CreateFileW(path.c_str(), GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

	if(this->usbHandle != INVALID_HANDLE_VALUE)
	{
		this->ovl.hEvent = CreateEvent(0, 0, 0, 0);
		this->ovlW.hEvent = CreateEvent(0, 0, 0, 0);

		HidD_GetAttributes(this->usbHandle, &(this->attr));
		HidD_GetPreparsedData(this->usbHandle, &pPreparsedData);
		HidP_GetCaps(pPreparsedData, &(this->caps));
		HidD_FreePreparsedData(pPreparsedData);
		return 0;
	}
	else
		fwprintf(stderr, L"Could not open device '%s'.\nPassthrough and FFB will not work.\n", path.c_str());

	return 1;
}

int RawInputPad::Close()
{
	if(this->usbHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(this->usbHandle);
		CloseHandle(this->ovl.hEvent);
		CloseHandle(this->ovlW.hEvent);
	}

	this->usbHandle = INVALID_HANDLE_VALUE;
	return 0;
}
};

HWND msgWindow = NULL;
WNDPROC eatenWndProc = NULL;
HWND eatenWnd = NULL;
HHOOK hHook = NULL, hHookKB = NULL;
extern HINSTANCE hInst;

static void ParseRawInput(PRAWINPUT pRawInput)
{
	if(pRawInput->header.dwType == RIM_TYPEKEYBOARD)
		usb_pad_raw::ParseRawInputKB(pRawInput);
	else
		usb_pad_raw::ParseRawInputHID(pRawInput);
}

void RegisterRaw(HWND hWnd, DWORD flags)
{
	RAWINPUTDEVICE Rid[3];
	Rid[0].usUsagePage = 0x01; 
	Rid[0].usUsage = HID_USAGE_GENERIC_GAMEPAD; 
	Rid[0].dwFlags = flags|RIDEV_INPUTSINK; // adds game pad
	Rid[0].hwndTarget = hWnd;

	Rid[1].usUsagePage = 0x01; 
	Rid[1].usUsage = HID_USAGE_GENERIC_JOYSTICK; 
	Rid[1].dwFlags = flags|RIDEV_INPUTSINK; // adds joystick
	Rid[1].hwndTarget = hWnd;

	Rid[2].usUsagePage = 0x01; 
	Rid[2].usUsage = HID_USAGE_GENERIC_KEYBOARD; 
	Rid[2].dwFlags = flags|RIDEV_INPUTSINK;// | RIDEV_NOLEGACY;   // adds HID keyboard and also !ignores legacy keyboard messages
	Rid[2].hwndTarget = hWnd;

	if (RegisterRawInputDevices(Rid, 3, sizeof(Rid[0])) == FALSE) {
		//registration failed. Call GetLastError for the cause of the error.
		fprintf(stderr, "Could not (de)register raw input devices.\n");
	}
}

LRESULT CALLBACK RawInputProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_CREATE:
		if(eatenWnd == NULL) RegisterRaw(hWnd, 0);
		break;
	case WM_INPUT:
		{
		//if(skipInput) return;
		PRAWINPUT pRawInput;
		UINT      bufferSize=0;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &bufferSize, sizeof(RAWINPUTHEADER));
		pRawInput = (PRAWINPUT)malloc(bufferSize);
		if(!pRawInput)
			break;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, pRawInput, &bufferSize, sizeof(RAWINPUTHEADER));
		ParseRawInput(pRawInput);
		free(pRawInput);
		break;
		}
	case WM_DESTROY:
		if(eatenWnd==NULL) RegisterRaw(hWnd, RIDEV_REMOVE);
		UninitWindow();
		break;
	}
	
	if(eatenWndProc)
		return CallWindowProc(eatenWndProc, hWnd, uMsg, wParam, lParam);
	//else
	//	return DefWindowProc(hWnd, uMsg, wParam, lParam);
	return 0;
}

LRESULT CALLBACK HookProc(INT code, WPARAM wParam, LPARAM lParam)
{
	MSG *msg = (MSG*)lParam;
	
	//fprintf(stderr, "hook: %d, %d, %d\n", code, wParam, lParam);
	if(code == HC_ACTION)
		RawInputProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
	return CallNextHookEx(hHook, code, wParam, lParam);
}

LRESULT CALLBACK KBHookProc(INT code, WPARAM wParam, LPARAM lParam)
{
	fprintf(stderr, "kb hook: %d, %d, %d\n", code, wParam, lParam);
	KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT*) lParam;
	//if(code == HC_ACTION)
	//	RawInputProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
	return CallNextHookEx(0, code, wParam, lParam);
}

int InitWindow(HWND hWnd)
{
#if 1
	if (!InitHid())
		return 0;

	RegisterRaw(hWnd, 0);
	hHook = SetWindowsHookEx(WH_GETMESSAGE, HookProc, hInst, 0);
	//hHookKB = SetWindowsHookEx(WH_KEYBOARD_LL, KBHookProc, hInst, 0);
	int err = GetLastError();
#else
	eatenWnd = hWnd;
	eatenWndProc = (WNDPROC) SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)RawInputProc);
	RegisterRaw(hWnd, 0);
#endif
	return 1;
}

void UninitWindow()
{
	if(hHook)
	{
		UnhookWindowsHookEx(hHook);
		//UnhookWindowsHookEx(hHookKB);
		hHook = 0;
	}
	if(eatenWnd != NULL)
	{
		RegisterRaw(eatenWnd, RIDEV_REMOVE);
		SetWindowLongPtr(eatenWnd, GWLP_WNDPROC, (LONG_PTR)eatenWndProc);
		eatenWndProc = NULL;
		eatenWnd = NULL;
	}
}

// ---------
#include "raw-config-res.h"
BOOL CALLBACK ConfigureRawDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
int usb_pad_raw::RawInputPad::Configure(int port, void *data)
{
	Win32Handles *h = (Win32Handles*)data;
	INT_PTR res = RESULT_FAILED;
	if (InitWindow(h->hWnd))
	{
		res = DialogBoxParam(h->hInst, MAKEINTRESOURCE(IDD_RAWCONFIG), h->hWnd, ConfigureRawDlgProc, port);
		UninitWindow();
	}
	return res;
}

namespace usb_pad_raw {
REGISTER_PAD(APINAME, RawInputPad);
};
