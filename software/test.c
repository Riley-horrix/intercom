#include "common.h"
#include "logicbackend/logic_backend.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        info("Usage: %s hostname", argv[0]);
        return 1;
    }

    struct server_secrets secrets;
    secrets.server_hostname = argv[1];
    secrets.server_port = NULL;

    logic_backend_start(NULL, &secrets);

    return 0;
}