int srv_pid = sfd_spawn("disk0",      /* Server instance name */
                        "/mnt/disk0", /* New server root directory */
                        "/run", /* UNIX socket directory -> /mnt/disk0/run */
                        100,    /* Maximum number of concurrent transfers */
                        10000); /* Open file timeout in milliseconds */
