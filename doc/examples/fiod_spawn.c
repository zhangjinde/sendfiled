int srv_pid = fiod_spawn("disk0",    /* server instance name */
                         "/usr/bin", /* directory containing fiod binary */
                         100,        /* maximum number of concurrent transfers */
                         10000);     /* open file timeout */
