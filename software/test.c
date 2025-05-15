#include "common.h"
#include "logicbackend/logic_backend.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        info("Usage: %s hostname port", argv[0]);
        return 1;
    }

    struct server_secrets secrets;
    secrets.server_hostname = argv[1];
    secrets.server_port = argv[2];

    struct logic_backend logic;
    struct program_conf conf;
    conf.useDefaults = true;

    init_logic_backend(&logic, &conf);
    logic_backend_start(&logic, &secrets);
    destroy_logic_backend(&logic);
    return 0;
}