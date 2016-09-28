/*
 *  USBRH on Linux/libusb Mar.14 2007 http://d.hatena.ne.jp/Briareos/
 *
 *    Copyright (C) 2007 Briareos <briareos@dd.iij4u.or.jp>
 *
 *     USBRH:Strawberry Linux co.ltd. http://strawberry-linux.com/
 *     libusb:libusb project  http://libusb.sourceforge.net/
 *
 *    Special Thanks: USBRH on *BSD http://www.nk-home.net/~aoyama/usbrh/
 *
 *
 *  This program is FREESOFTWARE, by the GPLv2.
 *
 *  History
 *   date       version    comments
 *   2007-03-28   0.02    Initial Version
 *   2007-04-10   0.03    add error-check when usb_control_msg/usb_bulk_read
 *                        add sleep function before usb_bulk_read
 *   2007-04-20   0.04    add device-listing mode   
 *   2008-03-24   0.05    add include for sting.h
 *   2016-09-10   0.06    add LED and HEATER control
 * 
 */
#include <stdio.h>
#include <usb.h>
#include <unistd.h>
#include <string.h>

#define USBRH_VENDOR  0x1774
#define USBRH_PRODUCT 0x1001

// parameter
// http://www.sensirion.com/en/pdf/product_information/Data_Sheet_humidity_sensor_SHT1x_SHT7x_E.pdf
// http://www.syscom-inc.co.jp/pdf/sht_datasheet_j.pdf
#define d1 -40.00
#define d2 0.01
#define c1 -4
#define c2 0.0405
#define c3 -0.0000028
#define t1 0.01
#define t2 0.00008

// Temperature = d1+d2*SOt
// d1  -40.00
// d2  0.04
double convert_temp(int in)
{
double tmp;

    tmp = 0;

    tmp = d1+d2*in;

    return(tmp);
}

double convert_humidity(int in)
{
double tmp;

    tmp = 0;

    tmp = c1+c2*in+c3*(in*in);

    return((t1-25)*(t1+t2*in)+tmp);
}

void dump(unsigned char *in, int length)
{
int i;

    for(i=1;i<length+1;i++){
        if(!(i%16)){
            printf("%02x\n",(unsigned int)in[i-1]);
        } else
        if(!(i%8)){
            printf("%02x-",(unsigned int)in[i-1]);
        } else {
            printf("%02x ", (unsigned int)in[i-1]);
        }
    }
    puts("");

}

struct usb_device *searchdevice(unsigned int vendor, unsigned int product, int num)
{
struct usb_bus *bus;
struct usb_device *dev;
int count;

    count = 0;

    for (bus = usb_get_busses(); bus; bus = bus->next){
        for (dev = bus->devices; dev; dev = dev->next){
    	    if (dev->descriptor.idVendor == vendor &&
	        dev->descriptor.idProduct == product){
                count++;
                if(count==num){
                    return(dev);
                }
	    }
        }
    }

    return((struct usb_device *)NULL);
}

struct usb_device *listdevice(unsigned int vendor, unsigned int product)
{
struct usb_bus *bus;
struct usb_device *dev;
int count;

    count=0;

    puts("listing:USBRH");

    for (bus = usb_get_busses(); bus; bus = bus->next){
        for (dev = bus->devices; dev; dev = dev->next){
    	    if (dev->descriptor.idVendor == vendor &&
	        dev->descriptor.idProduct == product){
                count++;
                printf("%d: bus=%s device=%s\n", count, bus->dirname, dev->filename);
	    }
        }
    }

    printf("%d device(s) found.\n", count);

    return((struct usb_device *)NULL);
}

void usage()
{
    puts("USBRH on Linux 0.06 by Briareos\nusage: usbrh [-vthm1fl]\n"
         "       -v : verbose\n"
         "       -t : temperature (for MRTG2)\n"
         "       -h : humidity (for MRTG2)\n"
         "       -m : temperature/humidity output(for MRTG2)\n"
         "       -1 : 1-line output\n"
         "       -fn: set device number(n>0)\n"
         "       -l : Device list\n"
         "       -Ln : LED Control(red/green : n=0(off/off),1(on/off),2(off/on),3(on/on))\n"
         "       -Hn : Heater Control(0:off, 1:on)\n");
}

int main(int argc, char *argv[])
{
struct usb_device *dev;
usb_dev_handle *dh;
char buff[512];
int rc;
unsigned int iTemperature, iHumidity, opt;
double temperature, humidity;
unsigned char data[8];
char flag_v, flag_t, flag_h, flag_d, flag_1line, flag_mrtg, flag_l, flag_LED, flag_Heater;
char tmpDevice[1];
int  DeviceNum;
char tmpLED[1], tmpHeater[1];
int LEDNum, HeaterNum;
unsigned char is_on;

    dev = NULL;
    dh = NULL;
    rc = 0;
    DeviceNum = 1;
    flag_v = flag_t = flag_h = flag_d = flag_1line = flag_mrtg = flag_l = flag_LED = flag_Heater = 0;
    temperature = humidity = 0;
    LEDNum = -1;
    HeaterNum = -1;
    memset(buff, 0, sizeof(buff));
    memset(data, 0, sizeof(data));
    memset(tmpDevice, 0, sizeof(tmpDevice));
    memset(tmpLED, 0, sizeof(tmpLED));
    memset(tmpHeater, 0, sizeof(tmpHeater));

    while((opt = getopt(argc, argv,"lvth1dmf:L:H:?")) != -1){
        switch(opt){
            case 'v':
                flag_v = 1;
                break;
            case 't':
                if(!flag_h)
                    flag_t = 1;
                break;
            case 'h':
                if(!flag_t)
                    flag_h = 1;
                break;
            case 'm':
                if(!flag_mrtg)
                    flag_mrtg = 1;
                break;
            case '1':
                flag_1line = 1;
                break;
            case 'd':
                flag_d = 1;
                break;
            case 'f':
                strncpy(tmpDevice, optarg, sizeof(tmpDevice));
                DeviceNum = atoi(tmpDevice);
                break;
            case 'l':
                flag_l = 1;
                break;
            case 'L':
                flag_LED = 1;
                strncpy(tmpLED, optarg, sizeof(tmpLED));
                LEDNum = atoi(tmpLED);
                break;
            case 'H':
                flag_Heater = 1;
                strncpy(tmpHeater, optarg, sizeof(tmpHeater));
                HeaterNum = atoi(tmpHeater);
                break;
            default:
                usage();
                exit(0);
                break;
        }
    }
    
    if ( flag_LED && !(LEDNum == 0 || LEDNum == 1 || LEDNum == 2 || LEDNum == 3 )) {
        usage();
        exit(0);
    }
    
    if( flag_Heater && !(HeaterNum == 0 || HeaterNum == 1)) {
        usage();
        exit(0);
    }

    usb_init();
    usb_find_busses();
    usb_find_devices();

    if(flag_l){
        listdevice(USBRH_VENDOR, USBRH_PRODUCT);
        exit(0);
    }

    if(flag_d)
        usb_set_debug(5);

    if((dev = searchdevice(USBRH_VENDOR, USBRH_PRODUCT, DeviceNum)) == (struct usb_device *)NULL){
        puts("USBRH not found");
        exit(1);
    } 

    if(flag_d){
        puts("USBRH is found");
    }
    dh = usb_open(dev);
    if(dh == NULL){
        puts("usb_open error");
        exit(2);
    }

    if( ( rc = usb_set_configuration(dh, dev->config->bConfigurationValue) ) < 0){
	if( ( rc = usb_detach_kernel_driver_np(dh, dev->config->interface->altsetting->bInterfaceNumber) ) < 0 ){
        puts("usb_set_configuration error");
        usb_close(dh);
        exit(3);
    }
    }

    if( ( rc = usb_claim_interface(dh, dev->config->interface->altsetting->bInterfaceNumber))<0){
        //puts("usb_claim_interface error");
        if((rc = usb_detach_kernel_driver_np(dh, dev->config->interface->altsetting->bInterfaceNumber))<0){
            puts("usb_detach_kernel_driver_np error");
            usb_close(dh);
            exit(4);
        }else{
            if((rc =usb_claim_interface(dh, dev->config->interface->altsetting->bInterfaceNumber))<0){
                puts("usb_claim_interface error");
                usb_close(dh);
                exit(4);
            }
        }
    }

    // LED control usbrh_control_led(dev, 1, (dev->led >> 1) & 0x1);
    if(flag_LED){
        for( is_on = 0; is_on <= 1; is_on ++ ){ // 0: red 1:green
            memset( buff, 0, sizeof(buff));
            buff[0] = 3 + is_on;
            buff[1] = (LEDNum >> is_on ) & 0x1;
            rc = usb_control_msg(dh, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                                USB_REQ_SET_CONFIGURATION,
                                0x0300, 0, buff, 7, 5000);
        } 
    }
    // Heater control
    if(flag_Heater){
        memset( buff, 0, sizeof(buff));
        buff[0] = 1;
        buff[1] = HeaterNum << 2;
        rc = usb_control_msg(dh, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                            USB_REQ_SET_CONFIGURATION,
                            0x0300, 0, buff, 7, 5000);
    }
    
    // Detect temperature and humidity
    if( !(flag_LED || flag_Heater)) {
        // SET_REPORT
        // http://www.ghz.cc/~clepple/libHID/doc/html/libusb_8c-source.html
        rc = usb_control_msg(dh, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                              0x09, 0x02<<8, 0, buff, 7, 5000);

        if(flag_d){
            if(rc<0){
                puts("usb_control_msg error");
            } else {
                printf("usb_control_msg OK:send bytes:[%d]\n", rc);
                dump((unsigned char*)buff, rc);
            }
        }

        // usb_control_msg() is successed
        if(rc>=0){
            sleep(1);

            // Read data from device
            rc = usb_bulk_read(dh, 1, buff, 7, 5000);
            if(flag_d){
                if(rc<0){
                    puts("usb_bulk_read error");
                } else {
                    printf("usb_bulk_read:[%d] bytes readed.\n", rc);
                    dump((unsigned char*)buff, rc);
                }
            }

            iTemperature = buff[2]<<8|(buff[3]&0xff);
            iHumidity = buff[0]<<8|(buff[1]&0xff);
            if(flag_d){
                printf("convert to integer(temperature):[%02x %02x] -> [%04x]\n", (unsigned int)buff[2], (unsigned int)buff[3], iTemperature);
                printf("convert to integer(humidity):[%02x %02x] -> [%04x]\n", (unsigned int)buff[0], (unsigned int)buff[1], iHumidity);
            }
            temperature = convert_temp(iTemperature);
            humidity    = convert_humidity(iHumidity);
        }

        // Display Result
        if(flag_v){
            printf("Temperature: %.2f C\n", temperature);
            printf("Humidity: %.2f %%\n", humidity);
        }else
        if(flag_t){
            printf("%.2f\n%.2f\n\nTemperature\n", temperature, temperature);
        }else
        if(flag_h){
            printf("%.2f\n%.2f\n\nHumidity\n", humidity, humidity);
        }else
        if(flag_mrtg){
            printf("%.2f\n%.2f\n\nTemperature/Humidity\n", temperature, humidity);
        }else
        if(flag_1line){
            printf("Temperature: %.2f C Humidity: %.2f %%\n", temperature, humidity);
        }else
            printf("%.2f %.2f\n", temperature, humidity);
    }

    if((rc = usb_release_interface(dh, dev->config->interface->altsetting->bInterfaceNumber))<0){
        puts("usb_release_interface error");
        usb_close(dh);
        exit(5);
    }

    usb_close(dh);

    return(0);
}
