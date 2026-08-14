
/**  IMU Controller  */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu_controller, LOG_LEVEL_INF);

#define ROTATION_STEP_DEG          8.0f
#define ROTATION_POLL_INTERVAL_MS  20
#define INACTIVITY_TIMEOUT_MS      2000  
#define BUTTON_DEBOUNCE_MS         40
#define LED_BLINK_ON_MS            120
#define LED_BLINK_GAP_MS           120

static const struct gpio_dt_spec led    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct device *bmi270_dev  = DEVICE_DT_GET(DT_NODELABEL(bmi270));

static struct gpio_callback button_cb_data;


static bool          m_ble_on   = false;
static float         m_accum_deg = 0.0f;
static struct bt_conn *m_conn   = NULL;
static bool           m_notify_enabled = false;

static void rotation_poll_work_handler(struct k_work *work);
static void inactivity_work_handler(struct k_work *work);
static void button_debounce_work_handler(struct k_work *work);
static void motion_trigger_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(rotation_poll_work, rotation_poll_work_handler);
K_WORK_DELAYABLE_DEFINE(inactivity_work, inactivity_work_handler);
K_WORK_DELAYABLE_DEFINE(button_debounce_work, button_debounce_work_handler);
K_WORK_DEFINE(motion_trigger_work, motion_trigger_work_handler);

static void led_on(void)  { gpio_pin_set_dt(&led, 1); }
static void led_off(void) { gpio_pin_set_dt(&led, 0); }

static void led_blink(uint8_t times)
{
    for (uint8_t i = 0; i < times; i++)
    {
        led_on();
        k_msleep(LED_BLINK_ON_MS);
        led_off();
        if (i + 1 < times)
        {
            k_msleep(LED_BLINK_GAP_MS);
        }
    }
}

static const uint8_t hid_report_map[] = {
    0x05, 0x0C,     
    0x09, 0x01,       
    0xA1, 0x01,      
    0x85, 0x01,      
    0x15, 0x00,    
    0x25, 0x01,       
    0x75, 0x01,   
    0x95, 0x02,      
    0x09, 0xE9,    
    0x09, 0xEA,      
    0x81, 0x02,     
    0x95, 0x01,     
    0x75, 0x06,      
    0x81, 0x01,      
    0xC0            
};

static uint8_t hid_info[] = { 0x11, 0x01, 0x00, 0x02 }; 
static uint8_t hid_report_ref[] = { 0x01, 0x01 };      
static uint8_t hid_ctrl_point;

static void hid_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    m_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("HID notifications %s", m_notify_enabled ? "enabled" : "disabled");
}

static ssize_t read_hid_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, hid_info, sizeof(hid_info));
}

static ssize_t read_report_map(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, hid_report_map, sizeof(hid_report_map));
}

static ssize_t read_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset)
{
    uint8_t idle = 0;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &idle, sizeof(idle));
}

static ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len == sizeof(hid_ctrl_point))
    {
        hid_ctrl_point = *((uint8_t *)buf);
    }
    return len;
}
BT_GATT_SERVICE_DEFINE(hid_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
                            BT_GATT_PERM_READ, read_hid_info, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
                            BT_GATT_PERM_READ, read_report_map, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                            BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                            BT_GATT_PERM_READ,
                            read_report, NULL, NULL),
    BT_GATT_CCC(hid_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
                        NULL, NULL, hid_report_ref),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                            BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                            BT_GATT_PERM_WRITE, NULL, write_ctrl_point, NULL),
);

typedef enum { HID_VOLUME_UP, HID_VOLUME_DOWN } hid_key_t;

static void hid_send_key(hid_key_t key)
{
    if (!m_conn || !m_notify_enabled)
    {
        return;
    }
    uint8_t press = (key == HID_VOLUME_UP) ? 0x01 : 0x02;
    bt_gatt_notify(m_conn, &hid_svc.attrs[6], &press, sizeof(press));
    uint8_t release = 0;
    bt_gatt_notify(m_conn, &hid_svc.attrs[6], &release, sizeof(release));
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_WRN("Connection failed (err %u)", err);
        return;
    }
    m_conn = bt_conn_ref(conn);
    LOG_INF("Connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);
    if (m_conn)
    {
        bt_conn_unref(m_conn);
        m_conn = NULL;
    }
    m_notify_enabled = false;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static const struct bt_data adv_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void motion_trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
    k_work_submit(&motion_trigger_work);
}

static void motion_trigger_work_handler(struct k_work *work)
{
    if (!m_ble_on)
    {
        return; 
    }

    m_accum_deg = 0.0f;
    k_work_schedule(&rotation_poll_work, K_MSEC(ROTATION_POLL_INTERVAL_MS));
    k_work_reschedule(&inactivity_work, K_MSEC(INACTIVITY_TIMEOUT_MS));
}
static void rotation_poll_work_handler(struct k_work *work)
{
    if (!m_ble_on)
    {
        return;
    }
    struct sensor_value gyro[3];
    if (sensor_sample_fetch_chan(bmi270_dev, SENSOR_CHAN_GYRO_XYZ) == 0 &&
        sensor_channel_get(bmi270_dev, SENSOR_CHAN_GYRO_XYZ, gyro) == 0)
    {
        double dps = sensor_value_to_double(&gyro[2]) * (180.0 / 3.14159265358979);
        double dt_s = ROTATION_POLL_INTERVAL_MS / 1000.0;
        m_accum_deg += (float)(dps * dt_s);
        if (m_accum_deg >= ROTATION_STEP_DEG)
        {
            hid_send_key(HID_VOLUME_UP);
            m_accum_deg -= ROTATION_STEP_DEG;
            k_work_reschedule(&inactivity_work, K_MSEC(INACTIVITY_TIMEOUT_MS));
        }
        else if (m_accum_deg <= -ROTATION_STEP_DEG)
        {
            hid_send_key(HID_VOLUME_DOWN);
            m_accum_deg += ROTATION_STEP_DEG;
            k_work_reschedule(&inactivity_work, K_MSEC(INACTIVITY_TIMEOUT_MS));
        }
    }
    k_work_schedule(&rotation_poll_work, K_MSEC(ROTATION_POLL_INTERVAL_MS));
}

static void inactivity_work_handler(struct k_work *work)
{
    k_work_cancel_delayable(&rotation_poll_work);
}

static void ble_turn_on(void)
{
    m_ble_on = true;

    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
    if (err)
    {
        LOG_ERR("Advertising failed to start (err %d)", err);
    }

    led_blink(2);
}

static void ble_turn_off(void)
{
    m_ble_on = false;

    k_work_cancel_delayable(&rotation_poll_work);
    k_work_cancel_delayable(&inactivity_work);

    bt_le_adv_stop();
    if (m_conn)
    {
        bt_conn_disconnect(m_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    led_blink(1);
}


static void button_debounce_work_handler(struct k_work *work)
{
    if (gpio_pin_get_dt(&button) == 1) 
    {
        if (m_ble_on) { ble_turn_off(); }
        else          { ble_turn_on();  }
    }
}
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_work_schedule(&button_debounce_work, K_MSEC(BUTTON_DEBOUNCE_MS));
}

static int init_led(void)
{
    if (!gpio_is_ready_dt(&led)) { return -ENODEV; }
    return gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
}
static int init_button(void)
{
    if (!gpio_is_ready_dt(&button)) { return -ENODEV; }

    int err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err) return err;

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_RISING);
    if (err) return err;

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    return gpio_add_callback(button.port, &button_cb_data);
}
static int init_imu(void)
{
    if (!device_is_ready(bmi270_dev))
    {
        LOG_ERR("BMI270 device not ready");
        return -ENODEV;
    }
    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_MOTION,
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };
    return sensor_trigger_set(bmi270_dev, &trig, motion_trigger_handler);
}

int main(void)
{
    int err;

    err = init_led();
    if (err) { LOG_ERR("LED init failed (%d)", err); return err; }

    err = init_button();
    if (err) { LOG_ERR("Button init failed (%d)", err); return err; }

    err = bt_enable(NULL);
    if (err) { LOG_ERR("bt_enable failed (%d)", err); return err; }

    err = init_imu();
    if (err)
    {
        while (true)
        {
            led_on();  k_msleep(100);
            led_off(); k_msleep(100);
        }
    }
    return 0;
}