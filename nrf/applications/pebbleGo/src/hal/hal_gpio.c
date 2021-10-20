#include <zephyr.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include "hal_gpio.h"
#include "ui.h"
#include "modem/modem_helper.h"
#include "nvs/local_storage.h"
#include "display.h"


#define GPIO_DIR_OUT  GPIO_OUTPUT
#define GPIO_DIR_IN   GPIO_INPUT
#define GPIO_INT  GPIO_INT_ENABLE
#define GPIO_INT_DOUBLE_EDGE  GPIO_INT_EDGE_BOTH
#define gpio_pin_write  gpio_pin_set


#define UART_COMTOOL  "UART_1"   
#define COMMAND_LENGTH      4
#define COM_HEAD    's'
#define COM_HEAD_LEN   1
#define COM_REV_STR_LEN  4
#define CMD_BUF_LEN      (COM_REV_STR_LEN+1)
#define  HOW_MANY_REGIONS      5
static struct device *guart_dev_comtool;
static unsigned  char rev_buf[CMD_BUF_LEN];
static unsigned char rev_index = 0;
static unsigned char testCmdReved = 0;


struct device *__gpio0_dev;
static u32_t g_key_press_start_time;
static struct gpio_callback chrq_gpio_cb, pwr_key_gpio_cb;

//extern struct k_delayed_work  event_work;

void checkCHRQ(void)
{
    u32_t chrq;    
    chrq = gpio_pin_get(__gpio0_dev, IO_NCHRQ);
    //gpio_pin_write(port, LED_RED, chrq);    
    //gpio_pin_write(port, LED_GREEN, (chrq + 1 ) % 2);
    if(!chrq)
    {// charging
 //       ui_led_active(BAT_CHARGING_MASK,0);
        //gpio_pin_write(__gpio0_dev, LED_GREEN, LED_OFF);
        gpio_pin_write(__gpio0_dev, LED_RED, LED_ON);
        sta_SetMeta(PEBBLE_POWER, STA_LINKER_ON);
    }
    else
    {// not charging
//        ui_led_deactive(BAT_CHARGING_MASK,0);
        gpio_pin_write(__gpio0_dev, LED_RED, LED_OFF);
        //gpio_pin_write(__gpio0_dev, LED_GREEN, LED_ON);
        sta_SetMeta(PEBBLE_POWER, STA_LINKER_OFF);
    }    
}

void CtrlGreenLED(bool on_off) {
    if(on_off)
        gpio_pin_write(__gpio0_dev, LED_GREEN, LED_ON);
    else
        gpio_pin_write(__gpio0_dev, LED_GREEN, LED_OFF);
}

static void chrq_input_callback(struct device *port, struct gpio_callback *cb, u32_t pins) {
    
    //printk("Charge pin %d triggered\n", IO_NCHRQ);

    checkCHRQ();
}

static void pwr_key_callback(struct device *port, struct gpio_callback *cb, u32_t pins) {

    u32_t pwr_key, end_time;
    int32_t key_press_duration, ret;

    //if ((ret = gpio_pin_read(port, POWER_KEY, &pwr_key))) {
    //    return;
    //}
    pwr_key = gpio_pin_get(port, POWER_KEY);

    if (IS_KEY_PRESSED(pwr_key)) {
        g_key_press_start_time = k_uptime_get_32();
        printk("Power key pressed:[%u]\n", g_key_press_start_time);
    }
    else {
        end_time = k_uptime_get_32();
        printk("Power key released:[%u]\n", end_time);

        if (end_time > g_key_press_start_time) {
            key_press_duration = end_time - g_key_press_start_time;
        }
        else {
            key_press_duration = end_time + (int32_t)g_key_press_start_time;
        }

        printk("Power key press duration: %d ms\n", key_press_duration);

        if (key_press_duration > KEY_POWER_OFF_TIME) {
            gpio_pin_write(port, IO_POWER_ON, POWER_OFF);
        }
    }
}

void iotex_hal_gpio_init(void) {

    __gpio0_dev = device_get_binding("GPIO_0");

    /* Set LED pin as output */
    gpio_pin_configure(__gpio0_dev, IO_POWER_ON, GPIO_DIR_OUT);	//p0.31 == POWER_ON
    gpio_pin_configure(__gpio0_dev, LED_GREEN, GPIO_DIR_OUT); 	//p0.00 == LED_GREEN
    gpio_pin_configure(__gpio0_dev, LED_BLUE, GPIO_DIR_OUT);	//p0.01 == LED_BLUE
    gpio_pin_configure(__gpio0_dev, LED_RED, GPIO_DIR_OUT); 	//p0.02 == LED_RED

    gpio_pin_write(__gpio0_dev, IO_POWER_ON, POWER_ON);	//p0.31 == POWER_ON
    gpio_pin_write(__gpio0_dev, LED_GREEN, LED_OFF);	//p0.00 == LED_GREEN ON
    gpio_pin_write(__gpio0_dev, LED_BLUE, LED_OFF);	//p0.00 == LED_BLUE OFF
    gpio_pin_write(__gpio0_dev, LED_RED, LED_OFF);	//p0.00 == LED_RED

    /* USB battery charge pin */
    gpio_pin_configure(__gpio0_dev, IO_NCHRQ,
                       (GPIO_DIR_IN | GPIO_INT |
                        GPIO_INT_EDGE | GPIO_INT_DOUBLE_EDGE |
                        GPIO_INT_DEBOUNCE));

    gpio_init_callback(&chrq_gpio_cb, chrq_input_callback, BIT(IO_NCHRQ));
    gpio_add_callback(__gpio0_dev, &chrq_gpio_cb);
    //gpio_pin_enable_callback(__gpio0_dev, IO_NCHRQ);
    gpio_pin_interrupt_configure(__gpio0_dev, IO_NCHRQ,GPIO_INT_EDGE_BOTH);
    
#if 0
    /* Power key pin configure */
    gpio_pin_configure(__gpio0_dev, POWER_KEY,
                       (GPIO_DIR_IN | GPIO_INT |
                        GPIO_INT_EDGE | GPIO_INT_DOUBLE_EDGE |
                        GPIO_INT_DEBOUNCE));

    gpio_init_callback(&pwr_key_gpio_cb, pwr_key_callback, BIT(POWER_KEY));
    gpio_add_callback(__gpio0_dev, &pwr_key_gpio_cb);
    gpio_pin_enable_callback(__gpio0_dev, POWER_KEY);
#endif
    /* Sync charge state */
    chrq_input_callback(__gpio0_dev, &chrq_input_callback, IO_NCHRQ);
    checkCHRQ();
}


int iotex_hal_gpio_set(uint32_t pin, uint32_t value) {
    return gpio_pin_write(__gpio0_dev, pin, value);
}

void gpio_poweroffNotice(void)
{
    /* qhm add 0830*/
    gpio_pin_write(__gpio0_dev, LED_GREEN, LED_ON);
    k_sleep(K_MSEC(300));
    gpio_pin_write(__gpio0_dev, LED_GREEN, LED_OFF);
    k_sleep(K_MSEC(300));
    gpio_pin_write(__gpio0_dev, LED_GREEN, LED_ON);
    k_sleep(K_MSEC(300));
    gpio_pin_write(__gpio0_dev, LED_GREEN, LED_OFF);
    k_sleep(K_MSEC(300));
}

void gpio_poweroff(void)
{
    /* qhm add 0830*/
    gpio_pin_write(__gpio0_dev, LED_RED, LED_ON);
    k_sleep(K_MSEC(300));
    gpio_pin_write(__gpio0_dev, LED_RED, LED_OFF);
    k_sleep(K_MSEC(300));
    gpio_pin_write(__gpio0_dev, LED_RED, LED_ON);
    k_sleep(K_MSEC(300));
    gpio_pin_write(__gpio0_dev, LED_RED, LED_OFF);
    k_sleep(K_MSEC(300));
    /*end of qhm add 0830*/
    gpio_pin_write(__gpio0_dev, IO_POWER_ON, POWER_OFF);
}

void PowerOffIndicator(void)
{
    int  i;
    //ui_leds_stop();
    for(i =0;i<3;i++){
        gpio_pin_write(__gpio0_dev, LED_RED, LED_ON);
        k_sleep(K_MSEC(1000));
        gpio_pin_write(__gpio0_dev, LED_RED, LED_OFF);
        k_sleep(K_MSEC(1000));
    }
    gpio_poweroff();
    k_sleep(K_MSEC(5000));
}

// read SN  in pebbleGo firmware
static void uart_comtool_rx_handler(u8_t character) {
    if(rev_index < COMMAND_LENGTH) {
        rev_buf[rev_index++]=character;
        if(rev_buf[0] != COM_HEAD)
            rev_index = 0;
        if(rev_index == COMMAND_LENGTH) {
            //uart_rx_disable(guart_dev_comtool);
            rev_index = 0;            
            rev_buf[COM_REV_STR_LEN] = 0;
            testCmdReved = 1;         
        }           
    }
}


static void uart_comtool_cb(struct device *dev) {
    u8_t character;
    while (uart_fifo_read(dev, &character, 1)) {
        uart_comtool_rx_handler(character);
    }    
}

void ComToolInit(void) { 
    guart_dev_comtool=device_get_binding(UART_COMTOOL);
    uart_irq_callback_set(guart_dev_comtool, uart_comtool_cb);
    rev_index = 0; 
    uart_irq_rx_enable(guart_dev_comtool);
    uart_irq_tx_disable(guart_dev_comtool);   
}
void stopTestCom(void) {
    uart_irq_rx_disable(guart_dev_comtool);
}
static bool isLegacySymble(uint8_t c) {
    if(((c>='0') && (c<='9')) || ((c>='A') && (c <= 'Z')))
        return true;
    else    
        return false;
}
static bool isActiveSn(uint8_t *sn) {
    uint32_t i;
    for(i = 0; i < 10; i++)
    {
        if(!isLegacySymble(sn[i]))
            return false;
    }
    return true;
}

static bool snExist(uint8_t *sn)
{
    uint8_t buf[300];
    uint8_t *pbuf;
    //PebbleSYSParam devparam;
    uint8_t devSN[20];

    memset(buf, 0, sizeof(buf));
    pbuf = ReadDataFromModem(PEBBLE_DEVICE_SN, buf, sizeof(buf));     
    if(pbuf != NULL)
    {        
        memcpy(devSN, pbuf, 10);        
        devSN[10] = 0;
        printk("SN:%s\n", devSN);
        //printk("devparam.app_ver[0]:%c,devparam.app_ver[1]:%c",devparam.app_ver[0],devparam.app_ver[1]);
        if(isActiveSn(devSN)){
            memcpy(sn, devSN,10);
            return true;
        }
    }
    return false;
}
static void comtoolOut(uint8_t *buf, uint32_t len)
{
    uint32_t i;
    for(i = 0; i < len; i++)
    {
        uart_poll_out(guart_dev_comtool,buf[i]); 
    }
    
}
void outputSn(void) {
    uint8_t sn[11];
    unsigned char *pub;
    uint8_t package[200];

    if(testCmdReved){
        testCmdReved = 0;
        if(!strcmp(&rev_buf[0], "secc")) {
            memset(sn,0, sizeof(sn));
            pub = readECCPubKey();
            if(pub == NULL)
                pub = "";            
            if(snExist(sn))
                snprintf(package, sizeof(package), "decc device-id:%s sn:%s ECC-Pub-Key_HexString(mpi_X,mpi_Y):%s ",iotex_mqtt_get_client_id(),sn,pub);
            else
                snprintf(package, sizeof(package), "decc device-id:%s sn:%s ECC-Pub-Key_HexString(mpi_X,mpi_Y):%s ",iotex_mqtt_get_client_id(),"",pub);
            comtoolOut(package,strlen(package));
        } 
    }
}



