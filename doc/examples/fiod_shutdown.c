int status = fiod_shutdown(srv_pid);

if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
   log(ERR, "fiod server has not shut down cleanly");
