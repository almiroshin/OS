#include "tinyos.h"

int tinyos_main(tinyos_api_t *api)
{
    (void)api;

    tinyos_write("HELLO FROM C .APP");
    tinyos_write("THIS PROGRAM WAS BUILT WITH LIBTINYOS");
    tinyos_proc_info();
    tinyos_mem_info();
    tinyos_list_dir("C:\\APPS");
    tinyos_write("C APP RETURNING TO LOADER");
    return 0;
}
