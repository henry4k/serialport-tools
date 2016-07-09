#include <stdio.h>
#include <libserialport.h>
#include "shared.h"


int main( int argc, const char** argv )
{
    struct sp_port** ports = NULL;
    HandleSerialPortError(sp_list_ports(&ports));
    for(int i = 0;; i++)
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
                printf("native");
            case SP_TRANSPORT_USB:
                printf("USB");
                int usbBus = 0;
                int usbAddress = 0;
                HandleSerialPortError(sp_get_port_usb_bus_address(port, &usbBus, &usbAddress));
                printf("bus=%d address=%d", usbBus, usbAddress);
            case SP_TRANSPORT_BLUETOOTH:
                printf("Bluetooth %s", sp_get_port_bluetooth_address(port));
            default:
                FatalError("Unknown transport type.");
        }

        printf("\n");
    }
    sp_free_port_list(ports);
    return 0;
}
