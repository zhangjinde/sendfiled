int status = sfd_shutdown(srv_pid);

if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
    log(ERR, "sendfiled server has not shut down cleanly");
