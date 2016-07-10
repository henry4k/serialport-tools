#include <errno.h>
#include <signal.h>
#include <string.h> // memset, strcmp
#include <stdio.h> // stdin, fread, fwrite
#include <sys/select.h> // pselect
#include <unistd.h> // STDIN_FILENO, STDOUT_FILENO
#include <popt.h>
#include <libserialport.h>
#include "shared.h"


static const int IOBufferSize = 1048576; // 1 MiB

enum ConversionMode
{
    CONVERSION_NONE,
    CONVERSION_CRLF_TO_LF,
    CONVERSION_LF_TO_CRLF
};

static char* ConvertCrLfStr = "on";
static int ConvertCrLf = 1;

struct poptOption PipeOptions[] =
{
    {"convert-crlf", '\0',
     POPT_ARG_STRING,
     &ConvertCrLfStr, 0,
     "Incoming CR-LF squences are replaced by LF and vice versa. Defaults to on.", "[on,off]"},

    POPT_TABLEEND
};

#define PIPE_OPTION_TABLE \
    {NULL, '\0', POPT_ARG_INCLUDE_TABLE, &PipeOptions, 0, "Pipe behaviour configuration:", NULL},

static struct poptOption options[] =
{
    PIPE_OPTION_TABLE
    SERIALPORT_OPTION_TABLE
    POPT_AUTOHELP
    POPT_TABLEEND
};

static struct sp_port* SerialPort = NULL;

static volatile int StopEventLoop = 0;


static void ParseOptions( int argc, const char** argv )
{
    poptContext optionContext = poptGetContext("serialport-pipe",
                                               argc, argv,
                                               options,
                                               0);
    int optionResult;
    while((optionResult = poptGetNextOpt(optionContext)) != -1)
    {
        if(optionResult < 0)
        {
            FatalError("%s: %s\n",
                       poptBadOption(optionContext, 0),
                       poptStrerror(optionResult));
        }
    }

    poptFreeContext(optionContext);
}

static void SignalHandler( int signal )
{
    StopEventLoop = 1;
}

static void InstallSignalHandler()
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SignalHandler;
    if(sigaction(SIGTERM, &action, 0))
        FatalError("sigaction");
    if(sigaction(SIGINT, &action, 0))
        FatalError("sigaction");
}

static inline void Write_ConvertLfToCrLf( int destinationFD,
                                          const char* source,
                                          int count )
{
    int chunkStart = 0;
    int i = 0;
    for(; i < count; i++)
    {
        if(source[i] == '\n')
        {
            write(destinationFD, &source[chunkStart], i-chunkStart);
            write(destinationFD, "\r\n", 2);
            chunkStart = i+1;
        }
    }
    write(destinationFD, &source[chunkStart], count-chunkStart);
}

static inline void Write_ConvertCrLfToLf( int destinationFD,
                                          const char* source,
                                          int count )
{
    int chunkStart = 0;
    int i = 1;
    for(; i < count; i++)
    {
        if(source[i-1] == '\r' &&
           source[i]   == '\n')
        {
            write(destinationFD, &source[chunkStart], (i-1)-chunkStart);
            write(destinationFD, "\n", 1);
            chunkStart = i+1;
        }
    }
    write(destinationFD, &source[chunkStart], count-chunkStart);
}

static void Pump( int sourceFD,
                  int destinationFD,
                  enum ConversionMode conversionMode )
{
    char buffer[IOBufferSize];
    int totalBytesRead = 0;
    for(;;)
    {
        const ssize_t bytesRead = read(sourceFD, buffer, IOBufferSize);
        if(bytesRead == 0)
        {
            break;
        }
        else if(bytesRead < 0)
        {
            if(errno == EAGAIN)
                break;
            else
                FatalError("read");
        }
        else
        {
            switch(conversionMode)
            {
                case CONVERSION_NONE:
                    write(destinationFD, buffer, bytesRead);
                    break;

                case CONVERSION_LF_TO_CRLF:
                    Write_ConvertLfToCrLf(destinationFD, buffer, bytesRead);
                    break;

                case CONVERSION_CRLF_TO_LF:
                    Write_ConvertCrLfToLf(destinationFD, buffer, bytesRead);
                    break;
            }
            totalBytesRead += bytesRead;

            if(bytesRead < IOBufferSize)
                break;
        }
    }
    if(totalBytesRead == 0)
    {
        StopEventLoop = 1;
    }
    fsync(destinationFD);
}

static void EventLoop()
{
    sigset_t oldMask;
    sigset_t newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGTERM);
    sigaddset(&newMask, SIGINT);

    if(sigprocmask(SIG_BLOCK, &newMask, &oldMask) < 0)
        FatalError("sigprocmask");

    int serialPortFD = -1;
    HandleSerialPortError(sp_get_port_handle(SerialPort, &serialPortFD));

    enum ConversionMode incomingConversion;
    enum ConversionMode outgoingConversion;
    if(ConvertCrLf)
    {
        incomingConversion = CONVERSION_CRLF_TO_LF;
        outgoingConversion = CONVERSION_LF_TO_CRLF;
    }
    else
    {
        incomingConversion = CONVERSION_NONE;
        outgoingConversion = CONVERSION_NONE;
    }

    while(!StopEventLoop)
    {
        fd_set fdReadSet;
        FD_ZERO(&fdReadSet);
        FD_SET(STDIN_FILENO, &fdReadSet);
        FD_SET(serialPortFD, &fdReadSet);
        const int maxFD = serialPortFD;

        const int ready = pselect(maxFD+1, &fdReadSet, NULL, NULL, NULL, &oldMask);
        if(ready < 0 && errno != EINTR)
        {
            FatalError("select");
        }
        else if(StopEventLoop)
        {
            puts("exit");
            break;
        }
        else if(ready == 0)
        {
            puts("not ready");
            continue;
        }
        else
        {
            if(FD_ISSET(STDIN_FILENO, &fdReadSet))
            {
                Pump(STDIN_FILENO, serialPortFD, outgoingConversion);
            }
            if(FD_ISSET(serialPortFD, &fdReadSet))
            {
                Pump(serialPortFD, STDOUT_FILENO, incomingConversion);
            }
        }
    }
}

int main( int argc, const char** argv )
{
    ParseOptions(argc, argv);
    ConvertCrLf = (strcmp(ConvertCrLfStr, "on") == 0);

    OpenSerialPort(&SerialPort, SP_MODE_READ|SP_MODE_WRITE);

    InstallSignalHandler();

    EventLoop();

    sp_close(SerialPort);
    sp_free_port(SerialPort);
    return 0;
}
