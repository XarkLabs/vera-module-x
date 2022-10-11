#include "usb.h"
//#include <libusb.h>
#include "ftd2xx.h"

#include <assert.h>

#define STR1(x) #x
#define STR(x) STR1(x)
#define FT_CHECK(x)                                                                                          \
    do                                                                                                       \
    {                                                                                                        \
        ftdi_status = (x);                                                                                   \
        if (ftdi_status != FT_OK)                                                                            \
            printf("FTDI: %s returned %d in %s:%d\n", STR(x), (int32_t)ftdi_status, __FUNCTION__, __LINE__); \
    } while (0)

#define LOG(...)                                    \
    do                                              \
    {                                               \
        printf("LOG %s(%d): ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                        \
        printf("\n");                               \
    } while (0)

FT_STATUS ftdi_status;
FT_HANDLE ftdi;
uint8_t ftdi_orig_latency = 16;
const uint8_t ftdi_new_latency = 2;

//int32_t CBUS_IO;
//int32_t ABBM_IO;

#define PIN_TX 0x01  /* Orange wire on FTDI cable */
#define PIX_RX 0x02  /* Yellow */
#define PIN_RTS 0x04 /* Green */
#define PIN_CTS 0x08 /* Brown */
#define PIN_DTR 0x10
#define PIN_DSR 0x20
#define PIN_DCD 0x40
#define PIN_RI 0x80

#define IO_FPGA_CDONE PIN_DCD // input
#define IO_FPGA_RESET PIN_RTS // output NOTE: same pin to reset FPGA and select flash!
#define IO_FPGA_SSEL PIN_RTS  // output NOTE: same pin to reset FPGA and select flash!
#define IO_SPI_SCK PIN_TX     // output
#define IO_SPI_MISO PIX_RX    // input
#define IO_SPI_MOSI PIN_DTR   // output

uint8_t io_spi_ddrout = IO_FPGA_RESET | IO_FPGA_SSEL | IO_SPI_SCK | IO_SPI_MOSI;
uint8_t io_data_out;

enum iomode
{
    IOMODE_IN = 0,
    IOMODE_OUT = 1
};

static enum spi_mode current_mode = SPI_MODE_FLASH;
static bool spi_active = false;

static DWORD written;

void ListFTDIDevices(void)
{
    static const char *FTDI_Type[] =
        {
            "FTDI-BM   ",
            "FTDI-AM   ",
            "FTDI-100AX",
            "FTDI-?    ",
            "FTDI-2232C",
            "FTDI-232R ",
            "FTDI-2232H",
            "FTDI-4232H",
            "FTDI-232H ",
            "FTDI-X    "};

    DWORD num_devices = 0;
    FT_DEVICE_LIST_INFO_NODE *dev_info;
    uint32_t i;

    printf("Available FTDI devices: (* = possible VERA programming device)\n");
    FT_CHECK(FT_CreateDeviceInfoList(&num_devices));

    if (ftdi_status != FT_OK)
    {
        printf("\nError: Can't list devices (error #%d)\n", (int)ftdi_status);
        return;
    }

    if (num_devices == 0)
    {
        printf(" (no devices found)\n");
        return;
    }

    dev_info = (FT_DEVICE_LIST_INFO_NODE *)malloc(num_devices * sizeof(FT_DEVICE_LIST_INFO_NODE));

    if (!dev_info)
    {
        printf("\nError: Can't allocate memory for %d devices.\n", (int)num_devices);
        return;
    }

    FT_CHECK(FT_GetDeviceInfoList(dev_info, &num_devices));
    if (ftdi_status != FT_OK)
    {
        printf("\nError: Get get device list (error #%d)\n", (int)ftdi_status);
        return;
    }

    for (i = 0; i < num_devices; i++)
    {
        bool suitable = (dev_info[i].Type == FT_DEVICE_232R || dev_info[i].Type == FT_DEVICE_232H || dev_info[i].Type == FT_DEVICE_2232H || dev_info[i].Type == FT_DEVICE_X_SERIES);
        printf("%2d %c %s - %-40.40s #%s\n", i, suitable ? '*' : ' ', dev_info[i].Type <= FT_DEVICE_X_SERIES ? FTDI_Type[dev_info[i].Type] : "FTDI-??   ", dev_info[i].Description, dev_info[i].SerialNumber);
    }
    printf("\n");
}

void closeFTDIDevice(void)
{
    printf("Closing...\n");
    Sleep(2000);
    static uint8_t def_handshake = 0x04 | 0x01;
    if (ftdi != 0)
    {
        FT_CHECK(FT_GetLatencyTimer(ftdi, &ftdi_orig_latency));
        FT_CHECK(FT_SetBitMode(ftdi, 0, FT_BITMODE_ASYNC_BITBANG)); // all input
        FT_CHECK(FT_Write(ftdi, &def_handshake, 1, &written));
        if (written != 1)
            printf("%s(%d): short FT_Write result (%d vs %d)?\n", __FUNCTION__, __LINE__, (int32_t)written, 1);

        Sleep(1);
        FT_CHECK(FT_ResetPort(ftdi));

        FT_Close(ftdi);
    }
    ftdi = 0;
}

int32_t openFTDIDevice(int devicenum, char *serialstr)
{
    printf("Openining FTDI device ");
    if (serialstr)
        printf("w/serial #%s...", serialstr);
    else
        printf("index %d...", devicenum);

    fflush(stdout);

    if (serialstr)
        ftdi_status = FT_OpenEx(serialstr, FT_OPEN_BY_SERIAL_NUMBER, &ftdi);
    else
        ftdi_status = FT_Open(devicenum, &ftdi);

    if (ftdi_status != FT_OK || ftdi == 0)
    {
        printf("failed.\n");
        return -1;
    }

    printf("success.\n");

    FT_DEVICE ftDevice;
    DWORD deviceID;
    const char *DevType = "unknown";
    char SerialNumber[16];
    char Description[64];

    FT_CHECK(FT_GetDeviceInfo(ftdi, &ftDevice, &deviceID, SerialNumber, Description, NULL));
    if (ftDevice == FT_DEVICE_232R)
        DevType = "FT232R"; // device is FT232R
    else if (ftDevice == FT_DEVICE_232H)
        DevType = "FT232H"; // device is FT232H
    else if (ftDevice == FT_DEVICE_2232H)
        DevType = "FT2232H"; // device is FT2232H
    else
    {
        printf("Unknown FTDI device detected 0x%x - \"%s\".\n", (uint32_t)ftDevice, Description);
        return -1;
    }

    printf("Using FTDI-%s USB-ID:%08x \"%s\" #%s\n", DevType, (uint32_t)deviceID, Description, SerialNumber);

    fflush(stdout);
    atexit(closeFTDIDevice);

    if (FT_ResetPort(ftdi) != FT_OK)
    {
        closeFTDIDevice();
        printf("FTDI ResetPort failed: Check cable, connection and FTDI driver.\n");
        return -1;
    }

    // set minimum latency
    FT_CHECK(FT_GetLatencyTimer(ftdi, &ftdi_orig_latency));
    FT_CHECK(FT_SetLatencyTimer(ftdi, ftdi_new_latency));

#if 0
    static uint8_t in_byte;

    // Do a sanity check for CBUS GPIO for CB0-CB2
    FT_CHECK(FT_SetBitMode(ftdi, CBUS_IO | 0x0, FT_BITMODE_CBUS_BITBANG)); // set CBUS 0 2 & 3 to zero
    FT_CHECK(FT_GetBitMode(ftdi, &in_byte));                               // read CBUS outputs
    if ((in_byte & testpins) != 0)
    {
        closeFTDIDevice();
        printf("FTDI Check failed: Wrote CBUS 0x0 but read 0x%1X\n", in_byte & testpins);
        return -2;
    }
    FT_CHECK(FT_SetBitMode(ftdi, CBUS_IO | testpins, FT_BITMODE_CBUS_BITBANG)); // set CBUS 0 2 & 3 to one
    FT_CHECK(FT_GetBitMode(ftdi, &in_byte));                                    // read CBUS outputs
    if ((in_byte & testpins) != testpins)
    {
        closeFTDIDevice();
        printf("FTDI Check failed: Wrote CBUS 0x%1X but read 0x%1X\n", testpins, in_byte & testpins);
        return -2;
    }
#endif
    FT_CHECK(FT_SetBitMode(ftdi, io_spi_ddrout, FT_BITMODE_ASYNC_BITBANG));

    return (0);
}

// FTDI

void flushPort()
{
}

void udelay(int usec)
{
    flushPort();

    int ms = usec / 1000;

    if (ms < 1)
    {
        ms = 1;
    }

    Sleep(ms);
}

void io_set_mode(int pin, enum iomode mode)
{
    LOG("io_set_mode(pin=%d, mode=%s)", pin, mode == IOMODE_IN ? "IN" : "OUT");
    if (mode == IOMODE_IN)
        io_spi_ddrout &= ~pin;
    else
        io_spi_ddrout |= pin;

    FT_CHECK(FT_SetBitMode(ftdi, io_spi_ddrout, FT_BITMODE_ASYNC_BITBANG));
}

void io_out(int pin, bool state)
{
    LOG("io_out(pin=%d, state=%d)", pin, state);
    if (state)
        io_data_out |= pin;
    else
        io_data_out &= ~pin;

    FT_CHECK(FT_Write(ftdi, &io_data_out, 1, &written));
    if (written != 1)
    {
        LOG("write %u, expected %u", (uint32_t)written, 1);
    }
}

#define CMD_EPIN_ADDR (0x83)
#define CMD_EPOUT_ADDR (0x04)

// static libusb_context *ctx = NULL;
// static libusb_device_handle *handle = NULL;

void usb_init(void)
{
    LOG("usb_init(void)");
    // int result;
    // result = libusb_init(&ctx);
    // if (result != 0)
    // {
    //     fprintf(stderr, "error initializing libusb\n");
    //     exit(EXIT_FAILURE);
    // }

    // handle = libusb_open_device_with_vid_pid(ctx, 0xc0de, 0xbabe);
    // if (handle == NULL)
    // {
    //     fprintf(stderr, "error opening device\n");
    //     exit(EXIT_FAILURE);
    // }

    // if (libusb_claim_interface(handle, 2) != 0)
    // {
    //     fprintf(stderr, "error claiming usb interface!\n");
    //     exit(EXIT_FAILURE);
    // }
}

void usb_deinit(void)
{
    LOG("usb_deinit(void)");

    // closeFTDIDevice();

    spi_active = false;
}

static void spi_init(void)
{
    if (spi_active)
    {
        return;
    }
    LOG("spi_init()");

    io_set_mode(IO_FPGA_SSEL, IOMODE_OUT);
    io_set_mode(IO_SPI_SCK, IOMODE_OUT);
    io_set_mode(IO_SPI_MOSI, IOMODE_OUT);
    io_set_mode(IO_SPI_MISO, IOMODE_IN);

    io_out(IO_FPGA_SSEL, 0);
    io_out(IO_SPI_SCK, 0);
    io_out(IO_SPI_MOSI, 0);

    spi_active = true;
}

void spi_set_mode(enum spi_mode mode)
{
    LOG("spi_set_mode(mode=%d)", mode);

    assert(SPI_MODE_FLASH == mode); // only mode supported

    spi_init();

    // for (int t = 0; t < 5; t++)
    // {
    //     io_out(IO_FPGA_SSEL, 0);
    //     udelay(500 * 1000);
    //     io_out(IO_FPGA_SSEL, 1);
    //     udelay(500 * 1000);
    // }

    // {
    //     spi_select(1);
    //     uint8_t cmd = 0xAB;
    //     spi_transfer(&cmd, NULL, 1);
    //     spi_select(0);
    // }
}

bool get_cdone(void)
{
    LOG("get_cdone(void)- N/A");
    // int transferred = 0;
    // uint8_t cmd = CMD_GET_CDONE;
    // libusb_bulk_transfer(handle, CMD_EPOUT_ADDR, &cmd, 1, &transferred, 0);

    // uint8_t result = 0;
    // libusb_bulk_transfer(handle, CMD_EPIN_ADDR, &result, 1, &transferred, 0);

    // return result != 0;
    return false;
}

void start_mass_erase(void)
{
    LOG("start_mass_erase(void) - N/A");
    // int transferred = 0;
    // uint8_t cmd = CMD_MASS_ERASE;
    // libusb_bulk_transfer(handle, CMD_EPOUT_ADDR, &cmd, 1, &transferred, 0);
}

// from spi.c

void spi_select(bool on)
{
    if (current_mode == SPI_MODE_FLASH)
    {
        // printf("FPGA_SSEL: %d\n", on);
        io_out(IO_FPGA_SSEL, on);
        udelay(2);
    }
}

void spi_tx_bit_bang(const void *tx_buf, size_t length)
{
    // printf("spi_tx_bit_bang: %u\n", length);
    if (length == 0)
    {
        return;
    }
    const uint8_t *tx_buf8 = (const uint8_t *)tx_buf;

    while (length--)
    {
        uint8_t data = *(tx_buf8++);
        for (unsigned i = 0; i < 8; i++)
        {
            io_out(IO_SPI_SCK, 0);
            io_out(IO_SPI_MISO, (data & (1 << (7 - i))) != 0);
            io_out(IO_SPI_SCK, 1);
        }
    }
}

void spi_transfer(const void *tx_buf, void *rx_buf, size_t length)
{
    if (length == 0)
    {
        return;
    }

    if (spi_active)
    {
        // printf("spi_transfer: %u\n", length);
        //        assert(false);
        return;

        uint8_t *rx_buf8 = (uint8_t *)rx_buf;
        const uint8_t *tx_buf8 = (const uint8_t *)tx_buf;

        if (tx_buf8 && rx_buf8)
        {
            while (length--)
            {
                // while ((SPI1->SR & SPI_SR_TXE) == 0)
                // {
                // }
                // *((volatile uint8_t *)&SPI1->DR) = *(tx_buf8++);

                // while ((SPI1->SR & SPI_SR_RXNE) == 0)
                // {
                // }
                // *(rx_buf8++) = *((volatile uint8_t *)&SPI1->DR);
            }
        }
        else if (tx_buf8)
        {
            while (length--)
            {
                // while ((SPI1->SR & SPI_SR_TXE) == 0)
                // {
                // }
                // *((volatile uint8_t *)&SPI1->DR) = *(tx_buf8++);

                // while ((SPI1->SR & SPI_SR_RXNE) == 0)
                // {
                // }
                // *((volatile uint8_t *)&SPI1->DR);
            }
        }
        else if (rx_buf8)
        {
            while (length--)
            {
                // while ((SPI1->SR & SPI_SR_TXE) == 0)
                // {
                // }
                // *((volatile uint8_t *)&SPI1->DR) = 0xFF;

                // while ((SPI1->SR & SPI_SR_RXNE) == 0)
                // {
                // }
                // *(rx_buf8++) = *((volatile uint8_t *)&SPI1->DR);
            }
        }
        else
        {
            while (length--)
            {
                // while ((SPI1->SR & SPI_SR_TXE) == 0)
                // {
                // }
                // *((volatile uint8_t *)&SPI1->DR) = 0xFF;

                // while ((SPI1->SR & SPI_SR_RXNE) == 0)
                // {
                // }
                // *((volatile uint8_t *)&SPI1->DR);
            }
        }
    }
    else
    {
        if (tx_buf)
        {
            spi_tx_bit_bang(tx_buf, length);
        }
    }
}
