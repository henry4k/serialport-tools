#ifndef SHARED_H
#define SHARED_H

struct sp_port;
enum sp_return;
struct poptOption;


extern struct poptOption SerialPortOptions[];

#define SERIALPORT_OPTION_TABLE \
    {NULL, '\0', POPT_ARG_INCLUDE_TABLE, &SerialPortOptions, 0, "Serial port configuration:", NULL},

void FatalError( const char* format, ... );
void HandleSerialPortError( enum sp_return returnCode );
void OpenSerialPort( struct sp_port** port, enum sp_mode flags );

#endif
