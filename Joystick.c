/*
Nintendo Switch Fightstick - Proof-of-Concept

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

#include <LUFA/Drivers/Peripheral/Serial.h>
#include "Commands.h"

// Main entry point.
int main(void) {
	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	// Once that's done, we'll enter an infinite loop.
	for (;;)
	{
		// We need to run our task to process and deliver data for our IN and OUT endpoints.
		HID_Task();
		// We also need to run the main USB management task.
		USB_USBTask();
	}
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
	// We need to disable watchdog if enabled by bootloader/fuses.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// We need to disable clock division before initializing the USB hardware.
	clock_prescale_set(clock_div_1);
	// We can then initialize our hardware and peripherals, including the USB stack.

	#ifdef ALERT_WHEN_DONE
	// Both PORTD and PORTB will be used for the optional LED flashing and buzzer.
	#warning LED and Buzzer functionality enabled. All pins on both PORTB and \
PORTD will toggle when printing is done.
	DDRD  = 0xFF; //Teensy uses PORTD
	PORTD =  0x0;
		 //We'll just flash all pins on both ports since the UNO R3
	DDRB  = 0xFF; //uses PORTB. Micro can use either or, but both give us 2 LEDs
	PORTB =  0x0; //The ATmega328P on the UNO will be resetting, so unplug it?
	#endif
	// The USB stack should be initialized last.
	USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
	// We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
	// We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	// We setup the HID report endpoints.
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

	// We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
	// We can handle two control requests: a GetReport and a SetReport.

	// Not used here, it looks like we don't receive control request from the Switch.
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
	// If the device isn't connected and properly configured, we can't do anything here.
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;

	// We'll start with the OUT endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
	// We'll check to see if we received something on the OUT endpoint.
	if (Endpoint_IsOUTReceived())
	{
		// If we did, and the packet has data, we'll react to it.
		if (Endpoint_IsReadWriteAllowed())
		{
			// We'll create a place to store our data received from the host.
			USB_JoystickReport_Output_t JoystickOutputData;
			// We'll then take in that data, setting it up in our storage.
			while(Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL) != ENDPOINT_RWSTREAM_NoError);
			// At this point, we can react to this data.

			// However, since we're not doing anything with this data, we abandon it.
		}
		// Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
		Endpoint_ClearOUT();
	}

	// We'll then move on to the IN endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
	// We first check to see if the host is ready to accept data.
	if (Endpoint_IsINReady())
	{
		// We'll create an empty report.
		USB_JoystickReport_Input_t JoystickInputData;
		// We'll then populate this report with what we want to send to the host.
		GetNextReport(&JoystickInputData);
		// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
		while(Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL) != ENDPOINT_RWSTREAM_NoError);
		// We then send an IN packet on this endpoint.
		Endpoint_ClearIN();
	}
}

// Serial communication
#define MAX_BUFFER 32
char pc_report_str[MAX_BUFFER];
uint8_t idx = 0;
USB_JoystickReport_Input_t pc_report;
const uint8_t pc_rep_duration_max = 5;
uint8_t pc_rep_duration = 0;

void ParseLine(char* line)
{
	char btns[16];

	// format [button LeftStickX LeftStickY RightStickX RightStickY HAT] 
	// button: A | B | X | Y | L | R | ZL | ZR | MINUS | PLUS | LCLICK | RCLICK | HOME | CAP
	// LeftStick : 0 to 255
	// RightStick: 0 to 255
	// HAT : 0(TOP) to 7(TOP_LEFT) in clockwise | 8(CENTER)
	sscanf(line, "%s %hhu %hhu %hhu %hhu %hhu", btns,
		&pc_report.LX, &pc_report.LY, &pc_report.RX, &pc_report.RY, &pc_report.HAT);

	if (btns[0] == '1')		pc_report.Button |= SWITCH_A;
	if (btns[1] == '1')		pc_report.Button |= SWITCH_B;
	if (btns[2] == '1')		pc_report.Button |= SWITCH_X;
	if (btns[3] == '1')		pc_report.Button |= SWITCH_Y;
	if (btns[4] == '1')		pc_report.Button |= SWITCH_L;
	if (btns[5] == '1')		pc_report.Button |= SWITCH_R;
	if (btns[6] == '1')		pc_report.Button |= SWITCH_ZL;
	if (btns[7] == '1')		pc_report.Button |= SWITCH_ZR;
	if (btns[8] == '1')		pc_report.Button |= SWITCH_MINUS;
	if (btns[9] == '1')		pc_report.Button |= SWITCH_PLUS;
	if (btns[10] == '1')	pc_report.Button |= SWITCH_LCLICK;
	if (btns[11] == '1')	pc_report.Button |= SWITCH_RCLICK;
	if (btns[12] == '1')	pc_report.Button |= SWITCH_HOME;
	if (btns[13] == '1')	pc_report.Button |= SWITCH_CAPTURE;	
}

ISR(USART1_RX_vect) 
{
	// one character comes at a time
	char c = fgetc(stdin);
	if (Serial_IsSendReady()) 
		printf("%c", c);

	if (c == '\r') 
	{
		ParseLine(pc_report_str);
		idx = 0;
		memset(pc_report_str, 0, sizeof(pc_report_str));
	} 
	else if (c != '\n' && idx < MAX_BUFFER)
		pc_report_str[idx++] = c;
}

typedef enum {
	INIT,
	SYNC,
	PROCESS,
	CLEANUP,
	DONE
} State_t;
State_t state = INIT;

typedef enum {
	NONE,		// do nothing

	// On MCU
	MASH_A,		// mash button A
	INF_WATT, 	// infinity watt
	INF_ID_WATT,// infinity id lottery & watt (not recommended now)

	// From PC
	PC_CALL,
} Proc_State_t;
Proc_State_t proc_state = MASH_A;

USB_JoystickReport_Input_t last_report;

int step_index;
int duration_count;

Command cur_command;
int duration_buf;
int step_size_buf;

int echo_ratio = 1; // for compatible mode

bool is_use_sync = true;

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t* const ReportData) {

	// Prepare an empty report
	memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
	ReportData->LX = STICK_CENTER;
	ReportData->LY = STICK_CENTER;
	ReportData->RX = STICK_CENTER;
	ReportData->RY = STICK_CENTER;
	ReportData->HAT = HAT_CENTER;

	// States and moves management
	switch (state)
	{
		case INIT:
			// initialize for avoiding first itereation mismatch
			step_index = 0;
			step_size_buf = 127;

			state = is_use_sync ? SYNC : PROCESS;
			break;

		case SYNC:
			if (!GetNextReportFromCommands(sync, sync_size, ReportData))
				state = PROCESS;

			break;

		case PROCESS:
			echo_ratio = 1;

			// Get a next command from flash memory
			switch (proc_state)
			{
				case NONE:
					break;

				case MASH_A:
					GetNextReportFromCommands(mash_a_commands, mash_a_size, ReportData);
					break;

				case INF_WATT:
					echo_ratio = 3;
					GetNextReportFromCommands(inf_watt_commands, inf_watt_size, ReportData);
					break;

				case INF_ID_WATT:
					echo_ratio = 3;
					GetNextReportFromCommands(inf_id_watt_commands, inf_id_watt_size, ReportData);
					break;
				
				case PC_CALL:
					if (pc_rep_duration++ < pc_rep_duration_max)
					{
						// copy a report that was sent from PC
						memcpy(ReportData, &pc_report, sizeof(USB_JoystickReport_Input_t));
					}
					else
						pc_rep_duration = 0;
					
					break;

				default:
					break;
			}

			break;

		case CLEANUP:
			state = DONE;
			break;

		case DONE:
			#ifdef ALERT_WHEN_DONE
			portsval = ~portsval;
			PORTD = portsval; //flash LED(s) and sound buzzer if attached
			PORTB = portsval;
			_delay_ms(250);
			#endif
			return;
	}
}

// return: commands have not reached to the end?
bool GetNextReportFromCommands(
	const Command* const commands, 
	const int step_size, 
	USB_JoystickReport_Input_t* const ReportData)
{
	// Repeat duration times the last report
	// As of now, duration_buf is mul by the ratio for concerning compatibility with code using echo variables
	if (duration_count++ < (int)(duration_buf * echo_ratio))
	{
		memcpy(ReportData, &last_report, sizeof(USB_JoystickReport_Input_t));
		return true;
	}
	else
	{
		duration_count = 0;		
	}

	// Check step size range
	if (step_index > step_size_buf - 1)
	{
		step_index = 0; // go back to first step

		ReportData->LX = STICK_CENTER;
		ReportData->LY = STICK_CENTER;
		ReportData->RX = STICK_CENTER;
		ReportData->RY = STICK_CENTER;
		ReportData->HAT = HAT_CENTER;

		memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t)); // create echo report
		return false;
	}

	memcpy_P(&cur_command, &commands[step_index++], sizeof(Command));
	step_size_buf = step_size;

	duration_buf = cur_command.duration;
	ApplyButtonCommand(cur_command.button, ReportData);

	memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t)); // create echo report
	return true;
}

void ApplyButtonCommand(const Buttons_t button, USB_JoystickReport_Input_t* const ReportData)
{
	switch (button)
	{
		case UP:
			ReportData->LY = STICK_MIN;				
			break;

		case LEFT:
			ReportData->LX = STICK_MIN;				
			break;

		case DOWN:
			ReportData->LY = STICK_MAX;				
			break;

		case RIGHT:
			ReportData->LX = STICK_MAX;				
			break;

		case A:
			ReportData->Button |= SWITCH_A;
			break;

		case B:
			ReportData->Button |= SWITCH_B;
			break;

		case X:
			ReportData->Button |= SWITCH_X;
			break;

		case Y:
			ReportData->Button |= SWITCH_Y;
			break;

		case L:
			ReportData->Button |= SWITCH_L;
			break;

		case R:
			ReportData->Button |= SWITCH_R;
			break;

		case TRIGGERS:
			ReportData->Button |= SWITCH_L | SWITCH_R;
			break;

		case UPLEFT:
			ReportData->LX = STICK_MIN;
			ReportData->LY = STICK_MIN;
			break;

		case UPRIGHT:
			ReportData->LX = STICK_MAX;
			ReportData->LY = STICK_MIN;
			break;	

		case DOWNRIGHT:
			ReportData->LX = STICK_MAX;
			ReportData->LY = STICK_MAX;
			break;

		case DOWNLEFT:
			ReportData->LX = STICK_MIN;
			ReportData->LY = STICK_MAX;
			break;

		case PLUS:
			ReportData->Button |= SWITCH_PLUS;
			break;

		case MINUS:
			ReportData->Button |= SWITCH_MINUS;
			break;

		case HOME:
			ReportData->Button |= SWITCH_HOME;
			break;

		default:
			ReportData->LX = STICK_CENTER;
			ReportData->LY = STICK_CENTER;
			ReportData->RX = STICK_CENTER;
			ReportData->RY = STICK_CENTER;
			ReportData->HAT = HAT_CENTER;
			break;
	}
}
