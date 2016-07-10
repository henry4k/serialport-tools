#include <stdio.h>
#include <libserialport.h>
#include "shared.h"


int main( int argc, const char** argv )
{
    struct sp_port** ports = NULL;
    HandleSerialPortError(sp_list_ports(&ports));
    int i = 0;
    for(;; i++)
    {
        const struct sp_port* port = ports[i];
        if(port == NULL)
            break;

        printf("%s - %s ",
               sp_get_port_name(port),
               sp_get_port_description(port));

        const enum sp_transport transport = sp_get_port_transport(port);
        switch(transport)
        {
            case SP_TRANSPORT_NATIVE:
                printf("(native)");
                break;

            case SP_TRANSPORT_USB:
            {
                int usbBus = 0;
                int usbAddress = 0;
                HandleSerialPortError(sp_get_port_usb_bus_address(port, &usbBus, &usbAddress));
                printf("(USB: bus %d, address %d)", usbBus, usbAddress);
                break;
            }

            case SP_TRANSPORT_BLUETOOTH:
                printf("(Bluetooth device %s)", sp_get_port_bluetooth_address(port));
                break;

            default:
                FatalError("Unknown transport type.");
        }

        printf("\n");
    }
    sp_free_port_list(ports);
    return 0;
}
