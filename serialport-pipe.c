#include <errno.h>
#include <signal.h>
#include <string.h> // memset
#include <stdio.h> // stdin, fread, fwrite
#include <sys/select.h> // pselect
#include <unistd.h> // STDIN_FILENO, STDOUT_FILENO
#include <popt.h>
#include <libserialport.h>
#include "shared.h"


static const int IOBufferSize = 8;


static struct poptOption options[] =
{
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

static void Pump( int sourceFD, int destinationFD )
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
            write(destinationFD, buffer, bytesRead);
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
                Pump(STDIN_FILENO, serialPortFD);
            }
            if(FD_ISSET(serialPortFD, &fdReadSet))
            {
                Pump(serialPortFD, STDOUT_FILENO);
            }
        }
    }
}

int main( int argc, const char** argv )
{
    ParseOptions(argc, argv);
    OpenSerialPort(&SerialPort, SP_MODE_READ|SP_MODE_WRITE);

    InstallSignalHandler();

    EventLoop();

    sp_close(SerialPort);
    sp_free_port(SerialPort);
    return 0;
}
