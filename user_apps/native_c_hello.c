#include "tinyos_app.h"

int tinyos_main(tinyos_api_t *api)
{
    api->write("HELLO FROM C .APP");
    api->write("THIS PROGRAM WAS BUILT FROM C");
    api->proc_info();
    api->mem_info();
    api->list_dir("C:\\APPS");
    api->write("C APP RETURNING TO LOADER");
    return 0;
}
