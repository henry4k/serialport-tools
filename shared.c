#include <stdarg.h>
#include <signal.h> // raise
#include <stdlib.h> // exit
#include <stdio.h> // fprintf
#include <string.h> // strcmp
#include <popt.h>
#include <libserialport.h>



void FatalError( const char* format, ... )
{
    fprintf(stderr, "fatal error: ");

    va_list vl;
    va_start(vl, format);
    vfprintf(stderr, format, vl);
    va_end(vl);

    fprintf(stderr, "\n");
    raise(SIGTRAP);
}


static char* PortName    = NULL;
static int   BaudRate    = 9600;
static int   Bits        = 8;
static char* Parity      = "none";
static int   StopBits    = 1;
static char* FlowControl = "none";

struct poptOption SerialPortOptions[] =
{
    {"port-name", 'f',
     POPT_ARG_STRING,
     &PortName, 0,
     "System dependent port name.", NULL},

    {"baud-rate", 'r',
     POPT_ARG_INT,
     &BaudRate, 0,
     "Defaults to 9600.", NULL},

    {"bits", '\0',
     POPT_ARG_INT,
     &Bits, 0,
     "Defaults to 8.", NULL},

    {"parity", '\0',
     POPT_ARG_STRING,
     &Parity, 0,
     "Defaults to none.", "[none,odd,even,mark,space]"},

    {"stop-bits", '\0',
     POPT_ARG_INT,
     &StopBits, 0,
     "Defaults to 1.", NULL},

    {"flow-control", '\0',
     POPT_ARG_STRING,
     &FlowControl, 0,
     "Defaults to none.", "[none,xon/xoff,rtc/cts,dtr/dsr]"},

    POPT_TABLEEND
};

static enum sp_parity StringToParity( const char* string )
{
    if(strcmp(string, "none") == 0)
        return SP_PARITY_NONE;
    if(strcmp(string, "odd") == 0)
        return SP_PARITY_ODD;
    if(strcmp(string, "even") == 0)
        return SP_PARITY_EVEN;
    if(strcmp(string, "mark") == 0)
        return SP_PARITY_MARK;
    if(strcmp(string, "space") == 0)
        return SP_PARITY_SPACE;
    FatalError("Unknown parity '%s'", string);
    return SP_PARITY_INVALID;
}

static enum sp_flowcontrol StringToFlowControl( const char* string )
{
    if(strcmp(string, "none") == 0)
        return SP_FLOWCONTROL_NONE;
    if(strcmp(string, "xon/xoff") == 0)
        return SP_FLOWCONTROL_XONXOFF;
    if(strcmp(string, "rtc/cts") == 0)
        return SP_FLOWCONTROL_RTSCTS;
    if(strcmp(string, "dtr/dsr") == 0)
        return SP_FLOWCONTROL_DTRDSR;
    FatalError("Unknown flow control '%s'", string);
    return -1;
}

static const char* ReturnCodeToString( enum sp_return returnCode )
{
    switch(returnCode)
    {
        case SP_ERR_ARG:
            return "Invalid arguments were passed to the function.";
        case SP_ERR_FAIL:
            return "A system error occured while executing the operation.";
        case SP_ERR_MEM:
            return "A memory allocation failed while executing the operation.";
        case SP_ERR_SUPP:
            return "The requested operation is not supported by this system or device.";
        default:
            return "Unknown return code!";
    }
}

void HandleSerialPortError( enum sp_return returnCode )
{
    if(returnCode != SP_OK)
    {
        if(returnCode == SP_ERR_FAIL)
        {
            char* errorMessage = sp_last_error_message();
            FatalError("%s", errorMessage);
            sp_free_error_message(errorMessage);
        }
        else
        {
            FatalError("%s", ReturnCodeToString(returnCode));
        }
    }
}

void OpenSerialPort( struct sp_port** port, enum sp_mode flags )
{
    enum sp_return result;

    result = sp_get_port_by_name(PortName, port);
    switch(result)
    {
        case SP_OK: break;
        case SP_ERR_ARG: FatalError("Invalid serial port name '%s'", PortName);
        default: HandleSerialPortError(result);
    }

    HandleSerialPortError(sp_open(*port, flags));

    result = sp_set_baudrate(*port, BaudRate);
    switch(result)
    {
        case SP_OK: break;
        case SP_ERR_ARG: FatalError("Invalid baud rate %d", BaudRate);
        default: HandleSerialPortError(result);
    }

    result = sp_set_bits(*port, Bits);
    switch(result)
    {
        case SP_OK: break;
        case SP_ERR_ARG: FatalError("Invalid bits %d", Bits);
        default: HandleSerialPortError(result);
    }

    HandleSerialPortError(sp_set_parity(*port, StringToParity(Parity)));

    result = sp_set_stopbits(*port, StopBits);
    switch(result)
    {
        case SP_OK: break;
        case SP_ERR_ARG: FatalError("Invalid stop bits %d", StopBits);
        default: HandleSerialPortError(result);
    }

    HandleSerialPortError(sp_set_flowcontrol(*port, StringToFlowControl(FlowControl)));
}
