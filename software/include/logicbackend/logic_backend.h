#ifndef SRC_LOGIC_BACKEND_SRC
#define SRC_LOGIC_BACKEND_SRC

#include "audiobackend/audio_backend.h"

struct hardware_config {
    int dial_gpio_pin;
    int dial_normal_state;
    int ringer_pwm_pin;
};

struct logic_backend {
    struct audio_backend* audio;
    struct sockaddr_in serverAddr;
    struct sockaddr_in udpServerAddr;
    struct hardware_config hardware;
};

struct server_secrets {
    const char* server_hostname;
    const char* server_port;
    const char* udp_port;
};

extern void init_logic_backend(struct logic_backend* logic, struct program_conf* config);
extern void destroy_logic_backend(struct logic_backend* logic);

extern int logic_backend_start(struct logic_backend* logic, struct server_secrets* secrets);

#endif