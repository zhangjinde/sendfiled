int srv_pid = sfd_spawn("disk0",    /* server instance name */
                        "/usr/bin", /* directory containing sfd binary */
                        100,        /* maximum number of concurrent transfers */
                        10000);     /* open file timeout */
