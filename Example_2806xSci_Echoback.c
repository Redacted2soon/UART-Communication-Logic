/* Baud rate = 9600
 * Data = 8
 * Stop bit = 1
 * Parity = none
 * Newline at CR+LF
 * Make sure terminating character is sent with data (marks end)
 */

// Included Files
#include "DSP28x_Project.h"     // Device Headerfile and Examples Include File
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Definitions
#define PWM_FREQUENCY_MIN 687
#define PWM_FREQUENCY_MAX 10000
#define SIN_FREQUENCY_MIN 0
#define SIN_FREQUENCY_MAX 300
#define MODULATION_DEPTH_MIN 0.0
#define MODULATION_DEPTH_MAX 1.0
#define CLKRATE 90.0*1000000.0
#define MAX_BUFFER_SIZE 100
#define MAX_MSG_SIZE 100

typedef struct
{
    float pwm_frequency;
    float sin_frequency;
    float modulation_depth;
    float offset;
    float angle_1;
    float angle_2;
    float angle_3;
    Uint32 epwmTimerTBPRD;
} EPwmParams;

// Struct to hold parameters
EPwmParams originalepwmParams = { .pwm_frequency = 2500, // frequency of pwm DO NOT GO BELOW 687Hz, counter wont work properly 65535 < 90*10^6 / (687*2)
        .sin_frequency = 60,       // sin frequency 0-150Hz
        .modulation_depth = 1.0,   // modulation depth between 0 and 1
        .offset = 0.0, // make sure offset is between +-(1-MODULATION_DEPTH)/2
        .angle_1 = 0,            // Phase shift angle in degree
        .angle_2 = 120,          // Phase shift angle in degree
        .angle_3 = 240,          // Phase shift angle in degree
        .epwmTimerTBPRD = 0 };

EPwmParams newEpwmParams;

// Function Prototypes
void scia_echoback_init(void);
void scia_fifo_init(void);
void scia_xmit(int a);
void scia_msg(const char *msg);
int populate_variable(const char *arr, float *var, float min, float max,
                      int *pindex);
int process_buffer(const char *buffer);
void report_invalid_input(char invalid_char);
int confirm_values(void);
void print_params(const EPwmParams *arr);
void float_to_string(char *msg, float value);
void clear_scia_rx_buffer(void);

// Main
void main(void)
{
    memcpy(&newEpwmParams, &originalepwmParams, sizeof(EPwmParams));

    InitSysCtrl();
    InitSciaGpio();
    scia_fifo_init();     // Initialize the SCI FIFO
    scia_echoback_init();  // Initalize SCI for echoback

    // Disable CPU interrupts
    DINT;
    InitPieCtrl();
    // Disable CPU interrupts and clear all CPU interrupt flags
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();
    scia_msg(
            "\r\n-------------------------------------------------------------------------------------------------");
    scia_msg(
            "\r\nPlease enter a string in the format PARAMATER VALUE, PARAMATER2 VALUE (for example: P 2500, S 60,M .13)\r\n\0");
    scia_msg("\r\nP = PWM frequency (in Hz,ACCEPTABLE INPUTS: 687 - 10000)");
    scia_msg("\r\nS = Sin wave frequency (in Hz, ACCEPTABLE INPUTS: 1 - 300)");
    scia_msg("\r\nM = Modulation depth (ACCEPTABLE INPUTS: 0.0 - 1.0)");
    scia_msg(
            "\r\nO = Offset (in volts, ACCEPTABLE INPUTS: +-((1-Modulation depth) / 2) )");
    scia_msg(
            "\r\nA1 = Angle 1 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)");
    scia_msg(
            "\r\nA2 = Angle 2 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)");
    scia_msg(
            "\r\nA3 = Angle 3 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)");

    static char buffer[MAX_BUFFER_SIZE];
    Uint16 bufferIndex = 0;
    Uint16 ReceivedChar;

    while (1)
    {
        while (SciaRegs.SCIFFRX.bit.RXFFST < 1)
        {
            // Wait for a character
        }

        ReceivedChar = SciaRegs.SCIRXBUF.all;

        // Check for terminating character
        if (ReceivedChar == '\0')
        {
            scia_msg("\r\n\r\nYou sent: ");
            buffer[bufferIndex] = '\0';
            scia_msg(buffer);

            // Process the buffer
            int confirm = process_buffer(buffer); //returns 0 if there's an error with input

            if (confirm) //checks to see if error was found
            {
                confirm = confirm_values(); //allows user to confirm values
            }

            // Confirm the values
            if (confirm)
            {
                scia_msg("\r\n\r\nValues confirmed and set.");
                memcpy(&originalepwmParams, &newEpwmParams, sizeof(EPwmParams));
                // Values are confirmed, proceed with using the parameters
            }
            else
            {
                scia_msg("\r\n\r\nValues reset to:");
                print_params(&originalepwmParams);
            }
            //can possibly call a function to print this
            scia_msg(
                    "\r\n\r\n-------------------------------------------------------------------------------------------------");
            scia_msg(
                    "\r\nPlease Enter a string in the format PARAMATER VALUE,PARAMATER2 VALUE (for example: P 2500, S 60,M .13)\r\n\0");
            scia_msg(
                    "\r\nP = PWM frequency (in Hz,ACCEPTABLE INPUTS: 687 - 10000)");
            scia_msg(
                    "\r\nS = Sin wave frequency (in Hz, ACCEPTABLE INPUTS: 1 - 300)");
            scia_msg("\r\nM = Modulation depth (ACCEPTABLE INPUTS: 0.0 - 1.0)");
            scia_msg(
                    "\r\nO = Offset (volts, ACCEPTABLE INPUTS: +-((1-Modulation depth) / 2) )");
            scia_msg(
                    "\r\nA1 = Angle 1 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)");
            scia_msg(
                    "\r\nA2 = Angle 2 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)");
            scia_msg(
                    "\r\nA3 = Angle 3 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)");

            bufferIndex = 0;
            memset(buffer, 0, MAX_BUFFER_SIZE);
            clear_scia_rx_buffer();
        }
        else
        {
            // Add character to buffer array if there's space
            if (bufferIndex < MAX_BUFFER_SIZE - 1)
            {
                buffer[bufferIndex++] = ReceivedChar;
            }
            else
            {
                scia_msg(
                        "\r\nError: Input buffer overflow. Too many characters added.");
                bufferIndex = 0; // Reset buffer index to avoid overflow
                memset(buffer, 0, MAX_BUFFER_SIZE);
            }
        }
    }
}

int process_buffer(const char *buffer)
{

    int i = 0, error = 0;
    while (buffer[i] != '\0' && error == 0) // Loops until null character is found
    {
        // Skips white space and skips to parameter
        if (buffer[i] == ',' || buffer[i] == '.')
            i++;
        while (buffer[i] == ' ')
            i++;

        switch (buffer[i])
        {
        case 'P':
        case 'p':
            error = populate_variable(&(buffer[i]),
                                      &newEpwmParams.pwm_frequency,
                                      PWM_FREQUENCY_MIN,
                                      PWM_FREQUENCY_MAX, &i);
            break;
        case 'S':
        case 's':
            error = populate_variable(&(buffer[i]),
                                      &newEpwmParams.sin_frequency,
                                      SIN_FREQUENCY_MIN,
                                      SIN_FREQUENCY_MAX, &i);
            break;
        case 'M':
        case 'm':
            error = populate_variable(&(buffer[i]),
                                      &newEpwmParams.modulation_depth,
                                      MODULATION_DEPTH_MIN,
                                      MODULATION_DEPTH_MAX, &i);
            break;
        case 'O':
        case 'o':
            error = populate_variable(&(buffer[i]), &newEpwmParams.offset,
                                      -1,
                                      1,
                                      &i);
            break;
        case 'A':
        case 'a':
            i++;
            if (buffer[i] == '1')
            {
                error = populate_variable(&buffer[i], &newEpwmParams.angle_1,
                                          -360, 360, &i);
            }
            else if (buffer[i] == '2')
            {
                error = populate_variable(&buffer[i], &newEpwmParams.angle_2,
                                          -360, 360, &i);
            }
            else if (buffer[i] == '3')
            {
                error = populate_variable(&buffer[i], &newEpwmParams.angle_3,
                                          -360, 360, &i);
            }
            else
            {
                report_invalid_input(buffer[i]);
                error = 1;
            }
            break;

        default:
            report_invalid_input(buffer[i]);
            error = 1;
        }
    }
    if(newEpwmParams.offset > (1 - newEpwmParams.modulation_depth)/2 ||newEpwmParams.offset < -(1 - newEpwmParams.modulation_depth)/2)
        error = 1;
    //checks for errors, returns 0 if error is 1
    return error ? 0 : 1;
}

// This function takes in where we left off in the array, and returns an int value
int populate_variable(const char *arr, float *var, float min, float max,
                      int *pindex)
{
    char temp[10] = { '\0' };  // Reduced the size of the temp array
    int i = 1, j = 0;

    // Skips white space between letter and number
    while (arr[i] == ' ')
        i++;

    // Check to make sure array is a number or decimal
    while (arr[i] != '\0' && (arr[i] == '.' || (arr[i] >= '0' && arr[i] <= '9')))
    {
        // Return error if number was too big
        if (j > 8)
        {
            scia_msg("\r\nNumber input has too many digits");
            report_invalid_input(*temp);
            return 1;
        }
        temp[j++] = arr[i++];   // Populates temp array to hold value
    }

    *var = atof(temp); // Converts characters to float, assigns to *var
    // Checks to make sure value is within range
    if (*var < min || *var > max)
    {
        scia_msg("\r\nFirst value out of bound: ");
        scia_msg(temp);
        *var = (min + max) / 2; // Set to a default value within bounds
        return 1;
    }
    // Updates index
    *pindex += i;
    return 0;
}

int confirm_values(void)
{
    scia_msg("\r\n\r\nPlease confirm the values (Y/N):  ");
    print_params(&newEpwmParams);

    char answer = '0';
    //loop to make sure only input is Y or N
    while (answer != 'Y' && answer != 'N' && answer != 'y' && answer != 'n')
    {
        while (SciaRegs.SCIFFRX.bit.RXFFST < 1)
        {
            // Wait for a character
        }
        answer = SciaRegs.SCIRXBUF.all;
    }

    if (answer == 'Y' || answer == 'y')
    {
        return 1;
    }
    return 0;
}

void print_params(const EPwmParams *arr)
{
    scia_msg("\r\n\r\nPWM frequency = ");
    char msg[MAX_MSG_SIZE];
    float_to_string(msg, arr->pwm_frequency);
    scia_msg(msg);

    scia_msg("\r\nSin wave frequency = ");
    float_to_string(msg, arr->sin_frequency);
    scia_msg(msg);

    scia_msg("\r\nModulation depth = ");
    float_to_string(msg, arr->modulation_depth);
    scia_msg(msg);

    scia_msg("\r\nOffset = ");
    float_to_string(msg, arr->offset);
    scia_msg(msg);

    scia_msg("\r\nAngle 1 = ");
    float_to_string(msg, arr->angle_1);
    scia_msg(msg);

    scia_msg("\r\nAngle 2 = ");
    float_to_string(msg, arr->angle_2);
    scia_msg(msg);

    scia_msg("\r\nAngle 3 = ");
    float_to_string(msg, arr->angle_3);
    scia_msg(msg);
}

void float_to_string(char *msg, float value)
{
    // Assuming msg is large enough

    int integerPart = (int) value;
    float fractionalPart = value - (float) integerPart;
    if (fractionalPart < 0)
        fractionalPart = -fractionalPart;

    int fractionalAsInt = (int) ((fractionalPart + .0005) * 1000); // 3 decimal places

    if (fractionalAsInt == 0)
    {
        sprintf(msg, "%d", integerPart);
    }
    else
    {
        sprintf(msg, "%d.%03d", integerPart, fractionalAsInt); // Ensure 3 decimal places

    }
}

void report_invalid_input(char invalid_char)
{
    char msg[50];
    sprintf(msg, "\r\n\r\nInvalid character: %c", invalid_char);
    scia_msg(msg);
}

void clear_scia_rx_buffer()
{
    while (SciaRegs.SCIFFRX.bit.RXFFST != 0)
    {
        // Read the character to clear the buffer
        SciaRegs.SCIRXBUF.all;
    }
}

// scia_xmit - Transmit a character from the SCI
void scia_xmit(int a)
{
    while (SciaRegs.SCIFFTX.bit.TXFFST != 0)
    {
        // Wait for the transmit buffer to be empty
    }
    SciaRegs.SCITXBUF = a; // Placed below to ensure that the transmit buffer is empty before attempting to send data
}

// Transmit a message to the SCI
void scia_msg(const char *msg)
{

    while (*msg) // Keeps transmitting data until end of line (enter has been pressed, denoted by '\0')
    {
        scia_xmit(*msg++);
    }
}

void scia_echoback_init()
{
    SciaRegs.SCICCR.all = 0x0007; // 1 stop bit,  No loopback, No parity, 8 char bits, async mode, idle-line protocol

    // Enable TX, RX, internal SCICLK, Disable RX ERR, SLEEP, TXWAKE
    SciaRegs.SCICTL1.all = 0x0003;
    SciaRegs.SCICTL2.bit.TXINTENA = 0;
    SciaRegs.SCICTL2.bit.RXBKINTENA = 0;

    // 9600 baud @LSPCLK = 22.5MHz (90 MHz SYSCLK)
    SciaRegs.SCIHBAUD = 0x0001;
    SciaRegs.SCILBAUD = 0x0024;

    SciaRegs.SCICCR.bit.LOOPBKENA = 0;  // Enable loop back
    SciaRegs.SCICTL1.all = 0x0023;  // Relinquish SCI from Reset
}

// scia_fifo_init - Initalize the SCI FIFO
void scia_fifo_init()
{
    SciaRegs.SCIFFTX.all = 0xE040;
    SciaRegs.SCIFFRX.all = 0x2044;
    SciaRegs.SCIFFCT.all = 0x0;
}
// End of File
