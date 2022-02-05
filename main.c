#include <stdio.h>
#include "libipinfo.h"

int main (){
    char *intranet = IntranetIP();
    printf("intranet: %s\n", intranet);

    char *host = "59.125.100.235";
    char *port = "3478";
    char *sourceip = SourceIP(host, port);
    printf("sourceip: %s\n", sourceip);
    return 0;
}