#include "mqtt/MQTTClient.h"
#include "system/includes.h"
#include "wifi/wifi_connect.h"
#include "app_config.h"
#include <time.h>
#include <sys/time.h>
#include "net_update.h"
#include "json_c/json.h"
#include "json_c/json_tokener.h"
#include "mbedtls/md.h"

#ifdef CONFIG_MQTT_IOT_ENABLE

#define COMMAND_TIMEOUT_MS      10000   //命令超时时间
#define MQTT_TIMEOUT_MS         10000   //接收阻塞时间
#define MQTT_KEEPALIVE_TIME     60000   //心跳时间
#define SEND_BUF_SIZE           1024    //发送buf大小
#define READ_BUF_SIZE           1024    //接收buf大小

//======================MQTT平台后台参数设置============================//
static char mqtt_ssl        = 1;                    //服务器是是否需要加密，0：不需要，1：需要
static char mqtt_version    = 4;                    //服务器版本：3 -> 3.1, 4 -> 3.1.1
static int ser_port         = 8883;                 //服务器端口号：默认不加密为1883，加密为8883
static char clientID[84]    = "c_client";           //服务器clientID
static char address[48]     = "home.test.com";      //服务器域名
static char username[64]    = "hometest11";         //服务器登录用户名
static char password[33]    = "AWu5Xqsu11";         //服务器登录用户密码


static Client client;
static char send_buf[SEND_BUF_SIZE];    //发送buf
static char read_buf[READ_BUF_SIZE];    //接收buf

//消息发布临时buf
static char sendbuf[1024];
static char payload_json[256];
static char mqtt_task_init = 0;
static char mqtt_ota_task_init = 0;

#define MQTT_OTA_URL_MAX_LEN        512
#define MQTT_OTA_VERSION_MAX_LEN    64
#define MQTT_OTA_MD5_MAX_LEN        33

struct mqtt_ota_info {
    char url[MQTT_OTA_URL_MAX_LEN];
    char version[MQTT_OTA_VERSION_MAX_LEN];
    char md5[MQTT_OTA_MD5_MAX_LEN];
    int size;
    char res_ota;
};

int mqtt_dev_info_flash_read(int *server_port,
                             char *client_ID, int client_ID_len,
                             char *address, int address_len,
                             char *username, int username_len,
                             char *password, int password_len,
                             char *product_key, int product_key_len,
                             char *device_name, int device_name_len,
                             char *device_secret, int device_secret_len);

extern int get_update_data(const char *url, char res_ota);

static int mqtt_ota_download_task(void *priv)
{
    struct mqtt_ota_info *ota = (struct mqtt_ota_info *)priv;
    int ret;

    if (!ota) {
        mqtt_ota_task_init = 0;
        return -1;
    }

    printf("MQTT OTA start, type:%s, version:%s, size:%d, md5:%s\n",
           ota->res_ota ? "res" : "app",
           ota->version[0] ? ota->version : "unknown",
           ota->size,
           ota->md5[0] ? ota->md5 : "none");
    printf("MQTT OTA url:%s\n", ota->url);

    ret = get_update_data(ota->url, ota->res_ota);
    printf("MQTT OTA finish, ret:%d\n", ret);

    free(ota);
    mqtt_ota_task_init = 0;
    return ret;
}

static int mqtt_ota_start(json_object *root)
{
    json_object *fw = NULL;
    json_object *obj = root;
    json_object *item = NULL;
    const char *url = NULL;
    const char *version = NULL;
    const char *md5 = NULL;
    const char *type = NULL;
    struct mqtt_ota_info *ota;

    if (json_object_object_get_ex(root, "firmware", &fw) && json_object_is_type(fw, json_type_object)) {
        obj = fw;
    }

    if (json_object_object_get_ex(obj, "url", &item) && json_object_is_type(item, json_type_string)) {
        url = json_object_get_string(item);
    }
    if (json_object_object_get_ex(obj, "version", &item) && json_object_is_type(item, json_type_string)) {
        version = json_object_get_string(item);
    }
    if (json_object_object_get_ex(obj, "md5", &item) && json_object_is_type(item, json_type_string)) {
        md5 = json_object_get_string(item);
    }
    if (json_object_object_get_ex(obj, "type", &item) && json_object_is_type(item, json_type_string)) {
        type = json_object_get_string(item);
    }

    if (!url || strncmp(url, "http", 4) != 0) {
        printf("MQTT OTA invalid url\n");
        return -1;
    }

    if (mqtt_ota_task_init || net_update_request()) {
        printf("MQTT OTA busy\n");
        return -1;
    }

    ota = calloc(1, sizeof(struct mqtt_ota_info));
    if (!ota) {
        printf("MQTT OTA no mem\n");
        return -1;
    }

    strncpy(ota->url, url, sizeof(ota->url) - 1);
    if (version) {
        strncpy(ota->version, version, sizeof(ota->version) - 1);
    }
    if (md5) {
        strncpy(ota->md5, md5, sizeof(ota->md5) - 1);
    }
    if (json_object_object_get_ex(obj, "size", &item) && json_object_is_type(item, json_type_int)) {
        ota->size = json_object_get_int(item);
    }
    if (json_object_object_get_ex(obj, "res_ota", &item) && json_object_is_type(item, json_type_int)) {
        ota->res_ota = json_object_get_int(item);
    }
    if (type && !strcmp(type, "res")) {
        ota->res_ota = 1;
    }

    // printf("MQTT OTA parsed ok\n");
    // printf("  type    : %s\n", type ? type : "null");
    // printf("  version : %s\n", version ? version : "null");
    // printf("  url     : %s\n", url ? url : "null");
    // printf("  md5     : %s\n", md5 ? md5 : "null");
    // printf("  size    : %d\n", ota->size);
    // printf("  res_ota : %d\n", ota->res_ota);

    // mqtt_ota_task_init = 1;
    // if (thread_fork("mqtt_ota", 10, 3 * 1024, 0, NULL, mqtt_ota_download_task, ota) != OS_NO_ERR) {
    //     printf("MQTT OTA thread fork fail\n");
    //     mqtt_ota_task_init = 0;
        free(ota);
        ota = NULL;
    //     return -1;
    // }

    return 0;
}

static void messageArrived(MessageData *data)
{
    char topic[128] = {0};
    char *payload = NULL;
    json_object *root = NULL;
    json_object *cmd_item = NULL;
    const char *cmd = NULL;
    int topic_len = data->topicName->lenstring.len;
    int payload_len = data->message->payloadlen;

    if (topic_len >= sizeof(topic)) {
        topic_len = sizeof(topic) - 1;
    }
    memcpy(topic, data->topicName->lenstring.data, topic_len);

    payload = calloc(1, payload_len + 1);
    if (!payload) {
        printf("MQTT payload no mem\n");
        return;
    }
    memcpy(payload, data->message->payload, payload_len);

    printf("Message arrived on topic (len:%d, topic:%s)\n", data->topicName->lenstring.len, topic);
    printf("MQTT payload (len:%d):%s\n", payload_len, payload);

    root = json_tokener_parse(payload);
    if (root) {
        if (json_object_object_get_ex(root, "cmd", &cmd_item) &&
            json_object_is_type(cmd_item, json_type_string)) {
            cmd = json_object_get_string(cmd_item);
        }
        if (cmd && !strcmp(cmd, "ota")) {
            mqtt_ota_start(root);
        } else if (cmd && !strcmp(cmd, "ping")) {
            printf("MQTT ping received\n");
        } else {
            printf("MQTT unknown cmd:%s\n", cmd ? cmd : "null");
        }
        json_object_put(root);
    } else {
        printf("MQTT payload is not json\n");
    }

    free(payload);
}

int MQTT_Subscribe_Topic(char *SubscribeTopic, int len)
{
    //订阅主题
    int err = MQTTSubscribe(&client, SubscribeTopic, QOS1, messageArrived);
    if (err != 0) {
        printf("MQTTSubscribe fail, err : 0x%x\n", err);
        return err;
    }
    return 0;
}
int MQTT_Publish_Message(char *PublishTopic, char *msg)
{
    MQTTMessage message = {0};

    message.qos = QOS1;
    message.retained = 0;
    message.payload = msg;
    message.payloadlen = strlen(msg) + 1;

    //发布消息
    int err = MQTTPublish(&client, PublishTopic, &message);
    if (err != 0) {
        printf("MQTTPublish fail, err : 0x%x\n", err);
        return err;
    }
    return 0;
}

static int mqtt_task(void)
{
    Network network;
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
    MQTTMessage message = {0};
    int err;
    int loop_cnt = 0;
    char *subscribeTopic = "aiGateway/read/";   //订阅主题
    char *publishTopic = "aiGateway/send/";     //发布消息的主题

    mqtt_task_init = 1;
    while (!sys_connect_net_success()) {
        printf("waitting net ...");
        os_time_dly(100);
    }
    mqtt_task_init = 2;

    if (mqtt_dev_info_flash_read(&ser_port,
                                 clientID, sizeof(clientID),
                                 address, sizeof(address),
                                 username, sizeof(username),
                                 password, sizeof(password),
                                 NULL, 0,
                                 NULL, 0,
                                 NULL, 0) == 0) {
        printf("---> read flash mqtt info : %d, %s , %s , %s , %s\n", ser_port, clientID, address, username, password);
    }

_reconnect:
    //初始化网络接口
    NewNetwork(&network);

    //初始化网络加密
    if (mqtt_ssl) {
        static const char EMQX_CA_CERT[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
"MrY=\n"
"-----END CERTIFICATE-----\n";
        NetWorkSetTLS(&network);
        //加载CA证书
        NetWorkSetTLS_key(&network,
                      EMQX_CA_CERT, strlen(EMQX_CA_CERT),
                      NULL, 0,
                      NULL, 0);
    }

    SetNetworkRecvTimeout(&network, 2000);

    //初始化客户端
    MQTTClient(&client, &network, COMMAND_TIMEOUT_MS, send_buf, sizeof(send_buf), read_buf, sizeof(read_buf));

    printf("---> MQTT connet ...\n");
    //tcp层连接服务器
    err = ConnectNetwork(&network, address, ser_port);
    if (err != 0) {
        printf("ConnectNetwork err : %d\n", err);
        goto exit;
    }

    connectData.willFlag = 0;
    connectData.MQTTVersion = mqtt_version;                        //mqtt版本号
    connectData.clientID.cstring = clientID;                       //客户端id
    connectData.username.cstring = username;                       //连接时的用户名
    connectData.password.cstring = password;                       //连接时的密码
    connectData.keepAliveInterval = MQTT_KEEPALIVE_TIME / 1000;    //心跳时间
    connectData.cleansession = 1;                                  //是否使能服务器的cleansession，0:禁止, 1:使能

    //mqtt层连接,向服务器发送连接请求
    err = MQTTConnect(&client, &connectData);
    if (err != 0) {
        network.disconnect(&network);
        printf("MQTTConnect fail, err : %d\n", err);
        goto exit;
    }
    printf("MQTTConnect:%d\n", err);

    //订阅主题
    err = MQTTSubscribe(&client, subscribeTopic, QOS1, messageArrived);
//    err = MQTTSubscribe(&client, publishTopic, QOS1, messageArrived);
    if (err != 0) {
        MQTTDisconnect(&client);
        network.disconnect(&network);
        printf("MQTTSubscribe fail, err : 0x%x\n", err);
        goto exit;
    }
    printf("---> MQTT subscribeTopic : %s\n", subscribeTopic);
    /*
        //订阅设置回复主题
        err = MQTTSubscribe(&client, subscribeTopic_reply, QOS1, messageArrived);
        if (err != 0) {
            MQTTDisconnect(&client);
            network.disconnect(&network);
            printf("MQTTSubscribe fail, err : 0x%x\n", err);
            goto exit;
        }
    */
    //取消主题订阅
    //MQTTUnsubscribe(&client, subscribeTopic);

    //填充要发送的json数据
    const char *json_str = "{\"massageCode\":2205,\"mac\":\"1962934412\",\"data\":{\"header\":{\"namespace\":\"ai.ConnectedHome.Discovery\",\"name\":\"DiscoverAppliancesRequest\",\"messageId\":\"1\",\"payloadVersion\":\"1\"},\"payload\":{\"accessToken\":\"efwefw\"}}}";
    strncpy(payload_json, json_str, sizeof(payload_json) - 1);
    payload_json[sizeof(payload_json) - 1] = '\0';

    while (sys_connect_net_success()) {
        // if (0 == loop_cnt % 2) {
        //     message.qos = QOS1;
        //     message.retained = 0;
        //     message.payload = sendbuf;
        //     message.payloadlen = strlen(sendbuf) + 1;
        //     //发布消息
        //     err = MQTTPublish(&client, publishTopic, &message);
        //     if (err != 0) {
        //         printf("MQTTPublish fail, err : 0x%x\n", err);
        //     }
        //     printf("MQTTPublish payload:(%s)\n", sendbuf);
        // }
        // loop_cnt++;

        err = MQTTYield(&client, MQTT_TIMEOUT_MS);
        if (err != 0) {
            if (client.isconnected) {
                //断开mqtt层连接
                err = MQTTDisconnect(&client);
                if (err != 0) {
                    printf("MQTTDisconnect fail\n");
                }

                //断开tcp层连接
                network.disconnect(&network);
            }

            printf("MQTT : Reconnecting\n");

            //重新连接
            goto _reconnect;
        }

    }
    MQTTDisconnect(&client);

exit:
    mqtt_task_init = 0;
    return 0;
}

void mqtt_example(void)
{
    printf("mqtt_example ...\n");
    if (!mqtt_task_init) {
        if (thread_fork("mqtt_task", 10, 2 * 1024, 0, NULL, mqtt_task, NULL) != OS_NO_ERR) {
            printf("thread fork fail\n");
        }
    }
}
//late_initcall(mqtt_example);
#endif

