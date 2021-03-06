#ifndef USBPAD_H
#define USBPAD_H

#include "../qemu-usb/vl.h"
#include "../usb-mic/usb.h"
#include "../configuration.h"

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define CHECK(exp)		{ if(!(exp)) goto Error; }
#define SAFE_FREE(p)	{ if(p) { free(p); (p) = NULL; } }

#define S_CONFIG_JOY TEXT("Joystick")
#define N_JOYSTICK TEXT("joystick")

// Most likely as seen on https://github.com/matlo/GIMX
#define CMD_DOWNLOAD			0x00
#define CMD_DOWNLOAD_AND_PLAY	0x01
#define CMD_PLAY				0x02
#define CMD_STOP				0x03
#define CMD_DEFAULT_SPRING_ON	0x04
#define CMD_DEFAULT_SPRING_OFF	0x05
#define CMD_NORMAL_MODE			0x08
#define CMD_EXTENDED_CMD		0xF8
#define CMD_SET_LED				0x09 //??
#define CMD_RAW_MODE			0x0B
#define CMD_SET_DEFAULT_SPRING	0x0E
#define CMD_SET_DEAD_BAND		0x0F

#define EXT_CMD_CHANGE_MODE_DFP				0x01
#define EXT_CMD_WHEEL_RANGE_200_DEGREES		0x02
#define EXT_CMD_WHEEL_RANGE_900_DEGREES		0x03
#define EXT_CMD_CHANGE_MODE					0x09
#define EXT_CMD_REVERT_IDENTITY				0x0a
#define EXT_CMD_CHANGE_MODE_G25				0x10
#define EXT_CMD_CHANGE_MODE_G25_NO_DETACH	0x11
#define EXT_CMD_SET_RPM_LEDS				0x12
#define EXT_CMD_CHANGE_WHEEL_RANGE			0x81

#define FTYPE_CONSTANT                           0x00
#define FTYPE_SPRING                             0x01
#define FTYPE_DAMPER                             0x02
#define FTYPE_AUTO_CENTER_SPRING                 0x03
#define FTYPE_SAWTOOTH_UP                        0x04
#define FTYPE_SAWTOOTH_DOWN                      0x05
#define FTYPE_TRAPEZOID                          0x06
#define FTYPE_RECTANGLE                          0x07
#define FTYPE_VARIABLE                           0x08
#define FTYPE_RAMP                               0x09
#define FTYPE_SQUARE_WAVE                        0x0A
#define FTYPE_HIGH_RESOLUTION_SPRING             0x0B
#define FTYPE_HIGH_RESOLUTION_DAMPER             0x0C
#define FTYPE_HIGH_RESOLUTION_AUTO_CENTER_SPRING 0x0D
#define FTYPE_FRICTION                           0x0E

enum PS2WheelTypes {
	WT_GENERIC, // DF or any other LT wheel in non-native mode
	WT_DRIVING_FORCE_PRO, //LPRC-11000? DF GT can be downgraded to Pro (?)
	WT_GT_FORCE, //formula gp
};

// hold intermediate wheel data
struct wheel_data_t
{
	int32_t steering;
	uint32_t buttons;
	uint32_t hatswitch;
	uint32_t hat_horz;
	uint32_t hat_vert;

	int32_t clutch; //no game uses though
	int32_t throttle;
	int32_t brake;
};

struct spring
{
	uint8_t dead1 : 8; //Lower limit of central dead band
	uint8_t dead2 : 8; //Upper limit of central dead band
	uint8_t k1 : 4;   //Low (left or push) side spring constant selector
	uint8_t k2 : 4;   //High (right or pull) side spring constant selector
	uint8_t s1 : 4;   //Low side slope inversion (1 = inverted)
	uint8_t s2 : 4;   //High side slope inversion (1 = inverted)
	uint8_t clip : 8; //Clip level (maximum force), on either side
};

struct autocenter
{
	uint8_t k1;
	uint8_t k2;
	uint8_t clip;
};

struct variable
{
	uint8_t initial1; //Initial level for Force 0
	uint8_t initial2; //Initial level for Force 2
	uint8_t s1 : 4; //Force 0 Step size
	uint8_t t1 : 4; //Force 0 Step duration (in main loops)
	uint8_t s2 : 4;
	uint8_t t2 : 4;
	uint8_t d1 : 4; //Force 0 Direction (0 = increasing, 1 = decreasing)
	uint8_t d2 : 4;
};

struct ramp
{
	uint8_t level1; //max force
	uint8_t level2; //min force
	uint8_t dir;
	uint8_t time : 4;
	uint8_t step : 4;
};

struct friction
{
	uint8_t k1;
	uint8_t k2;
	uint8_t clip;
	uint8_t s1 : 4;
	uint8_t s2 : 4;
};

struct damper
{
	uint8_t k1;
	uint8_t s1;
	uint8_t k2;
	uint8_t s2;
	uint8_t clip; //dfp only
};

//packet is 8 bytes
struct ff_data
{
	uint8_t cmdslot;    // 0x0F cmd, 0xF0 slot
	uint8_t type;       // force type or cmd param
	union u
	{
		uint8_t params[5];
		struct spring spring;
		struct autocenter autocenter;
		struct variable variable;
		struct friction friction;
		struct damper damper;
	} u; //if anon union: gcc needs -fms-extensions?
	uint8_t padd0;
};

struct ff_state
{
	uint8_t slot_type[4];
	uint8_t slot_force[4];
	ff_data slot_ffdata[4];
	bool deadband;
};

class Pad
{
public:
	Pad(int port) : mPort(port), mFFstate({ 0 }) {}
	virtual ~Pad() {}
	virtual int Open() = 0;
	virtual int Close() = 0;
	virtual int TokenIn(uint8_t *buf, int len) = 0;
	virtual int TokenOut(const uint8_t *data, int len) = 0;
	virtual int Reset() = 0;

	virtual PS2WheelTypes Type() { return mType; }
	virtual void Type(PS2WheelTypes type) { mType = type; }
	virtual int Port() { return mPort; }
	virtual void Port(int port) { mPort = port; }

	static std::vector<CONFIGVARIANT> GetSettings();

protected:
	PS2WheelTypes mType;
	wheel_data_t mWheelData;
	ff_state mFFstate;
	int mPort;
};


//L3/R3 for newer wheels
//enum PS2Buttons : uint32_t {
//	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE, 
//	PAD_L1, PAD_L2, PAD_R1, PAD_R2,
//	PAD_SELECT, PAD_START,
//	PAD_L3, PAD_R3, //order
//	PAD_BUTTON_COUNT
//};

//???
//enum DFButtons : uint32_t {
//	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE, 
//	PAD_R2, 
//	PAD_L2,
//	PAD_R1, 
//	PAD_L1,
//	PAD_SELECT, PAD_START,
//	PAD_BUTTON_COUNT
//};

//DF Pro buttons (?)
//Based on Tokyo Xtreme Racer Drift 2
//GT4 flips R1/L1 with R2/L2 with DF wheel type
enum PS2Buttons : uint32_t {
	PAD_CROSS = 0, //menu up - GT Force
	PAD_SQUARE, //menu down
	PAD_CIRCLE, //X
	PAD_TRIANGLE, //Y
	PAD_R1, //A? <pause> in GT4
	PAD_L1, //B
	PAD_R2, 
	PAD_L2,
	PAD_SELECT, PAD_START,
	PAD_R3, PAD_L3, //order, afaik not used on any PS2 wheel anyway
	PAD_BUTTON_COUNT
};

enum PS2Axis : uint32_t {
	PAD_AXIS_X,
	PAD_AXIS_Y,
	PAD_AXIS_Z,
	PAD_AXIS_RZ,
	PAD_AXIS_HAT,//Treat as axis for mapping purposes
	PAD_AXIS_COUNT
};

enum PS2HatSwitch {
	PAD_HAT_N = 0,
	PAD_HAT_NE,
	PAD_HAT_E,
	PAD_HAT_SE,
	PAD_HAT_S,
	PAD_HAT_SW,
	PAD_HAT_W,
	PAD_HAT_NW,
	PAD_HAT_COUNT
};

static const int HATS_8TO4 [] = {PAD_HAT_N, PAD_HAT_E, PAD_HAT_S, PAD_HAT_W};

#define PAD_VID			0x046D
#define PAD_PID			0xCA03 //black MOMO
#define GENERIC_PID		0xC294 //actually Driving Force aka PID that most logitech wheels initially report
#define DF_PID			0xC294
#define DFP_PID			0xC298 //SELECT + R3 + RIGHT SHIFT PADDLE (R1) ???
#define DFGT_PID		0xC29A
#define FORMULA_PID		0xC202 //Yellow Wingman Formula
#define FGP_PID			0xC20E //Formula GP (maybe GT FORCE LPRC-1000)
#define FFGP_PID		0xC293 // Formula Force GP
#define MAX_BUTTONS		32
#define MAX_AXES		7 //random 7: axes + hatswitch
#define MAX_JOYS		16

/**
  linux hid-lg4ff.c
  http://www.spinics.net/lists/linux-input/msg16570.html
  Every Logitech wheel reports itself as generic Logitech Driving Force wheel (VID 046d, PID c294). This is done to ensure that the 
  wheel will work on every USB HID-aware system even when no Logitech driver is available. It however limits the capabilities of the 
  wheel - range is limited to 200 degrees, G25/G27 don't report the clutch pedal and there is only one combined axis for throttle and 
  brake. The switch to native mode is done via hardware-specific command which is different for each wheel. When the wheel 
  receives such command, it simulates reconnect and reports to the OS with its actual PID.
  Currently not emulating reattachment. Any games that expect to?
**/

// Any game actually queries for hid reports?
//#define pad_hid_report_descriptor pad_driving_force_pro_hid_report_descriptor
//#define pad_hid_report_descriptor pad_momo_hid_report_descriptor
//#define pad_hid_report_descriptor pad_generic_hid_report_descriptor

/* descriptor Logitech Driving Force Pro */
static const uint8_t dfp_dev_descriptor[] = {
	/* bLength             */ 0x12, //(18)
	/* bDescriptorType     */ 0x01, //(1)
	/* bcdUSB              */ WBVAL(0x0110), //(272) //USB 1.1
	/* bDeviceClass        */ 0x00, //(0)
	/* bDeviceSubClass     */ 0x00, //(0)
	/* bDeviceProtocol     */ 0x00, //(0)
	/* bMaxPacketSize0     */ 0x08, //(8)
	/* idVendor            */ WBVAL(0x046d),
	/* idProduct           */ WBVAL(DFP_PID),
	/* bcdDevice           */ WBVAL(0x0001), //(1)
	/* iManufacturer       */ 0x03, //(1)
	/* iProduct            */ 0x01, //(2)
	/* iSerialNumber       */ 0x00, //(0)
	/* bNumConfigurations  */ 0x01, //(1)
};

static const uint8_t pad_dev_descriptor[] = {
	/* bLength             */ 0x12, //(18)
	/* bDescriptorType     */ 0x01, //(1)
	/* bcdUSB              */ WBVAL(0x0110), //(272) //USB 1.1
	/* bDeviceClass        */ 0x00, //(0)
	/* bDeviceSubClass     */ 0x00, //(0)
	/* bDeviceProtocol     */ 0x00, //(0)
	/* bMaxPacketSize0     */ 0x08, //(8)
	/* idVendor            */ WBVAL(0x046d),
	/* idProduct           */ WBVAL(GENERIC_PID), //WBVAL(0xc294), 0xc298 dfp
	/* bcdDevice           */ WBVAL(0x0001), //(1)
	/* iManufacturer       */ 0x01, //(1)
	/* iProduct            */ 0x02, //(2)
	/* iSerialNumber       */ 0x00, //(0)
	/* bNumConfigurations  */ 0x01, //(1)

};

//https://lkml.org/lkml/2011/5/28/140
//https://github.com/torvalds/linux/blob/master/drivers/hid/hid-lg.c
// separate axes version
static const uint8_t pad_driving_force_hid_report_descriptor[] = {
	0x05, 0x01, /* Usage Page (Desktop), */
	0x09, 0x04, /* Usage (Joystik), */
	0xA1, 0x01, /* Collection (Application), */
	0xA1, 0x02, /* Collection (Logical), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x0A, /* Report Size (10), */
	0x14, /* Logical Minimum (0), */
	0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
	0x34, /* Physical Minimum (0), */
	0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
	0x09, 0x30, /* Usage (X), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x0C, /* Report Count (12), */
	0x75, 0x01, /* Report Size (1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x05, 0x09, /* Usage (Buttons), */
	0x19, 0x01, /* Usage Minimum (1), */
	0x29, 0x0c, /* Usage Maximum (12), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x02, /* Report Count (2), */
	0x06, 0x00, 0xFF, /* Usage Page (Vendor: 65280), */
	0x09, 0x01, /* Usage (?: 1), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x08, /* Report Size (8), */
	0x81, 0x02, /* Input (Variable), */
	0x25, 0x07, /* Logical Maximum (7), */
	0x46, 0x3B, 0x01, /* Physical Maximum (315), */
	0x75, 0x04, /* Report Size (4), */
	0x65, 0x14, /* Unit (Degrees), */
	0x09, 0x39, /* Usage (Hat Switch), */
	0x81, 0x42, /* Input (Variable, Null State), */
	0x75, 0x01, /* Report Size (1), */
	0x95, 0x04, /* Report Count (4), */
	0x65, 0x00, /* Unit (none), */
	0x06, 0x00, 0xFF, /* Usage Page (Vendor: 65280), */
	0x09, 0x01, /* Usage (?: 1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x08, /* Report Size (8), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x09, 0x31, /* Usage (Y), */
	0x81, 0x02, /* Input (Variable), */
	0x09, 0x35, /* Usage (Rz), */
	0x81, 0x02, /* Input (Variable), */
	0xC0, /* End Collection, */
	0xA1, 0x02, /* Collection (Logical), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x95, 0x07, /* Report Count (7), */
	0x75, 0x08, /* Report Size (8), */
	0x09, 0x03, /* Usage (?: 3), */
	0x91, 0x02, /* Output (Variable), */
	0xC0, /* End Collection, */
	0xC0 /* End Collection */
};

static const uint8_t pad_driving_force_pro_hid_report_descriptor[] = {
	0x05, 0x01, /* Usage Page (Desktop), */
	0x09, 0x04, /* Usage (Joystik), */
	0xA1, 0x01, /* Collection (Application), */
	0xA1, 0x02, /* Collection (Logical), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x0E, /* Report Size (14), */
	0x14, /* Logical Minimum (0), */
	0x26, 0xFF, 0x3F, /* Logical Maximum (16383), */
	0x34, /* Physical Minimum (0), */
	0x46, 0xFF, 0x3F, /* Physical Maximum (16383), */
	0x09, 0x30, /* Usage (X), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x0E, /* Report Count (14), */
	0x75, 0x01, /* Report Size (1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x05, 0x09, /* Usage Page (Button), */
	0x19, 0x01, /* Usage Minimum (01h), */
	0x29, 0x0E, /* Usage Maximum (0Eh), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x04, /* Report Size (4), */
	0x25, 0x07, /* Logical Maximum (7), */
	0x46, 0x3B, 0x01, /* Physical Maximum (315), */
	0x65, 0x14, /* Unit (Degrees), */
	0x09, 0x39, /* Usage (Hat Switch), */
	0x81, 0x42, /* Input (Variable, Nullstate), */
	0x65, 0x00, /* Unit, */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x75, 0x08, /* Report Size (8), */
	0x81, 0x01, /* Input (Constant), */
	0x09, 0x31, /* Usage (Y), */
	0x81, 0x02, /* Input (Variable), */
	0x09, 0x35, /* Usage (Rz), */
	0x81, 0x02, /* Input (Variable), */
	0x81, 0x01, /* Input (Constant), */
	0xC0, /* End Collection, */
	0xA1, 0x02, /* Collection (Logical), */
	0x09, 0x02, /* Usage (02h), */
	0x95, 0x07, /* Report Count (7), */
	0x91, 0x02, /* Output (Variable), */
	0xC0, /* End Collection, */
	0xC0 /* End Collection */
};

static const uint8_t pad_momo_hid_report_descriptor[] = {
	0x05, 0x01, /* Usage Page (Desktop), */
	0x09, 0x04, /* Usage (Joystik), */
	0xA1, 0x01, /* Collection (Application), */
	0xA1, 0x02, /* Collection (Logical), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x0A, /* Report Size (10), */
	0x14, 0x00, /* Logical Minimum (0), */
	0x25, 0xFF, 0x03, /* Logical Maximum (1023), */
	0x35, 0x00, /* Physical Minimum (0), */
	0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
	0x09, 0x30, /* Usage (X), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x08, /* Report Count (8), */
	0x75, 0x01, /* Report Size (1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x05, 0x09, /* Usage Page (Button), */
	0x19, 0x01, /* Usage Minimum (01h), */
	0x29, 0x08, /* Usage Maximum (08h), */
	0x81, 0x02, /* Input (Variable), */
	0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
	0x75, 0x0E, /* Report Size (14), */
	0x95, 0x01, /* Report Count (1), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x09, 0x00, /* Usage (00h), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x75, 0x08, /* Report Size (8), */
	0x09, 0x31, /* Usage (Y), */
	0x81, 0x02, /* Input (Variable), */
	0x09, 0x32, /* Usage (Z), */
	0x81, 0x02, /* Input (Variable), */
	0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
	0x09, 0x01, /* Usage (01h), */
	0x81, 0x02, /* Input (Variable), */
	0xC0, /* End Collection, */
	0xA1, 0x02, /* Collection (Logical), */
	0x09, 0x02, /* Usage (02h), */
	0x95, 0x07, /* Report Count (7), */
	0x91, 0x02, /* Output (Variable), */
	0xC0, /* End Collection, */
	0xC0 /* End Collection */
};

static const uint8_t pad_generic_hid_report_descriptor[] = {
	0x05, 0x01, /* Usage Page (Desktop), */
	0x09, 0x04, /* Usage (Joystik), */
	0xA1, 0x01, /* Collection (Application), */
	0xA1, 0x02, /* Collection (Logical), */
	0x95, 0x01, /* Report Count (1), */
	0x75, 0x0A, /* Report Size (10), */
	0x14, 0x00, /* Logical Minimum (0), */
	0x25, 0xFF, 0x03, /* Logical Maximum (1023), */
	0x35, 0x00, /* Physical Minimum (0), */
	0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
	0x09, 0x30, /* Usage (X), */
	0x81, 0x02, /* Input (Variable), */
	0x95, 0x0a, /* Report Count (10), */
	0x75, 0x01, /* Report Size (1), */
	0x25, 0x01, /* Logical Maximum (1), */
	0x45, 0x01, /* Physical Maximum (1), */
	0x05, 0x09, /* Usage Page (Button), */
	0x19, 0x01, /* Usage Minimum (01h), */
	0x29, 0x0a, /* Usage Maximum (0ah), */
	0x81, 0x02, /* Input (Variable), */
	0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
	0x75, 0x0C, /* Report Size (12), */
	0x95, 0x01, /* Report Count (1), */
	0x26, 0xFF, 0x00, /* Logical Maximum (255), */
	0x46, 0xFF, 0x00, /* Physical Maximum (255), */
	0x09, 0x00, /* Usage (00h), */
	0x81, 0x02, /* Input (Variable), */
	0x05, 0x01, /* Usage Page (Desktop), */
	0x75, 0x08, /* Report Size (8), */
	0x09, 0x31, /* Usage (Y), */
	0x81, 0x02, /* Input (Variable), */
	0x09, 0x32, /* Usage (Z), */
	0x81, 0x02, /* Input (Variable), */
	0x09, 0x35, /* Usage (RZ), */
	0x81, 0x02, /* Input (Variable), */
	0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
	0x09, 0x01, /* Usage (01h), */
	0x81, 0x02, /* Input (Variable), */
	0xC0, /* End Collection, */
	0xA1, 0x02, /* Collection (Logical), */
	0x09, 0x02, /* Usage (02h), */
	0x95, 0x07, /* Report Count (7), */
	0x91, 0x02, /* Output (Variable), */
	0xC0, /* End Collection, */
	0xC0 /* End Collection */
};

#define USB_PSIZE 8
#define DESC_CONFIG_WORD(a) (a&0xFF),((a>>8)&0xFF)

static const uint8_t df_config_descriptor[] = {
	0x09,   /* bLength */
	USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
	WBVAL(41),                        /* wTotalLength */
	0x01,                                 /* bNumInterfaces */
	0x01,                                 /* bConfigurationValue */
	0x00,                                 /* iConfiguration */
	0xc0,               /* bmAttributes */
	USB_CONFIG_POWER_MA(80),              /* bMaxPower */

	/* Interface Descriptor */
	0x09,//sizeof(USB_INTF_DSC),   // Size of this descriptor in bytes
	0x04,                   // INTERFACE descriptor type
	0,                      // Interface Number
	0,                      // Alternate Setting Number
	2,                      // Number of endpoints in this intf
	USB_CLASS_HID,               // Class code
	0,     // Subclass code
	0,     // Protocol code
	0,                      // Interface string index

	/* HID Class-Specific Descriptor */
	0x09,//sizeof(USB_HID_DSC)+3,    // Size of this descriptor in bytes RRoj hack
	0x21,                // HID descriptor type
	DESC_CONFIG_WORD(0x0100),                 // HID Spec Release Number in BCD format (1.11)
	0x21,                   // Country Code (0x00 for Not supported, 0x21 for US)
	1,                      // Number of class descriptors, see usbcfg.h
	0x22,//DSC_RPT,                // Report descriptor type
	DESC_CONFIG_WORD(sizeof(pad_driving_force_hid_report_descriptor)), // Size of the report descriptor

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x80, //HID_EP | _EP_IN,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(USB_PSIZE),        //size
	0x02,                       //Interval

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x0, //HID_EP | _EP_OUT,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(USB_PSIZE),        //size
	0x02,                        //Interval 0x2 - 2ms (G27) , 0x0A default?
};

static const uint8_t dfp_config_descriptor[] = {
	0x09,   /* bLength */
	USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
	WBVAL(41),                        /* wTotalLength */
	0x01,                                 /* bNumInterfaces */
	0x01,                                 /* bConfigurationValue */
	0x00,                                 /* iConfiguration */
	0xc0,               /* bmAttributes */
	USB_CONFIG_POWER_MA(80),              /* bMaxPower */

	/* Interface Descriptor */
	0x09,//sizeof(USB_INTF_DSC),   // Size of this descriptor in bytes
	0x04,                   // INTERFACE descriptor type
	0,                      // Interface Number
	0,                      // Alternate Setting Number
	2,                      // Number of endpoints in this intf
	USB_CLASS_HID,               // Class code
	0,     // Subclass code
	0,     // Protocol code
	0,                      // Interface string index

	/* HID Class-Specific Descriptor */
	0x09,//sizeof(USB_HID_DSC)+3,    // Size of this descriptor in bytes RRoj hack
	0x21,                // HID descriptor type
	DESC_CONFIG_WORD(0x0100),                 // HID Spec Release Number in BCD format (1.11)
	0x21,                   // Country Code (0x00 for Not supported, 0x21 for US)
	1,                      // Number of class descriptors, see usbcfg.h
	0x22,//DSC_RPT,                // Report descriptor type
	DESC_CONFIG_WORD(sizeof(pad_driving_force_pro_hid_report_descriptor)),

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x80, //HID_EP | _EP_IN,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(USB_PSIZE),        //size, might be 16 bytes
	0x02,                       //Interval

	/* Endpoint Descriptor */
	0x07,/*sizeof(USB_EP_DSC)*/
	0x05, //USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
	0x1|0x0, //HID_EP | _EP_OUT,        //EndpointAddress
	0x03, //_INTERRUPT,                 //Attributes
	DESC_CONFIG_WORD(USB_PSIZE),        //size
	0x02,                        //Interval 0x2 - 2ms (G27) , 0x0A default?
};

struct dfp_buttons_t
{
	uint16_t cross : 1;
	uint16_t square : 1;
	uint16_t circle : 1;
	uint16_t triangle : 1;
	uint16_t rpaddle_R1 : 1;
	uint16_t lpaddle_L1 : 1;
	uint16_t R2 : 1;
	uint16_t L2 : 1;
	uint16_t select : 1;
	uint16_t start : 1;
	uint16_t R3 : 1;
	uint16_t L3 : 1;
	uint16_t shifter_back : 1;
	uint16_t shifter_fwd : 1;
	uint16_t padding : 2;
};

struct dfgt_buttons_t
{
	uint16_t cross : 1;
	uint16_t square : 1;
	uint16_t circle : 1;
	uint16_t triangle : 1;
	uint16_t rpaddle_R1 : 1;
	uint16_t lpaddle_L1 : 1;
	uint16_t R2 : 1;
	uint16_t L2 : 1;
	uint16_t select : 1;
	uint16_t start : 1;
	uint16_t R3 : 1;
	uint16_t L3 : 1;
	uint16_t shifter_back : 1;
	uint16_t shifter_fwd : 1;
	uint16_t dial_center : 1;
	uint16_t dial_cw : 1;

	uint16_t dial_ccw : 1;
	uint16_t rocker_minus : 1;
	uint16_t horn : 1;
	uint16_t ps_button : 1;
	uint16_t padding: 12;
};

struct dfp_data_t
{
	uint32_t axis_x : 14;
	uint32_t buttons : 14;
	uint32_t hatswitch : 4;
	uint32_t pad0 : 8;
	uint32_t magic1 : 2; //constant?
	uint32_t axis_z : 6;

	uint32_t magic2 : 1; //constant?
	uint32_t axis_rz : 6;

	uint32_t magic3 : 1;

	uint32_t magic4 : 8; //constant
};

struct momo2_data_t
{
	uint32_t pad0 : 8;//report id probably
	uint32_t axis_x : 10;
	uint32_t buttons : 10;
	uint32_t padding0 : 4;//32

	uint8_t padding1;
	uint8_t axis_z;
	uint8_t axis_rz;
	uint8_t padding2;//32
};

// DF or any LG wheel in non-native mode
struct generic_data_t
{
	uint32_t axis_x : 10;
	uint32_t buttons : 12;
	uint32_t pad0 : 2;//vendor
	uint32_t axis_y : 8;//constant (0x7f on PC, 0xFF on console?)

	uint32_t hatswitch : 4;
	uint32_t pad1 : 4;//vendor
	uint32_t axis_z : 8;
	uint32_t axis_rz : 8;
	uint32_t pad2 : 8;
};

// GT Force?
struct gtforce_data_t
{
	uint32_t axis_x : 10;
	uint32_t buttons : 6;
	uint32_t pad0 : 8;
	uint32_t axis_y : 8;

	uint32_t axis_z : 8;
	uint32_t axis_rz : 8;
	
	uint32_t pad1 : 16;
};

struct random_data_t 
{
	uint32_t axis_x : 10;
	uint32_t buttons : 10;
	uint32_t pad1 : 12;

	uint32_t axis_y : 8;//constant
	uint32_t axis_z : 8;
	uint32_t axis_rz : 8;
	uint32_t pad2 : 8;
};

void ResetData(generic_data_t *d);
void ResetData(dfp_data_t *d);
void pad_copy_data(PS2WheelTypes type, uint8_t *buf, wheel_data_t &data);
//Convert DF Pro buttons to selected wheel type
uint32_t convert_wt_btn(PS2WheelTypes type, uint32_t inBtn);
#endif
