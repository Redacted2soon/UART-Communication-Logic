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
#define MIN_ANGLE -360
#define MAX_ANGLE 360
#define MAX_BUFFER_SIZE 100
#define MAX_MSG_SIZE 100

// Struct to hold PWM parameters
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

// Initialize default PWM parameters
EPwmParams originalePwmParams = { .pwm_frequency = 2500, // frequency of pwm DO NOT GO BELOW 687Hz, counter wont work properly 65535 < 90*10^6 / (687*2)
        .sin_frequency = 60,       // sin frequency 0-150Hz
        .modulation_depth = 1.0,   // modulation depth between 0 and 1
        .offset = 0.0, // make sure offset is between +-(1-MODULATION_DEPTH)/2
        .angle_1 = 0,            // Phase shift angle in degree
        .angle_2 = 120,          // Phase shift angle in degree
        .angle_3 = 240,          // Phase shift angle in degree
        .epwmTimerTBPRD = 0 };

// Struct for new PWM parameters
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
void float_to_string(float value);
void clear_scia_rx_buffer(void);
void print_welcome_screen(void);

// Main function
void main(void)
{
    // Copy default PWM parameters to new PWM parameters
    memcpy(&newEpwmParams, &originalePwmParams, sizeof(EPwmParams));

    InitSysCtrl();          // Initialize System Control
    InitSciaGpio();         // Initialize SCI GPIO
    scia_fifo_init();       // Initialize the SCI FIFO
    scia_echoback_init();   // Initalize SCI for echoback

    // Disable CPU interrupts
    DINT;
    InitPieCtrl();         // Initialize PIE control registers
    // Disable CPU interrupts and clear all CPU interrupt flags
    IER = 0x0000;          // Disable CPU interrupts
    IFR = 0x0000;          // Clear all CPU interrupt flags
    InitPieVectTable();    // Initialize the PIE vector table

    print_welcome_screen(); // Print the welcome message

    // Buffer for incoming data and buffer index
    static char buffer[MAX_BUFFER_SIZE];
    Uint16 bufferIndex = 0;
    Uint16 ReceivedChar;

    // Main loop
    while (1)
    {
        while (SciaRegs.SCIFFRX.bit.RXFFST < 1)
        {
            // Wait for a character to be received
        }

        ReceivedChar = SciaRegs.SCIRXBUF.all;  // Read received character

        // Check for terminating character
        if (ReceivedChar == '\0')
        {
            //echo back user input
            scia_msg("\r\n\r\nYou sent: ");
            buffer[bufferIndex] = '\0';
            scia_msg(buffer);

            // Process the buffer
            int confirm = process_buffer(buffer); //returns 0 if there's an error with input

            if (confirm) // Checks if there was an error in processing
            {
                confirm = confirm_values(); // Allows user to confirm values
            }

            // Confirm the values
            if (confirm)
            {
                scia_msg("\r\n\r\nValues confirmed and set.");
                memcpy(&originalePwmParams, &newEpwmParams, sizeof(EPwmParams)); // Copy new values to original
            }
            else
            {
                scia_msg("\r\n\r\nValues reset to:");
                print_params(&originalePwmParams);  // Print the original values
            }

            // Reset the buffer for the next input
            bufferIndex = 0;
            memset(buffer, 0, MAX_BUFFER_SIZE);
            clear_scia_rx_buffer();
            print_welcome_screen();  // Print the welcome message again
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
                // Handle buffer overflow error
                scia_msg(
                        "\r\n\r\nError: Input buffer overflow. Too many characters added. Buffer reset");
                bufferIndex = 0; // Reset buffer index to avoid overflow
                memset(buffer, 0, MAX_BUFFER_SIZE);
                clear_scia_rx_buffer();
                print_welcome_screen();  // Print the welcome message again
            }
        }
    }
}

// Function to process the buffer and extract parameters
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

        // Identify the parameter and populate its value
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
            error = populate_variable(&(buffer[i]), &newEpwmParams.offset, -1,
                                      1, &i);
            break;
        case 'A':
        case 'a':
            i++;
            if (buffer[i] == '1')
            {
                error = populate_variable(&buffer[i], &newEpwmParams.angle_1,
                MIN_ANGLE,
                                          MAX_ANGLE, &i);
            }
            else if (buffer[i] == '2')
            {
                error = populate_variable(&buffer[i], &newEpwmParams.angle_2,
                MIN_ANGLE,
                                          MAX_ANGLE, &i);
            }
            else if (buffer[i] == '3')
            {
                error = populate_variable(&buffer[i], &newEpwmParams.angle_3,
                MIN_ANGLE,
                                          MAX_ANGLE, &i);
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

    // Check offset range (since offset depends on modulation depth
    if (newEpwmParams.offset > (1 - newEpwmParams.modulation_depth) / 2
            || newEpwmParams.offset < -(1 - newEpwmParams.modulation_depth) / 2)
    {
        newEpwmParams.offset = originalePwmParams.offset;
        scia_msg("\r\n\r\nOffset out of range");
        error = 1;
    }

    return error ? 0 : 1;    //Returns 0 if there was an error
}

// Function to populate a variable from the buffer
int populate_variable(const char *arr, float *var, float min, float max,
                      int *pindex)
{
    char temp[8] = { '\0' };  // Temporary array to hold value
    int i = 1, j = 0, periodCount = 0;

    // Check for invalid input (double letter)
    if (arr[i] > '@' && arr[i] < 'z')
    {
        scia_msg("\r\nInvalid input : ");
        scia_msg(&arr[i - 1]);
        return 1;
    }

    // Skips white space between letter and number
    while (arr[i] == ' ')
        i++;

    // Extract the number
    while (arr[i] != '\0'
            && (arr[i] == '.' || arr[i] == '-'
                    || (arr[i] >= '0' && arr[i] <= '9')))
    {
        // Check if number is too big
        if (j > 8)
        {
            scia_msg("\r\nNumber input has too many digits");
            report_invalid_input(*temp);
            return 1;
        }

        if (arr[i] == '.')
        {
            periodCount++;  // Increment period count
        }

        temp[j++] = arr[i++];   // Copy the character to the temporary array
    }

    // Check if there are multiple periods
    if (periodCount > 1)
    {
        scia_msg("\r\nInput has too many decimal points");
        report_invalid_input(*temp);
        return 1;
    }

    // Convert the temporary array to a float and check its range
    *var = atof(temp);
    if (*var < min || *var > max)
    {
        scia_msg("\r\nFirst value out of bound: ");
        scia_msg(temp);
        return 1;
    }

    // Update the index and return success
    *pindex += i;
    return 0;
}

// Function to confirm the values with the user
int confirm_values(void)
{
    Uint16 confirm = 0;
    Uint16 RecivedChar = 0;

    // Ask the user to confirm the values
    scia_msg("\r\n\r\nPLEASE CONFIRM THE VALUES (Y/N):  ");
    print_params(&newEpwmParams);

    // Wait for a valid response
    while (confirm == 0)
    {
        while (SciaRegs.SCIFFRX.bit.RXFFST < 1)
        {
            // Wait for a character
        }
        RecivedChar = SciaRegs.SCIRXBUF.all;    // Read received character

        // Check the response
        if (RecivedChar == 'Y' || RecivedChar == 'y')
        {
            confirm = 1;
        }
        else if (RecivedChar == 'N' || RecivedChar == 'n')
        {
            confirm = 2;
        }
        else
        {
            scia_msg("\r\nInvalid input. Please enter Y or N.\r\n");
            SciaRegs.SCIRXBUF.all;
        }

    }
    return (confirm == 1) ? 1 : 0;
}

void print_params(const EPwmParams *arr)
{
    scia_msg("\r\n\r\nPWM frequency = ");
    float_to_string(arr->pwm_frequency);

    scia_msg("\r\nSin wave frequency = ");
    float_to_string(arr->sin_frequency);

    scia_msg("\r\nModulation depth = ");
    float_to_string(arr->modulation_depth);

    scia_msg("\r\nOffset = ");
    float_to_string(arr->offset);

    scia_msg("\r\nAngle 1 = ");
    float_to_string(arr->angle_1);

    scia_msg("\r\nAngle 2 = ");
    float_to_string(arr->angle_2);

    scia_msg("\r\nAngle 3 = ");
    float_to_string(arr->angle_3);
}

// Function to convert a float to a string and send it via SCI
void float_to_string(float value)
{
    // Assuming msg is large enough
    char msg[20];

    //split float value into whole and fractional
    int integerPart = (int) value;
    float fractionalPart = value - (float) integerPart;
    //absolute value of a negative decimal
    if (fractionalPart < 0)
        fractionalPart = -fractionalPart;

    //round to 3 decimal places
    int fractionalAsInt = (int) ((fractionalPart + .0001) * 1000); // 3 decimal places

    //print checks if whole or decimal was input
    if (fractionalAsInt == 0)
    {
        sprintf(msg, "%d", integerPart);
    }
    else
    {
        sprintf(msg, "%d.%03d", integerPart, fractionalAsInt); // Ensure 3 decimal places
    }

    //print value
    scia_msg(msg);
}

// Function to report invalid input
void report_invalid_input(char invalid_char)
{
    char msg[50];
    sprintf(msg, "\r\n\r\nInvalid character: %c", invalid_char);
    scia_msg(msg);
}

// Function to clear the SCI A RX buffer
void clear_scia_rx_buffer()
{
    while (SciaRegs.SCIFFRX.bit.RXFFST != 0)
    {
        char temp = SciaRegs.SCIRXBUF.all;
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

// Function to print the welcome screen
void print_welcome_screen(void)
{
    scia_msg(
            "\r\n\r\n-------------------------------------------------------------------------------------------------"

            "\r\nPlease Enter a string in the format PARAMATER1 VALUE1,PARAMATER2 VALUE2 (for example: P 2500, S 60,M .13)\r\n"

            "\r\nP = PWM frequency (in Hz,ACCEPTABLE INPUTS: 687 - 10000)"

            "\r\nS = Sin wave frequency (in Hz, ACCEPTABLE INPUTS: 1 - 300)"

            "\r\nM = Modulation depth (ACCEPTABLE INPUTS: 0.0 - 1.0, up to three decimal points)"

            "\r\nO = Offset (volts, ACCEPTABLE INPUTS: +-((1-Modulation depth) / 2), up to three decimal points )"

            "\r\nA1 = Angle 1 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)"

            "\r\nA2 = Angle 2 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)"

            "\r\nA3 = Angle 3 offset (in degrees, ACCEPTABLE INPUTS: 0 - 360)");

}

// Functions for SCI initialization and communication
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
