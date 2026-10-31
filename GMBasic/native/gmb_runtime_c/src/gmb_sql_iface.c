/* gmb_sql_iface.c — installa l'interfaccia del modulo SQL (core -> SQL). */
#include "gmb_sql_iface.h"
#include "gmb_community_db.h"

__attribute__((constructor))
static void gmb_sql_iface_install(void)
{
    gmb_sql_iface.exec_params_raw = gmb_sql_exec_params_raw;
    gmb_sql_iface.set_park_hook   = gmb_sql_set_park_hook;
    gmb_sql_iface.selfcheck       = gmb_sql_selfcheck;
}
